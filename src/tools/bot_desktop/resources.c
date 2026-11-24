/*
 * resources.c - C11 port of upstream tools/bot_desktop/resources.py
 * (gate w26_resources_caddbd).
 *
 * Ported value->value: the memory gate decision. Proven by the differential
 * oracle gate w26_resources_caddbd (tests/oracle_w26_resources_caddbd.py).
 * PoP: w26_resources_caddbd_read_int @ tools/bot_desktop/resources.py:_read_int
 * PoP: w26_resources_caddbd_meminfo @ tools/bot_desktop/resources.py:_meminfo
 * PoP: w26_resources_caddbd_stat_value @ tools/bot_desktop/resources.py:_stat_value
 * PoP: w26_resources_caddbd_cgroup @ tools/bot_desktop/resources.py:_cgroup_limit_and_usage
 * PoP: w26_resources_caddbd_memory_info @ tools/bot_desktop/resources.py:memory_info
 * PoP: w26_resources_caddbd_min_free_mb @ tools/bot_desktop/resources.py:min_free_mb
 * PoP: w26_resources_caddbd_tight_headroom_mb @ tools/bot_desktop/resources.py:tight_headroom_mb
 * PoP: w26_resources_caddbd_memory_blocker @ tools/bot_desktop/resources.py:memory_blocker
 * PoP: w26_resources_caddbd_constants @ tools/bot_desktop/resources.py:<literals>
 *
 * Seams, all fixed inputs here: /proc/meminfo, the cgroup memory.max /
 * memory.current / memory.stat files, and the loaded config.yaml.  What is
 * ported is the arithmetic and the advisory that a host's numbers decide.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "noc_w26_resources_caddbd.h"

/* 1 << 60 -- cgroup v1's "unlimited" sentinel. */
#define RS_V1_UNLIMITED (1LL << 60)

/* ---------------------------------------------------------------- helpers */

/* Python str.strip() over ASCII whitespace.  Caller frees. */
static char *rs_strip(const char *s) {
    const char *b = s ? s : "";
    const char *e;
    size_t n;
    char *out;
    while (*b == ' ' || *b == '\t' || *b == '\n' || *b == '\r' || *b == '\v' || *b == '\f') b++;
    e = b + strlen(b);
    while (e > b && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r' ||
                     e[-1] == '\v' || e[-1] == '\f'))
        e--;
    n = (size_t)(e - b);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

/* str.isdigit() for the ASCII range: non-empty, all 0-9. */
static int rs_all_digits(const char *s) {
    if (!s || !*s) return 0;
    for (; *s; s++)
        if (*s < '0' || *s > '9') return 0;
    return 1;
}

/* Python int(<str>) over ASCII: optional sign, then digits only. */
static int rs_parse_int_str(const char *s, long long *out) {
    char *t = rs_strip(s);
    const char *p;
    long long v = 0;
    int neg = 0, any = 0;
    if (!t) return 0;
    p = t;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    for (; *p; p++) {
        if (*p < '0' || *p > '9') { free(t); return 0; }
        if (v > (LLONG_MAX - (*p - '0')) / 10) { free(t); return 0; }
        v = v * 10 + (*p - '0');
        any = 1;
    }
    free(t);
    if (!any) return 0;
    *out = neg ? -v : v;
    return 1;
}

/* Python int(x) for a JSON value: bool -> 0/1, number -> trunc toward zero,
 * string -> digits, everything else TypeError. */
static int rs_py_int(const json_t *v, long long *out) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_BOOL:   *out = v->bool_val ? 1 : 0; return 1;
    case JSON_NUMBER:
        if (v->num_val != v->num_val) return 0;                 /* nan */
        if (v->num_val >= 9.2233720368547758e18) { *out = LLONG_MAX; return 1; }
        if (v->num_val <= -9.2233720368547758e18) { *out = LLONG_MIN; return 1; }
        *out = (long long)v->num_val; return 1;
    case JSON_STRING: return rs_parse_int_str(v->str_val ? v->str_val : "", out);
    default: return 0;
    }
}

