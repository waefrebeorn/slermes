/*
 * cron_jobs_paths.c - cron jobs directory / lock-file helpers.
 *
 * Split from port_cron_jobs.c (soul.md: self-contained modules). Path
 * construction plus the directory lock. No file-scope state.
 */
#include "cron_jobs_paths.h"
#include "cron_jobs.h"
#include "cron_jobs_time.h"
#include "hermes_strutil.h"
#include "hermes_fs.h"
#include "slermes_home.h"
#include "datetime.h"
#include "cron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>
#include <limits.h>
#include "uuid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>



/* ── small utilities ──────────────────────────────────────────────── */ char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
 char *set_err(char **err, const char *msg) {
    if (err) *err = xstrdup(msg);
    return NULL;
}


/* strip leading+trailing ASCII whitespace, return malloc'd copy */

/* in-place ASCII lowercase */

/* Join home + relpath into a malloc'd absolute path. */
static char *home_join(const char *relpath) {
    const char *home = slermes_home();
    if (!home) home = "/tmp/.slermes";
    size_t n = strlen(home) + 1 + strlen(relpath) + 1;
    char *p = malloc(n);
    if (!p) return NULL;
    snprintf(p, n, "%s/%s", home, relpath);
    return p;
}


/* mkdir -p for a single path (parents assumed to exist or created by caller). */

/* ── Paths (PoP: module-level path helpers) ───────────────────────── */

/* PoP: cronjobs_cron_dir @ cron/jobs.py:CRON_DIR */
char *cronjobs_cron_dir(void) { return home_join("cron"); }


/* PoP: cronjobs_jobs_file @ cron/jobs.py:JOBS_FILE */
char *cronjobs_jobs_file(void) { return home_join("cron/jobs.json"); }


/* PoP: cronjobs_jobs_lock_file @ cron/jobs.py:_jobs_lock_file */
char *cronjobs_jobs_lock_file(void) { return home_join("cron/.jobs.lock"); }


/* PoP: cronjobs_output_dir @ cron/jobs.py:OUTPUT_DIR */
char *cronjobs_output_dir(void) { return home_join("cron/output"); }


/* PoP: cronjobs_ticker_heartbeat_file @ cron/jobs.py:TICKER_HEARTBEAT_FILE */
char *cronjobs_ticker_heartbeat_file(void) { return home_join("cron/ticker_heartbeat"); }


/* PoP: cronjobs_ticker_success_file @ cron/jobs.py:TICKER_SUCCESS_FILE */
char *cronjobs_ticker_success_file(void) { return home_join("cron/ticker_last_success"); }


/* PoP: cronjobs_job_output_dir @ cron/jobs.py:_job_output_dir */
char *cronjobs_job_output_dir(const char *job_id, char **err) {
    char *text = str_strip_dup(job_id ? job_id : "");
    if (!text) return set_err(err, "out of memory");
    /* Reject empty, ".", "..", or anything with path separators. */
    if (!*text || strcmp(text, ".") == 0 || strcmp(text, "..") == 0 ||
        strchr(text, '/') || strchr(text, '\\') || text[0] == '/') {
        free(text);
        return set_err(err, "Invalid cron job id for output path");
    }
    char *base = cronjobs_output_dir();
    if (!base) { free(text); return set_err(err, "out of memory"); }
    size_t n = strlen(base) + 1 + strlen(text) + 1;
    char *out = malloc(n);
    if (out) snprintf(out, n, "%s/%s", base, text);
    free(base);
    free(text);
    if (!out) return set_err(err, "out of memory");
    return out;
}


/* PoP: cronjobs_secure_dir @ cron/jobs.py:_secure_dir */ void cronjobs_secure_dir(const char *path) {
    if (path) chmod(path, 0700); /* best-effort; ignore errors */
}


/* PoP: cronjobs_secure_file @ cron/jobs.py:_secure_file */ void cronjobs_secure_file(const char *path) {
    struct stat st;
    if (path && stat(path, &st) == 0) chmod(path, 0600);
}


/* PoP: cronjobs_ensure_dirs @ cron/jobs.py:ensure_dirs */
bool cronjobs_ensure_dirs(void) {
    char *cron = cronjobs_cron_dir();
    char *out = cronjobs_output_dir();
    bool ok = false;
    if (cron && out) {
        if (fs_mkdir_p(cron, 0700) == 0 && fs_mkdir_p(out, 0700) == 0) {
            cronjobs_secure_dir(cron);
            cronjobs_secure_dir(out);
            ok = true;
        }
    }
    free(cron);
    free(out);
    return ok;
}


/* ── Cross-process advisory lock (mirrors _jobs_lock) ─────────────── */
/* Returns an open fd holding LOCK_EX on <cron>/.jobs.lock, or -1 (degrade to
 * no-lock, matching the Python fall-open behaviour). Release with
 * cronjobs_unlock(). */ int cronjobs_lock(void) {
    cronjobs_ensure_dirs();
    char *lp = cronjobs_jobs_lock_file();
    if (!lp) return -1;
    int fd = open(lp, O_CREAT | O_RDWR, 0600);
    free(lp);
    if (fd < 0) return -1;
    if (flock(fd, LOCK_EX) != 0) {
        /* fall open: keep fd (still gives us a handle to close) */
    }
    return fd;
}
 void cronjobs_unlock(int fd) {
    if (fd >= 0) {
        flock(fd, LOCK_UN);
        close(fd);
    }
}
