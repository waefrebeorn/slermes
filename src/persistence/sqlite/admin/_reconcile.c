/*
 * _reconcile.c - C11 port of upstream persistence/sqlite/admin/_reconcile.py
 * (gates w34_reconcile_7686f7_p1 and w34_reconcile_7686f7_p2).
 *
 * PoP: w34_reconcile_7686f7_p1_json_file @ persistence/sqlite/admin/_reconcile.py:_read_json_file
 * PoP: w34_reconcile_7686f7_p1_same_home @ persistence/sqlite/admin/_reconcile.py:_same_hermes_home
 * PoP: w34_reconcile_7686f7_p1_profile_label @ persistence/sqlite/admin/_reconcile.py:_profile_label_for_home
 * PoP: w34_reconcile_7686f7_p1_repair_command @ persistence/sqlite/admin/_reconcile.py:_repair_command
 * PoP: w34_reconcile_7686f7_p1_owner_remedy @ persistence/sqlite/admin/_reconcile.py:_owner_remedy
 * PoP: w34_reconcile_7686f7_p1_owning_gateway_restart @ persistence/sqlite/admin/_reconcile.py:_owning_gateway_restart
 * PoP: w34_reconcile_7686f7_p1_lagging_damaged @ persistence/sqlite/admin/_reconcile.py:_LaggingStoreDamaged.__init__
 * PoP: w34_reconcile_7686f7_p2_history_tables @ persistence/sqlite/admin/_reconcile.py:_require_history_tables
 * PoP: w34_reconcile_7686f7_p2_integrity_problem @ persistence/sqlite/admin/_reconcile.py:_integrity_problem (row shaping)
 * PoP: w34_reconcile_7686f7_p2_budget @ persistence/sqlite/admin/_reconcile.py:_Budget.remaining/_Budget.busy_timeout
 * PoP: w34_reconcile_7686f7_p2_budget_check @ persistence/sqlite/admin/_reconcile.py:_Budget.check
 * PoP: w34_reconcile_7686f7_p2_constants @ persistence/sqlite/admin/_reconcile.py:<browse budget constants>
 *
 * This lane never decides whether a store is corrupt by itself; it decides what
 * the values collected around it mean.  The seams left to the caller are: the
 * sqlite3 connections (drift probe, integrity scan, DDL transaction), the
 * monotonic clock the browse budget runs on, filesystem reads (gateway.lock,
 * the store signature), flock ownership, and path resolution -- every function
 * here takes the already-resolved value and returns the decision.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_reconcile_7686f7_p1.h"
#include "noc_w34_reconcile_7686f7_p2.h"

#ifdef _WIN32
#define W34_SEP '\\'
#define W34_IS_SEP(c) ((c) == '/' || (c) == '\\')
#else
#define W34_SEP '/'
#define W34_IS_SEP(c) ((c) == '/')
#endif

#define W34_PATH_MAX 4096
#define W34_MAX_COMPONENTS 128

/* _PROFILE_LABEL_RE = re.compile(r"^[a-z0-9][a-z0-9_-]{0,63}$") */
static int w34_label_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

/* Path(...).__str__ for an already-absolute path: host separator, empty and
 * "." components dropped, ".." resolved lexically (clamped at the root), one
 * trailing separator dropped.  expanduser()/resolve() are the caller's seam. */
static void w34_path_str(const char *in, char *out, size_t cap) {
    char work[W34_PATH_MAX];
    const char *comp[W34_MAX_COMPONENTS];
    size_t clen[W34_MAX_COMPONENTS];
    char root[4];
    size_t root_len = 0, n = 0, i = 0, o = 0;
    int depth = 0, k;

    if (!cap) return;
    if (!in) in = "";
    while (in[n] && n + 1 < sizeof(work)) { work[n] = in[n]; n++; }
    work[n] = '\0';

#ifdef _WIN32
    if (n >= 2 && work[1] == ':') {
        root[0] = work[0];
        root[1] = ':';
        root[2] = W34_SEP;
        root_len = 3;
        i = 2;
    } else if (n >= 1 && W34_IS_SEP(work[0])) {
        root[0] = W34_SEP;
        root_len = 1;
        i = 1;
    }
#else
    if (n >= 1 && work[0] == '/') {
        root[0] = '/';
        root_len = 1;
        i = 1;
    }
#endif
    for (;;) {
        size_t s, l;
        while (i < n && W34_IS_SEP(work[i])) i++;
        s = i;
        while (i < n && !W34_IS_SEP(work[i])) i++;
        l = i - s;
        if (l == 0) break;
        if (l == 1 && work[s] == '.') continue;
        if (l == 2 && work[s] == '.' && work[s + 1] == '.') {
            if (depth > 0 && !(clen[depth - 1] == 2 && comp[depth - 1][0] == '.' &&
                               comp[depth - 1][1] == '.')) {
                depth--;
            } else if (root_len == 0 && depth < W34_MAX_COMPONENTS) {
                comp[depth] = work + s;
                clen[depth] = l;
                depth++;
            }
            /* a rooted path clamps ".." at its root, exactly like resolve() */
            continue;
        }
        if (depth < W34_MAX_COMPONENTS) {
            comp[depth] = work + s;
            clen[depth] = l;
            depth++;
        }
    }

    if (root_len > 0) {
        if (root_len >= cap) root_len = cap - 1;
        memcpy(out, root, root_len);
        o = root_len;
    }
    for (k = 0; k < depth; k++) {
        if (o > 0 && out[o - 1] != W34_SEP) {
            if (o + 1 >= cap) break;
            out[o++] = W34_SEP;
        }
        if (o + clen[k] + 1 > cap) break;
        memcpy(out + o, comp[k], clen[k]);
        o += clen[k];
    }
    out[o] = '\0';
}

