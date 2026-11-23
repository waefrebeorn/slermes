/*
 * web_build_limits.c — C port of upstream Hermes hermes_cli/web_build_limits.py.
 *
 * PoP: w15_webbuildl_3b4d8f_light_build_requested @ hermes_cli/web_build_limits.py:light_build_requested
 * PoP: w15_webbuildl_3b4d8f_bounded_threads @ hermes_cli/web_build_limits.py:_bounded_threads
 * PoP: w15_webbuildl_3b4d8f_bounded_heap_mb @ hermes_cli/web_build_limits.py:_bounded_heap_mb
 * PoP: w15_webbuildl_3b4d8f_web_build_limits @ hermes_cli/web_build_limits.py:web_build_limits
 * PoP: w15_webbuildl_3b4d8f_apply_web_build_limits @ hermes_cli/web_build_limits.py:apply_web_build_limits
 *
 * System-dependent inputs are injected: available_cores replaces
 * _available_cores(), and (cgroup_present, cgroup_mb) replaces
 * _cgroup_memory_limit_mb(). The oracle patches those two live helpers from the
 * same fixture values, so the cgroup/CPU branches are exercised deterministically.
 */
#include "noc_w15_webbuildl_3b4d8f.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_HEAP_MB 1024
#define MAX_HEAP_MB 4096
#define MAX_THREADS 8

/* tiny string vector for str.split() */
typedef struct { char **v; size_t n, cap; } StrVec;
static void sv_free(StrVec *sv) {
    for (size_t i = 0; i < sv->n; i++) free(sv->v[i]);
    free(sv->v); sv->v = NULL; sv->n = sv->cap = 0;
}
static void sv_pushn(StrVec *sv, const char *s, size_t n) {
    if (sv->n == sv->cap) {
        size_t nc = sv->cap ? sv->cap * 2 : 8;
        char **nv = (char **)realloc(sv->v, nc * sizeof(char *));
        if (!nv) return;
        sv->v = nv; sv->cap = nc;
    }
    char *copy = (char *)malloc(n + 1);
    if (!copy) return;
    memcpy(copy, s, n); copy[n] = '\0';
    sv->v[sv->n++] = copy;
}

/* Python: value.strip().lower() in {"1","true","yes","on"} */
static int env_flag(const struct json_t *env, const char *name) {
    const char *raw = json_get_str(env, name, "");
    const char *p = raw;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - p);
    const char *words[] = {"1", "true", "yes", "on"};
    for (int i = 0; i < 4; i++) {
        size_t w = strlen(words[i]);
        if (n != w) continue;
        int same = 1;
        for (size_t j = 0; j < n; j++)
            if (tolower((unsigned char)p[j]) != words[i][j]) { same = 0; break; }
        if (same) return 1;
    }
    return 0;
}

int w15_webbuildl_3b4d8f_light_build_requested(const struct json_t *env) {
    return env_flag(env, "HERMES_WEB_BUILD_LIGHT") ? 1 : 0;
}

int w15_webbuildl_3b4d8f_bounded_threads(int available_cores) {
    int cores = available_cores > 0 ? available_cores : 0;
    int half = cores / 2;
    if (half == 0) half = 1;                 /* `cores // 2 or 1` */
    if (half > MAX_THREADS) half = MAX_THREADS;
    return half < 1 ? 1 : half;
}

int w15_webbuildl_3b4d8f_bounded_heap_mb(int cgroup_present, long long cgroup_mb) {
    if (!cgroup_present || cgroup_mb <= 0) return MAX_HEAP_MB;
    int heap = (int)(cgroup_mb * 0.75);      /* int(limit_mb * 0.75) */
    if (heap < MIN_HEAP_MB) heap = MIN_HEAP_MB;
    if (heap > MAX_HEAP_MB) heap = MAX_HEAP_MB;
    return heap;
}

/* Python int(str) base 10: full-string parse, tolerate surrounding whitespace. */
static int parse_int(const char *s, long long *out) {
    if (!s) return 0;
    const char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 0;
    errno = 0;
    char *end = NULL;
    long long v = strtoll(p, &end, 10);
    if (end == p || errno == ERANGE) return 0;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end) return 0;
    *out = v;
    return 1;
}

/* Python: str.split() -- runs of whitespace, no empty tokens. */
static void tokenize(const char *s, StrVec *out) {
    if (!s) return;
    const char *p = s;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        sv_pushn(out, start, (size_t)(p - start));
    }
}

static struct json_t *limits_impl(const struct json_t *env, int cgroup_present,
                                  long long cgroup_mb, int available_cores, int *light_out) {
    struct json_t *out = json_is_object(env) ? json_copy(env) : json_object();
    int light = w15_webbuildl_3b4d8f_light_build_requested(out);
    if (light_out) *light_out = light;

    /* --- V8 heap cap ----------------------------------------------------- */
    const char *node_opts = json_get_str(out, "NODE_OPTIONS", "");
    StrVec toks = {0};
    tokenize(node_opts, &toks);
    int has_heap_flag = 0;
    for (size_t i = 0; i < toks.n; i++)
        if (strncmp(toks.v[i], "--max-old-space-size=", 21) == 0) { has_heap_flag = 1; break; }
    if (!has_heap_flag) {
        long long heap = 0;
        const char *explicit = json_get_str(out, "HERMES_WEB_BUILD_MAX_OLD_SPACE_SIZE", "");
        if (!parse_int(explicit, &heap))
            heap = light ? MIN_HEAP_MB : w15_webbuildl_3b4d8f_bounded_heap_mb(cgroup_present, cgroup_mb);
        if (heap < MIN_HEAP_MB) heap = MIN_HEAP_MB;
        char flag[64];
        snprintf(flag, sizeof(flag), "--max-old-space-size=%lld", heap);
        sv_pushn(&toks, flag, strlen(flag));
        size_t total = 1;
        for (size_t i = 0; i < toks.n; i++) total += strlen(toks.v[i]) + 1;
        char *joined = (char *)malloc(total);
        if (joined) {
            joined[0] = '\0';
            for (size_t i = 0; i < toks.n; i++) {
                if (i) strcat(joined, " ");
                strcat(joined, toks.v[i]);
            }
            json_set(out, "NODE_OPTIONS", json_string(joined));
            free(joined);
        }
    }
    sv_free(&toks);

    /* --- Native bundler parallelism cap ---------------------------------- */
    if (json_get_str(out, "RAYON_NUM_THREADS", "")[0] == '\0') {
        long long threads = 0;
        const char *explicit = json_get_str(out, "HERMES_WEB_BUILD_THREADS", "");
        if (!parse_int(explicit, &threads))
            threads = light ? 1 : w15_webbuildl_3b4d8f_bounded_threads(available_cores);
        if (threads < 1) threads = 1;
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", threads);
        json_set(out, "RAYON_NUM_THREADS", json_string(buf));
    }
    return out;
}

struct json_t *w15_webbuildl_3b4d8f_web_build_limits(const struct json_t *env,
                                                    int cgroup_present, long long cgroup_mb,
                                                    int available_cores) {
    return limits_impl(env, cgroup_present, cgroup_mb, available_cores, NULL);
}

struct json_t *w15_webbuildl_3b4d8f_apply_web_build_limits(const struct json_t *env,
                                                          int cgroup_present, long long cgroup_mb,
                                                          int available_cores) {
    /* Python: env.update(web_build_limits(env)); return env -- same key/value set. */
    return limits_impl(env, cgroup_present, cgroup_mb, available_cores, NULL);
}
