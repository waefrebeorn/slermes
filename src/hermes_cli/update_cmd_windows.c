/*
 * update_cmd_windows.c — C home for upstream Hermes hermes_cli/update_cmd_windows.py.
 *
 * PoP: w17_updatecmd_736da6_planned_stop_marker_path @ hermes_cli/update_cmd_windows.py:_planned_stop_marker_path
 * PoP: w17_updatecmd_736da6_lower_dir_prefix @ hermes_cli/update_cmd_windows.py:_lower_dir_prefix
 * PoP: w17_updatecmd_736da6_pending_relaunch_pids @ hermes_cli/update_cmd_windows.py:_pending_relaunch_pids
 *
 * Windows path strings are emitted with backslash separators (Path.__str__ on
 * Windows), and PIDs travel as 64-bit decimal strings (libjson numbers are
 * doubles). Ordering of the PID set is sorted, matching Python's sorted(set).
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w17_updatecmd_736da6.h"

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* Windows separators, case preserved, one trailing separator removed. */
static char *win_join(const char *in, const char *tail, bool lower) {
    if (!in || !*in) {
        if (!tail) return NULL;
        return dupstr(tail);
    }
    size_t n = strlen(in);
    char *work = (char *)malloc(n + 1);
    if (!work) return NULL;
    for (size_t i = 0; i < n; i++) {
        char ch = in[i];
        if (ch == '/') ch = '\\';
        if (lower && ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        work[i] = ch;
    }
    work[n] = '\0';
    /* rstrip(os.sep): strip trailing backslashes, then apppend exactly one. */
    size_t end = n;
    while (end > 0 && work[end - 1] == '\\') { work[end - 1] = '\0'; end--; }
    size_t tn = tail ? strlen(tail) : 0;
    char *out = (char *)malloc(end + 1 + tn + 1);
    if (!out) { free(work); return NULL; }
    memcpy(out, work, end);
    size_t at = end;
    if (at > 0 || tn > 0) out[at++] = '\\';
    if (tn) { memcpy(out + at, tail, tn); at += tn; }
    out[at] = '\0';   /* NUL at final length */
    free(work);
    return out;
}

/* Resolve '.'/'..' and normalise to a lowercase backslash path with no
 * trailing separator (models Windows Path.resolve() for absolute inputs). */
static char *resolve_lower(const char *in) {
    if (!in || !*in) return NULL;
    size_t n = strlen(in);
    char *work = (char *)malloc(n + 1);
    if (!work) return NULL;
    for (size_t i = 0; i < n; i++) {
        char ch = in[i];
        if (ch == '/') ch = '\\';
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        work[i] = ch;
    }
    work[n] = '\0';
    char **parts = (char **)calloc(n + 1, sizeof(char *));
    size_t count = 0;
    char *tok = work;
    while (*tok) {
        char *sep = strchr(tok, '\\');
        if (sep) *sep = '\0';
        if (*tok && strcmp(tok, ".") != 0) {
            if (strcmp(tok, "..") == 0) {
                if (count > 0 && strcmp(parts[count - 1], "..") != 0) count--;
                else parts[count++] = tok;
            } else {
                parts[count++] = tok;
            }
        }
        if (!sep) break;
        tok = sep + 1;
    }
    size_t total = 0;
    for (size_t i = 0; i < count; i++) total += strlen(parts[i]) + 1;
    char *out = (char *)malloc(total + 1);
    if (!out) { free(work); free(parts); return NULL; }
    size_t at = 0;
    for (size_t i = 0; i < count; i++) {
        size_t k = strlen(parts[i]);
        memcpy(out + at, parts[i], k);
        at += k;
        if (i + 1 < count) out[at++] = '\\';
    }
    out[at] = '\0';   /* write NUL at the new length after shortening */
    free(parts);
    free(work);
    return out;
}

char *w17_updatecmd_736da6_planned_stop_marker_path(const char *profile_path) {
    return win_join(profile_path, ".gateway-planned-stop.json", false);
}

char *w17_updatecmd_736da6_lower_dir_prefix(const char *path) {
    char *resolved = resolve_lower(path);
    if (!resolved) return NULL;
    char *out = win_join(resolved, "", false);
    free(resolved);
    return out;
}

static int cmp_i64(const void *a, const void *b) {
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}

static long long pid_of(const json_t *n) {
    if (!n) return 0;
    if (n->type == JSON_STRING) return strtoll(n->str_val ? n->str_val : "0", NULL, 10);
    if (n->type == JSON_NUMBER) return (long long)n->num_val;
    return 0;
}

json_t *w17_updatecmd_736da6_pending_relaunch_pids(json_t *profiles, json_t *unmapped,
                                                   json_t *alive) {
    long long buf[512];
    size_t n = 0;
    if (json_is_object(profiles)) {
        for (size_t i = 0; i < json_object_size(profiles) && n < 512; i++) {
            long long pid = pid_of(json_object_get_at(profiles, i));
            buf[n++] = pid;
        }
    }
    if (json_is_array(unmapped)) {
        for (size_t i = 0; i < json_len(unmapped) && n < 512; i++) {
            json_t *entry = json_get(unmapped, i);
            json_t *argv = json_obj_get(entry, "argv");
            json_t *pj = json_obj_get(entry, "pid");
            /* gate on `entry.get("argv") and entry.get("pid")` truthiness */
            bool has_argv = json_is_array(argv) && json_len(argv) > 0;
            bool has_pid = (pj && ((pj->type == JSON_STRING && pj->str_val && pj->str_val[0]) ||
                                  (pj->type == JSON_NUMBER && pj->num_val != 0)));
            if (has_argv && has_pid) buf[n++] = pid_of(pj);
        }
    }
    json_t *out = json_array();
    /* alive is the injected pid_exists: build a membership array */
    long long alive_buf[512];
    size_t na = 0;
    if (json_is_array(alive)) {
        for (size_t i = 0; i < json_len(alive) && na < 512; i++)
            alive_buf[na++] = pid_of(json_get(alive, i));
    }
    /* pid > 0 AND pid_exists(pid) */
    size_t keep = 0;
    for (size_t i = 0; i < n; i++) {
        if (buf[i] <= 0) continue;
        bool found = false;
        for (size_t j = 0; j < na; j++) if (alive_buf[j] == buf[i]) { found = true; break; }
        if (found) buf[keep++] = buf[i];
    }
    qsort(buf, keep, sizeof(long long), cmp_i64);
    for (size_t i = 0; i < keep; i++) {
        if (i > 0 && buf[i] == buf[i - 1]) continue;   /* set() dedupe */
        json_append(out, json_number((double)buf[i]));
    }
    return out;
}
