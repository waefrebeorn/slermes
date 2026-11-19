/*
 * local_env_flow.c — local execution-environment FLOW cores of
 * tools/environments/local.py. See local_env_flow.h for the models, the
 * universal seam contract and the seam-name -> live-call table.
 *
 * Decision sequences only: every os.path probe, signal, process poll/wait/kill,
 * psutil snapshot, clock read, shell discovery, policy read and foreign env
 * mutator enters through lef_seams_t.call. Python truthiness, or-chains,
 * short-circuit order, dict insertion order and exception suppress/propagate
 * semantics are transcribed exactly. BaseException-only kinds (SystemExit /
 * KeyboardInterrupt / GeneratorExit / CancelledError) pass through every
 * `except Exception` / suppress(Exception) site.
 */

#include "local_env_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *lef_xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* ── trace recorder ── */
void lef_trace_add(lef_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
void lef_trace_free(lef_trace_t *t) {
    if (t && t->events) { json_free(t->events); t->events = NULL; }
}

/* ── raise spec ── */
const char *lef_raise(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}
const char *lef_raise_msg(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return "";
    return json_get_str(r, "msg", "");
}

/* ── exception hierarchy (scripted kinds) ──
 * `except Exception` / suppress(Exception) catch everything EXCEPT the
 * BaseException-only kinds. OSError-family matching covers the subclasses the
 * fixtures script. TimeoutExpired is subprocess.TimeoutExpired (NOT OSError). */
static bool lef_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0
                 || strcmp(kind, "KeyboardInterrupt") == 0
                 || strcmp(kind, "GeneratorExit") == 0
                 || strcmp(kind, "CancelledError") == 0
                 || strcmp(kind, "BaseException") == 0);
}
static bool lef_is_oserror_kind(const char *kind) {
    static const char *fam[] = {
        "OSError", "ProcessLookupError", "PermissionError", "FileNotFoundError",
        "FileExistsError", "NotADirectoryError", "IsADirectoryError",
        "InterruptedError", "TimeoutError", "BlockingIOError", "ChildProcessError",
        "ConnectionError", "BrokenPipeError", "ConnectionAbortedError",
        "ConnectionRefusedError", "ConnectionResetError",
    };
    if (!kind) return false;
    for (size_t i = 0; i < sizeof(fam) / sizeof(fam[0]); i++)
        if (strcmp(kind, fam[i]) == 0) return true;
    return false;
}
static bool lef_exc_matches(const char *kind, const char *handler) {
    if (!kind) return false;
    if (strcmp(handler, "Exception") == 0) return !lef_is_base_exc(kind);
    if (strcmp(handler, "OSError") == 0) return lef_is_oserror_kind(kind);
    if (strcmp(handler, "TimeoutExpired") == 0) return strcmp(kind, "TimeoutExpired") == 0;
    return strcmp(kind, handler) == 0;
}

/* ── Python truthiness / None ── */
static bool lef_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool lef_is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool lef_num_eq(const json_t *a, const json_t *b) {
    return a && b && a->type == JSON_NUMBER && b->type == JSON_NUMBER
        && a->num_val == b->num_val;
}
static bool lef_json_eq(const json_t *a, const json_t *b) {
    bool an = lef_is_none(a), bn = lef_is_none(b);
    if (an || bn) return an && bn;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val, b->str_val) == 0;
        default:          return false;
    }
}

/* ── dynamic string buffer ── */
typedef struct { char *buf; size_t len, cap; } lef_sb_t;
static void lef_sb_init(lef_sb_t *sb) { sb->buf = NULL; sb->len = 0; sb->cap = 0; }
static void lef_sb_grow(lef_sb_t *sb, size_t need) {
    if (sb->len + need + 1 <= sb->cap) return;
    size_t nc = sb->cap ? sb->cap : 64;
    while (nc < sb->len + need + 1) nc *= 2;
    char *nb = realloc(sb->buf, nc);
    if (!nb) return;
    sb->buf = nb; sb->cap = nc;
}
static void lef_sb_addn(lef_sb_t *sb, const char *s, size_t n) {
    if (!s || !n) return;
    lef_sb_grow(sb, n);
    if (!sb->buf) return;
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
}
static void lef_sb_add(lef_sb_t *sb, const char *s) { if (s) lef_sb_addn(sb, s, strlen(s)); }
static void lef_sb_addch(lef_sb_t *sb, char c) { lef_sb_addn(sb, &c, 1); }
static char *lef_sb_take(lef_sb_t *sb) {
    char *r = sb->buf ? sb->buf : lef_xstrdup("");
    sb->buf = NULL; sb->len = 0; sb->cap = 0;
    return r;
}
static void lef_sb_free(lef_sb_t *sb) { free(sb->buf); sb->buf = NULL; sb->len = 0; sb->cap = 0; }

/* ── string helpers ── */
static bool lef_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    size_t n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}
static char *lef_upper(const char *s) {
    char *d = lef_xstrdup(s ? s : "");
    if (d) for (char *p = d; *p; p++)
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
    return d;
}
/* Python str.split(sep) with a non-empty sep: "" -> [""]; keeps empty fields. */
static char **lef_split(const char *s, const char *sep, size_t *n) {
    *n = 0;
    if (!s) s = "";
    if (!sep || !sep[0]) { char **r = malloc(sizeof(char *)); r[0] = lef_xstrdup(s); *n = 1; return r; }
    size_t sl = strlen(sep), cap = 8, cnt = 0;
    char **parts = malloc(cap * sizeof(char *));
    const char *start = s;
    for (;;) {
        const char *hit = strstr(start, sep);
        size_t len = hit ? (size_t)(hit - start) : strlen(start);
        if (cnt == cap) { cap *= 2; parts = realloc(parts, cap * sizeof(char *)); }
        char *p = malloc(len + 1);
        if (p) { memcpy(p, start, len); p[len] = '\0'; }
        parts[cnt++] = p;
        if (!hit) break;
        start = hit + sl;
    }
    *n = cnt;
    return parts;
}
static void lef_free_split(char **parts, size_t n) {
    if (!parts) return;
    for (size_t i = 0; i < n; i++) free(parts[i]);
    free(parts);
}
static char *lef_join(char *const *parts, size_t n, const char *sep) {
    lef_sb_t sb; lef_sb_init(&sb);
    for (size_t i = 0; i < n; i++) {
        if (i) lef_sb_add(&sb, sep);
        lef_sb_add(&sb, parts[i] ? parts[i] : "");
    }
    return lef_sb_take(&sb);
}

