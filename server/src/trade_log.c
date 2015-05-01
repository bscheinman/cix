#include <ck_pr.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "event.h"
#include "trade_data.h"
#include "trade_log.h"

/* XXX: Make configurable */
#define CIX_TRADE_LOG_FILE_SIZE		(((off_t)1) << 20)
#define CIX_TRADE_LOG_FILE_BYTE_SIZE	\
(CIX_TRADE_LOG_FILE_SIZE * sizeof(struct cix_trade_log_data))

#define CIX_TRADE_LOG_PAGE_BASE(P) (((uintptr_t)(P)) & cix_page_mask)

struct cix_trade_log_data {
	cix_execution_id_t exec_id;
	cix_user_id_t buyer;
	cix_user_id_t seller;
	cix_symbol_t symbol;
	cix_quantity_t quantity;
	cix_price_t price;
} CIX_STRUCT_PACKED;

static unsigned long cix_page_size;
static uintptr_t cix_page_mask;

bool
cix_trade_log_init(void)
{
	long r;

	errno = 0;
	r = sysconf(_SC_PAGESIZE);
	if (r < 0) {
		fprintf(stderr, "failed to determine page size\n");
		return false;
	}

	cix_page_size = (unsigned long)r;
	if ((cix_page_size & (cix_page_size - 1)) != 0) {
		fprintf(stderr, "page size is not a power of two\n");
		return false;
	}

	cix_page_mask = ~(cix_page_size - 1);
	return true;
}

/* This assumes that the file has already been closed */
static bool
cix_trade_log_file_open(struct cix_trade_log_manager *manager,
    struct cix_trade_log_file *file)
{
	char path[PATH_MAX];
	int fd;
	bool success = false;

	/* XXX: Add file header */

	/* XXX: Make naming configurable */
	if (snprintf(path, sizeof path, "%s/cixlog_%u", manager->path,
	    manager->file_count) >= sizeof path) {
		fprintf(stderr, "log file path exceeded maximum size\n");
		return false;
	}

	fd = open(path, O_RDWR | O_CREAT | O_TRUNC,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (fd == -1) {
		/* XXX: Make reentrant version of strerror */
		fprintf(stderr, "failed to create log file %s: %s\n",
		    path, strerror(errno));
		return false;
	}

	if (ftruncate(fd, CIX_TRADE_LOG_FILE_BYTE_SIZE) == -1) {
		fprintf(stderr, "failed to expand log file %s: %s\n",
		    path, strerror(errno));
		goto finish;
	}

	file->log.start = mmap(NULL, CIX_TRADE_LOG_FILE_BYTE_SIZE, PROT_WRITE,
	    MAP_SHARED, fd, 0);
	if (file->log.start == MAP_FAILED) {
		fprintf(stderr, "failed to map log file %s: %s\n",
		    path, strerror(errno));
		goto finish;
	}

	file->log.cursor = file->log.start;
	file->log.end = file->log.start + CIX_TRADE_LOG_FILE_BYTE_SIZE;

	printf("replacing %s with %s\n", file->path, path);
	strcpy(file->path, path);

	ck_pr_fence_store();
	ck_pr_store_uint(&file->ready, 1);

	success = true;
	++manager->file_count;

finish:
	while (close(fd) == -1 && errno == EINTR);
	return success;
}

static bool
cix_trade_log_file_close(struct cix_trade_log_file *file)
{
	int r;

	r = munmap(file->log.start, CIX_TRADE_LOG_FILE_SIZE);
	if (r == -1) {
		fprintf(stderr, "failed to unmap log file\n");
	}

	file->log.start = NULL;
	file->log.cursor = NULL;
	file->log.end = NULL;

	return true;
}

static void
cix_trade_log_rotate(struct cix_event *event, cix_event_flags_t flags,
    void *closure)
{
	struct cix_trade_log_manager *manager = closure;
	unsigned int rotate_index = manager->active_file ^ 1;
	struct cix_trade_log_file *file = &manager->files[rotate_index];

	(void)event;
	(void)flags;

	printf("rotating file %s\n", file->path);

	if (cix_trade_log_file_close(file) == false ||
	    cix_trade_log_file_open(manager, file) == false) {
		fprintf(stderr, "failed to rotate log files\n");
	}

	printf("finished rotating file\n");
	return;
}

static void *
cix_trade_log_rotate_thread(void *p)
{
	struct cix_trade_log_manager *manager = p;
	struct cix_event_manager event_manager;

	if (cix_event_manager_init(&event_manager) == false) {
		fprintf(stderr, "failed to initialize trade log event "
		    "manager\n");
		exit(EXIT_FAILURE);
	}

	if (cix_event_add(&event_manager, &manager->update_event) == false) {
		fprintf(stderr,
		    "failed to initialize trade log update thread\n");
		return NULL;
	}

	cix_event_manager_run(&event_manager);
	return NULL;
}

bool
cix_trade_log_manager_init(struct cix_trade_log_manager *manager,
    struct cix_trade_log_config *config)
{
	DIR *dir;