/* json value -> long long, or 0 with *ok = 0 when it is null/absent. */
static long long rs_int_or_null(const json_t *v, int *ok) {
    long long n = 0;
    if (v && v->type != JSON_NULL && rs_py_int(v, &n)) { if (ok) *ok = 1; return n; }
    if (ok) *ok = 0;
    return 0;
}

/* Python int // int: floor toward -inf. */
static long long rs_floor_div(long long a, long long b) {
    long long q;
    if (b == 0) return 0;
    q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q -= 1;
    return q;
}

/* str.splitlines() over the ASCII terminators. */
typedef struct {
    const char *p;
    int alive;
} rs_lines;

static void rs_lines_init(rs_lines *it, const char *text) {
    it->p = text ? text : "";
    it->alive = (text && *text) ? 1 : 0;
}

static int rs_is_break(char c) {
    return c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *rs_lines_next(rs_lines *it) {
    const char *s, *q;
    size_t n;
    char *out;
    if (!it->alive) return NULL;
    s = it->p;
    q = s;
    while (*q && !rs_is_break(*q)) q++;
    n = (size_t)(q - s);
    out = (char *)malloc(n + 1);
    if (!out) { it->alive = 0; return NULL; }
    memcpy(out, s, n);
    out[n] = '\0';
    if (*q == '\0') {
        it->alive = 0;
    } else {
        if (*q == '\r' && q[1] == '\n') q++;
        q++;
        it->p = q;
        if (*q == '\0') it->alive = 0;   /* a trailing break yields no last line */
    }
    return out;
}

/* ---------------------------------------------------------------- entries */

json_t *w26_resources_caddbd_read_int(const json_t *in) {
    const json_t *t = json_obj_get(in, "text");
    char *s;
    json_t *out;
    if (!t || t->type != JSON_STRING) return json_null();
    s = rs_strip(t->str_val ? t->str_val : "");
    if (!s) return json_null();
    if (!rs_all_digits(s)) { free(s); return json_null(); }  /* "max" -> None */
    out = json_number((double)strtoll(s, NULL, 10));
    free(s);
    return out;
}

json_t *w26_resources_caddbd_meminfo(const json_t *in) {
    const json_t *t = json_obj_get(in, "text");
    json_t *out = json_object();
    rs_lines it;
    char *line;
    if (!t || t->type != JSON_STRING) return out;   /* OSError -> {} */
    rs_lines_init(&it, t->str_val);
    while ((line = rs_lines_next(&it)) != NULL) {
        char *colon = strchr(line, ':');
        const char *rest;
        char *p, *tok;
        if (colon) { *colon = '\0'; rest = colon + 1; }
        else rest = line + strlen(line);   /* writable empty tail, not a literal */
        p = (char *)rest;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '\v' || *p == '\f') p++;
        tok = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != '\v' && *p != '\f') p++;
        { char saved = *p; *p = '\0';
          if (rs_all_digits(tok))
              json_set(out, line, json_number((double)(strtoll(tok, NULL, 10) * 1024)));
          *p = saved; }
        free(line);
    }
    return out;
}

json_t *w26_resources_caddbd_stat_value(const json_t *in) {
    const json_t *t = json_obj_get(in, "text");
    const char *key = json_get_str(in, "key", "");
    rs_lines it;
    char *line;
    if (!t || t->type != JSON_STRING) return json_null();
    rs_lines_init(&it, t->str_val);
    while ((line = rs_lines_next(&it)) != NULL) {
        char *sp = strchr(line, ' ');
        const char *rest = "";
        char *name = line;
        long long v;
        if (sp) { *sp = '\0'; rest = sp + 1; }
        if (strcmp(name, key) == 0) {
            /* ValueError inside the live try block abandons the whole scan. */
            if (!rs_parse_int_str(rest, &v)) { free(line); return json_null(); }
            free(line);
            return json_number((double)v);
        }
        free(line);
    }
    return json_null();
}

