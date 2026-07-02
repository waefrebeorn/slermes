/**
 * port_jobs.c — Port of Python: cron/jobs.py
 *
 * Real C implementations for cron job locking.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

/* Port of Python: _jobs_lock */
char *jobs_lock(void)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char *lock_path = malloc(4096);
    if (!lock_path) return NULL;
    snprintf(lock_path, 4096, "%s/cron/jobs.lock", home);

    int fd = open(lock_path, O_CREAT | O_WRONLY | O_EXCL, 0644);
    if (fd < 0) {
        if (errno == EEXIST) {
            hermes_log(LOG_WARNING, "port", "jobs_lock: lock already held");
        } else {
            hermes_log(LOG_ERROR, "port", "jobs_lock: cannot create lock: %s", strerror(errno));
        }
        free(lock_path);
        return NULL;
    }
    char pid_buf[32];
    snprintf(pid_buf, sizeof(pid_buf), "%d\n", getpid());
    write(fd, pid_buf, strlen(pid_buf));
    close(fd);
    hermes_log(LOG_DEBUG, "port", "jobs_lock: acquired %s", lock_path);
    return lock_path;
}
