#ifndef _CIX_TRADE_LOG_H
#define _CIX_TRADE_LOG_H

#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>

#include "event.h"

/*
 * An interface for writing execution data to disk.  Instances of struct
 * cix_trade_log_manager are not thread-safe and therefore each thread
 * responsible for executing trades should use its own manager instance.
 * Calls to the manager are asynchronous and a return value of true does
 * not necessarily indicate that the trade data has been persisted.
 * If there are errors writing data to disk, the log manager will bring down
 * the server because we cannot continue to execute trades if we cannot
 * persist them.
 */

struct cix_trade_log_file {
	struct {
		unsigned char *start;
		unsigned char *cursor;
		unsigned char *end;
	} log;

	unsigned int ready;
	char path[PATH_MAX];
};

struct cix_trade_log_config {
	char *path;
};

struct cix_trade_log_manager {
	/*
	 * Alternate between two log files-- when one fills up,
	 * switch to the other and update the first one with a
	 * new file on a background thread.
	 */
	struct cix_trade_log_file files[2];

	/* The index (into the above array) of the active file */
	unsigned int active_file;

	/*
	 * The number of files that have been used so far.
	 * This is mainly useful for file numbering.
	 */
	unsigned int file_count;

	/* Path for saving log files */
	char path[PATH_MAX];

	/* Triggers when a new log file is needed */
	struct cix_event update_event;

	/* Thread responsible for setting up new log files */
	pthread_t rotate_thread;
};

/*
 * Initialize trade log subsystem.
 * This is required before using any trade logs.
 */
bool cix_trade_log_init(void);

/* Initialize a specific trade log manager instance */
bool cix_trade_log_manager_init(struct cix_trade_log_manager *,
    struct cix_trade_log_config *);

struct cix_execution;

bool cix_trade_log_execution(struct cix_trade_log_manager *,
    const struct cix_execution *);

/*
 * API for reading from trade log files
 */

struct cix_trade_log_file;

struct cix_trade_log_iterator {
	char path[PATH_MAX];
	DIR *dir;
	int fd;
	unsigned char *data;
	unsigned char *cursor;
	size_t size;
};

bool cix_trade_log_iterator_init(struct cix_trade_log_iterator *, const char *);
void cix_trade_log_iterator_destroy(struct cix_trade_log_iterator *);
bool cix_trade_log_iterator_next(struct cix_trade_log_iterator *,
    struct cix_execution *);

#endif /* _CIX_TRADE_LOG_H */
