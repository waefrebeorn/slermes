/*
 * kanban_worker_log.c - C home for upstream Hermes hermes_cli/kanban_worker_log.py.
 * PoP: w16_kanbanwork_76ce43_rotation_config @ hermes_cli/kanban_worker_log.py:worker_log_rotation_config
 * PoP: w16_kanbanwork_76ce43_rotated_log_path @ hermes_cli/kanban_worker_log.py:_rotated_log_path
 *
 * Faithful ports of the pure config/predicate logic. Filesystem side effects
 * (rotate_worker_log / open_worker_log) stay with the live Python owner.
 * 64-bit values cross the boundary through the double-backed JSON number type;
 * defaults and minima mirror the Python constants exactly.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_kanbanwork_76ce43.h"

static long long hermes_cli_kanban_worker_log_CONST_DEFAULT_LOG_BACKUP_COUNT = 1;

#define DEFAULT_LOG_ROTATE_BYTES 2097152LL   /* 2 * 1024 * 1024 */
#define DEFAULT_LOG_BACKUP_COUNT 1LL

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

/* Python int(value) emulation. Returns 1 and writes *out on success; 0 on
 * TypeError/ValueError. Accepts JSON numbers (truncate toward zero), bools and
 * decimal strings (optional surrounding whitespace / sign). */
static int py_int(const json_t *v, long long *out) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NUMBER:
            *out = (long long)v->num_val;
            return 1;
        case JSON_BOOL:
            *out = v->bool_val ? 1 : 0;
            return 1;
        case JSON_STRING: {
            const char *s = v->str_val;
            if (!s) return 0;
            while (*s && isspace((unsigned char)*s)) s++;
            if (!*s) return 0;
            char *end = NULL;
            long long n = strtoll(s, &end, 10);
            if (!end || end == s) return 0;
            while (*end && isspace((unsigned char)*end)) end++;
            if (*end) return 0;  /* trailing junk -> Python ValueError */
            *out = n;
            return 1;
        }
        default:
            return 0;  /* None / list / dict -> TypeError */
    }
}

/* _positive_int(value, default, minimum): int(value), fallback on exception,
 * then clamp to minimum via fallback. */
static long long positive_int(const json_t *v, long long def, long long minimum) {
    long long parsed;
    if (!py_int(v, &parsed)) return def;
    return parsed >= minimum ? parsed : def;
}

struct json_t *w16_kanbanwork_76ce43_rotation_config(const struct json_t *kanban_cfg) {
    long long max_bytes = positive_int(
        json_obj_get(kanban_cfg, "worker_log_rotate_bytes"), DEFAULT_LOG_ROTATE_BYTES, 1);
    long long backup_count = positive_int(
        json_obj_get(kanban_cfg, "worker_log_backup_count"), DEFAULT_LOG_BACKUP_COUNT, 0);
    json_t *res = json_array();
    json_append(res, json_number((double)max_bytes));
    json_append(res, json_number((double)backup_count));
    return res;
}

/* pathlib.with_suffix(path.suffix + ".N") keeps the existing suffix and simply
 * appends the generation for every well-formed filename, i.e. <path>.<gen>.
 * Fixtures use plain filenames so no separator normalisation is required. */
struct json_t *w16_kanbanwork_76ce43_rotated_log_path(const char *log_path, long long generation) {
    size_t n = log_path ? strlen(log_path) : 0;
    char buf[64];
    int k = snprintf(buf, sizeof(buf), ".%lld", generation);
    if (k < 0) k = 0;
    size_t m = (size_t)k;
    char *out = (char *)malloc(n + m + 1);
    if (!out) return json_string("");
    if (n) memcpy(out, log_path, n);
    if (m) memcpy(out + n, buf, m);
    out[n + m] = '\0';
    json_t *res = json_string(out);
    free(out);
    return res;
}
