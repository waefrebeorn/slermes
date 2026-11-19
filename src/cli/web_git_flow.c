/* web_git_flow.c -- FLOW port of the UNCLAIMED hermes_cli/web_git.py git ops.
 *
 * Mission: match the program's FLOW (call order, branches, error paths) of the
 * web dashboard git-operations module's still-unclaimed functions: the git
 * query/projection leaves (_git_line, _status_z, _origin_head*, _ref_exists,
 * _is_dir), porcelain-v2 classification (_entry_staged, _classify,
 * _status_letter, _has_staged), the review-pane helpers (_review_result,
 * _all_add_diff, _single_file_diff, file_diff_vs_head, review_stage/unstage/
 * revert/rev_parse/push, _review_push), the gh ship leaves (_gh_json,
 * review_ship_info, _pr_query, _pr_payload, _own_pr, review_create_pr) and the
 * worktree/branch decision chains (worktree_list, _main_root, worktree_remove,
 * _fetch_tracking_ref, _slugify, _default_branch, _ensure_repo, _remote_of_ref,
 * _ref_names, branch_list, branch_switch, base_branch_list).
 *
 * Self-contained, libjson-only; no real I/O inside the decision core. Every
 * live external call is INJECTED through a single universal seam
 * `wg_io_t.call(io, name, args)`:
 *   - wg_call appends {"name": name, "args": args} to io->trace (ordered);
 *   - returns the scripted result (a NEW json_t* the caller frees);
 *   - a raised exception is {"__raise__": "<Type>", "message": str(exc)};
 *   - args passed to wg_call are BORROWED (the caller frees them); the seam
 *     result is OWNED by the caller. NULL result == Python None.
 *
 * Live counterpart of each seam name (CLAIMED siblings are seamed, not
 * re-ported here -- see the PoP grep of src/):
 *   git              _git(cwd, args, *, timeout=30)        [wgf claim]
 *   git_write        _git_write(cwd, args, *, timeout=30)  [wgf claim]
 *                    args {cwd, args, timeout} -> [code, out, err]
 *   git_out          _git_out(cwd, args, *, run=_git)      [wgf claim]
 *                    args {cwd, args, run:"read"|"write"} -> stdout str
 *   git_ok           _git_ok(cwd, args)                    [wgf claim]
 *                    args {cwd, args} -> null (raises RuntimeError on fail)
 *   gh               _gh(cwd, args)                        [env_passthrough claim]
 *                    args {cwd, args} -> [ok, out, err]
 *   walk_entries     _walk_entries(raw)                    [web_git_branch claim]
 *                    args {raw} -> [[tag, xy, path], ...]
 *   untracked_insertions  _untracked_insertions(cwd, rel)  [web_git_branch claim]
 *                    args {cwd, rel} -> int
 *   sanitize_branch  _sanitize_branch(name)                [web_git_branch claim]
 *                    args {name} -> str
 *   path.is_dir      Path(cwd).is_dir() in _is_dir; args {path}=as_posix()
 *   path.is_file     (Path(cwd)/file_path).is_file() in _all_add_diff;
 *                    args {path}=joined as_posix()
 *   path.resolve     Path.resolve() in _single_file_diff; args {path}=
 *                    self.as_posix(); ret string wrapped back into Path() by
 *                    the oracle. Path equality folds ASCII case under _WIN32.
 *   tracking_refspec hermes_cli.update_cmd_check.tracking_refspec(remote,
 *                    branch) (in-function import in _fetch_tracking_ref);
 *                    args {remote, branch} -> str
 *
 * Representation contract (shared with the oracle):
 * - Tuples project to JSON arrays: git/git_write/gh -> [a, b, c];
 *   _status_z -> [code, raw]; walk_entries yields -> [tag, xy, path].
 * - None == JSON null. Raise descriptors {"__raise__": kind, "message"} propagate
 *   verbatim; message == str(exc) EXACTLY. The BaseException trio (SystemExit,
 *   KeyboardInterrupt, GeneratorExit) passes through EVERY catch site; only the
 *   specifically-named exception types are swallowed (OSError in _is_dir,
 *   RuntimeError in review_create_pr, json.JSONDecodeError in _gh_json).
 * - NUL-record convention: libjson strings are NUL-terminated, so porcelain -z
 *   payloads encode NUL as U+0001 in fixtures; the C core splits records on
 *   \x01 and the oracle decodes \x01 -> \x00 on inputs / encodes \x00 -> \x01 on
 *   outputs. Fixtures never carry a literal \x01.
 * - Paths reach seams via as_posix(); the C core models the HOST Path
 *   (WindowsPath under _WIN32, PurePosixPath otherwise): drive/root parse,
 *   "."/empty component collapse, ".." kept, join replace rules. Fixture path
 *   domain: POSIX-absolute ("/r"), drive-absolute ("C:/r"), relative, empty;
 *   NO UNC double-leading-slash ("//host") inputs -- WindowsPath preserves a
 *   UNC drive the single-root C model collapses (verified out-of-domain only).
 * - str.strip() strips the full Unicode White_Space set; str.splitlines() splits
 *   on \n \r \r\n \v \f \x1c \x1d \x1e \x85 U+2028 U+2029. int()/str()/json.dumps
 *   are ASCII-domain (fixtures avoid Unicode digits / non-ASCII case / control
 *   chars in JSON-string positions). Integral doubles serialize int-like.
 * - Dict key order is insertion order (libjson json_set keeps position on
 *   replace). _review_result sorts STABLY by path (UTF-8 byte order).
 *
 * Ownership: every wg_* target returns a NEW json_t* (caller frees). Inputs are
 * borrowed. Internal helpers return NEW nodes; a raise descriptor IS the return
 * value (wg_raised detects it) and propagates up the call chain.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define WG_GIT_TIMEOUT 30
#define WG_GH_ERR_TAIL_CHARS 400
#define WG_PR_QUERY_BRANCH_CHUNK 50
#define WG_PR_QUERY_BRANCH_CAP 300
#define WG_PR_NODE_FIELDS "number state isDraft isCrossRepository title url headRefName"

/* -- universal seam ------------------------------------------------------ */
typedef struct wg_io_t {
    json_t *(*call)(struct wg_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"name","args"}: ordered seam sequence */
    void   *ctx;     /* harness backend (scripted results) */
} wg_io_t;

static json_t *wg_call(wg_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_append(io->trace, ev);
    if (!io->call) return json_null();
    return io->call(io, name, args);
}

static bool wg_raised(const json_t *result) {
    return result && result->type == JSON_OBJECT && json_obj_get(result, "__raise__") != NULL;
}

static json_t *raise_desc(const char *type, const char *message) {
    json_t *o = json_object();
    json_set_hdr(o, "__raise__", type);
    json_set_hdr(o, "message", message ? message : "");
    return o;
}