/* os.path.normcase: lowercase, host separator (Windows). */
static void w34_normcase(const char *in, char *out, size_t cap) {
    size_t i;
    w34_path_str(in, out, cap);
#ifdef _WIN32
    for (i = 0; out[i] && i + 1 < cap; i++) {
        char c = out[i];
        if (c == '/') out[i] = '\\';
        else if (c >= 'A' && c <= 'Z') out[i] = (char)(c - 'A' + 'a');
    }
#endif
}

/* int(text) for a bare PID file: optional sign, digits with single underscores
 * between them.  Values outside long long are a documented seam. */
static int w34_py_int(const char *s, long long *out) {
    const char *p = s;
    long long v = 0;
    int neg = 0, digits = 0, prev_digit = 0;
    if (!s) return 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    for (; *p; p++) {
        if (*p == '_') {
            if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) return 0;
            prev_digit = 0;
            continue;
        }
        if (*p < '0' || *p > '9') return 0;
        if (v > 922337203685477580LL) return 0;      /* would overflow */
        v = v * 10 + (*p - '0');
        digits++;
        prev_digit = 1;
    }
    if (!digits) return 0;
    *out = neg ? -v : v;
    return 1;
}

/* Python str() over the scalar shapes a sqlite row can hold. */
static void w34_py_str(const json_t *v, char *out, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(out, cap, "None"); return; }
    switch (v->type) {
    case JSON_BOOL:
        snprintf(out, cap, "%s", v->bool_val ? "True" : "False");
        return;
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
            snprintf(out, cap, "%lld", (long long)d);
        else
            snprintf(out, cap, "%.17g", d);   /* non-integral REALs: seam */
        return;
    }
    case JSON_STRING:
        snprintf(out, cap, "%s", v->str_val ? v->str_val : "");
        return;
    default: {
        char *s = json_dumps(v, 0);           /* container rows: seam */
        snprintf(out, cap, "%s", s ? s : "");
        free(s);
        return;
    }
    }
}

/* str.strip() plus the utf-8-sig BOM strip read_text(encoding="utf-8-sig") does. */
static char *w34_strip(const char *raw) {
    const char *s = raw ? raw : "";
    char *out;
    size_t n;
    if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        s += 3;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n' || *s == '\f' || *s == '\v') s++;
    n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' ||
                     s[n - 1] == '\n' || s[n - 1] == '\f' || s[n - 1] == '\v')) n--;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static json_t *w34_pid_object(long long pid) {
    json_t *o = json_object();
    json_set(o, "pid", json_number((double)pid));
    return o;
}

/* _read_json_file: JSON object at a status file, or None.  The read is the
 * seam; the text is the value. */
static json_t *w34_read_json_text(const char *raw, int bare_pid_ok) {
    char *text = w34_strip(raw);
    json_t *v = NULL;
    long long pid = 0;
    int int_literal;

    if (!text) return NULL;
    if (!text[0]) { free(text); return NULL; }
    int_literal = w34_py_int(text, &pid);
    v = json_parse(text, NULL);
    free(text);

    if (v) {
        if (bare_pid_ok && v->type == JSON_NUMBER && int_literal) {
            json_free(v);
            return w34_pid_object(pid);
        }
        if (bare_pid_ok && v->type == JSON_BOOL) {
            bool b = v->bool_val;
            json_t *o;
            json_free(v);
            o = json_object();
            json_set(o, "pid", json_bool(b));
            return o;
        }
        if (v->type == JSON_OBJECT) return v;
        json_free(v);
        return NULL;
    }
    if (bare_pid_ok && int_literal) return w34_pid_object(pid);
    return NULL;
}