	strncpy(manager->path, config->path, sizeof manager->path);
	if (manager->path[sizeof(manager->path) - 1] != '\0') {
		fprintf(stderr, "log path %s exceeds maximum length\n",
		    config->path);
		return false;
	}

	if ((dir = opendir(manager->path)) == NULL) {
		int r = mkdir(manager->path, S_IRWXU);

		if (r == -1) {
			fprintf(stderr, "failed to create log directory %s: "
			    "%s\n", manager->path, strerror(errno));
			return false;
		}
	} else {
		closedir(dir);
	}

	if (cix_trade_log_file_open(manager, &manager->files[0]) == false ||
	    cix_trade_log_file_open(manager, &manager->files[1]) == false) {
		fprintf(stderr, "failed to initialize log files\n");
		return false;
	}

	if (cix_event_init_managed(&manager->update_event, cix_trade_log_rotate,
	    manager) == false) {
		fprintf(stderr,
		    "failed to initialize trade log update event\n");
		return false;
	}

	manager->active_file = 0;

	if (pthread_create(&manager->rotate_thread, NULL,
	    cix_trade_log_rotate_thread, manager) != 0) {
		fprintf(stderr,
		    "failed to initialize log maintenance thread\n");
		    return false;
	}

	return true;
}

static bool
cix_trade_log_trade_write(const struct cix_execution *exec, void *target)
{
	struct cix_trade_log_data *trade_data = target;
	uintptr_t sync_base = CIX_TRADE_LOG_PAGE_BASE(trade_data);
	uintptr_t sync_end = CIX_TRADE_LOG_PAGE_BASE(
	    (unsigned char *)(trade_data + 1) - 1);
	size_t sync_size = cix_page_size + (sync_end - sync_base);

	/*
	 * XXX: Ensure that log records never cross page boundaries
	 * so that we can skip the above check.
	 */

	trade_data->exec_id = exec->id;
	trade_data->buyer = exec->buyer;
	trade_data->seller = exec->seller;
	memcpy(trade_data->symbol.symbol, exec->symbol.symbol,
	    sizeof trade_data->symbol.symbol);
	trade_data->quantity = exec->quantity;
	trade_data->price = exec->price;

	/*
	 * Flush data asynchronously so that we can continue processing more
	 * trades.  We will still broadcast executions immediately.
	 * The disk serves as the master record of trading activity, so if a
	 * trade was not written to disk before a crash, then that trade is
	 * considered never to have happened.  We will need to implement
	 * recovery procedures to ensure that clients are able to learn
	 * which of their trades were persisted and which were not.
	 */
	if (msync((void *)sync_base, sync_size, MS_ASYNC) == -1) {
		perror("syncing log file");
		return false;
	}

	return true;
}

bool
cix_trade_log_execution(struct cix_trade_log_manager *manager,
    const struct cix_execution *execution)
{
	unsigned int file_index;
	struct cix_trade_log_file *file, *new_file;
	unsigned char *target;

start:
	file_index = manager->active_file;
	file = &manager->files[file_index];

	target = file->log.cursor;
	file->log.cursor += sizeof(struct cix_trade_log_data);
	if (file->log.cursor <= file->log.end) {
		return cix_trade_log_trade_write(execution, target);
	}

	printf("log file %s is full\n", file->path);
	printf("rotating log file\n");

	file->ready = 0;
	file_index ^= 1;

	new_file = &manager->files[file_index];

	while (ck_pr_load_uint(&new_file->ready) == 0);
	ck_pr_fence_acquire();

	manager->active_file = file_index;
	printf("new log file is %s\n", manager->files[file_index].path);
	ck_pr_fence_store_load();
	if (cix_event_managed_trigger(&manager->update_event) == false) {
		fprintf(stderr, "failed to initiate log rotation\n");
		exit(EXIT_FAILURE);
	}

	goto start;
}

bool
cix_trade_log_iterator_init(struct cix_trade_log_iterator *iter,
    const char *path)
{

	strncpy(iter->path, path, sizeof iter->path);
	if (iter->path[sizeof(iter->path) - 1] != '\0') {
		fprintf(stderr, "path %s exceeds maximum length\n", path);
		return false;
	}

	iter->dir = opendir(path);
	if (iter->dir == NULL) {
		fprintf(stderr, "failed to open directory %s: %s\n",
		    path, strerror(errno));
		return false;
	}

	iter->fd = -1;
	iter->data = NULL;
	iter->cursor = NULL;
	iter->size = 0;

	return true;
}

void
cix_trade_log_iterator_destroy(struct cix_trade_log_iterator *iter)
{

	/* XXX */
	closedir(iter->dir);
	return;
}

static bool
cix_trade_log_iterator_file_next(struct cix_trade_log_iterator *iter)
{
	struct dirent entry;
	struct dirent *result;
	char entry_path[PATH_MAX];
	int b;
	struct stat s;

	for (;;) {
		if (readdir_r(iter->dir, &entry, &result) != 0) {
			fprintf(stderr, "failed to read next file\n");
			return false;
		}

		if (result == NULL) {
			return false;
		}

		if (entry.d_type == DT_REG) {
			break;
		}
	}

	b = snprintf(entry_path, sizeof entry_path, "%s/%s", iter->path,
	    entry.d_name);
	if (b < 0) {
		fprintf(stderr, "failed to create file path\n");
		return false;
	} else if ((size_t)b >= sizeof entry_path) {
		fprintf(stderr, "file path exceeded maximum length\n");
		return false;
	}

	iter->fd = open(entry_path, O_RDONLY);
	if (iter->fd == -1) {
		fprintf(stderr, "failed to open file %s: %s\n", entry_path,
		    strerror(errno));
		return false;
	}

	if (fstat(iter->fd, &s) == -1) {
		fprintf(stderr, "failed to stat file %s: %s\n", entry_path,
		    strerror(errno));
		return false;
	}

	iter->size = s.st_size;
	iter->data = mmap(NULL, iter->size, PROT_READ, MAP_PRIVATE,
	    iter->fd, 0);
	if (iter->data == MAP_FAILED) {
		fprintf(stderr, "failed to map file %s: %s\n", entry_path,
		    strerror(errno));
		return false;
	}

	iter->cursor = iter->data;

	return true;
}

bool
cix_trade_log_iterator_next(struct cix_trade_log_iterator *iter,
    struct cix_execution *exec)
{
	struct cix_trade_log_data *data;

	for (;;) {
		if (iter->cursor == NULL) {
			goto next_file;
		}

		if (iter->cursor + sizeof(struct cix_trade_log_data) >
		    iter->data + iter->size) {
			goto next_file;
		}

		data = (struct cix_trade_log_data *)iter->cursor;

		/*
		 * XXX: Need better way to detect end of useful data in file.
		 * This will be easier once we have a real file header.
		 */
		if (data->symbol.symbol[0] != '\0') {
			break;
		}

next_file:
		if (cix_trade_log_iterator_file_next(iter) == false) {
			return false;
		}
	}

	exec->id = data->exec_id;
	exec->buyer = data->buyer;
	exec->seller = data->seller;
	memcpy(exec->symbol.symbol, data->symbol.symbol,
	    sizeof exec->symbol.symbol);
	exec->quantity = data->quantity;
	exec->price = data->price;

	iter->cursor += sizeof(struct cix_trade_log_data);
	return true;
}