/* -- Python truthiness / type tests -------------------------------------- */
static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool j_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool j_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool j_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool j_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static const char *py_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* -- exception classification (live except clauses) ---------------------- */
static const char *exc_type(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static bool exc_is_oserror(const json_t *r) {
    const char *t = exc_type(r);
    return strcmp(t, "OSError") == 0 || strcmp(t, "FileNotFoundError") == 0
        || strcmp(t, "PermissionError") == 0 || strcmp(t, "NotADirectoryError") == 0
        || strcmp(t, "IsADirectoryError") == 0 || strcmp(t, "FileExistsError") == 0
        || strcmp(t, "BlockingIOError") == 0 || strcmp(t, "InterruptedError") == 0
        || strcmp(t, "ProcessLookupError") == 0 || strcmp(t, "TimeoutError") == 0
        || strcmp(t, "WindowsError") == 0;
}

/* -- UTF-8 helpers ------------------------------------------------------- */
static unsigned long utf8_next(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned char c = s[0];
    if (c < 0x80) { *p += 1; return c; }
    unsigned long cp; int n;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 2; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 3; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 4; }
    else { *p += 1; return 0xFFFD; }
    for (int i = 1; i < n; i++) {
        if ((s[i] & 0xC0) != 0x80) { *p += 1; return 0xFFFD; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *p += n;
    return cp;
}
static bool py_isspace(unsigned long cp) {
    if (cp <= 0x20) return (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F) || cp == 0x20;
    return cp == 0x85 || cp == 0xA0 || cp == 0x1680
        || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029
        || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}
static char *py_strip(const char *s) {
    if (!s) s = "";
    size_t len = strlen(s);
    size_t a = 0, b = len;
    while (a < b) {
        const char *q = s + a;
        unsigned long cp = utf8_next(&q);
        if (!py_isspace(cp)) break;
        a = (size_t)(q - s);
    }
    while (b > a) {
        size_t k = b - 1;
        while (k > a && (((unsigned char)s[k]) & 0xC0) == 0x80) k--;
        const char *q = s + k;
        unsigned long cp = utf8_next(&q);
        if (!py_isspace(cp) || (size_t)(q - s) != b) break;
        b = k;
    }
    char *out = malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
static bool py_linebreak(unsigned long cp) {
    return cp == '\n' || cp == '\r' || cp == 0x0B || cp == 0x0C
        || cp == 0x1C || cp == 0x1D || cp == 0x1E || cp == 0x85
        || cp == 0x2028 || cp == 0x2029;
}
/* mode 0 = split('\n') (keeps trailing empties); 1 = splitlines(). */
static json_t *py_split(const char *s, int mode) {
    json_t *arr = json_array();
    if (!s) s = "";
    const char *p = s;
    const char *start = s;
    if (mode == 1) {
        while (*p) {
            const char *q = p;
            unsigned long cp = utf8_next(&q);
            if (py_linebreak(cp)) {
                size_t n = (size_t)(p - start);
                char *seg = malloc(n + 1);
                if (seg) { memcpy(seg, start, n); seg[n] = '\0'; json_append(arr, json_string(seg)); free(seg); }
                if (cp == '\r' && *q == '\n') q++;
                p = q; start = q;
            } else {
                p = q;
            }
        }
        if (start < p) json_append(arr, json_string(start));
        return arr;
    }
    for (;;) {
        const char *z = strchr(p, '\n');
        if (!z) { json_append(arr, json_string(p)); break; }
        size_t n = (size_t)(z - p);
        char *seg = malloc(n + 1);
        if (seg) { memcpy(seg, p, n); seg[n] = '\0'; json_append(arr, json_string(seg)); free(seg); }
        p = z + 1;
    }
    return arr;
}
static size_t py_len(const char *s) {
    size_t n = 0;
    const char *p = s ? s : "";
    while (*p) { utf8_next(&p); n++; }
    return n;
}
static char *py_slice_left(const char *s, size_t count) {
    const char *p = s ? s : "";
    size_t n = 0;
    while (*p && n < count) { utf8_next(&p); n++; }
    size_t bytes = (size_t)(p - (s ? s : ""));
    char *out = malloc(bytes + 1);
    if (!out) return NULL;
    memcpy(out, s ? s : "", bytes);
    out[bytes] = '\0';
    return out;
}
/* s[-count:] -- the last `count` codepoints (whole string when shorter). */
static char *py_slice_right(const char *s, size_t count) {
    if (!s) s = "";
    size_t total = py_len(s);
    if (count >= total) return strdup(s);
    size_t skip = total - count;
    const char *p = s;
    size_t n = 0;
    while (*p && n < skip) { utf8_next(&p); n++; }
    return strdup(p);
}
static char *py_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    for (char *p = out; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return out;
}
static char *py_replace(const char *s, const char *old, const char *nw) {
    size_t ol = strlen(old), nl = strlen(nw), sl = strlen(s);
    if (ol == 0) return strdup(s);
    size_t cap = sl + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    const char *p = s;
    while (*p) {
        if (strncmp(p, old, ol) == 0) {
            while (o + nl + 1 > cap) { cap *= 2; out = realloc(out, cap); if (!out) return NULL; }
            memcpy(out + o, nw, nl); o += nl; p += ol;
        } else {
            if (o + 2 > cap) { cap *= 2; out = realloc(out, cap); if (!out) return NULL; }
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
    return out;
}
static char *py_repr_str(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c < 0x20 || c == 0x7F) { o += (size_t)snprintf(out + o, cap - o, "\\x%02x", c); }
        else if (c >= 0x80) { o += (size_t)snprintf(out + o, cap - o, "\\x%02x", c); }
        else out[o++] = (char)c;
    }
    out[o++] = q;
    out[o] = '\0';
    return out;
}
/* str() of a JSON scalar -- None -> "None" (the true str(), not `or ""`). */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(jstr(v));
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        char buf[64];
        if (d == floor(d) && fabs(d) < 1e16) {
            snprintf(buf, sizeof buf, "%.0f", d);
        } else {
            int prec;
            for (prec = 1; prec <= 17; prec++) {
                snprintf(buf, sizeof buf, "%.*g", prec, d);
                if (strtod(buf, NULL) == d) break;
            }
        }
        return strdup(buf);
    }
    return strdup("");   /* containers out of the str() fixture domain */
}
/* int(str) -- ASCII digits, sign, Unicode-whitespace trim, between-digit
 * underscores; exact CPython 3.14 ValueError message. Domain |value| <= 2^53. */
static bool py_int_parse(const char *s, double *out, char **err) {
    char *trimmed = py_strip(s);
    const char *p = trimmed;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    double acc = 0;
    int ndigits = 0;
    bool last_was_digit = false;
    bool ok = *p != '\0';
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            acc = acc * 10 + (*p - '0');
            ndigits++;
            last_was_digit = true;
        } else if (*p == '_') {
            if (!last_was_digit || !(p[1] >= '0' && p[1] <= '9')) { ok = false; break; }
            last_was_digit = false;
        } else { ok = false; break; }
    }
    if (!ok || ndigits == 0 || !last_was_digit) {
        char *rep = py_repr_str(s);
        size_t need = strlen(rep) + 64;
        *err = malloc(need);
        if (*err) snprintf(*err, need, "invalid literal for int() with base 10: %s", rep);
        free(rep);
        free(trimmed);
        return false;
    }
    *out = neg ? -acc : acc;
    free(trimmed);
    return true;
}
/* int(v) for a JSON scalar (number truncates toward zero; bool -> 0/1; str
 * parses). On failure returns a raise descriptor; on success *out is set and
 * NULL returned. */
static json_t *to_int(const json_t *v, double *out) {
    if (j_is_num(v)) { *out = trunc(v->num_val); return NULL; }
    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return NULL; }
    if (j_is_str(v)) {
        char *emsg = NULL;
        if (py_int_parse(jstr(v), out, &emsg)) { free(emsg); return NULL; }
        json_t *e = raise_desc("ValueError", emsg ? emsg : "");
        free(emsg);
        return e;
    }
    char msg[160];
    snprintf(msg, sizeof msg,
             "int() argument must be a string, a bytes-like object or a real number, not '%s'",
             py_type_name(v));
    return raise_desc("TypeError", msg);
}

/* -- NUL-record convention (U+0001 sentinel) ----------------------------- */
#define WG_NUL_SENTINEL '\x01'
static json_t *wg_split_records(const char *s) {
    json_t *arr = json_array();
    if (!s) s = "";
    const char *p = s;
    for (;;) {
        const char *z = strchr(p, WG_NUL_SENTINEL);
        if (!z) { json_append(arr, json_string(p)); break; }
        size_t n = (size_t)(z - p);
        char *seg = malloc(n + 1);
        if (seg) { memcpy(seg, p, n); seg[n] = '\0'; json_append(arr, json_string(seg)); free(seg); }
        p = z + 1;
    }
    return arr;
}