/* The label rule of _profile_label_for_home: <root>/profiles/<name> with a
 * valid name.  Returns 1 and copies the label into out. */
static int w34_profile_label(const char *home, char *out, size_t cap) {
    char path[W34_PATH_MAX];
    size_t len, name_start, parent_start, parent_len, name_len, i;

    w34_path_str(home, path, sizeof(path));
    len = strlen(path);
    while (len > 1 && path[len - 1] == W34_SEP) len--;   /* keeps "C:\" */
    if (len == 0) return 0;
    name_start = len;
    while (name_start > 0 && path[name_start - 1] != W34_SEP) name_start--;
    name_len = len - name_start;
    if (name_len == 0 || name_len > 64) return 0;
    if (!((path[name_start] >= 'a' && path[name_start] <= 'z') ||
          (path[name_start] >= '0' && path[name_start] <= '9'))) return 0;
    for (i = 1; i < name_len; i++)
        if (!w34_label_char(path[name_start + i])) return 0;

    parent_len = name_start;
    while (parent_len > 0 && path[parent_len - 1] == W34_SEP) parent_len--;
    parent_start = parent_len;
    while (parent_start > 0 && path[parent_start - 1] != W34_SEP) parent_start--;
    parent_len -= parent_start;
    if (parent_len != 8 || memcmp(path + parent_start, "profiles", 8) != 0) return 0;

    if (name_len + 1 > cap) return 0;
    memcpy(out, path + name_start, name_len);
    out[name_len] = '\0';
    return 1;
}

/* The command that restarts the gateway holding a labelled home. */
static void w34_gateway_restart(const char *label, char *out, size_t cap) {
    if (label && label[0] && strcmp(label, "default") != 0)
        snprintf(out, cap, "hermes -p %s gateway restart", label);
    else
        snprintf(out, cap, "hermes gateway restart");
}

/* _owner_remedy -> [label-or-home, restart command] */
static void w34_owner_remedy(const char *home, char *first, size_t fcap,
                             char *command, size_t ccap) {
    char label[W34_PATH_MAX];
    label[0] = '\0';
    if (w34_profile_label(home, label, sizeof(label))) snprintf(first, fcap, "%s", label);
    else w34_path_str(home, first, fcap);
    w34_gateway_restart(label, command, ccap);
}

json_t *w34_reconcile_7686f7_p1_json_file(const char *raw, int bare_pid_ok) {
    json_t *v = w34_read_json_text(raw, bare_pid_ok);
    return v ? v : json_null();
}

json_t *w34_reconcile_7686f7_p1_same_home(const char *left, const char *right) {
    char a[W34_PATH_MAX], b[W34_PATH_MAX];
    w34_normcase(left ? left : "", a, sizeof(a));
    w34_normcase(right ? right : "", b, sizeof(b));
    return json_bool(strcmp(a, b) == 0);
}

json_t *w34_reconcile_7686f7_p1_profile_label(const char *path) {
    char label[W34_PATH_MAX];
    if (w34_profile_label(path, label, sizeof(label))) return json_string(label);
    return json_null();
}

json_t *w34_reconcile_7686f7_p1_repair_command(const char *home) {
    char label[W34_PATH_MAX], cmd[W34_PATH_MAX + 64], path[W34_PATH_MAX];
    if (w34_profile_label(home, label, sizeof(label))) {
        if (strcmp(label, "default") == 0)
            snprintf(cmd, sizeof(cmd), "hermes sessions repair");
        else
            snprintf(cmd, sizeof(cmd), "hermes -p %s sessions repair", label);
        return json_string(cmd);
    }
    w34_path_str(home, path, sizeof(path));
    snprintf(cmd, sizeof(cmd), "HERMES_HOME=%s hermes sessions repair", path);
    return json_string(cmd);
}

json_t *w34_reconcile_7686f7_p1_owner_remedy(const char *home) {
    char first[W34_PATH_MAX], command[W34_PATH_MAX + 64];
    json_t *out = json_array();
    w34_owner_remedy(home, first, sizeof(first), command, sizeof(command));
    json_append(out, json_string(first));
    json_append(out, json_string(command));
    return out;
}

