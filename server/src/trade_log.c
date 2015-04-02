#include <ck_pr.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "event.h"
#include "trade_data.h"
#include "trade_log.h"

/* XXX: Make configurable */
#define CIX_TRADE_LOG_FILE_SIZE		(((off_t)1) << 26)

struct cix_trade_log_data {
	cix_execution_id_t exec_id;
	cix_user_id_t buyer;
	cix_user_id_t seller;
	cix_symbol_t symbol;
	cix_quantity_t quantity;
	cix_price_t price;
} CIX_STRUCT_PACKED;

/* This assumes that the file has already been closed */
static bool
cix_trade_log_file_open(struct cix_trade_log_manager *manager,
    struct cix_trade_log_file *file)
{
	char path[PATH_MAX];
	int fd;
	bool success = false;

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

	if (ftruncate(fd, CIX_TRADE_LOG_FILE_SIZE) == -1) {
		fprintf(stderr, "failed to expand log file %s: %s\n",
		    path, strerror(errno));
		goto finish;
	}

	file->log.start = mmap(NULL, CIX_TRADE_LOG_FILE_SIZE, PROT_WRITE,
	    MAP_SHARED, fd, 0);
	if (file->log.start == MAP_FAILED) {
		fprintf(stderr, "failed to map log file %s: %s\n",
		    path, strerror(errno));
		goto finish;
	}

	file->log.cursor = file->log.start;
	file->log.end = file->log.start + CIX_TRADE_LOG_FILE_SIZE;

	ck_pr_fence_store();
	ck_pr_store_uint(&file->ready, 1);

	success = true;

finish:
	while (close(fd) == -1 && errno == EINTR);
	return success;
}

static bool
cix_trade_log_file_close(struct cix_trade_log_file *file)
{
	int r;

	r = munmap(file->log.start, CIX_TRADE_LOG_FILE_SIZE);
	if (r == -1)
		perror("unmapping log file");

	file->log.start = NULL;
	file->log.cursor = NULL;
	file->log.end = NULL;

	ck_pr_store_uint(&file->ready, 0);
	ck_pr_fence_store();

	return true;
}

static void
cix_trade_log_rotate(struct cix_event *event, cix_event_flags_t flags,
    void *closure)
{
	struct cix_trade_log_manager *manager = closure;
	unsigned int rotate_index = (manager->active_file + 1) % 2;
	struct cix_trade_log_file *file = &manager->files[rotate_index];

	if (cix_trade_log_file_close(file) == false ||
	    cix_trade_log_file_open(manager, file) == false) {
		fprintf(stderr, "failed to rotate log files\n");
	}

	return;
}

static void *
cix_trade_log_rotate_thread(void *p)
{
	struct cix_trade_log_manager *manager = p;
	struct cix_event_manager event_manager;

	cix_event_manager_init(&event_manager);
	if (cix_event_add(&event_manager, &manager->update_event) == false) {
		fprintf(stderr,
		    "failed to initialize trade log update thread\n");
		return NULL;
	}

	cix_event_manager_run(&event_manager);
	return NULL;
}

bool
cix_trade_log_init(struct cix_trade_log_manager *manager,
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
	}
	closedir(dir);

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

	trade_data->exec_id = exec->id;
	trade_data->buyer = exec->buyer;
	trade_data->seller = exec->seller;
	memcpy(trade_data->symbol, exec->symbol, sizeof trade_data->symbol);
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
	if (msync(trade_data, sizeof *trade_data, MS_ASYNC) == -1) {
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

	file_index = (file_index + 1) % 2;

	new_file = &manager->files[file_index];

	while (ck_pr_load_uint(&new_file->ready) == 0);
	ck_pr_fence_acquire();

	manager->active_file = file_index;

	goto start;
}