/* -- small string helpers ------------------------------------------------ */
static bool str_starts(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool str_ends(const char *s, const char *suffix) {
    size_t sl = strlen(s), xl = strlen(suffix);
    return sl >= xl && strcmp(s + sl - xl, suffix) == 0;
}
static bool str_contains(const char *s, const char *needle) {
    return strstr(s, needle) != NULL;
}
static void str_partition(const char *s, const char *sep, char **before, char **after, bool *found) {
    const char *z = strstr(s, sep);
    if (!z) { *before = strdup(s); *after = strdup(""); *found = false; return; }
    size_t n = (size_t)(z - s);
    *before = malloc(n + 1);
    if (*before) { memcpy(*before, s, n); (*before)[n] = '\0'; }
    *after = strdup(z + strlen(sep));
    *found = true;
}
/* name.split("/", 1)[-1]: after the FIRST '/' (whole string when none). */
static const char *split1_last(const char *name) {
    const char *slash = strchr(name, '/');
    return slash ? slash + 1 : name;
}
/* name.split("\t")[0]: before the FIRST tab (whole string when none). */
static char *split_tab0(const char *line) {
    const char *z = strchr(line, '\t');
    if (!z) return strdup(line);
    size_t n = (size_t)(z - line);
    char *out = malloc(n + 1);
    if (out) { memcpy(out, line, n); out[n] = '\0'; }
    return out;
}

/* -- host Path model (WindowsPath under _WIN32, PurePosixPath else) ------- */
typedef struct {
    char drive[8];
    bool has_root;
    char **parts;
    size_t nparts;
} wg_path_t;

static void path_free(wg_path_t *p) {
    for (size_t i = 0; i < p->nparts; i++) free(p->parts[i]);
    free(p->parts);
    p->parts = NULL; p->nparts = 0;
}
static bool is_sep(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}
static int wg_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}
static void path_splitroot(const char *s, wg_path_t *out) {
    memset(out, 0, sizeof *out);
    const char *rest = s;
#ifdef _WIN32
    if (s[0] && s[1] == ':' && ((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z'))) {
        out->drive[0] = s[0]; out->drive[1] = ':'; out->drive[2] = '\0';
        rest = s + 2;
    }
#endif
    if (is_sep(rest[0])) { out->has_root = true; rest++; }
    size_t cap = 8;
    out->parts = malloc(cap * sizeof(char *));
    const char *p = rest;
    while (*p) {
        const char *start = p;
        while (*p && !is_sep(*p)) p++;
        size_t n = (size_t)(p - start);
        if (n > 0 && !(n == 1 && start[0] == '.')) {
            if (out->nparts == cap) { cap *= 2; out->parts = realloc(out->parts, cap * sizeof(char *)); }
            char *seg = malloc(n + 1);
            memcpy(seg, start, n); seg[n] = '\0';
            out->parts[out->nparts++] = seg;
        }
        if (*p) p++;
    }
}
static char *path_as_posix(const wg_path_t *p) {
    size_t cap = 64, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    size_t dl = strlen(p->drive);
    while (len + dl + 2 > cap) { cap *= 2; out = realloc(out, cap); }
    memcpy(out + len, p->drive, dl); len += dl;
    if (p->has_root) {
        if (len + 2 > cap) { cap *= 2; out = realloc(out, cap); }
        out[len++] = '/';
    }
    for (size_t i = 0; i < p->nparts; i++) {
        size_t sl = strlen(p->parts[i]);
        while (len + sl + 2 > cap) { cap *= 2; out = realloc(out, cap); }
        if (i) out[len++] = '/';
        memcpy(out + len, p->parts[i], sl); len += sl;
    }
    out[len] = '\0';
    if (len == 0) { out[0] = '.'; out[1] = '\0'; }
    return out;
}
static char *wg_path_norm(const char *s) {
    wg_path_t p;
    path_splitroot(s ? s : "", &p);
    char *out = path_as_posix(&p);
    path_free(&p);
    return out;
}
static char *wg_path_join(const char *a, const char *b) {
    wg_path_t pa, pb;
    path_splitroot(a ? a : "", &pa);
    path_splitroot(b ? b : "", &pb);
    wg_path_t r;
    memset(&r, 0, sizeof r);
    if (pb.has_root) {
        snprintf(r.drive, sizeof r.drive, "%s", pb.drive[0] ? pb.drive : pa.drive);
        r.has_root = true;
        r.parts = pb.parts; r.nparts = pb.nparts;
        pb.parts = NULL; pb.nparts = 0;
    } else if (pb.drive[0]) {
#ifdef _WIN32
        bool same = pa.drive[0] && (wg_strcasecmp(pa.drive, pb.drive) == 0);
#else
        bool same = pa.drive[0] && strcmp(pa.drive, pb.drive) == 0;
#endif
        if (!same) {
            snprintf(r.drive, sizeof r.drive, "%s", pb.drive);
            r.has_root = pb.has_root;
            r.parts = pb.parts; r.nparts = pb.nparts;
            pb.parts = NULL; pb.nparts = 0;
        } else {
            snprintf(r.drive, sizeof r.drive, "%s", pb.drive);
            r.has_root = pa.has_root;
            r.parts = pa.parts; r.nparts = pa.nparts;
            pa.parts = NULL; pa.nparts = 0;
            for (size_t i = 0; i < pb.nparts; i++) {
                r.parts = realloc(r.parts, (r.nparts + 1) * sizeof(char *));
                r.parts[r.nparts++] = pb.parts[i];
            }
            free(pb.parts); pb.parts = NULL; pb.nparts = 0;
        }
    } else {
        snprintf(r.drive, sizeof r.drive, "%s", pa.drive);
        r.has_root = pa.has_root;
        r.parts = pa.parts; r.nparts = pa.nparts;
        pa.parts = NULL; pa.nparts = 0;
        for (size_t i = 0; i < pb.nparts; i++) {
            r.parts = realloc(r.parts, (r.nparts + 1) * sizeof(char *));
            r.parts[r.nparts++] = pb.parts[i];
        }
        free(pb.parts); pb.parts = NULL; pb.nparts = 0;
    }
    char *out = path_as_posix(&r);
    path_free(&r);
    path_free(&pa);
    path_free(&pb);
    return out;
}
static bool wg_path_eq(const char *a, const char *b) {
#ifdef _WIN32
    return wg_strcasecmp(a, b) == 0;
#else
    return strcmp(a, b) == 0;
#endif
}
static json_t *wg_path_type_error(const json_t *cwd) {
    char msg[256];
    snprintf(msg, sizeof msg,
             "argument should be a str or an os.PathLike object where __fspath__ returns a str, not '%s'",
             py_type_name(cwd));
    return raise_desc("TypeError", msg);
}

/* str.split() with no args: runs of Unicode whitespace, no empties. */
static json_t *py_split_ws(const char *s) {
    json_t *arr = json_array();
    const char *p = s ? s : "";
    while (*p) {
        const char *q = p;
        unsigned long cp = utf8_next(&q);
        if (py_isspace(cp)) { p = q; continue; }
        const char *start = p;
        while (*p) {
            q = p;
            cp = utf8_next(&q);
            if (py_isspace(cp)) break;
            p = q;
        }
        size_t n = (size_t)(p - start);
        char *seg = malloc(n + 1);
        if (seg) { memcpy(seg, start, n); seg[n] = '\0'; json_append(arr, json_string(seg)); free(seg); }
    }
    return arr;
}

/* -- dynamic string builder ---------------------------------------------- */
typedef struct { char *s; size_t len, cap; } wg_sb;
static void sb_init(wg_sb *b) { b->cap = 64; b->len = 0; b->s = malloc(b->cap); if (b->s) b->s[0] = '\0'; }
static void sb_add(wg_sb *b, const char *s) {
    if (!b->s || !s) return;
    size_t n = strlen(s);
    while (b->len + n + 1 > b->cap) { b->cap *= 2; b->s = realloc(b->s, b->cap); if (!b->s) return; }
    memcpy(b->s + b->len, s, n); b->len += n; b->s[b->len] = '\0';
}
static void sb_addn(wg_sb *b, const char *s, size_t n) {
    if (!b->s || !s) return;
    while (b->len + n + 1 > b->cap) { b->cap *= 2; b->s = realloc(b->s, b->cap); if (!b->s) return; }
    memcpy(b->s + b->len, s, n); b->len += n; b->s[b->len] = '\0';
}/* -- claimed-sibling seam dispatchers ------------------------------------ */
/* _git / _git_write -> [code, out, err]; run tag selects the seam name. */
static json_t *seam_runner(wg_io_t *io, const json_t *cwd, const json_t *args, bool write) {
    json_t *a = json_object();
    json_set(a, "cwd", cwd ? json_copy(cwd) : json_null());
    json_set(a, "args", args ? json_copy(args) : json_array());
    json_set(a, "timeout", json_number(WG_GIT_TIMEOUT));
    json_t *r = wg_call(io, write ? "git_write" : "git", a);
    json_free(a);
    return r ? r : json_null();
}
/* _git_out -> stdout str; run tag recorded as "read"/"write". */
static json_t *seam_git_out(wg_io_t *io, const json_t *cwd, const json_t *args, bool write) {
    json_t *a = json_object();
    json_set(a, "cwd", cwd ? json_copy(cwd) : json_null());
    json_set(a, "args", args ? json_copy(args) : json_array());
    json_set_hdr(a, "run", write ? "write" : "read");
    json_t *r = wg_call(io, "git_out", a);
    json_free(a);
    return r ? r : json_null();
}
/* _git_ok -> null (the seam raises RuntimeError on failure). */
static json_t *seam_git_ok(wg_io_t *io, const json_t *cwd, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "cwd", cwd ? json_copy(cwd) : json_null());
    json_set(a, "args", args ? json_copy(args) : json_array());
    json_t *r = wg_call(io, "git_ok", a);
    json_free(a);
    return r ? r : json_null();
}
/* _gh -> [ok, out, err]. */
static json_t *seam_gh(wg_io_t *io, const json_t *cwd, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "cwd", cwd ? json_copy(cwd) : json_null());
    json_set(a, "args", args ? json_copy(args) : json_array());
    json_t *r = wg_call(io, "gh", a);
    json_free(a);
    return r ? r : json_null();
}
static json_t *seam_walk_entries(wg_io_t *io, const json_t *raw) {
    json_t *a = json_object();
    json_set(a, "raw", raw ? json_copy(raw) : json_null());
    json_t *r = wg_call(io, "walk_entries", a);
    json_free(a);
    return r ? r : json_null();
}
static json_t *seam_untracked(wg_io_t *io, const json_t *cwd, const char *rel) {
    json_t *a = json_object();
    json_set(a, "cwd", cwd ? json_copy(cwd) : json_null());
    json_set_hdr(a, "rel", rel ? rel : "");
    json_t *r = wg_call(io, "untracked_insertions", a);
    json_free(a);
    return r ? r : json_null();
}
static json_t *seam_sanitize(wg_io_t *io, const json_t *name) {
    json_t *a = json_object();
    json_set(a, "name", name ? json_copy(name) : json_null());
    json_t *r = wg_call(io, "sanitize_branch", a);
    json_free(a);
    return r ? r : json_null();
}
static json_t *seam_tracking_refspec(wg_io_t *io, const char *remote, const char *branch) {
    json_t *a = json_object();
    json_set_hdr(a, "remote", remote ? remote : "");
    json_set_hdr(a, "branch", branch ? branch : "");
    json_t *r = wg_call(io, "tracking_refspec", a);
    json_free(a);
    return r ? r : json_null();
}

