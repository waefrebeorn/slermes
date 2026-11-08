/*
 * cron_jobs_axes.c - cron inference-axes and misc helpers.
 *
 * Split from port_cron_jobs.c (soul.md: self-contained modules). Provider
 * model snapshot helpers, job-id generation, small string normalisers and
 * a recursive rmtree. No file-scope state.
 */
#include "cron_jobs_axes.h"

char * normalize_optional_text(const json_t *value, bool strip_trailing_slash);
#include "cron_jobs.h"
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
#include "cron_jobs_paths.h"   /* xstrdup, set_err */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>



/* PoP: cronjobs_resolve_default_model_snapshot @ cron/jobs.py:_resolve_default_model_snapshot */
/* Single-home: there is no multi-profile config resolver wired here, and the
 * Python path fail-opens to None when config is missing/unreadable. We mirror
 * that exactly (null snapshot), so an unpinned-model job carries no snapshot
 * and the fire-time drift guard treats "no snapshot" as "no assertion". */
static char *resolve_default_model_snapshot(void) { return NULL; }


/* PoP: cronjobs_compute_provider_model_snapshots @ cron/jobs.py:_compute_provider_model_snapshots */
static void compute_provider_model_snapshots(const char *provider, const char *model,
                                             const char *base_url, bool no_agent,
                                             char **out_provider, char **out_model) {
    *out_provider = NULL;
    *out_model = NULL;
    if (no_agent) return;
    (void)base_url; /* reserved for future runtime resolution (single-home = null) */
    if (!provider || !*provider) *out_provider = NULL; /* fail-open: no snapshot */
    if (!model || !*model) *out_model = resolve_default_model_snapshot();
}


/* PoP: cronjobs_normalized_inference_axes @ cron/jobs.py:_normalized_inference_axes */
static void normalized_inference_axes(const json_t *job, char **provider, char **model,
                                      char **base_url, bool *no_agent) {
    *provider = normalize_optional_text(json_object_get(job, "provider"), false);
    *model = normalize_optional_text(json_object_get(job, "model"), false);
    *base_url = normalize_optional_text(json_object_get(job, "base_url"), true);
    *no_agent = json_get_bool(job, "no_agent", false);
}


/* PoP: cronjobs_normalize_job_optional_text @ cron/jobs.py:_normalize_job_optional_text */
char *normalize_optional_text(const json_t *value, bool strip_trailing_slash) {
    if (!json_is_string(value)) return NULL;
    char *text = str_strip_dup(json_string_value(value));
    if (!text) return NULL;
    if (strip_trailing_slash) {
        size_t n = strlen(text);
        while (n > 0 && text[n - 1] == '/') text[--n] = '\0';
    }
    if (!*text) { free(text); return NULL; }
    return text;
}


/* Convenience overload taking a C string. */ char *normalize_optional_cstr(const char *value, bool strip_trailing_slash) {
    if (!value) return NULL;
    json_t *s = json_string(value);
    char *r = normalize_optional_text(s, strip_trailing_slash);
    json_free(s);
    return r;
}


/* uuid4().hex[:12] — 12 lowercase hex chars from a v4 UUID. */ char *new_job_id(void) {
    uint8_t b[16];
    if (!uuid_v4_bytes(b)) {
        char *p = malloc(13);
        if (p) snprintf(p, 13, "%06lx%06x", (long)time(NULL) & 0xffffff, rand() & 0xffffff);
        return p;
    }
    char *p = malloc(13);
    if (!p) return NULL;
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 6; i++) {
        p[i * 2] = hx[b[i] >> 4];
        p[i * 2 + 1] = hx[b[i] & 0xf];
    }
    p[12] = '\0';
    return p;
}


/* ── Update / lifecycle ───────────────────────────────────────────── */

/* Return the semantic (provider,model,base_url,no_agent) tuple as a string key
 * for change comparison. Caller frees. */ char *inference_axes_key(const json_t *job) {
    char *p = normalize_optional_text(json_object_get(job, "provider"), false);
    char *m = normalize_optional_text(json_object_get(job, "model"), false);
    char *b = normalize_optional_text(json_object_get(job, "base_url"), true);
    bool na = json_get_bool(job, "no_agent", false);
    size_t n = (p ? strlen(p) : 0) + (m ? strlen(m) : 0) + (b ? strlen(b) : 0) + 16;
    char *key = malloc(n);
    if (key) snprintf(key, n, "%s|%s|%s|%d", p ? p : "", m ? m : "", b ? b : "", na ? 1 : 0);
    free(p); free(m); free(b);
    return key;
}


/* Recursively rmtree a directory. */ void rmtree(const char *path) {
    DIR *d = opendir(path);
    if (!d) { unlink(path); return; }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char child[PATH_MAX];
        snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
        struct stat st;
        if (lstat(child, &st) == 0 && S_ISDIR(st.st_mode)) rmtree(child);
        else unlink(child);
    }
    closedir(d);
    rmdir(path);
}


/* PoP: cronjobs_machine_id @ cron/jobs.py:_machine_id */ char *machine_id(void) {
    const char *explicit = getenv("HERMES_MACHINE_ID");
    if (explicit) {
        char *e = str_strip_dup(explicit);
        if (e && *e) return e;
        free(e);
    }
    char host[256];
    if (gethostname(host, sizeof(host)) != 0) strcpy(host, "unknown");
    host[sizeof(host) - 1] = '\0';
    char *out = malloc(300);
    if (out) snprintf(out, 300, "%s:%d", host, (int)getpid());
    return out;
}