json_t *w34_reconcile_7686f7_p1_owning_gateway_restart(const char *home,
                                                       const char *lock_text) {
    char first[W34_PATH_MAX], command[W34_PATH_MAX + 64];
    char owner_home[W34_PATH_MAX];
    json_t *record = w34_read_json_text(lock_text, 1);
    const json_t *owner = record ? json_obj_get(record, "hermes_home") : NULL;
    json_t *out;

    if (owner && owner->type == JSON_STRING && owner->str_val && owner->str_val[0]) {
        char a[W34_PATH_MAX], b[W34_PATH_MAX];
        snprintf(owner_home, sizeof(owner_home), "%s", owner->str_val);
        w34_normcase(owner_home, a, sizeof(a));
        w34_normcase(home, b, sizeof(b));
        if (strcmp(a, b) != 0) {
            w34_owner_remedy(owner_home, first, sizeof(first), command, sizeof(command));
            out = json_string(command);
            json_free(record);
            return out;
        }
    }
    json_free(record);
    w34_owner_remedy(home, first, sizeof(first), command, sizeof(command));
    return json_string(command);
}

json_t *w34_reconcile_7686f7_p1_lagging_damaged(const char *detail) {
    size_t need = strlen("database disk image is malformed (integrity_check: )") +
                  (detail ? strlen(detail) : 0) + 1;
    char *buf = (char *)malloc(need);
    json_t *out;
    if (!buf) return json_null();
    snprintf(buf, need, "database disk image is malformed (integrity_check: %s)",
             detail ? detail : "");
    out = json_string(buf);
    free(buf);
    return out;
}

json_t *w34_reconcile_7686f7_p2_history_tables(const json_t *names) {
    int have_sessions = 0, have_messages = 0;
    json_t *out = json_object();
    size_t i, n = names && names->type == JSON_ARRAY ? json_len(names) : 0;

    for (i = 0; i < n; i++) {
        const json_t *v = json_get(names, i);
        if (!v || v->type != JSON_STRING) continue;
        if (!strcmp(v->str_val, "sessions")) have_sessions = 1;
        else if (!strcmp(v->str_val, "messages")) have_messages = 1;
    }
    json_set(out, "ok", json_bool(have_sessions && have_messages));
    json_set(out, "error", (have_sessions && have_messages) ? json_null() : json_string(
        "canonical history tables are missing; use sessions recover, not additive reconciliation"));
    return out;
}

json_t *w34_reconcile_7686f7_p2_integrity_problem(const json_t *rows) {
    char joined[W34_PATH_MAX * 2];
    size_t i, n = rows && rows->type == JSON_ARRAY ? json_len(rows) : 0;
    int kept = 0;
    size_t o = 0;

    joined[0] = '\0';
    for (i = 0; i < n && kept < 3; i++) {
        const json_t *row = json_get(rows, i);
        const json_t *cell;
        char text[W34_PATH_MAX];
        size_t tl, is_ok;

        if (!row || row->type != JSON_ARRAY || json_len(row) == 0) continue;
        cell = json_get(row, 0);
        w34_py_str(cell, text, sizeof(text));
        tl = strlen(text);
        is_ok = (tl == 2) && (text[0] == 'o' || text[0] == 'O') &&
                (text[1] == 'k' || text[1] == 'K');
        if (is_ok) continue;
        if (kept > 0 && o + 2 < sizeof(joined)) { joined[o++] = ';'; joined[o++] = ' '; }
        if (o + tl + 1 < sizeof(joined)) { memcpy(joined + o, text, tl); o += tl; }
        joined[o] = '\0';
        kept++;
    }
    return kept ? json_string(joined) : json_null();
}

json_t *w34_reconcile_7686f7_p2_budget(double deadline, double now) {
    double remaining = deadline - now;
    json_t *out = json_object();
    if (remaining < 0.0) remaining = 0.0;
    json_set(out, "remaining", json_number(remaining));
    json_set(out, "busy_timeout", json_number(remaining > 0.05 ? remaining : 0.05));
    return out;
}

json_t *w34_reconcile_7686f7_p2_budget_check(int expired, double remaining) {
    int raises = expired || remaining <= 0.0;
    json_t *out = json_object();
    json_set(out, "raise", json_bool(raises));
    json_set(out, "message", raises
        ? json_string("browse schema catch-up exceeded its time budget") : json_null());
    return out;
}

json_t *w34_reconcile_7686f7_p2_constants(void) {
    json_t *o = json_object();
    json_set(o, "browse_reconcile_budget_s", json_number(3.0));   /* BROWSE_RECONCILE_BUDGET_S */
    json_set(o, "browse_integrity_budget_s", json_number(3.0));   /* = BROWSE_RECONCILE_BUDGET_S */
    json_set(o, "budget_progress_ops", json_number(1000.0));      /* _BUDGET_PROGRESS_OPS */
    json_set(o, "sibling_poll_s", json_number(0.1));              /* _BROWSE_SIBLING_POLL_S */
    return o;
}