/* -- _git_line: stripped stdout of _git_out; non-str -> AttributeError --- */
static json_t *h_git_line(wg_io_t *io, const json_t *cwd, const json_t *args, bool write) {
    json_t *out = seam_git_out(io, cwd, args, write);
    if (wg_raised(out)) return out;
    if (!j_is_str(out)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", py_type_name(out));
        json_free(out);
        return raise_desc("AttributeError", msg);
    }
    char *s = py_strip(jstr(out));
    json_free(out);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* -- _is_dir: Path(cwd).is_dir(); OSError -> False; Path(None) TypeError -- */
static json_t *h_is_dir(wg_io_t *io, const json_t *cwd) {
    if (!j_is_str(cwd)) return wg_path_type_error(cwd);
    char *norm = wg_path_norm(jstr(cwd));
    json_t *a = json_object();
    json_set_hdr(a, "path", norm ? norm : "");
    free(norm);
    json_t *r = wg_call(io, "path.is_dir", a);
    json_free(a);
    if (!r) r = json_null();
    if (wg_raised(r)) {
        if (exc_is_oserror(r)) { json_free(r); return json_bool(false); }
        return r;
    }
    return r;
}

/* -- _status_z(cwd, *extra, run=...): [code, raw] ------------------------ */
static json_t *h_status_z(wg_io_t *io, const json_t *cwd, const json_t *extra, bool write) {
    json_t *args = json_array();
    json_append(args, json_string("status"));
    json_append(args, json_string("--porcelain=v2"));
    if (j_is_arr(extra)) {
        for (size_t i = 0; i < json_len(extra); i++) json_append(args, json_copy(json_get(extra, i)));
    }
    json_append(args, json_string("-z"));
    json_t *t = seam_runner(io, cwd, args, write);
    json_free(args);
    if (wg_raised(t)) return t;
    json_t *r = json_array();
    json_t *code = json_get(t, 0);
    json_t *raw = json_get(t, 1);
    json_append(r, code ? json_copy(code) : json_null());
    json_append(r, raw ? json_copy(raw) : json_null());
    json_free(t);
    return r;
}

static json_t *h_origin_head(wg_io_t *io, const json_t *cwd, bool write) {
    json_t *args = json_array();
    json_append(args, json_string("symbolic-ref"));
    json_append(args, json_string("--quiet"));
    json_append(args, json_string("--short"));
    json_append(args, json_string("refs/remotes/origin/HEAD"));
    json_t *r = h_git_line(io, cwd, args, write);
    json_free(args);
    return r;
}

static json_t *h_origin_head_abbrev(wg_io_t *io, const json_t *cwd) {
    json_t *args = json_array();
    json_append(args, json_string("rev-parse"));
    json_append(args, json_string("--abbrev-ref"));
    json_append(args, json_string("origin/HEAD"));
    json_t *r = h_git_line(io, cwd, args, false);
    json_free(args);
    return r;
}

static json_t *h_ref_exists(wg_io_t *io, const json_t *cwd, const char *ref, bool write) {
    json_t *args = json_array();
    json_append(args, json_string("rev-parse"));
    json_append(args, json_string("--verify"));
    json_append(args, json_string("--quiet"));
    json_append(args, json_string(ref));
    json_t *t = seam_runner(io, cwd, args, write);
    json_free(args);
    if (wg_raised(t)) return t;
    json_t *code = json_get(t, 0);
    json_t *r = json_bool(j_is_num(code) && code->num_val == 0.0);
    json_free(t);
    return r;
}

/* -- pure porcelain leaves ----------------------------------------------- */
static json_t *h_entry_staged(const char *tag, const char *xy) {
    bool tracked = strcmp(tag, "1") == 0 || strcmp(tag, "2") == 0;
    if (!tracked) return json_bool(false);
    if (!xy[0]) return raise_desc("IndexError", "string index out of range");
    return json_bool(xy[0] != '.' && xy[0] != '?');
}

static json_t *h_classify(const char *tag, const char *xy, const char *path) {
    char y = (xy[0] && xy[1]) ? xy[1] : '.';
    json_t *staged = h_entry_staged(tag, xy);
    if (wg_raised(staged)) return staged;
    bool tracked = strcmp(tag, "1") == 0 || strcmp(tag, "2") == 0;
    bool unstaged = strcmp(tag, "?") == 0 || (tracked && y != '.' && y != '?');
    json_t *o = json_object();
    json_set_hdr(o, "path", path);
    json_set(o, "staged", staged);
    json_set(o, "unstaged", json_bool(unstaged));
    json_set(o, "untracked", json_bool(strcmp(tag, "?") == 0));
    json_set(o, "conflicted", json_bool(strcmp(tag, "u") == 0));
    return o;
}

static json_t *h_status_letter(const char *tag, const char *xy) {
    if (strcmp(tag, "?") == 0 || strcmp(tag, "u") == 0) {
        char up[2] = { (char)toupper((unsigned char)tag[0]), '\0' };
        return json_string(up);
    }
    if (!xy[0]) return raise_desc("IndexError", "string index out of range");
    char code = xy[0];
    if (code == '.') code = (xy[0] && xy[1]) ? xy[1] : '.';
    if (code == '.') code = 'M';
    char up[2] = { (char)toupper((unsigned char)code), '\0' };
    return json_string(up);
}

/* -- _slugify(name) ------------------------------------------------------ */
static json_t *h_slugify(const json_t *name) {
    char *s;
    if (!jtruth(name)) s = strdup("");
    else s = py_str(name);
    char *stripped = py_strip(s ? s : "");
    free(s);
    char *lower = py_lower(stripped ? stripped : "");
    free(stripped);
    size_t cap = strlen(lower) * 3 + 4, o = 0;
    char *slug = malloc(cap);
    const char *p = lower;
    bool in_run = false;
    while (*p) {
        const char *q = p;
        unsigned long cp = utf8_next(&q);
        bool alnum = (cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9');
        if (alnum) {
            size_t n = (size_t)(q - p);
            while (o + n + 2 > cap) { cap *= 2; slug = realloc(slug, cap); }
            memcpy(slug + o, p, n); o += n;
            in_run = false;
        } else if (!in_run) {
            if (o + 2 > cap) { cap *= 2; slug = realloc(slug, cap); }
            slug[o++] = '-';
            in_run = true;
        }
        p = q;
    }
    slug[o] = '\0';
    free(lower);
    char *start = slug;
    while (*start == '-') start++;
    size_t sl = strlen(start);
    while (sl > 0 && start[sl - 1] == '-') sl--;
    start[sl] = '\0';
    char *cut = py_slice_left(start, 40);
    size_t cl = strlen(cut);
    while (cl > 0 && cut[cl - 1] == '-') cl--;
    cut[cl] = '\0';
    json_t *r = json_string(cut[0] ? cut : "work");
    free(cut);
    free(slug);
    return r;
}

/* -- _has_staged(raw): any(_entry_staged ... for _walk_entries(raw)) ----- */
static json_t *h_has_staged(wg_io_t *io, const json_t *raw) {
    json_t *entries = seam_walk_entries(io, raw);
    if (wg_raised(entries)) return entries;
    json_t *r = json_bool(false);
    if (j_is_arr(entries)) {
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *trip = json_get(entries, i);
            const char *tag = jstr(json_get(trip, 0));
            const char *xy = jstr(json_get(trip, 1));
            json_t *es = h_entry_staged(tag, xy);
            if (wg_raised(es)) { json_free(entries); return es; }
            bool yes = es->type == JSON_BOOL && es->bool_val;
            json_free(es);
            if (yes) { json_free(r); r = json_bool(true); break; }
        }
    }
    json_free(entries);
    return r;
}

/* -- _review_result(cwd, files, base): stable sort + untracked fill ------ */
static json_t *h_review_result(wg_io_t *io, const json_t *cwd, json_t *files, const json_t *base) {
    size_t n = j_is_arr(files) ? json_len(files) : 0;
    for (size_t i = 1; i < n; i++) {
        json_t *key = files->c.items[i];
        const char *kp = json_get_str(key, "path", "");
        size_t j = i;
        while (j > 0 && strcmp(json_get_str(files->c.items[j - 1], "path", ""), kp) > 0) {
            files->c.items[j] = files->c.items[j - 1];
            j--;
        }
        files->c.items[j] = key;
    }
    for (size_t i = 0; i < n; i++) {
        json_t *f = files->c.items[i];
        const char *status = json_get_str(f, "status", "");
        json_t *a = json_obj_get(f, "added");
        json_t *r = json_obj_get(f, "removed");
        bool zero = j_is_num(a) && a->num_val == 0.0 && j_is_num(r) && r->num_val == 0.0;
        if (strcmp(status, "?") == 0 && zero) {
            json_t *ui = seam_untracked(io, cwd, json_get_str(f, "path", ""));
            if (wg_raised(ui)) { json_free(files); return ui; }
            json_set(f, "added", ui);
        }
    }
    json_t *out = json_object();
    json_set(out, "files", files);   /* adopts */
    json_set(out, "base", base ? json_copy(base) : json_null());
    return out;
}

/* -- _all_add_diff(cwd, file_path) --------------------------------------- */
static json_t *h_all_add_diff(wg_io_t *io, const json_t *cwd, const char *fp) {
    if (!j_is_str(cwd)) return wg_path_type_error(cwd);
    char *joined = wg_path_join(jstr(cwd), fp);
    json_t *a = json_object();
    json_set_hdr(a, "path", joined ? joined : "");
    free(joined);
    json_t *isf = wg_call(io, "path.is_file", a);
    json_free(a);
    if (!isf) isf = json_null();
    if (wg_raised(isf)) return isf;   /* no live try at this site */
    if (!jtruth(isf)) { json_free(isf); return json_string(""); }
    json_free(isf);
    json_t *ga = json_array();
    json_append(ga, json_string("diff"));
    json_append(ga, json_string("--no-index"));
    json_append(ga, json_string("--"));
#ifdef _WIN32
    json_append(ga, json_string("nul"));
#else
    json_append(ga, json_string("/dev/null"));
#endif
    json_append(ga, json_string(fp));
    json_t *t = seam_runner(io, cwd, ga, false);
    json_free(ga);
    if (wg_raised(t)) return t;
    json_t *out = json_get(t, 1);
    json_t *r = out ? json_copy(out) : json_null();
    json_free(t);
    return r;
}

static json_t *h_resolve_seam(wg_io_t *io, const char *path) {
    json_t *a = json_object();
    json_set_hdr(a, "path", path ? path : "");
    json_t *r = wg_call(io, "path.resolve", a);
    json_free(a);
    if (!r) r = json_null();
    if (wg_raised(r)) return r;
    if (!j_is_str(r)) {
        json_t *e = wg_path_type_error(r);
        json_free(r);
        return e;
    }
    return r;
}

/* -- _single_file_diff(cwd, args, file_path) ----------------------------- */
static json_t *h_single_file_diff(wg_io_t *io, const json_t *cwd, const json_t *args, const char *fp) {
    size_t litlen = strlen(fp) + 16;
    char *literal = malloc(litlen);
    if (literal) snprintf(literal, litlen, ":(literal)%s", fp);
    json_t *ga = json_array();
    json_append(ga, json_string("diff"));
    if (j_is_arr(args)) {
        for (size_t i = 0; i < json_len(args); i++) json_append(ga, json_copy(json_get(args, i)));
    }
    json_append(ga, json_string("--no-renames"));
    json_append(ga, json_string("--name-only"));
    json_append(ga, json_string("-z"));
    json_append(ga, json_string("--"));
    json_append(ga, json_string(literal ? literal : ""));
    json_t *names = seam_git_out(io, cwd, ga, false);
    json_free(ga);
    if (wg_raised(names)) { free(literal); return names; }
    if (!jtruth(names)) { json_free(names); free(literal); return json_string(""); }

    json_t *tlargs = json_array();
    json_append(tlargs, json_string("rev-parse"));
    json_append(tlargs, json_string("--show-toplevel"));
    json_t *tl = h_git_line(io, cwd, tlargs, false);
    json_free(tlargs);
    if (wg_raised(tl)) { json_free(names); free(literal); return tl; }
    char *root = wg_path_norm(jstr(tl));
    json_free(tl);

    char *tjoined = wg_path_join(jstr(cwd), fp);
    json_t *target = h_resolve_seam(io, tjoined);
    free(tjoined);
    if (wg_raised(target)) { json_free(names); free(root); free(literal); return target; }

    json_t *recs = wg_split_records(jstr(names));
    json_free(names);
    for (size_t i = 0; i < json_len(recs); i++) {
        const char *name = jstr(json_get(recs, i));
        if (!name[0]) continue;
        char *j = wg_path_join(root, name);
        json_t *res = h_resolve_seam(io, j);
        free(j);
        if (wg_raised(res)) {
            json_free(recs); json_free(target); free(root); free(literal);
            return res;
        }
        bool ne = !wg_path_eq(jstr(res), jstr(target));
        json_free(res);
        if (ne) {
            json_free(recs); json_free(target); free(root); free(literal);
            return raise_desc("RuntimeError", "Expected a single file, not a directory");
        }
    }
    json_free(recs);
    json_free(target);
    free(root);

    json_t *ga2 = json_array();
    json_append(ga2, json_string("diff"));
    if (j_is_arr(args)) {
        for (size_t i = 0; i < json_len(args); i++) json_append(ga2, json_copy(json_get(args, i)));
    }
    json_append(ga2, json_string("--no-renames"));
    json_append(ga2, json_string("--"));
    json_append(ga2, json_string(literal ? literal : ""));
    free(literal);
    return seam_git_out(io, cwd, ga2, false);
}/* str.replace(old, new, 1) -- FIRST occurrence only (live count=1 sites). */
static char *py_replace1(const char *s, const char *old, const char *nw) {
    const char *z = strstr(s, old);
    if (!z) return strdup(s);
    size_t pre = (size_t)(z - s), ol = strlen(old), nl = strlen(nw);
    size_t rest = strlen(z + ol);
    char *out = malloc(pre + nl + rest + 1);
    if (!out) return NULL;
    memcpy(out, s, pre);
    memcpy(out + pre, nw, nl);
    memcpy(out + pre + nl, z + ol, rest + 1);
    return out;
}

/* -- _review_push(cwd) --------------------------------------------------- */
static json_t *h_review_push(wg_io_t *io, const json_t *cwd) {
    json_t *args = json_array();
    json_append(args, json_string("rev-parse"));
    json_append(args, json_string("--abbrev-ref"));
    json_append(args, json_string("--symbolic-full-name"));
    json_append(args, json_string("@{u}"));
    json_t *up = h_git_line(io, cwd, args, true);
    json_free(args);
    if (wg_raised(up)) return up;
    if (jtruth(up)) {
        json_free(up);
        json_t *pa = json_array();
        json_append(pa, json_string("push"));
        json_t *r = seam_git_ok(io, cwd, pa);
        json_free(pa);
        return r;
    }
    json_free(up);
    json_t *args2 = json_array();
    json_append(args2, json_string("rev-parse"));
    json_append(args2, json_string("--abbrev-ref"));
    json_append(args2, json_string("HEAD"));
    json_t *branch = h_git_line(io, cwd, args2, true);
    json_free(args2);
    if (wg_raised(branch)) return branch;
    if (jtruth(branch) && strcmp(jstr(branch), "HEAD") != 0) {
        json_t *pa = json_array();
        json_append(pa, json_string("push"));
        json_append(pa, json_string("-u"));
        json_append(pa, json_string("origin"));
        json_append(pa, json_copy(branch));
        json_free(branch);
        json_t *r = seam_git_ok(io, cwd, pa);
        json_free(pa);
        return r;
    }
    json_free(branch);
    return json_null();
}

/* -- worktree_list(cwd, *, run=_git) ------------------------------------- */
static json_t *h_worktree_list(wg_io_t *io, const json_t *cwd, bool write) {
    json_t *args = json_array();
    json_append(args, json_string("worktree"));
    json_append(args, json_string("list"));
    json_append(args, json_string("--porcelain"));
    json_t *out = seam_git_out(io, cwd, args, write);
    json_free(args);
    if (wg_raised(out)) return out;
    if (!j_is_str(out)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'split'", py_type_name(out));
        json_free(out);
        return raise_desc("AttributeError", msg);
    }
    json_t *lines = py_split(jstr(out), 0);
    json_free(out);
    json_t *trees = json_array();
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *line = jstr(json_get(lines, i));
        if (str_starts(line, "worktree ")) {
            char *path = py_strip(line + 9);
            json_t *tree = json_object();
            json_set_hdr(tree, "path", path ? path : "");
            free(path);
            json_set(tree, "branch", json_null());
            json_set(tree, "isMain", json_bool(json_len(trees) == 0));
            json_set(tree, "detached", json_bool(false));
            json_set(tree, "locked", json_bool(false));
            json_append(trees, tree);
        } else if (json_len(trees) == 0) {
            continue;
        } else if (str_starts(line, "branch ")) {
            char *v = py_strip(line + 7);
            char *repl = py_replace1(v ? v : "", "refs/heads/", "");
            json_t *last = json_get(trees, json_len(trees) - 1);
            json_set_hdr(last, "branch", repl ? repl : "");
            free(repl); free(v);
        } else if (strcmp(line, "detached") == 0) {
            json_t *last = json_get(trees, json_len(trees) - 1);
            json_set(last, "detached", json_bool(true));
        } else if (str_starts(line, "locked")) {
            json_t *last = json_get(trees, json_len(trees) - 1);
            json_set(last, "locked", json_bool(true));
        }
    }
    json_free(lines);
    return trees;
}

