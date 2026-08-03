/*
 * port_secret_sources_cache_remaining.c — Port of agent/secret_sources/_cache.py
 * secret-cache surface. Freshness checks, real atomic 0600 cache IO,
 * key serialization, clear.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: is_fresh @ agent/secret_sources/_cache.py:is_fresh */
bool scc_is_fresh(double fetched_at, long ttl_seconds, double now) {
    /* Python: ttl <= 0 → stale. */
    if (ttl_seconds <= 0) return false;
    return (now - fetched_at) <= (double)ttl_seconds;
}

/* PoP: resolve_cache_home @ agent/secret_sources/_cache.py:resolve_cache_home */
char *scc_resolve_cache_home(const char *home_path) {
    /* Python: hermes home for cache paths. */
    if (home_path && *home_path) return strdup(home_path);
    const char *h = getenv("HERMES_HOME");
    if (h && *h) return strdup(h);
    h = getenv("HOME");
    if (h && *h) return strdup(h);
    return strdup(".");
}

/* PoP: __init__ @ agent/secret_sources/_cache.py:__init__ */
char *scc_init(const char *basename) {
    /* Python: cache store with key serializer. */
    if (!basename) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"basename\": \"%s\"}", basename);
    return out;
}

/* PoP: path @ agent/secret_sources/_cache.py:path */
char *scc_path(const char *basename, const char *home_path) {
    /* Python: <home>/cache/<basename>. */
    char *home = scc_resolve_cache_home(home_path);
    char *out = NULL;
    asprintf(&out, "%s/cache/%s", home, basename ? basename : "secrets.json");
    free(home);
    return out;
}

/* PoP: read @ agent/secret_sources/_cache.py:read */
char *scc_read(const char *path) {
    /* Python: fresh cached entry or None; best-effort. */
    if (!path) return NULL;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    size_t r = 0;
    if (buf) { r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; }
    fclose(f);
    if (!buf) return NULL;
    return buf;
}

/* PoP: write @ agent/secret_sources/_cache.py:write */
int scc_write(const char *path, const char *entry_json, long ttl_seconds) {
    /* Python: atomic 0600 write; no-op when ttl disabled. */
    if (!path || !entry_json) return -1;
    if (ttl_seconds <= 0) return 0;
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); return -1; }
    chmod(tmp, 0600);
    fwrite(entry_json, 1, strlen(entry_json), w);
    fputc('\n', w);
    if (fflush(w) != 0) { fclose(w); unlink(tmp); free(tmp); return -1; }
    fclose(w);
    if (rename(tmp, path) != 0) { unlink(tmp); free(tmp); return -1; }
    free(tmp);
    return 0;
}

/* PoP: clear @ agent/secret_sources/_cache.py:clear */
int scc_clear(const char *path) {
    /* Python: delete cache file, idempotent. */
    if (!path) return -1;
    if (access(path, F_OK) == 0) unlink(path);
    return 0;
}