/* The shared body of _cgroup_limit_and_usage, over already-read values. */
static void rs_cgroup_fold(int version, long long limit, int has_limit,
                           long long usage, int has_usage,
                           long long cache, int has_cache,
                           long long *out_limit, int *out_has_limit,
                           long long *out_usage, int *out_has_usage) {
    if (version == 1 && has_limit && limit >= RS_V1_UNLIMITED) has_limit = 0;
    if (has_usage && has_cache && cache != 0) {
        usage = usage - cache;
        if (usage < 0) usage = 0;
    }
    *out_limit = limit;
    *out_has_limit = has_limit;
    *out_usage = usage;
    *out_has_usage = has_usage;
}

json_t *w26_resources_caddbd_cgroup(const json_t *in) {
    const json_t *cg = json_obj_get(in, "cgroup");
    long long limit = 0, usage = 0, cache = 0;
    int has_limit = 0, has_usage = 0, has_cache = 0;
    int version = 2;
    json_t *out = json_array();

    if (cg && cg->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(cg, "version");
        long long ver = 0;
        if (v && rs_py_int(v, &ver)) version = (ver == 1) ? 1 : 2;
        limit = rs_int_or_null(json_obj_get(cg, "limit"), &has_limit);
        usage = rs_int_or_null(json_obj_get(cg, "usage"), &has_usage);
        cache = rs_int_or_null(json_obj_get(cg, "cache"), &has_cache);
    }
    rs_cgroup_fold(version, limit, has_limit, usage, has_usage, cache, has_cache,
                   &limit, &has_limit, &usage, &has_usage);
    json_append(out, has_limit ? json_number((double)limit) : json_null());
    json_append(out, has_usage ? json_number((double)usage) : json_null());
    return out;
}

/* memory_info over already-known numbers. */
static void rs_memory_fold(const json_t *meminfo,
                           long long limit, int has_limit,
                           long long usage, int has_usage,
                           long long *out_avail_mb, int *out_has_avail,
                           long long *out_limit_mb, int *out_has_limit_mb) {
    int host_has_avail = 0, host_has_total = 0;
    long long host_avail = 0, host_total = 0, available = 0, limit_bytes = 0;
    int have_candidate = 0;

    if (meminfo && meminfo->type == JSON_OBJECT) {
        host_avail = rs_int_or_null(json_obj_get(meminfo, "MemAvailable"), &host_has_avail);
        host_total = rs_int_or_null(json_obj_get(meminfo, "MemTotal"), &host_has_total);
    }
    if (host_has_avail) { available = host_avail; have_candidate = 1; }
    if (has_limit && has_usage) {
        long long room = limit - usage;
        if (!have_candidate || room < available) available = room;
        have_candidate = 1;
    }
    limit_bytes = has_limit ? limit : host_total;
    *out_has_avail = have_candidate;
    if (have_candidate) {
        long long mb = rs_floor_div(available, 1048576);
        *out_avail_mb = mb < 0 ? 0 : mb;
    } else {
        *out_avail_mb = 0;
    }
    *out_has_limit_mb = has_limit || host_has_total;
    *out_limit_mb = *out_has_limit_mb ? rs_floor_div(limit_bytes, 1048576) : 0;
}

json_t *w26_resources_caddbd_memory_info(const json_t *in) {
    const json_t *cg = json_obj_get(in, "cgroup");
    long long limit = 0, usage = 0, cache = 0, avail_mb = 0, limit_mb = 0;
    int has_limit = 0, has_usage = 0, has_cache = 0, has_avail = 0, has_limit_mb = 0;
    int version = 2;
    json_t *out;

    if (cg && cg->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(cg, "version");
        long long ver = 0;
        if (v && rs_py_int(v, &ver)) version = (ver == 1) ? 1 : 2;
        limit = rs_int_or_null(json_obj_get(cg, "limit"), &has_limit);
        usage = rs_int_or_null(json_obj_get(cg, "usage"), &has_usage);
        cache = rs_int_or_null(json_obj_get(cg, "cache"), &has_cache);
    }
    rs_cgroup_fold(version, limit, has_limit, usage, has_usage, cache, has_cache,
                   &limit, &has_limit, &usage, &has_usage);
    rs_memory_fold(json_obj_get(in, "meminfo"), limit, has_limit, usage, has_usage,
                   &avail_mb, &has_avail, &limit_mb, &has_limit_mb);
    out = json_object();
    json_set(out, "available_mb", has_avail ? json_number((double)avail_mb) : json_null());
    json_set(out, "limit_mb", has_limit_mb ? json_number((double)limit_mb) : json_null());
    return out;
}