static json_t *h_main_root(wg_io_t *io, const json_t *cwd) {
    json_t *trees = h_worktree_list(io, cwd, true);
    if (wg_raised(trees)) return trees;
    json_t *r;
    if (j_is_arr(trees) && json_len(trees) > 0) {
        json_t *path = json_obj_get(json_get(trees, 0), "path");
        r = path ? json_copy(path) : json_null();
    } else {
        r = cwd ? json_copy(cwd) : json_null();
    }
    json_free(trees);
    return r;
}

/* -- _ensure_repo(cwd) --------------------------------------------------- */
static json_t *h_ensure_repo(wg_io_t *io, const json_t *cwd) {
    json_t *args = json_array();
    json_append(args, json_string("rev-parse"));
    json_append(args, json_string("--is-inside-work-tree"));
    json_t *line = h_git_line(io, cwd, args, true);
    json_free(args);
    if (wg_raised(line)) return line;
    bool needs_root;
    if (strcmp(jstr(line), "true") != 0) {
        json_t *ia = json_array();
        json_append(ia, json_string("init"));
        json_t *ok = seam_git_ok(io, cwd, ia);
        json_free(ia);
        if (wg_raised(ok)) { json_free(line); return ok; }
        json_free(ok);
        needs_root = true;
    } else {
        json_t *ex = h_ref_exists(io, cwd, "HEAD", true);
        if (wg_raised(ex)) { json_free(line); return ex; }
        needs_root = !(ex->type == JSON_BOOL && ex->bool_val);
        json_free(ex);
    }
    json_free(line);
    if (needs_root) {
        json_t *ca = json_array();
        json_append(ca, json_string("-c"));
        json_append(ca, json_string("user.email=hermes@localhost"));
        json_append(ca, json_string("-c"));
        json_append(ca, json_string("user.name=Hermes"));
        json_append(ca, json_string("commit"));
        json_append(ca, json_string("--allow-empty"));
        json_append(ca, json_string("-m"));
        json_append(ca, json_string("Initial commit"));
        json_t *ok = seam_git_ok(io, cwd, ca);
        json_free(ca);
        return ok;
    }
    return json_null();
}

/* -- _default_branch(cwd, *, run=_git) ----------------------------------- */
static json_t *h_default_branch(wg_io_t *io, const json_t *cwd, bool write) {
    json_t *oh = h_origin_head(io, cwd, write);
    if (wg_raised(oh)) return oh;
    char *repl = py_replace1(jstr(oh), "origin/", "");
    json_free(oh);
    if (repl && repl[0]) {
        json_t *r = json_string(repl);
        free(repl);
        return r;
    }
    free(repl);
    json_t *cargs = json_array();
    json_append(cargs, json_string("config"));
    json_append(cargs, json_string("--get"));
    json_append(cargs, json_string("init.defaultBranch"));
    json_t *cfg = h_git_line(io, cwd, cargs, write);
    json_free(cargs);
    if (wg_raised(cfg)) return cfg;
    if (jtruth(cfg)) return cfg;
    json_free(cfg);
    const char *trunk[2] = { "main", "master" };
    for (int i = 0; i < 2; i++) {
        char ref[64];
        snprintf(ref, sizeof ref, "refs/heads/%s", trunk[i]);
        json_t *sa = json_array();
        json_append(sa, json_string("show-ref"));
        json_append(sa, json_string("--verify"));
        json_append(sa, json_string(ref));
        json_t *sr = h_git_line(io, cwd, sa, write);
        json_free(sa);
        if (wg_raised(sr)) return sr;
        if (jtruth(sr)) { json_free(sr); return json_string(trunk[i]); }
        json_free(sr);
    }
    return json_string("");
}

/* -- _remote_of_ref(cwd, name) ------------------------------------------- */
static json_t *h_remote_of_ref(wg_io_t *io, const json_t *cwd, const char *name) {
    if (!strchr(name, '/')) return json_string("");
    size_t need = strlen(name) + 32;
    char *ref = malloc(need);
    if (ref) snprintf(ref, need, "refs/remotes/%s", name);
    json_t *args = json_array();
    json_append(args, json_string("show-ref"));
    json_append(args, json_string("--verify"));
    json_append(args, json_string("--quiet"));
    json_append(args, json_string(ref ? ref : ""));
    free(ref);
    json_t *t = seam_runner(io, cwd, args, true);
    json_free(args);
    if (wg_raised(t)) return t;
    json_t *code = json_get(t, 0);
    bool ok = j_is_num(code) && code->num_val == 0.0;
    json_free(t);
    if (!ok) return json_string("");
    const char *slash = strchr(name, '/');
    size_t n = (size_t)(slash - name);
    char *out = malloc(n + 1);
    if (out) { memcpy(out, name, n); out[n] = '\0'; }
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

/* -- _fetch_tracking_ref(root, remote, branch) --------------------------- */
static json_t *h_fetch_tracking_ref(wg_io_t *io, const json_t *root, const char *remote, const char *branch) {
    json_t *ts = seam_tracking_refspec(io, remote, branch);
    if (wg_raised(ts)) return ts;
    json_t *fa = json_array();
    json_append(fa, json_string("fetch"));
    json_append(fa, json_string(remote));
    json_append(fa, ts);   /* adopts */
    json_t *t = seam_runner(io, root, fa, true);
    json_free(fa);
    if (wg_raised(t)) return t;
    json_t *code = json_get(t, 0);
    json_t *r = json_bool(j_is_num(code) && code->num_val == 0.0);
    json_free(t);
    return r;
}

/* -- _gh_json(cwd, args): parsed JSON stdout of a successful gh call ----- */
static json_t *h_gh_json(wg_io_t *io, const json_t *cwd, const json_t *args) {
    json_t *t = seam_gh(io, cwd, args);
    if (wg_raised(t)) return t;
    json_t *ok = json_get(t, 0);
    json_t *out = json_get(t, 1);
    if (!jtruth(ok)) { json_free(t); return json_null(); }
    if (!j_is_str(out)) {
        char msg[160];
        snprintf(msg, sizeof msg, "the JSON object must be str, bytes or bytearray, not %s", py_type_name(out));
        json_free(t);
        return raise_desc("TypeError", msg);
    }
    json_t *parsed = json_parse(jstr(out), NULL);
    json_free(t);
    if (!parsed) return json_null();   /* json.JSONDecodeError -> None */
    return parsed;
}

/* -- review_ship_info(cwd) ----------------------------------------------- */
static json_t *ship_not_ready(void) {
    json_t *o = json_object();
    json_set(o, "ghReady", json_bool(false));
    json_set(o, "pr", json_null());
    return o;
}
static json_t *h_review_ship_info(wg_io_t *io, const json_t *cwd) {
    json_t *isd = h_is_dir(io, cwd);
    if (wg_raised(isd)) return isd;
    if (!jtruth(isd)) { json_free(isd); return ship_not_ready(); }
    json_free(isd);
    json_t *aa = json_array();
    json_append(aa, json_string("auth"));
    json_append(aa, json_string("status"));
    json_t *gh = seam_gh(io, cwd, aa);
    json_free(aa);
    if (wg_raised(gh)) return gh;
    json_t *ok = json_get(gh, 0);
    if (!jtruth(ok)) { json_free(gh); return ship_not_ready(); }
    json_free(gh);
    json_t *pa = json_array();
    json_append(pa, json_string("pr"));
    json_append(pa, json_string("view"));
    json_append(pa, json_string("--json"));
    json_append(pa, json_string("url,state,number"));
    json_t *pr = h_gh_json(io, cwd, pa);
    json_free(pa);
    if (wg_raised(pr)) return pr;
    if (jtruth(pr)) {
        if (!j_is_obj(pr)) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", py_type_name(pr));
            json_free(pr);
            return raise_desc("AttributeError", msg);
        }
        json_t *url = json_obj_get(pr, "url");
        if (jtruth(url)) {
            json_t *inner = json_object();
            json_set(inner, "url", json_copy(url));
            json_t *st = json_obj_get(pr, "state");
            json_set(inner, "state", st ? json_copy(st) : json_null());
            json_t *nu = json_obj_get(pr, "number");
            json_set(inner, "number", nu ? json_copy(nu) : json_null());
            json_t *o = json_object();
            json_set(o, "ghReady", json_bool(true));
            json_set(o, "pr", inner);
            json_free(pr);
            return o;
        }
    }
    json_free(pr);
    json_t *o = json_object();
    json_set(o, "ghReady", json_bool(true));
    json_set(o, "pr", json_null());
    return o;
}