/* shlex.quote (stdlib pure leaf): safe set [A-Za-z0-9_@%+=:,./-]; empty -> '';
 * otherwise wrap in single quotes, escaping ' as '"'"'. Bytes >= 0x80 unsafe. */
static char *lef_shlex_quote(const char *s) {
    if (!s || !s[0]) return lef_xstrdup("''");
    bool safe = true;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
               || (c >= '0' && c <= '9') || c == '_' || c == '@' || c == '%'
               || c == '+' || c == '=' || c == ':' || c == ',' || c == '.'
               || c == '/' || c == '-';
        if (!ok) { safe = false; break; }
    }
    if (safe) return lef_xstrdup(s);
    lef_sb_t sb; lef_sb_init(&sb);
    lef_sb_addch(&sb, '\'');
    for (const char *p = s; *p; p++) {
        if (*p == '\'') lef_sb_add(&sb, "'\"'\"'");
        else lef_sb_addch(&sb, *p);
    }
    lef_sb_addch(&sb, '\'');
    return lef_sb_take(&sb);
}

/* str(KeyError(k)) == repr(k). ASCII fixture keys: wrap in single quotes,
 * escaping backslash and single quote (double-quote wrap when ' present, " not). */
static json_t *lef_keyerror_spec(const char *key) {
    lef_sb_t sb; lef_sb_init(&sb);
    bool has_sq = strchr(key, '\'') != NULL;
    bool has_dq = strchr(key, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    lef_sb_addch(&sb, q);
    for (const char *p = key; *p; p++) {
        if (*p == '\\' || *p == q) lef_sb_addch(&sb, '\\');
        lef_sb_addch(&sb, *p);
    }
    lef_sb_addch(&sb, q);
    json_t *e = json_object();
    json_set(e, "__raise__", json_string("KeyError"));
    char *msg = lef_sb_take(&sb);
    json_set(e, "msg", json_string(msg));
    free(msg);
    return e;
}

/* ── seam call helpers ── */
/* Takes ownership of args (frees it); returns the owned seam result. */
static json_t *lef_call(const lef_seams_t *s, const char *name, json_t *args) {
    json_t *a = args ? args : json_object();
    json_t *r = s->call(s->ctx, name, a);
    json_free(a);
    return r;
}
/* args builders: None-valued keys are OMITTED (mirrored by the oracle). */
static void lef_arg(json_t *a, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void lef_arg_str(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void lef_arg_num(json_t *a, const char *k, double v) {
    json_set(a, k, json_number(v));
}
static void lef_arg_bool(json_t *a, const char *k, bool v) {
    json_set(a, k, json_bool(v));
}
/* seam result -> malloc'd string (non-string results read as ""; fixtures
 * script strings for string-valued seams). */
static char *lef_str_of(const json_t *r) {
    if (r && r->type == JSON_STRING && r->str_val) return lef_xstrdup(r->str_val);
    return lef_xstrdup("");
}
/* Unguarded string seam: propagates any raise via *exc (returns NULL). */
static char *lef_call_str(const lef_seams_t *s, const char *name, json_t *args,
                          json_t **exc) {
    json_t *r = lef_call(s, name, args);
    if (lef_raise(r)) { *exc = r; return NULL; }
    char *v = lef_str_of(r);
    json_free(r);
    return v;
}
/* Unguarded bool seam: propagates any raise via *exc (returns false). */
static bool lef_call_bool(const lef_seams_t *s, const char *name, json_t *args,
                          json_t **exc) {
    json_t *r = lef_call(s, name, args);
    if (lef_raise(r)) { *exc = r; return false; }
    bool v = r && r->type == JSON_BOOL && r->bool_val;
    json_free(r);
    return v;
}
/* Apply an effect-seam updates object to dst (json_set keeps key positions). */
static void lef_apply_updates(json_t *dst, const json_t *updates) {
    if (!dst || !updates || updates->type != JSON_OBJECT) return;
    for (size_t i = 0; i < updates->c.count; i++)
        json_set(dst, updates->c.keys[i], json_copy(updates->c.items[i]));
}
/* ── ASCII lowercase (str.lower on ASCII fixture paths) ── */
static char *lef_lower(const char *s) {
    char *d = lef_xstrdup(s ? s : "");
    if (d) for (char *p = d; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return d;
}
static bool lef_strv_has(char *const *arr, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++)
        if (arr[i] && strcmp(arr[i], s) == 0) return true;
    return false;
}

/* ═══ PATH construction ═══════════════════════════════════════════════════ */

/* PoP: lef_prepend_missing_path_entries @ tools/environments/local.py:_prepend_missing_path_entries */
char *lef_prepend_missing_path_entries(const char *existing_path,
                                       const json_t *dirs, const char *pathsep) {
    /* entries = [e for e in existing_path.split(os.pathsep) if e] */
    size_t na = 0;
    char **all = lef_split(existing_path, pathsep, &na);
    char **entries = malloc((na + 1) * sizeof(char *));
    size_t en = 0;
    for (size_t i = 0; i < na; i++)
        if (all[i] && all[i][0]) entries[en++] = all[i];
    /* missing = [d for d in dirs if d not in entries] (order kept, dups kept) */
    size_t dn = (dirs && dirs->type == JSON_ARRAY) ? dirs->c.count : 0;
    char **missing = malloc((dn + 1) * sizeof(char *));
    size_t mn = 0;
    for (size_t i = 0; i < dn; i++) {
        const json_t *d = dirs->c.items[i];
        const char *ds = (d && d->type == JSON_STRING) ? d->str_val : "";
        if (!lef_strv_has(entries, en, ds)) missing[mn++] = (char *)ds;
    }
    char *ret;
    if (mn) {
        char **joined = malloc((mn + en + 1) * sizeof(char *));
        for (size_t i = 0; i < mn; i++) joined[i] = missing[i];
        for (size_t i = 0; i < en; i++) joined[mn + i] = entries[i];
        ret = lef_join(joined, mn + en, pathsep);
        free(joined);
    } else {
        ret = lef_xstrdup(existing_path);
    }
    free(entries);
    free(missing);
    lef_free_split(all, na);
    return ret;
}

/* PoP: lef_append_dropped_path_entries @ tools/environments/local.py:_append_dropped_path_entries */
char *lef_append_dropped_path_entries(const char *cmd_string,
                                      const char *parent_path, bool is_windows,
                                      const lef_seams_t *s, json_t **exc) {
    if (is_windows) return lef_xstrdup(cmd_string);   /* native PATH passthrough */
    /* dict.fromkeys(parent_path.split(":")) — first-occurrence order, empties kept */
    size_t na = 0;
    char **all = lef_split(parent_path, ":", &na);
    char **uniq = malloc((na + 1) * sizeof(char *));
    size_t un = 0;
    for (size_t i = 0; i < na; i++)
        if (!lef_strv_has(uniq, un, all[i])) uniq[un++] = all[i];
    /* entries = [d for d in uniq if d and (not isabs(d) or isdir(d))] */
    char **entries = malloc((un + 1) * sizeof(char *));
    size_t en = 0;
    for (size_t i = 0; i < un; i++) {
        const char *d = uniq[i];
        if (!d[0]) continue;
        json_t *a = json_object();
        lef_arg_str(a, "path", d);
        bool abs_v = lef_call_bool(s, "isabs", a, exc);
        if (*exc) { free(entries); free(uniq); lef_free_split(all, na); return NULL; }
        if (!abs_v) { entries[en++] = (char *)d; continue; }
        a = json_object();
        lef_arg_str(a, "path", d);
        bool isd = lef_call_bool(s, "isdir", a, exc);
        if (*exc) { free(entries); free(uniq); lef_free_split(all, na); return NULL; }
        if (isd) entries[en++] = (char *)d;
    }
    char *ret = NULL;
    if (en) {
        /* ' '.join(shlex.quote(d) for d in entries) */
        lef_sb_t qb; lef_sb_init(&qb);
        for (size_t i = 0; i < en; i++) {
            if (i) lef_sb_addch(&qb, ' ');
            char *q = lef_shlex_quote(entries[i]);
            lef_sb_add(&qb, q);
            free(q);
        }
        char *quoted = lef_sb_take(&qb);
        lef_sb_t rb; lef_sb_init(&rb);
        lef_sb_add(&rb, "for __hermes_p in ");
        lef_sb_add(&rb, quoted);
        lef_sb_add(&rb, "; do case \":$PATH:\" in *\":$__hermes_p:\"*) ;; *) "
                        "PATH=\"${PATH:+$PATH:}$__hermes_p\" ;; esac; "
                        "done; unset __hermes_p; export PATH");
        lef_sb_addch(&rb, '\n');
        lef_sb_add(&rb, cmd_string);
        ret = lef_sb_take(&rb);
        free(quoted);
    } else {
        ret = lef_xstrdup(cmd_string);
    }
    free(entries);
    free(uniq);
    lef_free_split(all, na);
    return ret;
}

/* PoP: lef_compute_git_bash_bin_dirs @ tools/environments/local.py:_compute_git_bash_bin_dirs */
json_t *lef_compute_git_bash_bin_dirs(const lef_seams_t *s, json_t **exc) {
    /* try: bash = _find_bash() except Exception: return [] */
    json_t *r = lef_call(s, "find_bash", NULL);
    if (lef_raise(r)) {
        if (lef_is_base_exc(lef_raise(r))) { *exc = r; return NULL; }
        json_free(r);
        return json_array();
    }
    char *bash = lef_str_of(r);
    json_free(r);
    /* parent = os.path.dirname(os.path.dirname(bash)) — inner first */
    json_t *a = json_object(); lef_arg_str(a, "path", bash);
    char *inner = lef_call_str(s, "dirname", a, exc);
    if (*exc) { free(bash); return NULL; }
    a = json_object(); lef_arg_str(a, "path", inner);
    char *parent = lef_call_str(s, "dirname", a, exc);
    if (*exc) { free(bash); free(inner); return NULL; }
    a = json_object(); lef_arg_str(a, "path", parent);
    char *base = lef_call_str(s, "basename", a, exc);
    if (*exc) { free(bash); free(inner); free(parent); return NULL; }
    char *blower = lef_lower(base);
    free(base);
    char *root;
    if (strcmp(blower, "usr") == 0) {
        a = json_object(); lef_arg_str(a, "path", parent);
        root = lef_call_str(s, "dirname", a, exc);
        if (*exc) { free(bash); free(inner); free(parent); free(blower); return NULL; }
    } else {
        root = lef_xstrdup(parent);
    }
    free(blower);
    static const char *subs[] = {
        "ucrt64/bin", "clangarm64/bin", "mingw64/bin", "mingw32/bin",
        "usr/local/bin", "usr/bin", "bin",
    };
    json_t *out = json_array();
    /* dirs generator consumed by dict.fromkeys: join THEN isdir per sub,
     * interleaved in subs order; first-occurrence dedup. */
    for (size_t i = 0; i < sizeof(subs) / sizeof(subs[0]); i++) {
        size_t np = 0;
        char **sp = lef_split(subs[i], "/", &np);
        a = json_object();
        json_t *parts = json_array();
        json_append(parts, json_string(root));
        for (size_t j = 0; j < np; j++) json_append(parts, json_string(sp[j]));
        json_set(a, "parts", parts);
        lef_free_split(sp, np);
        char *joined = lef_call_str(s, "path_join", a, exc);
        if (*exc) { free(bash); free(inner); free(parent); free(root); json_free(out); return NULL; }
        a = json_object(); lef_arg_str(a, "path", joined);
        bool isd = lef_call_bool(s, "isdir", a, exc);
        if (*exc) { free(joined); free(bash); free(inner); free(parent); free(root); json_free(out); return NULL; }
        if (isd) {
            bool dup = false;
            for (size_t k = 0; k < out->c.count; k++) {
                const json_t *e = out->c.items[k];
                if (e && e->type == JSON_STRING && strcmp(e->str_val, joined) == 0) { dup = true; break; }
            }
            if (!dup) json_append(out, json_string(joined));
        }
        free(joined);
    }
    free(bash); free(inner); free(parent); free(root);
    return out;
}

/* PoP: lef_user_tools_first @ tools/environments/local.py:_user_tools_first */
char *lef_user_tools_first(const char *existing_path, const char *pathsep,
                           const lef_seams_t *s, json_t **exc) {
    /* path = user_facing_path(existing_path) */
    json_t *a = json_object(); lef_arg_str(a, "path", existing_path);
    char *path = lef_call_str(s, "user_facing_path", a, exc);
    if (*exc) return NULL;
    /* bash_dirs = _git_bash_bin_dirs() */
    json_t *bd = lef_call(s, "git_bash_bin_dirs", NULL);
    if (lef_raise(bd)) { free(path); *exc = bd; return NULL; }
    size_t bdn = (bd && bd->type == JSON_ARRAY) ? bd->c.count : 0;
    /* keys = {os.path.normcase(d) for d in bash_dirs} — normcase per element */
    char **keys = malloc((bdn + 1) * sizeof(char *));
    size_t kn = 0;
    for (size_t i = 0; i < bdn; i++) {
        const json_t *d = bd->c.items[i];
        const char *ds = (d && d->type == JSON_STRING) ? d->str_val : "";
        a = json_object(); lef_arg_str(a, "path", ds);
        char *n = lef_call_str(s, "normcase", a, exc);
        if (*exc) { free(path); json_free(bd); for (size_t k = 0; k < kn; k++) free(keys[k]); free(keys); return NULL; }
        if (!lef_strv_has(keys, kn, n)) keys[kn++] = n;
        else free(n);
    }
    /* rest = [p for p in path.split(pathsep) if p and normcase(p) not in keys] */
    size_t np = 0;
    char **pp = lef_split(path, pathsep, &np);
    char **rest = malloc((np + 1) * sizeof(char *));
    size_t rn = 0;
    for (size_t i = 0; i < np; i++) {
        const char *p = pp[i];
        if (!p[0]) continue;
        a = json_object(); lef_arg_str(a, "path", p);
        char *n = lef_call_str(s, "normcase", a, exc);
        if (*exc) {
            free(path); json_free(bd);
            for (size_t k = 0; k < kn; k++) free(keys[k]);
            free(keys); free(rest); lef_free_split(pp, np);
            return NULL;
        }
        if (!lef_strv_has(keys, kn, n)) rest[rn++] = (char *)p;
        free(n);
    }
    char *ret;
    if (bdn) {
        char **joined = malloc((bdn + rn + 1) * sizeof(char *));
        for (size_t i = 0; i < bdn; i++) {
            const json_t *d = bd->c.items[i];
            joined[i] = (d && d->type == JSON_STRING) ? d->str_val : (char *)"";
        }
        for (size_t i = 0; i < rn; i++) joined[bdn + i] = rest[i];
        ret = lef_join(joined, bdn + rn, pathsep);
        free(joined);
    } else {
        ret = lef_xstrdup(path);
    }
    free(path);
    json_free(bd);
    for (size_t k = 0; k < kn; k++) free(keys[k]);
    free(keys);
    free(rest);
    lef_free_split(pp, np);
    return ret;
}
/* ═══ Child-env resolution ════════════════════════════════════════════════ */

/* PoP: lef_terminal_env @ tools/environments/local.py:_terminal_env */
json_t *lef_terminal_env(json_t *env, const char *pathsep,
                         const lef_seams_t *s, json_t **exc) {
    /* path_key = _path_env_key(env) */
    json_t *a = json_object();
    json_set(a, "env", json_copy(env));
    json_t *r = lef_call(s, "path_env_key", a);
    if (lef_raise(r)) { *exc = r; return NULL; }
    if (lef_is_none(r)) { json_free(r); return env; }   /* None -> env unchanged */
    char *pk = lef_str_of(r);
    json_free(r);
    /* env[path_key] is evaluated BEFORE _user_tools_first: KeyError when absent */
    json_t *cur = json_obj_get(env, pk);
    if (!cur) { *exc = lef_keyerror_spec(pk); free(pk); return NULL; }
    const char *cur_s = (cur->type == JSON_STRING) ? cur->str_val : "";
    json_t *sub = NULL;
    char *np = lef_user_tools_first(cur_s, pathsep, s, &sub);
    if (sub) { free(pk); *exc = sub; return NULL; }
    json_set(env, pk, json_string(np));
    free(np);
    free(pk);
    return env;
}

/* PoP: lef_apply_profile_home @ tools/environments/local.py:_apply_profile_home */
void lef_apply_profile_home(json_t *env, const lef_seams_t *s, json_t **exc) {
    /* try: if value := get_hermes_home_override(): env["HERMES_HOME"] = value
     * except Exception: pass */
    json_t *r = lef_call(s, "get_hermes_home_override", NULL);
    if (lef_raise(r)) {
        if (lef_is_base_exc(lef_raise(r))) { *exc = r; return; }
        json_free(r);
    } else {
        if (lef_truthy(r)) json_set(env, "HERMES_HOME", json_copy(r));
        json_free(r);
    }
    /* apply_subprocess_home_env(env) — foreign in-place mutator (effect seam) */
    json_t *a = json_object();
    json_set(a, "env", json_copy(env));
    r = lef_call(s, "apply_subprocess_home_env", a);
    if (lef_raise(r)) { *exc = r; return; }
    lef_apply_updates(env, r);
    json_free(r);
}

/* PoP: lef_carry_bridge_markers @ tools/environments/local.py:_carry_bridge_markers */
void lef_carry_bridge_markers(json_t *env, const json_t *source,
                              bool own_profile, const lef_seams_t *s,
                              json_t **exc) {
    static const char *PREFIX = "HERMES_BRIDGED_";
    size_t plen = strlen(PREFIX);
    /* for key in [k for k in env if k.upper().startswith(PREFIX)]: del env[key]
     * — snapshot the matching keys first (list comprehension), then delete. */
    size_t n = env->c.count;
    char **del_keys = malloc((n + 1) * sizeof(char *));
    size_t dn = 0;
    for (size_t i = 0; i < n; i++) {
        const char *k = env->c.keys[i];
        char *ku = lef_upper(k);
        if (lef_starts_with(ku, PREFIX)) del_keys[dn++] = lef_xstrdup(k);
        free(ku);
    }
    for (size_t i = 0; i < dn; i++) {
        json_obj_del(env, del_keys[i]);
        free(del_keys[i]);
    }
    free(del_keys);
    if (!own_profile) return;
    if (!source || source->type != JSON_OBJECT) return;
    /* for key, digest in source.items():
     *   name = key[len(PREFIX):]
     *   if key.startswith(PREFIX) and name and digest(env.get(name)) == digest:
     *       env[key] = digest */
    for (size_t i = 0; i < source->c.count; i++) {
        const char *key = source->c.keys[i];
        const json_t *digest = source->c.items[i];
        if (!lef_starts_with(key, PREFIX)) continue;   /* case-SENSITIVE here */
        const char *name = key + plen;
        if (!name[0]) continue;
        json_t *val = json_obj_get(env, name);   /* None when absent */
        json_t *a = json_object();
        json_set(a, "value", val ? json_copy(val) : json_null());
        json_t *r = lef_call(s, "bridged_env_digest", a);
        if (lef_raise(r)) { *exc = r; return; }
        bool eq = lef_json_eq(r, digest);
        json_free(r);
        if (eq) json_set(env, key, json_copy(digest));
    }
}

/* PoP: lef_restore_bootstrap_tokens @ tools/environments/local.py:_restore_bootstrap_tokens */
void lef_restore_bootstrap_tokens(json_t *env, const json_t *source,
                                  const lef_seams_t *s, json_t **exc) {
    /* for name in _secret_source_bootstrap_env():
     *   if name not in env and source.get(name): env[name] = source[name] */
    json_t *r = lef_call(s, "secret_source_bootstrap_env", NULL);
    if (lef_raise(r)) { *exc = r; return; }
    if (r && r->type == JSON_ARRAY) {
        for (size_t i = 0; i < r->c.count; i++) {
            const json_t *nm = r->c.items[i];
            if (!nm || nm->type != JSON_STRING) continue;
            const char *name = nm->str_val;
            if (json_has(env, name)) continue;
            const json_t *sv = source ? json_obj_get(source, name) : NULL;
            if (lef_truthy(sv)) json_set(env, name, json_copy(sv));
        }
    }
    json_free(r);
}
/* ═══ cwd resolution / marker parsing ═════════════════════════════════════ */

/* PoP: lef_usable_cwd @ tools/environments/local.py:_usable_cwd */
json_t *lef_usable_cwd(const char *cwd, const char *self_cwd,
                       const lef_seams_t *s, json_t **exc) {
    /* resolved = os.path.expanduser(cwd) */
    json_t *a = json_object(); lef_arg_str(a, "path", cwd);
    char *resolved = lef_call_str(s, "expanduser", a, exc);
    if (*exc) return NULL;
    /* if resolved and not os.path.isabs(_msys_to_windows_path(resolved)):
     *     resolved = os.path.join(self.cwd, resolved) */
    if (resolved[0]) {
        a = json_object(); lef_arg_str(a, "cwd", resolved);
        char *ms = lef_call_str(s, "msys_to_windows_path", a, exc);
        if (*exc) { free(resolved); return NULL; }
        a = json_object(); lef_arg_str(a, "path", ms);
        bool abs_v = lef_call_bool(s, "isabs", a, exc);
        if (*exc) { free(resolved); free(ms); return NULL; }
        if (!abs_v) {
            a = json_object();
            json_t *parts = json_array();
            json_append(parts, json_string(self_cwd));
            json_append(parts, json_string(resolved));
            json_set(a, "parts", parts);
            char *jr = lef_call_str(s, "path_join", a, exc);
            if (*exc) { free(resolved); free(ms); return NULL; }
            free(resolved);
            resolved = jr;
        }
        free(ms);
    }
    /* safe_cwd = _resolve_safe_cwd(resolved) */
    a = json_object(); lef_arg_str(a, "cwd", resolved);
    char *safe = lef_call_str(s, "resolve_safe_cwd", a, exc);
    if (*exc) { free(resolved); return NULL; }
    /* if safe_cwd == resolved or safe_cwd == _msys_to_windows_path(resolved):
     * — the second msys call fires only when the first compare fails. */
    bool match = strcmp(safe, resolved) == 0;
    if (!match) {
        a = json_object(); lef_arg_str(a, "cwd", resolved);
        char *ms2 = lef_call_str(s, "msys_to_windows_path", a, exc);
        if (*exc) { free(resolved); free(safe); return NULL; }
        match = strcmp(safe, ms2) == 0;
        free(ms2);
    }
    json_t *ret = json_array();
    if (match) {
        /* return (cwd if resolved != cwd else safe_cwd), None */
        json_append(ret, json_string(strcmp(resolved, cwd) != 0 ? cwd : safe));
        json_append(ret, json_null());
    } else {
        /* return safe_cwd, {"requested": cwd, "used": safe_cwd} */
        json_append(ret, json_string(safe));
        json_t *fb = json_object();
        json_set(fb, "requested", json_string(cwd));
        json_set(fb, "used", json_string(safe));
        json_append(ret, fb);
    }
    free(resolved);
    free(safe);
    return ret;
}

/* PoP: lef_parse_cwd_marker @ tools/environments/local.py:_parse_cwd_marker */
void lef_parse_cwd_marker(json_t *result, const lef_seams_t *s, json_t **exc) {
    /* super()._parse_cwd_marker(result) — foreign in-place mutator (effect seam) */
    json_t *a = json_object();
    json_set(a, "result", json_copy(result));
    json_t *r = lef_call(s, "base_parse_cwd_marker", a);
    if (lef_raise(r)) { *exc = r; return; }
    lef_apply_updates(result, r);
    json_free(r);
    /* if result.get("cwd_observed"): */
    json_t *obs = json_obj_get(result, "cwd_observed");
    if (!lef_truthy(obs)) return;
    /* result["cwd"] — KeyError when the marker set cwd_observed but not cwd */
    json_t *cwdn = json_obj_get(result, "cwd");
    if (!cwdn) { *exc = lef_keyerror_spec("cwd"); return; }
    /* normalized = _msys_to_windows_path(result["cwd"]) — arg mirrors the node
     * (None dropped), so a null cwd records {} exactly as the live call. */
    a = json_object(); lef_arg(a, "cwd", cwdn);
    char *norm = lef_call_str(s, "msys_to_windows_path", a, exc);
    if (*exc) return;
    /* if normalized and os.path.isdir(normalized): result["cwd"] = normalized
     * else: pop cwd_observed / cwd */
    bool keep = false;
    if (norm[0]) {
        a = json_object(); lef_arg_str(a, "path", norm);
        keep = lef_call_bool(s, "isdir", a, exc);
        if (*exc) { free(norm); return; }
    }
    if (keep) {
        json_set(result, "cwd", json_string(norm));
    } else {
        json_obj_del(result, "cwd_observed");
        json_obj_del(result, "cwd");
    }
    free(norm);
}
/* ═══ Process-group timeout / kill ladders ════════════════════════════════ */

/* PoP: lef_leader_is_ours @ tools/environments/local.py:_leader_is_ours */
bool lef_leader_is_ours(const json_t *pgid, const json_t *expected_start,
                        const lef_seams_t *s, json_t **exc) {
    if (lef_is_none(pgid)) return false;
    if (lef_is_none(expected_start)) return true;
    /* try: current = get_process_start_time(pgid) except Exception: return True */
    json_t *a = json_object(); lef_arg(a, "pid", pgid);
    json_t *current = lef_call(s, "get_process_start_time", a);
    if (lef_raise(current)) {
        if (lef_is_base_exc(lef_raise(current))) { *exc = current; return false; }
        json_free(current);
        return true;
    }
    if (lef_is_none(current)) { json_free(current); return true; }
    /* try: return start_time_fingerprints_match(expected_start, current)
     * except (TypeError, ValueError): return True */
    a = json_object();
    lef_arg(a, "expected", expected_start);
    lef_arg(a, "current", current);
    json_t *m = lef_call(s, "fingerprints_match", a);
    json_free(current);
    if (lef_raise(m)) {
        const char *kind = lef_raise(m);
        if (strcmp(kind, "TypeError") == 0 || strcmp(kind, "ValueError") == 0) {
            json_free(m);
            return true;
        }
        *exc = m;   /* any other exception (incl. BaseException) propagates */
        return false;
    }
    bool v = m && m->type == JSON_BOOL && m->bool_val;
    json_free(m);
    return v;
}

/* PoP: lef_wait_for_group_exit @ tools/environments/local.py:_wait_for_group_exit */
bool lef_wait_for_group_exit(const json_t *proc, const json_t *pgid,
                             double timeout, const lef_seams_t *s,
                             json_t **exc) {
    /* deadline = time.monotonic() + timeout */
    json_t *r = lef_call(s, "monotonic", NULL);
    if (lef_raise(r)) { *exc = r; return false; }
    double deadline = ((r && r->type == JSON_NUMBER) ? r->num_val : 0.0) + timeout;
    json_free(r);
    const json_t *pid = json_obj_get(proc, "pid");
    for (;;) {
        /* try: proc.poll() except Exception: pass */
        json_t *a = json_object(); lef_arg(a, "pid", pid);
        r = lef_call(s, "proc_poll", a);
        if (lef_raise(r)) {
            if (lef_is_base_exc(lef_raise(r))) { *exc = r; return false; }
            json_free(r);
        } else {
            json_free(r);
        }
        /* try: os.killpg(pgid, 0)
         * except ProcessLookupError: return True
         * except PermissionError: pass */
        a = json_object(); lef_arg(a, "pgid", pgid); lef_arg_num(a, "sig", 0);
        r = lef_call(s, "killpg", a);
        if (lef_raise(r)) {
            const char *kind = lef_raise(r);
            if (strcmp(kind, "ProcessLookupError") == 0) { json_free(r); return true; }
            if (strcmp(kind, "PermissionError") == 0) { json_free(r); }
            else { *exc = r; return false; }   /* anything else propagates */
        } else {
            json_free(r);
        }
        /* if time.monotonic() >= deadline: return False */
        r = lef_call(s, "monotonic", NULL);
        if (lef_raise(r)) { *exc = r; return false; }
        double now = (r && r->type == JSON_NUMBER) ? r->num_val : 0.0;
        json_free(r);
        if (now >= deadline) return false;
        /* time.sleep(0.05) */
        a = json_object(); lef_arg_num(a, "seconds", 0.05);
        r = lef_call(s, "sleep", a);
        if (lef_raise(r)) { *exc = r; return false; }
        json_free(r);
    }
}

/* PoP: lef_sweep_escaped_descendants @ tools/environments/local.py:_sweep_escaped_descendants */
void lef_sweep_escaped_descendants(const json_t *descendants, const json_t *pgid,
                                   const lef_seams_t *s, json_t **exc) {
    if (!descendants || descendants->type != JSON_ARRAY) return;
    for (size_t i = 0; i < descendants->c.count; i++) {
        const json_t *child = descendants->c.items[i];
        const json_t *cpid = json_obj_get(child, "pid");
        /* try: if not child.is_running(): continue */
        json_t *a = json_object(); lef_arg(a, "pid", cpid);
        json_t *r = lef_call(s, "child_is_running", a);
        if (lef_raise(r)) {
            if (lef_is_base_exc(lef_raise(r))) { *exc = r; return; }
            json_free(r);
            continue;   /* except Exception: continue */
        }
        bool running = r && r->type == JSON_BOOL && r->bool_val;
        json_free(r);
        if (!running) continue;
        /* try: if os.getpgid(child.pid) == pgid: continue
         * except OSError: pass */
        a = json_object(); lef_arg(a, "pid", cpid);
        r = lef_call(s, "getpgid", a);
        bool skip = false;
        if (lef_raise(r)) {
            const char *kind = lef_raise(r);
            if (lef_is_oserror_kind(kind)) {
                json_free(r);   /* inner except OSError: pass -> fall to kill */
            } else if (lef_is_base_exc(kind)) {
                *exc = r; return;
            } else {
                json_free(r);
                continue;   /* outer except Exception: continue */
            }
        } else {
            if (lef_num_eq(r, pgid)) skip = true;
            json_free(r);
        }
        if (skip) continue;
        /* child.kill() */
        a = json_object(); lef_arg(a, "pid", cpid);
        r = lef_call(s, "child_kill", a);
        if (lef_raise(r)) {
            if (lef_is_base_exc(lef_raise(r))) { *exc = r; return; }
            json_free(r);
            continue;   /* except Exception: continue */
        }
        json_free(r);
    }
}

/* PoP: lef_kill_known_pids @ tools/environments/local.py:_kill_known_pids */
void lef_kill_known_pids(const json_t *proc, const json_t *descendants,
                         const lef_seams_t *s, json_t **exc) {
    /* for target in (proc, *descendants): with suppress(Exception): target.kill() */
    const json_t *pid = json_obj_get(proc, "pid");
    json_t *a = json_object(); lef_arg(a, "pid", pid);
    json_t *r = lef_call(s, "proc_kill", a);
    if (lef_raise(r)) {
        if (lef_is_base_exc(lef_raise(r))) { *exc = r; return; }
        json_free(r);
    } else {
        json_free(r);
    }
    if (descendants && descendants->type == JSON_ARRAY) {
        for (size_t i = 0; i < descendants->c.count; i++) {
            const json_t *child = descendants->c.items[i];
            const json_t *cpid = json_obj_get(child, "pid");
            a = json_object(); lef_arg(a, "pid", cpid);
            r = lef_call(s, "child_kill", a);
            if (lef_raise(r)) {
                if (lef_is_base_exc(lef_raise(r))) { *exc = r; return; }
                json_free(r);
            } else {
                json_free(r);
            }
        }
    }
}

/* PoP: lef_kill_process_windows @ tools/environments/local.py:_kill_process_windows */
void lef_kill_process_windows(const json_t *proc, bool status_import_fail,
                              const lef_seams_t *s, json_t **exc) {
    const json_t *pid = json_obj_get(proc, "pid");
    bool failed = false;
    if (status_import_fail) {
        failed = true;   /* from gateway.status import ... -> ImportError caught */
    } else {
        /* terminate_pid(proc.pid, force=True,
         *               expected_start_time=get_process_start_time(proc.pid))
         * — the kwarg get_process_start_time is evaluated BEFORE terminate_pid. */
        json_t *a = json_object(); lef_arg(a, "pid", pid);
        json_t *st = lef_call(s, "get_process_start_time", a);
        if (lef_raise(st)) {
            if (lef_is_base_exc(lef_raise(st))) { *exc = st; return; }
            json_free(st);
            failed = true;
        } else {
            a = json_object();
            lef_arg(a, "pid", pid);
            lef_arg_bool(a, "force", true);
            lef_arg(a, "expected_start_time", st);
            json_t *tr = lef_call(s, "terminate_pid", a);
            json_free(st);
            if (lef_raise(tr)) {
                if (lef_is_base_exc(lef_raise(tr))) { *exc = tr; return; }
                json_free(tr);
                failed = true;
            } else {
                json_free(tr);
            }
        }
    }
    if (failed) {
        /* except Exception: proc.kill() — unguarded */
        json_t *a = json_object(); lef_arg(a, "pid", pid);
        json_t *r = lef_call(s, "proc_kill", a);
        if (lef_raise(r)) { *exc = r; return; }
        json_free(r);
    }
    /* with suppress(subprocess.TimeoutExpired, OSError): proc.wait(timeout=2.0) */
    json_t *a = json_object(); lef_arg(a, "pid", pid); lef_arg_num(a, "timeout", 2.0);
    json_t *r = lef_call(s, "proc_wait", a);
    if (lef_raise(r)) {
        const char *kind = lef_raise(r);
        if (lef_exc_matches(kind, "TimeoutExpired") || lef_exc_matches(kind, "OSError"))
            json_free(r);
        else { *exc = r; return; }
    } else {
        json_free(r);
    }
}

/* PoP: lef_force_kill_process @ tools/environments/local.py:_force_kill_process */
void lef_force_kill_process(const json_t *proc, bool is_windows,
                            const lef_seams_t *s, json_t **exc) {
    const json_t *pid = json_obj_get(proc, "pid");
    if (is_windows) {
        /* return self._kill_process(proc) — claimed method, modeled as a seam */
        json_t *a = json_object(); lef_arg(a, "pid", pid);
        json_t *r = lef_call(s, "kill_process", a);
        if (lef_raise(r)) { *exc = r; return; }
        json_free(r);
        return;
    }
    /* with suppress(OSError):
     *   pgid = getattr(proc, "_hermes_pgid", None) or os.getpgid(proc.pid)
     *   if pgid != os.getpgrp() and _leader_is_ours(pgid, _hermes_pgid_start):
     *       os.killpg(pgid, signal.SIGKILL) */
    json_t *b1 = NULL;
    json_t *pgid_n = NULL;
    const json_t *hp = json_obj_get(proc, "_hermes_pgid");
    if (lef_truthy(hp)) {
        pgid_n = json_copy(hp);   /* truthy (non-zero) -> getpgid NOT called */
    } else {
        json_t *a = json_object(); lef_arg(a, "pid", pid);
        json_t *r = lef_call(s, "getpgid", a);
        if (lef_raise(r)) { b1 = r; goto block1_done; }
        pgid_n = r;
    }
    {
        json_t *r = lef_call(s, "getpgrp", NULL);
        if (lef_raise(r)) { b1 = r; goto block1_done; }
        double pgrp = (r && r->type == JSON_NUMBER) ? r->num_val : 0.0;
        json_free(r);
        bool differs = !(pgid_n && pgid_n->type == JSON_NUMBER && pgid_n->num_val == pgrp);
        if (differs) {
            json_t *sub = NULL;
            const json_t *hstart = json_obj_get(proc, "_hermes_pgid_start");
            bool ours = lef_leader_is_ours(pgid_n, hstart, s, &sub);
            if (sub) { b1 = sub; goto block1_done; }
            if (ours) {
                json_t *a = json_object(); lef_arg(a, "pgid", pgid_n); lef_arg_num(a, "sig", 9);
                json_t *rr = lef_call(s, "killpg", a);
                if (lef_raise(rr)) { b1 = rr; goto block1_done; }
                json_free(rr);
            }
        }
    }
block1_done:
    if (pgid_n) json_free(pgid_n);
    if (b1) {
        if (lef_is_oserror_kind(lef_raise(b1))) json_free(b1);
        else { *exc = b1; return; }
    }
    /* with suppress(OSError): proc.kill() */
    {
        json_t *a = json_object(); lef_arg(a, "pid", pid);
        json_t *r = lef_call(s, "proc_kill", a);
        if (lef_raise(r)) {
            if (lef_is_oserror_kind(lef_raise(r))) json_free(r);
            else { *exc = r; return; }
        } else {
            json_free(r);
        }
    }
}

/* PoP: lef_kill_process_group_posix @ tools/environments/local.py:_kill_process_group_posix */
void lef_kill_process_group_posix(const json_t *proc, bool psutil_import_fail,
                                  const lef_seams_t *s, json_t **exc) {
    const json_t *pid = json_obj_get(proc, "pid");
    const json_t *expected_start = json_obj_get(proc, "_hermes_pgid_start");
    json_t *pgid_n = NULL;
    json_t *desc = NULL;
    json_t *r = NULL;
    json_t *sub = NULL;

    /* try: pgid = os.getpgid(proc.pid)
     * except ProcessLookupError:
     *     if (pgid := getattr(proc, "_hermes_pgid", None)) is None: raise */
    {
        json_t *a = json_object(); lef_arg(a, "pid", pid);
        r = lef_call(s, "getpgid", a);
        if (lef_raise(r)) {
            const char *kind = lef_raise(r);
            if (strcmp(kind, "ProcessLookupError") == 0) {
                const json_t *hp = json_obj_get(proc, "_hermes_pgid");
                if (lef_is_none(hp)) { *exc = r; return; }   /* re-raise */
                pgid_n = json_copy(hp);
                json_free(r);
            } else {
                *exc = r; return;   /* only ProcessLookupError is caught here */
            }
        } else {
            pgid_n = r;   /* adopt the number */
        }
    }

    /* if not _leader_is_ours(pgid, expected_start): sweep escapees and return */
    sub = NULL;
    bool ours = lef_leader_is_ours(pgid_n, expected_start, s, &sub);
    if (sub) { json_free(pgid_n); *exc = sub; return; }
    if (!ours) {
        desc = json_array();
        if (!psutil_import_fail) {
            json_t *a = json_object(); lef_arg(a, "pid", pid);
            r = lef_call(s, "psutil_children", a);
            if (lef_raise(r)) {
                if (lef_is_base_exc(lef_raise(r))) { json_free(desc); json_free(pgid_n); *exc = r; return; }
                json_free(r);   /* except Exception: pass -> descendants stays [] */
            } else if (r && r->type == JSON_ARRAY) {
                json_free(desc); desc = r;
            } else {
                json_free(r);   /* null snapshot == empty */
            }
        }
        sub = NULL;
        lef_sweep_escaped_descendants(desc, pgid_n, s, &sub);
        json_free(desc);
        json_free(pgid_n);
        if (sub) { *exc = sub; return; }
        return;
    }

    /* try: descendants = psutil.Process(proc.pid).children(recursive=True)
     * except Exception: descendants = [] */
    desc = json_array();
    if (!psutil_import_fail) {
        json_t *a = json_object(); lef_arg(a, "pid", pid);
        r = lef_call(s, "psutil_children", a);
        if (lef_raise(r)) {
            if (lef_is_base_exc(lef_raise(r))) { json_free(desc); json_free(pgid_n); *exc = r; return; }
            json_free(r);   /* descendants = [] */
        } else if (r && r->type == JSON_ARRAY) {
            json_free(desc); desc = r;
        } else {
            json_free(r);
        }
    }

    /* if pgid == os.getpgrp(): _kill_known_pids(proc, descendants) */
    r = lef_call(s, "getpgrp", NULL);
    if (lef_raise(r)) { json_free(desc); json_free(pgid_n); *exc = r; return; }
    double pgrp = (r && r->type == JSON_NUMBER) ? r->num_val : 0.0;
    json_free(r);
    bool same_group = pgid_n && pgid_n->type == JSON_NUMBER && pgid_n->num_val == pgrp;
    if (same_group) {
        sub = NULL;
        lef_kill_known_pids(proc, desc, s, &sub);
        if (sub) { json_free(desc); json_free(pgid_n); *exc = sub; return; }
    } else {
        /* TERM -> wait -> (re-check leader) -> KILL -> wait -> proc.wait ladder,
         * wrapped in except ProcessLookupError / except PermissionError. */
        json_t *lad_exc = NULL;
        bool early_return = false;
        bool gone = false;
        {
            json_t *a = json_object(); lef_arg(a, "pgid", pgid_n); lef_arg_num(a, "sig", 15);
            r = lef_call(s, "killpg", a);
            if (lef_raise(r)) { lad_exc = r; goto ladder_handled; }
            json_free(r);
        }
        sub = NULL;
        gone = lef_wait_for_group_exit(proc, pgid_n, 1.0, s, &sub);
        if (sub) { lad_exc = sub; goto ladder_handled; }
        if (!gone) {
            sub = NULL;
            ours = lef_leader_is_ours(pgid_n, expected_start, s, &sub);
            if (sub) { lad_exc = sub; goto ladder_handled; }
            if (!ours) { early_return = true; goto ladder_handled; }
            {
                json_t *a = json_object(); lef_arg(a, "pgid", pgid_n); lef_arg_num(a, "sig", 9);
                r = lef_call(s, "killpg", a);
                if (lef_raise(r)) { lad_exc = r; goto ladder_handled; }
                json_free(r);
            }
            sub = NULL;
            lef_wait_for_group_exit(proc, pgid_n, 2.0, s, &sub);
            if (sub) { lad_exc = sub; goto ladder_handled; }
            /* with suppress(TimeoutExpired, OSError): proc.wait(timeout=0.2) */
            {
                json_t *a = json_object(); lef_arg(a, "pid", pid); lef_arg_num(a, "timeout", 0.2);
                r = lef_call(s, "proc_wait", a);
                if (lef_raise(r)) {
                    const char *kind = lef_raise(r);
                    if (lef_exc_matches(kind, "TimeoutExpired") || lef_exc_matches(kind, "OSError"))
                        json_free(r);
                    else { lad_exc = r; goto ladder_handled; }
                } else {
                    json_free(r);
                }
            }
        }
    ladder_handled:
        if (early_return) {
            /* leader recycled during the grace window: return, sweep skipped */
            json_free(desc);
            json_free(pgid_n);
            return;
        }
        if (lad_exc) {
            const char *kind = lef_raise(lad_exc);
            if (strcmp(kind, "ProcessLookupError") == 0) {
                json_free(lad_exc);   /* except ProcessLookupError: pass */
            } else if (strcmp(kind, "PermissionError") == 0) {
                json_free(lad_exc);   /* except PermissionError: _kill_known_pids */
                sub = NULL;
                lef_kill_known_pids(proc, desc, s, &sub);
                if (sub) { json_free(desc); json_free(pgid_n); *exc = sub; return; }
            } else {
                json_free(desc); json_free(pgid_n); *exc = lad_exc; return;
            }
        }
    }

    /* _sweep_escaped_descendants(descendants, pgid) */
    sub = NULL;
    lef_sweep_escaped_descendants(desc, pgid_n, s, &sub);
    json_free(desc);
    json_free(pgid_n);
    if (sub) { *exc = sub; return; }
}