/* min_free_mb over an already-loaded config object. */
static long long rs_min_free_from_cfg(const json_t *cfg) {
    const json_t *v;
    long long n = 0;
    if (!cfg || cfg->type != JSON_OBJECT) return W26_RESOURCES_CADDBD_DEFAULT_MIN_FREE_MB;
    v = json_obj_get(cfg, "min_free_memory_mb");
    if (!v) return W26_RESOURCES_CADDBD_DEFAULT_MIN_FREE_MB;      /* .get(default) */
    if (!rs_py_int(v, &n)) return W26_RESOURCES_CADDBD_DEFAULT_MIN_FREE_MB;
    return n < 0 ? 0 : n;
}

json_t *w26_resources_caddbd_min_free_mb(const json_t *in) {
    return json_number((double)rs_min_free_from_cfg(json_obj_get(in, "cfg")));
}

json_t *w26_resources_caddbd_tight_headroom_mb(const json_t *in) {
    const json_t *floor_v = json_obj_get(in, "floor");
    long long floor_mb;
    if (floor_v && floor_v->type != JSON_NULL) {
        if (!rs_py_int(floor_v, &floor_mb)) return json_null();
    } else {
        floor_mb = rs_min_free_from_cfg(json_obj_get(in, "cfg"));
    }
    return json_number((double)(floor_mb + rs_floor_div(floor_mb, 3)));
}

json_t *w26_resources_caddbd_memory_blocker(const json_t *in) {
    const json_t *need_v = json_obj_get(in, "need");
    const json_t *info = json_obj_get(in, "info");
    long long need;
    long long avail_mb = 0, limit_mb = 0;
    int has_avail = 0, has_limit_mb = 0;
    char buf[256];
    size_t used;

    if (need_v && need_v->type != JSON_NULL) {
        if (!rs_py_int(need_v, &need)) return json_null();
    } else {
        need = rs_min_free_from_cfg(json_obj_get(in, "cfg"));
    }
    if (need == 0) return json_null();
    if (info && info->type == JSON_OBJECT) {
        avail_mb = rs_int_or_null(json_obj_get(info, "available_mb"), &has_avail);
        limit_mb = rs_int_or_null(json_obj_get(info, "limit_mb"), &has_limit_mb);
    } else {
        json_t *computed = w26_resources_caddbd_memory_info(in);
        avail_mb = rs_int_or_null(json_obj_get(computed, "available_mb"), &has_avail);
        limit_mb = rs_int_or_null(json_obj_get(computed, "limit_mb"), &has_limit_mb);
        json_free(computed);
    }
    if (!has_avail || avail_mb >= need) return json_null();
    used = (size_t)snprintf(buf, sizeof(buf),
                            "Not enough free memory for a desktop: %lld MB available",
                            avail_mb);
    if (has_limit_mb && limit_mb != 0)
        used += (size_t)snprintf(buf + used, sizeof(buf) - used, " of %lld MB", limit_mb);
    snprintf(buf + used, sizeof(buf) - used,
             ", %lld MB needed (bot_desktop.min_free_memory_mb)", need);
    return json_string(buf);
}

json_t *w26_resources_caddbd_constants(void) {
    json_t *o = json_object();
    json_set(o, "default_min_free_mb", json_number(W26_RESOURCES_CADDBD_DEFAULT_MIN_FREE_MB));
    json_set(o, "mib", json_number(W26_RESOURCES_CADDBD_MIB));
    json_set(o, "v1_unlimited_sentinel", json_number((double)RS_V1_UNLIMITED));
    return o;
}