/* -- _pr_query(owner, name, branches, numbers) --------------------------- */
static json_t *h_pr_query(wg_io_t *io, const json_t *in) {
    (void)io;
    const char *owner = json_get_str(in, "owner", "");
    const char *name = json_get_str(in, "name", "");
    json_t *branches = json_obj_get(in, "branches");
    json_t *numbers = json_obj_get(in, "numbers");
    size_t nbr = j_is_arr(branches) ? json_len(branches) : 0;
    size_t nnum = j_is_arr(numbers) ? json_len(numbers) : 0;
    wg_sb q; sb_init(&q);
    sb_add(&q, "query { repository(owner: ");
    char *eo = json_escape_string(owner); sb_add(&q, eo ? eo : "\"\""); free(eo);
    sb_add(&q, ", name: ");
    char *en = json_escape_string(name); sb_add(&q, en ? en : "\"\""); free(en);
    sb_add(&q, ") {\n");
    size_t idx = 0;
    char ibuf[32];
    for (size_t i = 0; i < nbr; i++) {
        if (idx > 0) sb_add(&q, "\n");
        snprintf(ibuf, sizeof ibuf, "%zu", i);
        sb_add(&q, "b"); sb_add(&q, ibuf);
        sb_add(&q, ": pullRequests(headRefName: ");
        char *eb = json_serialize(json_get(branches, i)); sb_add(&q, eb ? eb : "null"); free(eb);
        sb_add(&q, ", first: 5, orderBy: {field: CREATED_AT, direction: DESC}) { nodes { ");
        sb_add(&q, WG_PR_NODE_FIELDS);
        sb_add(&q, " } }");
        idx++;
    }
    for (size_t i = 0; i < nnum; i++) {
        if (idx > 0) sb_add(&q, "\n");
        snprintf(ibuf, sizeof ibuf, "%zu", i);
        sb_add(&q, "n"); sb_add(&q, ibuf);
        sb_add(&q, ": pullRequest(number: ");
        json_t *nv = json_get(numbers, i);
        char nbuf[64];
        if (j_is_num(nv)) snprintf(nbuf, sizeof nbuf, "%.0f", nv->num_val);
        else snprintf(nbuf, sizeof nbuf, "%s", jstr(nv));
        sb_add(&q, nbuf);
        sb_add(&q, ") { "); sb_add(&q, WG_PR_NODE_FIELDS); sb_add(&q, " }");
        idx++;
    }
    sb_add(&q, "\n} }");
    json_t *r = json_string(q.s ? q.s : "");
    free(q.s);
    return r;
}

/* -- _pr_payload(pr) ----------------------------------------------------- */
static json_t *h_pr_payload(const json_t *pr) {
    if (!j_is_obj(pr)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", py_type_name(pr));
        return raise_desc("AttributeError", msg);
    }
    json_t *o = json_object();
    char *branch = py_str(json_obj_get(pr, "headRefName"));
    json_set_hdr(o, "branch", branch ? branch : "None"); free(branch);
    json_set(o, "draft", json_bool(jtruth(json_obj_get(pr, "isDraft"))));
    json_t *numv = json_obj_get(pr, "number");
    double num = 0;
    if (jtruth(numv)) {
        json_t *e = to_int(numv, &num);
        if (e) { json_free(o); return e; }
    }
    json_set(o, "number", json_number(num));
    json_t *stv = json_obj_get(pr, "state");
    char *sts = jtruth(stv) ? py_str(stv) : strdup("");
    char *stl = py_lower(sts ? sts : ""); free(sts);
    json_set_hdr(o, "state", stl ? stl : ""); free(stl);
    json_t *tiv = json_obj_get(pr, "title");
    char *tis = jtruth(tiv) ? py_str(tiv) : strdup("");
    json_set_hdr(o, "title", tis ? tis : ""); free(tis);
    json_t *urv = json_obj_get(pr, "url");
    char *urs = jtruth(urv) ? py_str(urv) : strdup("");
    json_set_hdr(o, "url", urs ? urs : ""); free(urs);
    return o;
}

/* -- _own_pr(key, field) ------------------------------------------------- */
static json_t *h_own_pr(const char *key, const json_t *field) {
    if (key[0] == 'n') return field ? json_copy(field) : json_null();
    if (!j_is_obj(field)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", py_type_name(field));
        return raise_desc("AttributeError", msg);
    }
    json_t *nodes = json_obj_get(field, "nodes");
    if (jtruth(nodes) && j_is_arr(nodes)) {
        for (size_t i = 0; i < json_len(nodes); i++) {
            json_t *n = json_get(nodes, i);
            if (!jtruth(n)) continue;
            if (!j_is_obj(n)) {
                char msg[128];
                snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", py_type_name(n));
                return raise_desc("AttributeError", msg);
            }
            json_t *cross = json_obj_get(n, "isCrossRepository");
            if (!jtruth(cross)) return json_copy(n);
        }
    }
    return json_null();
}

/* -- review_create_pr(cwd) ----------------------------------------------- */
static json_t *h_review_create_pr(wg_io_t *io, const json_t *cwd) {
    json_t *push = h_review_push(io, cwd);
    if (wg_raised(push)) {
        if (strcmp(exc_type(push), "RuntimeError") == 0) json_free(push);
        else return push;
    } else {
        json_free(push);
    }
    json_t *ga = json_array();
    json_append(ga, json_string("pr"));
    json_append(ga, json_string("create"));
    json_append(ga, json_string("--fill"));
    json_t *gh = seam_gh(io, cwd, ga);
    json_free(ga);
    if (wg_raised(gh)) return gh;
    json_t *created = json_get(gh, 0);
    json_t *out = json_get(gh, 1);
    json_t *err = json_get(gh, 2);
    if (!jtruth(created)) {
        if (!j_is_str(err)) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", py_type_name(err));
            json_free(gh);
            return raise_desc("AttributeError", msg);
        }
        char *es = py_strip(jstr(err));
        char *tail = py_slice_right(es ? es : "", WG_GH_ERR_TAIL_CHARS);
        free(es);
        const char *detail = (tail && tail[0]) ? tail : "is gh installed and authenticated?";
        size_t need = strlen(detail) + 32;
        char *msg = malloc(need);
        if (msg) snprintf(msg, need, "gh pr create failed: %s", detail);
        json_t *r = raise_desc("RuntimeError", msg ? msg : "");
        free(msg); free(tail); json_free(gh);
        return r;
    }
    if (!j_is_str(out)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", py_type_name(out));
        json_free(gh);
        return raise_desc("AttributeError", msg);
    }
    char *os = py_strip(jstr(out));
    json_t *lines = py_split(os ? os : "", 1);
    free(os);
    char *url = strdup("");
    for (size_t i = json_len(lines); i-- > 0;) {
        const char *line = jstr(json_get(lines, i));
        char *ls = py_strip(line);
        if (ls && ls[0]) { free(url); url = strdup(line); free(ls); break; }
        free(ls);
    }
    json_free(lines);
    json_free(gh);
    json_t *o = json_object();
    json_set_hdr(o, "url", url ? url : "");
    free(url);
    return o;
}

/* -- _ref_names(cwd, *patterns, fmt=...) --------------------------------- */
static json_t *h_ref_names(wg_io_t *io, const json_t *cwd, const json_t *patterns, const char *fmt) {
    size_t fl = strlen(fmt) + 16;
    char *format = malloc(fl);
    if (format) snprintf(format, fl, "--format=%s", fmt);
    json_t *args = json_array();
    json_append(args, json_string("for-each-ref"));
    json_append(args, json_string(format ? format : "--format="));
    free(format);
    json_append(args, json_string("--sort=-committerdate"));
    if (j_is_arr(patterns)) {
        for (size_t i = 0; i < json_len(patterns); i++) json_append(args, json_copy(json_get(patterns, i)));
    }
    json_t *out = seam_git_out(io, cwd, args, false);
    json_free(args);
    if (wg_raised(out)) return out;
    if (!j_is_str(out)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'split'", py_type_name(out));
        json_free(out);
        return raise_desc("AttributeError", msg);
    }
    json_t *lines = py_split(jstr(out), 0);
    json_free(out);
    json_t *arr = json_array();
    for (size_t i = 0; i < json_len(lines); i++) {
        char *s = py_strip(jstr(json_get(lines, i)));
        if (s && s[0]) json_append(arr, json_string(s));
        free(s);
    }
    json_free(lines);
    return arr;
}

/* -- branch_list(cwd) ---------------------------------------------------- */
static bool arr_has_str(const json_t *arr, const char *s) {
    for (size_t i = 0; i < json_len(arr); i++) {
        if (strcmp(jstr(json_get(arr, i)), s) == 0) return true;
    }
    return false;
}
static json_t *h_branch_list(wg_io_t *io, const json_t *cwd) {
    json_t *pats = json_array();
    json_append(pats, json_string("refs/heads"));
    json_t *locals = h_ref_names(io, cwd, pats, "%(refname:short)");
    json_free(pats);
    if (wg_raised(locals)) return locals;
    if (json_len(locals) == 0) { json_free(locals); return json_array(); }

    json_t *trees = h_worktree_list(io, cwd, false);
    if (wg_raised(trees)) { json_free(locals); return trees; }
    json_t *pbb = json_object();
    for (size_t i = 0; i < json_len(trees); i++) {
        json_t *t = json_get(trees, i);
        json_t *br = json_obj_get(t, "branch");
        if (jtruth(br)) {
            json_t *path = json_obj_get(t, "path");
            json_set(pbb, jstr(br), path ? json_copy(path) : json_null());
        }
    }
    json_free(trees);

    json_t *trunk = h_default_branch(io, cwd, false);
    if (wg_raised(trunk)) { json_free(locals); json_free(pbb); return trunk; }

    json_t *rpats = json_array();
    json_append(rpats, json_string("refs/remotes"));
    json_t *remotes_out = h_ref_names(io, cwd, rpats, "%(refname:short)");
    json_free(rpats);
    if (wg_raised(remotes_out)) { json_free(locals); json_free(pbb); json_free(trunk); return remotes_out; }
    json_t *remotes = json_array();
    for (size_t i = 0; i < json_len(remotes_out); i++) {
        const char *nm = jstr(json_get(remotes_out, i));
        if (str_ends(nm, "/HEAD")) continue;
        if (arr_has_str(locals, split1_last(nm))) continue;
        json_append(remotes, json_string(nm));
    }
    json_free(remotes_out);

    json_t *result = json_array();
    for (size_t i = 0; i < json_len(locals); i++) {
        const char *nm = jstr(json_get(locals, i));
        json_t *row = json_object();
        json_set_hdr(row, "name", nm);
        json_set(row, "checkedOut", json_bool(json_has(pbb, nm)));
        json_set(row, "isDefault", json_bool(jtruth(trunk) && strcmp(nm, jstr(trunk)) == 0));
        json_set(row, "isRemote", json_bool(false));
        json_t *wp = json_obj_get(pbb, nm);
        json_set(row, "worktreePath", wp ? json_copy(wp) : json_null());
        json_append(result, row);
    }
    for (size_t i = 0; i < json_len(remotes); i++) {
        const char *nm = jstr(json_get(remotes, i));
        json_t *row = json_object();
        json_set_hdr(row, "name", nm);
        json_set(row, "checkedOut", json_bool(false));
        json_set(row, "isDefault", json_bool(false));
        json_set(row, "isRemote", json_bool(true));
        json_set(row, "worktreePath", json_null());
        json_append(result, row);
    }
    json_free(locals); json_free(pbb); json_free(trunk); json_free(remotes);
    return result;
}

/* -- base_branch_list(cwd) ----------------------------------------------- */
static json_t *h_base_branch_list(wg_io_t *io, const json_t *cwd) {
    json_t *pats = json_array();
    json_append(pats, json_string("refs/heads"));
    json_append(pats, json_string("refs/remotes"));
    json_t *lines = h_ref_names(io, cwd, pats, "%(refname:short)\t%(committerdate:iso)");
    json_free(pats);
    if (wg_raised(lines)) return lines;
    if (json_len(lines) == 0) { json_free(lines); return json_array(); }

    json_t *oh = h_origin_head(io, cwd, false);
    if (wg_raised(oh)) { json_free(lines); return oh; }
    json_t *def;
    if (jtruth(oh)) {
        def = oh;   /* adopts */
    } else {
        json_free(oh);
        def = h_default_branch(io, cwd, false);
        if (wg_raised(def)) { json_free(lines); return def; }
    }
    json_t *result = json_array();
    for (size_t i = 0; i < json_len(lines); i++) {
        char *nm = split_tab0(jstr(json_get(lines, i)));
        json_t *row = json_object();
        json_set_hdr(row, "name", nm ? nm : "");
        json_set(row, "isRemote", json_bool(str_starts(nm ? nm : "", "origin/")));
        json_set(row, "isDefault", json_bool(jtruth(def) && strcmp(nm ? nm : "", jstr(def)) == 0));
        json_append(result, row);
        free(nm);
    }
    json_free(lines); json_free(def);
    return result;
}

/* -- file_diff_vs_head(cwd, file_path) ----------------------------------- */
static json_t *h_file_diff_vs_head(wg_io_t *io, const json_t *cwd, const char *fp) {
    json_t *isd = h_is_dir(io, cwd);
    if (wg_raised(isd)) return isd;
    if (!jtruth(isd)) { json_free(isd); return json_string(""); }
    json_free(isd);
    size_t litlen = strlen(fp) + 16;
    char *literal = malloc(litlen);
    if (literal) snprintf(literal, litlen, ":(literal)%s", fp);
    json_t *ha = json_array();
    json_append(ha, json_string("HEAD"));
    json_t *head = h_single_file_diff(io, cwd, ha, fp);
    json_free(ha);
    if (wg_raised(head)) { free(literal); return head; }
    if (!j_is_str(head)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", py_type_name(head));
        json_free(head); free(literal);
        return raise_desc("AttributeError", msg);
    }
    char *hs = py_strip(jstr(head));
    if (hs && hs[0]) { free(hs); free(literal); return head; }
    free(hs);
    json_t *sa = json_array();
    json_append(sa, json_string("status"));
    json_append(sa, json_string("--porcelain"));
    json_append(sa, json_string("--"));
    json_append(sa, json_string(literal ? literal : ""));
    free(literal);
    json_t *status = seam_git_out(io, cwd, sa, false);
    json_free(sa);
    if (wg_raised(status)) { json_free(head); return status; }
    if (!j_is_str(status)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", py_type_name(status));
        json_free(head); json_free(status);
        return raise_desc("AttributeError", msg);
    }
    char *ss = py_strip(jstr(status));
    bool untracked = str_starts(ss ? ss : "", "??");
    free(ss);
    json_free(status);
    json_free(head);
    if (untracked) return h_all_add_diff(io, cwd, fp);
    return json_string("");
}/* -- PoP-claimed targets (38 unclaimed hermes_cli/web_git.py functions) --- */
static bool run_write(const json_t *in) {
    return strcmp(json_get_str(in, "run", "read"), "write") == 0;
}
static json_t *ok_true(void) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    return o;
}

/* PoP: wg__git_line @ hermes_cli/web_git.py:_git_line */
json_t *wg__git_line(wg_io_t *io, json_t *in) {
    return h_git_line(io, json_obj_get(in, "cwd"), json_obj_get(in, "args"), run_write(in));
}

/* PoP: wg__is_dir @ hermes_cli/web_git.py:_is_dir */
json_t *wg__is_dir(wg_io_t *io, json_t *in) {
    return h_is_dir(io, json_obj_get(in, "cwd"));
}

/* PoP: wg__status_z @ hermes_cli/web_git.py:_status_z */
json_t *wg__status_z(wg_io_t *io, json_t *in) {
    return h_status_z(io, json_obj_get(in, "cwd"), json_obj_get(in, "extra"), run_write(in));
}

/* PoP: wg__origin_head @ hermes_cli/web_git.py:_origin_head */
json_t *wg__origin_head(wg_io_t *io, json_t *in) {
    return h_origin_head(io, json_obj_get(in, "cwd"), run_write(in));
}

/* PoP: wg__origin_head_abbrev @ hermes_cli/web_git.py:_origin_head_abbrev */
json_t *wg__origin_head_abbrev(wg_io_t *io, json_t *in) {
    return h_origin_head_abbrev(io, json_obj_get(in, "cwd"));
}

/* PoP: wg__ref_exists @ hermes_cli/web_git.py:_ref_exists */
json_t *wg__ref_exists(wg_io_t *io, json_t *in) {
    return h_ref_exists(io, json_obj_get(in, "cwd"), json_get_str(in, "ref", ""), run_write(in));
}

/* PoP: wg__entry_staged @ hermes_cli/web_git.py:_entry_staged */
json_t *wg__entry_staged(wg_io_t *io, json_t *in) {
    (void)io;
    return h_entry_staged(json_get_str(in, "tag", ""), json_get_str(in, "xy", ""));
}

/* PoP: wg__classify @ hermes_cli/web_git.py:_classify */
json_t *wg__classify(wg_io_t *io, json_t *in) {
    (void)io;
    return h_classify(json_get_str(in, "tag", ""), json_get_str(in, "xy", ""), json_get_str(in, "path", ""));
}

/* PoP: wg__status_letter @ hermes_cli/web_git.py:_status_letter */
json_t *wg__status_letter(wg_io_t *io, json_t *in) {
    (void)io;
    return h_status_letter(json_get_str(in, "tag", ""), json_get_str(in, "xy", ""));
}

/* PoP: wg__review_result @ hermes_cli/web_git.py:_review_result */
json_t *wg__review_result(wg_io_t *io, json_t *in) {
    json_t *files = json_obj_get(in, "files");
    return h_review_result(io, json_obj_get(in, "cwd"), files ? json_copy(files) : json_array(),
                           json_obj_get(in, "base"));
}

/* PoP: wg__all_add_diff @ hermes_cli/web_git.py:_all_add_diff */
json_t *wg__all_add_diff(wg_io_t *io, json_t *in) {
    return h_all_add_diff(io, json_obj_get(in, "cwd"), json_get_str(in, "file_path", ""));
}

/* PoP: wg__single_file_diff @ hermes_cli/web_git.py:_single_file_diff */
json_t *wg__single_file_diff(wg_io_t *io, json_t *in) {
    return h_single_file_diff(io, json_obj_get(in, "cwd"), json_obj_get(in, "args"),
                              json_get_str(in, "file_path", ""));
}

/* PoP: wg_file_diff_vs_head @ hermes_cli/web_git.py:file_diff_vs_head */
json_t *wg_file_diff_vs_head(wg_io_t *io, json_t *in) {
    return h_file_diff_vs_head(io, json_obj_get(in, "cwd"), json_get_str(in, "file_path", ""));
}

/* PoP: wg_review_stage @ hermes_cli/web_git.py:review_stage */
json_t *wg_review_stage(wg_io_t *io, json_t *in) {
    json_t *cwd = json_obj_get(in, "cwd");
    json_t *fp = json_obj_get(in, "file_path");
    json_t *args = json_array();
    json_append(args, json_string("add"));
    if (jtruth(fp)) { json_append(args, json_string("--")); json_append(args, json_copy(fp)); }
    else json_append(args, json_string("-A"));
    json_t *ok = seam_git_ok(io, cwd, args);
    json_free(args);
    if (wg_raised(ok)) return ok;
    json_free(ok);
    return ok_true();
}

/* PoP: wg_review_unstage @ hermes_cli/web_git.py:review_unstage */
json_t *wg_review_unstage(wg_io_t *io, json_t *in) {
    json_t *cwd = json_obj_get(in, "cwd");
    json_t *fp = json_obj_get(in, "file_path");
    json_t *args = json_array();
    json_append(args, json_string("reset"));
    json_append(args, json_string("-q"));
    json_append(args, json_string("HEAD"));
    if (jtruth(fp)) { json_append(args, json_string("--")); json_append(args, json_copy(fp)); }
    json_t *ok = seam_git_ok(io, cwd, args);
    json_free(args);
    if (wg_raised(ok)) return ok;
    json_free(ok);
    return ok_true();
}

/* PoP: wg_review_revert @ hermes_cli/web_git.py:review_revert */
json_t *wg_review_revert(wg_io_t *io, json_t *in) {
    json_t *cwd = json_obj_get(in, "cwd");
    json_t *fp = json_obj_get(in, "file_path");
    const char *t = jtruth(fp) ? jstr(fp) : ".";
    json_t *a1 = json_array();
    json_append(a1, json_string("checkout"));
    json_append(a1, json_string("HEAD"));
    json_append(a1, json_string("--"));
    json_append(a1, json_string(t));
    json_t *r1 = seam_runner(io, cwd, a1, true);
    json_free(a1);
    if (wg_raised(r1)) return r1;
    json_free(r1);
    json_t *a2 = json_array();
    json_append(a2, json_string("clean"));
    json_append(a2, json_string("-fd"));
    json_append(a2, json_string("--"));
    json_append(a2, json_string(t));
    json_t *r2 = seam_runner(io, cwd, a2, true);
    json_free(a2);
    if (wg_raised(r2)) return r2;
    json_free(r2);
    return ok_true();
}

/* PoP: wg_review_rev_parse @ hermes_cli/web_git.py:review_rev_parse */
json_t *wg_review_rev_parse(wg_io_t *io, json_t *in) {
    json_t *ref = json_obj_get(in, "ref");
    const char *r = jtruth(ref) ? jstr(ref) : "HEAD";
    json_t *args = json_array();
    json_append(args, json_string("rev-parse"));
    json_append(args, json_string(r));
    json_t *line = h_git_line(io, json_obj_get(in, "cwd"), args, false);
    json_free(args);
    if (wg_raised(line)) return line;
    if (jtruth(line)) return line;
    json_free(line);
    return json_null();
}

/* PoP: wg__has_staged @ hermes_cli/web_git.py:_has_staged */
json_t *wg__has_staged(wg_io_t *io, json_t *in) {
    return h_has_staged(io, json_obj_get(in, "raw"));
}

/* PoP: wg__review_push @ hermes_cli/web_git.py:_review_push */
json_t *wg__review_push(wg_io_t *io, json_t *in) {
    return h_review_push(io, json_obj_get(in, "cwd"));
}

/* PoP: wg_review_push @ hermes_cli/web_git.py:review_push */
json_t *wg_review_push(wg_io_t *io, json_t *in) {
    json_t *r = h_review_push(io, json_obj_get(in, "cwd"));
    if (wg_raised(r)) return r;
    json_free(r);
    return ok_true();
}

/* PoP: wg__gh_json @ hermes_cli/web_git.py:_gh_json */
json_t *wg__gh_json(wg_io_t *io, json_t *in) {
    return h_gh_json(io, json_obj_get(in, "cwd"), json_obj_get(in, "args"));
}

/* PoP: wg_review_ship_info @ hermes_cli/web_git.py:review_ship_info */
json_t *wg_review_ship_info(wg_io_t *io, json_t *in) {
    return h_review_ship_info(io, json_obj_get(in, "cwd"));
}

/* PoP: wg__pr_query @ hermes_cli/web_git.py:_pr_query */
json_t *wg__pr_query(wg_io_t *io, json_t *in) {
    return h_pr_query(io, in);
}

/* PoP: wg__pr_payload @ hermes_cli/web_git.py:_pr_payload */
json_t *wg__pr_payload(wg_io_t *io, json_t *in) {
    (void)io;
    return h_pr_payload(json_obj_get(in, "pr"));
}

/* PoP: wg__own_pr @ hermes_cli/web_git.py:_own_pr */
json_t *wg__own_pr(wg_io_t *io, json_t *in) {
    (void)io;
    return h_own_pr(json_get_str(in, "key", ""), json_obj_get(in, "field"));
}

/* PoP: wg_review_create_pr @ hermes_cli/web_git.py:review_create_pr */
json_t *wg_review_create_pr(wg_io_t *io, json_t *in) {
    return h_review_create_pr(io, json_obj_get(in, "cwd"));
}

/* PoP: wg_worktree_list @ hermes_cli/web_git.py:worktree_list */
json_t *wg_worktree_list(wg_io_t *io, json_t *in) {
    return h_worktree_list(io, json_obj_get(in, "cwd"), run_write(in));
}

/* PoP: wg__main_root @ hermes_cli/web_git.py:_main_root */
json_t *wg__main_root(wg_io_t *io, json_t *in) {
    return h_main_root(io, json_obj_get(in, "cwd"));
}

/* PoP: wg__fetch_tracking_ref @ hermes_cli/web_git.py:_fetch_tracking_ref */
json_t *wg__fetch_tracking_ref(wg_io_t *io, json_t *in) {
    return h_fetch_tracking_ref(io, json_obj_get(in, "root"),
                                json_get_str(in, "remote", ""), json_get_str(in, "branch", ""));
}

/* PoP: wg__slugify @ hermes_cli/web_git.py:_slugify */
json_t *wg__slugify(wg_io_t *io, json_t *in) {
    (void)io;
    return h_slugify(json_obj_get(in, "name"));
}

/* PoP: wg__default_branch @ hermes_cli/web_git.py:_default_branch */
json_t *wg__default_branch(wg_io_t *io, json_t *in) {
    return h_default_branch(io, json_obj_get(in, "cwd"), run_write(in));
}

/* PoP: wg__ensure_repo @ hermes_cli/web_git.py:_ensure_repo */
json_t *wg__ensure_repo(wg_io_t *io, json_t *in) {
    return h_ensure_repo(io, json_obj_get(in, "cwd"));
}

/* PoP: wg__remote_of_ref @ hermes_cli/web_git.py:_remote_of_ref */
json_t *wg__remote_of_ref(wg_io_t *io, json_t *in) {
    return h_remote_of_ref(io, json_obj_get(in, "cwd"), json_get_str(in, "name", ""));
}

/* PoP: wg_worktree_remove @ hermes_cli/web_git.py:worktree_remove */
json_t *wg_worktree_remove(wg_io_t *io, json_t *in) {
    json_t *root = h_main_root(io, json_obj_get(in, "cwd"));
    if (wg_raised(root)) return root;
    json_t *wp = json_obj_get(in, "worktree_path");
    bool force = json_get_bool(in, "force", false);
    json_t *args = json_array();
    json_append(args, json_string("worktree"));
    json_append(args, json_string("remove"));
    if (force) json_append(args, json_string("--force"));
    json_append(args, wp ? json_copy(wp) : json_null());
    json_t *ok = seam_git_ok(io, root, args);
    json_free(args);
    json_free(root);
    if (wg_raised(ok)) return ok;
    json_free(ok);
    json_t *o = json_object();
    json_set(o, "removed", wp ? json_copy(wp) : json_null());
    return o;
}

/* PoP: wg__ref_names @ hermes_cli/web_git.py:_ref_names */
json_t *wg__ref_names(wg_io_t *io, json_t *in) {
    return h_ref_names(io, json_obj_get(in, "cwd"), json_obj_get(in, "patterns"),
                       json_get_str(in, "fmt", "%(refname:short)"));
}

/* PoP: wg_branch_list @ hermes_cli/web_git.py:branch_list */
json_t *wg_branch_list(wg_io_t *io, json_t *in) {
    return h_branch_list(io, json_obj_get(in, "cwd"));
}

/* PoP: wg_branch_switch @ hermes_cli/web_git.py:branch_switch */
json_t *wg_branch_switch(wg_io_t *io, json_t *in) {
    json_t *cwd = json_obj_get(in, "cwd");
    json_t *req = seam_sanitize(io, json_obj_get(in, "branch"));
    if (wg_raised(req)) return req;
    if (!jtruth(req)) {
        json_free(req);
        return raise_desc("RuntimeError", "Branch name is required.");
    }
    json_t *args = json_array();
    json_append(args, json_string("switch"));
    json_append(args, json_copy(req));
    json_t *ok = seam_git_ok(io, cwd, args);
    json_free(args);
    if (wg_raised(ok)) { json_free(req); return ok; }
    json_free(ok);
    json_t *o = json_object();
    json_set(o, "branch", req);   /* adopts */
    return o;
}

/* PoP: wg_base_branch_list @ hermes_cli/web_git.py:base_branch_list */
json_t *wg_base_branch_list(wg_io_t *io, json_t *in) {
    return h_base_branch_list(io, json_obj_get(in, "cwd"));
}