/* port_web_git_flow.c — FLOW-core port of hermes_cli/web_git.py.
 * See include/port_web_git_flow.h for the seam table and the representation
 * contract. libjson-only; no real I/O inside the decision core.
 */
#include "port_web_git_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <inttypes.h>

#define WGF_GIT_TIMEOUT 30
#define WGF_GH_ERR_TAIL_CHARS 400
#define WGF_UNTRACKED_SCAN_CAP 500
#define WGF_COMMIT_CONTEXT_DIFF_MAX_CHARS 120000
#define WGF_COMMIT_CONTEXT_UNTRACKED_MAX 80
#define WGF_FILTER_DISCOVERY_FAILED "git filter discovery failed"

/* ── seam dispatch ─────────────────────────────────────────────────────── */
json_t *wgf_call(wgf_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_append(io->trace, ev);
    if (!io->call) return json_null();
    return io->call(io, name, args);
}

bool wgf_raised(const json_t *result) {
    return result && result->type == JSON_OBJECT && json_obj_get(result, "__raise__") != NULL;
}

static json_t *raise_desc(const char *type, const char *message) {
    json_t *o = json_object();
    json_set_hdr(o, "__raise__", type);
    json_set_hdr(o, "message", message);
    return o;
}

/* ── Python truthiness / type tests ────────────────────────────────────── */
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

/* Python type name for a JSON node (for TypeError/AttributeError messages). */
static const char *py_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";   /* integral doubles read as int */
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* ── exception classification (live except clauses) ────────────────────── */
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
static bool exc_is_subprocesserror(const json_t *r) {
    const char *t = exc_type(r);
    return strcmp(t, "SubprocessError") == 0 || strcmp(t, "TimeoutExpired") == 0
        || strcmp(t, "CalledProcessError") == 0;
}

/* ── UTF-8 helpers ─────────────────────────────────────────────────────── */
/* Decode one codepoint; advance *p. Invalid bytes advance one byte and read
 * as U+FFFD (fixtures carry valid UTF-8; documented). */
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

/* CPython Py_UNICODE_ISSPACE set used by str.strip() (includes 0x1C-0x1F). */
static bool py_isspace(unsigned long cp) {
    if (cp <= 0x20) return (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F) || cp == 0x20;
    return cp == 0x85 || cp == 0xA0 || cp == 0x1680
        || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029
        || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

/* str.strip() — both ends, full Unicode whitespace set. Returns malloc'd. */
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
        /* walk back one codepoint */
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

/* str.splitlines() boundary codepoints (\r\n handled as one). */
static bool py_linebreak(unsigned long cp) {
    return cp == '\n' || cp == '\r' || cp == 0x0B || cp == 0x0C
        || cp == 0x1C || cp == 0x1D || cp == 0x1E || cp == 0x85
        || cp == 0x2028 || cp == 0x2029;
}

/* Split into a JSON array of strings. mode: 0 = split('\n') (keeps trailing
 * empties), 1 = splitlines() (no trailing empty). NUL payloads use
 * wgf_split_nul below (embedded NULs need an explicit length). */
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
        if (start < p) {
            json_append(arr, json_string(start));
        }
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

/* Split a raw byte buffer (may contain embedded NULs) on '\0'. */
static json_t *wgf_split_nul(const char *s, size_t len) {
    json_t *arr = json_array();
    size_t start = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\0') {
            char *seg = malloc(i - start + 1);
            if (seg) { memcpy(seg, s + start, i - start); seg[i - start] = '\0'; json_append(arr, json_string(seg)); free(seg); }
            start = i + 1;
        }
    }
    char *seg = malloc(len - start + 1);
    if (seg) { memcpy(seg, s + start, len - start); seg[len - start] = '\0'; json_append(arr, json_string(seg)); free(seg); }
    return arr;
}

/* Codepoint length of a UTF-8 string. */
static size_t py_len(const char *s) {
    size_t n = 0;
    const char *p = s ? s : "";
    while (*p) { utf8_next(&p); n++; }
    return n;
}

/* Codepoint slice s[0:count] (count >= 0). Returns malloc'd. */
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

/* ASCII lower (fixture domain; documented). Returns malloc'd. */
static char *py_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    for (char *p = out; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return out;
}

/* str.replace(old, new) — single left-to-right non-overlapping pass. */
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

/* Python repr of an ASCII-domain string (for int() ValueError messages). */
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

/* str() of a JSON scalar (str/int/float/bool/None domain; documented). */
static char *py_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("");
    if (v->type == JSON_STRING) return strdup(jstr(v));
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        char buf[64];
        if (d == floor(d) && fabs(d) < 1e16) {
            snprintf(buf, sizeof buf, "%.0f", d);
        } else {
            /* shortest round-trip repr */
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

/* int(str) — ASCII digits, sign, Unicode-whitespace trim, between-digit
 * underscores. Returns true on success; on false *err carries the exact
 * CPython ValueError message (caller frees). Domain: |value| <= 2^53. */
static bool py_int_parse(const char *s, double *out, char **err) {
    char *trimmed = py_strip(s);
    const char *p = trimmed;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    const char *digits_start = p;
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
    (void)digits_start;
    *out = neg ? -acc : acc;
    free(trimmed);
    return true;
}


/* ── NUL-record convention ─────────────────────────────────────────────────
 * libjson strings are NUL-terminated, so porcelain -z payloads cannot carry
 * literal NUL bytes through the JSON tree. Fixtures encode NUL as U+0001;
 * the C core splits records on \x01, and the oracle decodes \x01 -> \x00 on
 * inputs / encodes \x00 -> \x01 on outputs (documented representation
 * contract; fixtures never carry a literal \x01). */
#define WGF_NUL_SENTINEL '\x01'

static json_t *wgf_split_records(const char *s) {
    json_t *arr = json_array();
    if (!s) s = "";
    const char *p = s;
    for (;;) {
        const char *z = strchr(p, WGF_NUL_SENTINEL);
        if (!z) { json_append(arr, json_string(p)); break; }
        size_t n = (size_t)(z - p);
        char *seg = malloc(n + 1);
        if (seg) { memcpy(seg, p, n); seg[n] = '\0'; json_append(arr, json_string(seg)); free(seg); }
        p = z + 1;
    }
    return arr;
}

/* ── small string helpers ──────────────────────────────────────────────── */
static bool str_starts(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool str_contains(const char *s, const char *needle) {
    return strstr(s, needle) != NULL;
}
/* str.partition(sep) -> (before, sep_found, after); returns malloc'd parts. */
static void str_partition(const char *s, const char *sep, char **before, char **after, bool *found) {
    const char *z = strstr(s, sep);
    if (!z) {
        *before = strdup(s); *after = strdup(""); *found = false;
        return;
    }
    size_t n = (size_t)(z - s);
    *before = malloc(n + 1);
    if (*before) { memcpy(*before, s, n); (*before)[n] = '\0'; }
    *after = strdup(z + strlen(sep));
    *found = true;
}
/* " ".join(array of strings) */
static char *str_join_sp(const json_t *arr) {
    size_t cap = 64, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    size_t n = j_is_arr(arr) ? json_len(arr) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *s = jstr(json_get(arr, i));
        size_t sl = strlen(s);
        while (len + sl + 2 > cap) { cap *= 2; out = realloc(out, cap); if (!out) return NULL; }
        if (i) out[len++] = ' ';
        memcpy(out + len, s, sl); len += sl; out[len] = '\0';
    }
    return out;
}

/* ── host Path model (WindowsPath under _WIN32, PurePosixPath else) ────────
 * Domain: POSIX-absolute ("/r"), drive-absolute ("C:/r"), relative, empty.
 * Normalization collapses ""/"." components, keeps ".."; as_posix output. */
typedef struct {
    char drive[8];      /* "C:" or "" (UNC out of domain) */
    bool has_root;
    char root_sep;      /* original root separator for os.path.join fidelity */
    char **parts;
    size_t nparts;
} wgf_path_t;

static void path_free(wgf_path_t *p) {
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

static int wgf_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* splitroot for the letter-drive domain (UNC excluded). */
static void path_splitroot(const char *s, wgf_path_t *out) {
    memset(out, 0, sizeof *out);
    const char *rest = s;
#ifdef _WIN32
    if (s[0] && s[1] == ':' && ((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z'))) {
        out->drive[0] = s[0]; out->drive[1] = ':'; out->drive[2] = '\0';
        rest = s + 2;
    }
#endif
    if (is_sep(rest[0])) {
        out->has_root = true;
        out->root_sep = rest[0];
        rest++;
    }
    /* split components on both separators, drop ""/"." */
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

/* as_posix string of a parsed path (malloc'd). */
static char *path_as_posix(const wgf_path_t *p) {
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

/* Path(s).as_posix() — normalize then render. */
static char *wgf_path_norm(const char *s) {
    wgf_path_t p;
    path_splitroot(s ? s : "", &p);
    char *out = path_as_posix(&p);
    path_free(&p);
    return out;
}

/* (Path(a) / b).as_posix() — pathlib join replace rules. */
static char *wgf_path_join(const char *a, const char *b) {
    wgf_path_t pa, pb;
    path_splitroot(a ? a : "", &pa);
    path_splitroot(b ? b : "", &pb);
    wgf_path_t r;
    memset(&r, 0, sizeof r);
    if (pb.has_root) {
        /* absolute b replaces; keep a's drive when b has none (Windows) */
        snprintf(r.drive, sizeof r.drive, "%s", pb.drive[0] ? pb.drive : pa.drive);
        r.has_root = true;
        r.parts = pb.parts; r.nparts = pb.nparts;
        pb.parts = NULL; pb.nparts = 0;
    } else if (pb.drive[0]) {
#ifdef _WIN32
        bool same = pa.drive[0] && (wgf_strcasecmp(pa.drive, pb.drive) == 0);
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

/* os.path.join(root, ".worktrees", slug) — ntpath.join under _WIN32
 * (splitroot transcription; mixed-separator output is faithful),
 * posixpath.join otherwise. General 3-arg form for the worktree sites. */
static char *wgf_os_path_join3(const char *a, const char *b, const char *c) {
#ifdef _WIN32
    wgf_path_t ra, rb, rc;
    path_splitroot(a ? a : "", &ra);
    path_splitroot(b ? b : "", &rb);
    path_splitroot(c ? c : "", &rc);
    /* result accumulates drive/root/path; path keeps ORIGINAL separators */
    char drive[8]; snprintf(drive, sizeof drive, "%s", ra.drive);
    bool root = ra.has_root;
    char rootsep = ra.root_sep ? ra.root_sep : '\\';
    /* tail = original remainder after drive+root (separators preserved) */
    const char *atail = a ? a : "";
    if (ra.drive[0]) atail += 2;
    if (ra.has_root) atail += 1;
    char *path = strdup(atail);
    const char *comps[2] = { b ? b : "", c ? c : "" };
    wgf_path_t *pcs[2] = { &rb, &rc };
    for (int k = 0; k < 2; k++) {
        wgf_path_t *pc = pcs[k];
        const char *p = comps[k];
        if (pc->has_root) {
            if (pc->drive[0] || !drive[0]) snprintf(drive, sizeof drive, "%s", pc->drive);
            root = true; rootsep = pc->root_sep;
            free(path);
            const char *tail = p;
            if (pc->drive[0]) tail += 2;
            if (pc->has_root) tail += 1;
            path = strdup(tail);
            continue;
        } else if (pc->drive[0] && strcmp(pc->drive, drive) != 0) {
            char dl1[8], dl2[8];
            snprintf(dl1, sizeof dl1, "%s", pc->drive);
            snprintf(dl2, sizeof dl2, "%s", drive);
            for (char *q = dl1; *q; q++) *q = (char)tolower((unsigned char)*q);
            for (char *q = dl2; *q; q++) *q = (char)tolower((unsigned char)*q);
            if (strcmp(dl1, dl2) != 0) {
                snprintf(drive, sizeof drive, "%s", pc->drive);
                root = pc->has_root; rootsep = pc->root_sep;
                free(path);
                const char *tail = p;
                if (pc->drive[0]) tail += 2;
                path = strdup(tail);
                continue;
            }
            snprintf(drive, sizeof drive, "%s", pc->drive);
        }
        size_t pl = strlen(path);
        if (pl && !is_sep(path[pl - 1])) {
            path = realloc(path, pl + 2);
            path[pl] = '\\'; path[pl + 1] = '\0';
        }
        const char *tail = p;
        if (pc->drive[0]) tail += 2;
        size_t tl = strlen(tail);
        size_t cur = strlen(path);
        path = realloc(path, cur + tl + 1);
        memcpy(path + cur, tail, tl + 1);
    }
    size_t dl = strlen(drive), pl = strlen(path);
    char *out = malloc(dl + pl + 3);
    if (pl && !root && drive[0] && drive[dl - 1] != ':' && !is_sep(drive[dl - 1])) {
        snprintf(out, dl + pl + 3, "%s\\%s", drive, path);
    } else {
        snprintf(out, dl + pl + 3, "%s%s%s", drive, root ? (rootsep == '/' ? "/" : "\\") : "", path);
    }
    free(path);
    path_free(&ra); path_free(&rb); path_free(&rc);
    return out;
#else
    const char *comps[3] = { a ? a : "", b ? b : "", c ? c : "" };
    size_t cap = 64, len = 0;
    char *out = malloc(cap);
    out[0] = '\0';
    for (int k = 0; k < 3; k++) {
        const char *p = comps[k];
        if (p[0] == '/') { len = 0; out[0] = '\0'; }
        else if (len > 0 && out[len - 1] != '/') {
            if (len + 2 > cap) { cap *= 2; out = realloc(out, cap); }
            out[len++] = '/';
        }
        size_t pl = strlen(p);
        while (len + pl + 1 > cap) { cap *= 2; out = realloc(out, cap); }
        memcpy(out + len, p, pl); len += pl; out[len] = '\0';
    }
    return out;
#endif
}

/* Path equality compare for resolve() results: WindowsPath folds case. */
static bool wgf_path_eq(const char *a, const char *b) {
#ifdef _WIN32
    return wgf_strcasecmp(a, b) == 0;
#else
    return strcmp(a, b) == 0;
#endif
}

/* Path(cwd) construction: non-str raises TypeError BEFORE the body. */
static json_t *wgf_path_type_error(const json_t *cwd) {
    char msg[256];
    snprintf(msg, sizeof msg,
             "argument should be a str or an os.PathLike object where __fspath__ returns a str, not '%s'",
             py_type_name(cwd));
    return raise_desc("TypeError", msg);
}


/* ── the non-interactive git runner chain ──────────────────────────────── */
/* _run(argv, cwd, timeout, env): subprocess seam; (OSError, SubprocessError)
 * -> None; every other exception (incl. BaseException kinds) propagates. */
static json_t *h_run(wgf_io_t *io, const json_t *argv, const json_t *cwd,
                     double timeout, const json_t *env) {
    json_t *args = json_object();
    json_set(args, "argv", argv ? json_copy(argv) : json_null());
    json_set(args, "cwd", cwd ? json_copy(cwd) : json_null());
    json_set(args, "timeout", json_number(timeout));
    json_set(args, "env", env ? json_copy(env) : json_null());
    json_t *r = wgf_call(io, "subprocess.run", args);
    json_free(args);
    if (!r) r = json_null();
    if (wgf_raised(r)) {
        if (exc_is_oserror(r) || exc_is_subprocesserror(r)) {
            json_free(r);
            return json_null();
        }
        return r;
    }
    return r;
}

/* _git_in: ["git", *harden_git_argv(args)] then _run; None -> (1,"",...). */
static json_t *h_git_in(wgf_io_t *io, const json_t *cwd, const json_t *args,
                        double timeout, const json_t *env) {
    json_t *ha = json_object();
    json_set(ha, "args", args ? json_copy(args) : json_null());
    json_t *hr = wgf_call(io, "harden_git_argv", ha);
    json_free(ha);
    if (wgf_raised(hr)) return hr;
    if (!j_is_arr(hr)) {
        char msg[128];
        snprintf(msg, sizeof msg, "Value after * must be an iterable, not %s", py_type_name(hr));
        json_free(hr);
        return raise_desc("TypeError", msg);
    }
    json_t *argv = json_array();
    json_append(argv, json_string("git"));
    for (size_t i = 0; i < json_len(hr); i++) json_append(argv, json_copy(json_get(hr, i)));
    json_free(hr);
    json_t *proc = h_run(io, argv, cwd, timeout, env);
    json_free(argv);
    if (wgf_raised(proc)) return proc;
    json_t *t = json_array();
    if (j_is_null(proc)) {
        json_append(t, json_number(1));
        json_append(t, json_string(""));
        json_append(t, json_string("git invocation failed"));
        json_free(proc);
        return t;
    }
    json_t *rc = json_obj_get(proc, "returncode");
    json_t *so = json_obj_get(proc, "stdout");
    json_t *se = json_obj_get(proc, "stderr");
    json_append(t, rc ? json_copy(rc) : json_null());
    json_append(t, so ? json_copy(so) : json_null());
    json_append(t, se ? json_copy(se) : json_null());
    json_free(proc);
    return t;
}

/* _git: repo-scoped env; None -> (1, "", FILTER_DISCOVERY_FAILED). */
static json_t *h_git(wgf_io_t *io, const json_t *cwd, const json_t *args, double timeout) {
    json_t *ea = json_object();
    json_set(ea, "cwd", cwd ? json_copy(cwd) : json_null());
    json_t *env = wgf_call(io, "noninteractive_repo_git_env", ea);
    json_free(ea);
    if (wgf_raised(env)) return env;
    if (j_is_null(env)) {
        json_free(env);
        json_t *t = json_array();
        json_append(t, json_number(1));
        json_append(t, json_string(""));
        json_append(t, json_string(WGF_FILTER_DISCOVERY_FAILED));
        return t;
    }
    json_t *r = h_git_in(io, cwd, args, timeout, env);
    json_free(env);
    return r;
}

/* _git_write: plain noninteractive env (filters stay active). */
static json_t *h_git_write(wgf_io_t *io, const json_t *cwd, const json_t *args, double timeout) {
    json_t *ea = json_object();
    json_t *env = wgf_call(io, "noninteractive_git_env", ea);
    json_free(ea);
    if (wgf_raised(env)) return env;
    json_t *r = h_git_in(io, cwd, args, timeout, env);
    json_free(env);
    return r;
}

static json_t *h_runner(wgf_io_t *io, const json_t *cwd, const json_t *args, bool write) {
    return write ? h_git_write(io, cwd, args, WGF_GIT_TIMEOUT)
                 : h_git(io, cwd, args, WGF_GIT_TIMEOUT);
}

/* _git_out: stdout on code==0 else "". */
static json_t *h_git_out(wgf_io_t *io, const json_t *cwd, const json_t *args, bool write) {
    json_t *t = h_runner(io, cwd, args, write);
    if (wgf_raised(t)) return t;
    json_t *code = json_get(t, 0);
    json_t *out = json_get(t, 1);
    json_t *r;
    if (j_is_num(code) && code->num_val == 0.0) {
        r = out ? json_copy(out) : json_null();
    } else {
        r = json_string("");
    }
    json_free(t);
    return r;
}

/* _git_line: stripped stdout; non-str stdout raises AttributeError. */
static json_t *h_git_line(wgf_io_t *io, const json_t *cwd, const json_t *args, bool write) {
    json_t *out = h_git_out(io, cwd, args, write);
    if (wgf_raised(out)) return out;
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

/* _git_ok: RuntimeError(err.strip() or f"git {' '.join(args)} failed"). */
static json_t *h_git_ok(wgf_io_t *io, const json_t *cwd, const json_t *args) {
    json_t *t = h_git_write(io, cwd, args, WGF_GIT_TIMEOUT);
    if (wgf_raised(t)) return t;
    json_t *code = json_get(t, 0);
    if (j_is_num(code) && code->num_val == 0.0) {
        json_free(t);
        return json_null();
    }
    json_t *err = json_get(t, 2);
    if (!j_is_str(err)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", py_type_name(err));
        json_free(t);
        return raise_desc("AttributeError", msg);
    }
    char *stripped = py_strip(jstr(err));
    json_t *r;
    if (stripped && stripped[0]) {
        r = raise_desc("RuntimeError", stripped);
    } else {
        char *joined = str_join_sp(args);
        size_t need = strlen(joined ? joined : "") + 32;
        char *msg = malloc(need);
        if (msg) snprintf(msg, need, "git %s failed", joined ? joined : "");
        r = raise_desc("RuntimeError", msg ? msg : "");
        free(msg);
        free(joined);
    }
    free(stripped);
    json_free(t);
    return r;
}

/* _is_dir: Path(cwd).is_dir(); OSError -> False; Path(None) TypeError first. */
static json_t *h_is_dir(wgf_io_t *io, const json_t *cwd) {
    if (!j_is_str(cwd)) return wgf_path_type_error(cwd);
    char *norm = wgf_path_norm(jstr(cwd));
    json_t *args = json_object();
    json_set_hdr(args, "path", norm ? norm : "");
    free(norm);
    json_t *r = wgf_call(io, "path.is_dir", args);
    json_free(args);
    if (!r) r = json_null();
    if (wgf_raised(r)) {
        if (exc_is_oserror(r)) {
            json_free(r);
            return json_bool(false);
        }
        return r;
    }
    return r;
}

/* _status_z(cwd, *extra, run=...): ["status","--porcelain=v2",*extra,"-z"]. */
static json_t *h_status_z(wgf_io_t *io, const json_t *cwd, const json_t *extra, bool write) {
    json_t *args = json_array();
    json_append(args, json_string("status"));
    json_append(args, json_string("--porcelain=v2"));
    if (j_is_arr(extra)) {
        for (size_t i = 0; i < json_len(extra); i++) json_append(args, json_copy(json_get(extra, i)));
    }
    json_append(args, json_string("-z"));
    json_t *t = h_runner(io, cwd, args, write);
    json_free(args);
    if (wgf_raised(t)) return t;
    json_t *r = json_array();
    json_t *code = json_get(t, 0);
    json_t *raw = json_get(t, 1);
    json_append(r, code ? json_copy(code) : json_null());
    json_append(r, raw ? json_copy(raw) : json_null());
    json_free(t);
    return r;
}

static json_t *h_origin_head(wgf_io_t *io, const json_t *cwd, bool write) {
    json_t *args = json_array();
    json_append(args, json_string("symbolic-ref"));
    json_append(args, json_string("--quiet"));
    json_append(args, json_string("--short"));
    json_append(args, json_string("refs/remotes/origin/HEAD"));
    json_t *r = h_git_line(io, cwd, args, write);
    json_free(args);
    return r;
}

static json_t *h_origin_head_abbrev(wgf_io_t *io, const json_t *cwd) {
    json_t *args = json_array();
    json_append(args, json_string("rev-parse"));
    json_append(args, json_string("--abbrev-ref"));
    json_append(args, json_string("origin/HEAD"));
    json_t *r = h_git_line(io, cwd, args, false);
    json_free(args);
    return r;
}

static json_t *h_ref_exists(wgf_io_t *io, const json_t *cwd, const char *ref, bool write) {
    json_t *args = json_array();
    json_append(args, json_string("rev-parse"));
    json_append(args, json_string("--verify"));
    json_append(args, json_string("--quiet"));
    json_append(args, json_string(ref));
    json_t *t = h_runner(io, cwd, args, write);
    json_free(args);
    if (wgf_raised(t)) return t;
    json_t *code = json_get(t, 0);
    json_t *r = json_bool(j_is_num(code) && code->num_val == 0.0);
    json_free(t);
    return r;
}

/* ── pure leaves ───────────────────────────────────────────────────────── */
/* resolve_rename_path(raw): str(raw or "").strip(); " => " projection. */
static json_t *h_resolve_rename_path(const json_t *raw) {
    char *s;
    if (!jtruth(raw)) {
        s = strdup("");
    } else {
        s = py_str_of(raw);
    }
    char *path = py_strip(s ? s : "");
    free(s);
    if (!path) return json_string("");
    if (!str_contains(path, " => ")) return json_string(path);
    char *head = NULL, *tail = NULL;
    bool found = false;
    str_partition(path, "{", &head, &tail, &found);
    json_t *r = NULL;
    if (found && tail && tail[0] && str_contains(tail, "}")) {
        char *inner = NULL, *suffix = NULL;
        bool f2 = false;
        str_partition(tail, "}", &inner, &suffix, &f2);
        char *from = NULL, *to = NULL;
        bool f3 = false;
        str_partition(inner ? inner : "", " => ", &from, &to, &f3);
        size_t need = strlen(head ? head : "") + strlen(to ? to : "") + strlen(suffix ? suffix : "") + 1;
        char *joined = malloc(need);
        if (joined) snprintf(joined, need, "%s%s%s", head ? head : "", to ? to : "", suffix ? suffix : "");
        char *repl = py_replace(joined ? joined : "", "//", "/");
        r = json_string(repl ? repl : "");
        free(repl); free(joined);
        free(from); free(to); free(inner); free(suffix);
    } else {
        /* path.split(" => ")[-1].strip() — after the LAST separator */
        const char *last = path;
        const char *p = path;
        while ((p = strstr(p, " => ")) != NULL) { last = p + 4; p += 4; }
        char *stripped = py_strip(last);
        r = json_string(stripped ? stripped : "");
        free(stripped);
    }
    free(head); free(tail); free(path);
    return r;
}

/* _entry_staged(tag, xy): tag in ("1","2") and xy[0] not in (".","?"). */
static json_t *h_entry_staged(const char *tag, const char *xy) {
    bool tracked = strcmp(tag, "1") == 0 || strcmp(tag, "2") == 0;
    if (!tracked) return json_bool(false);
    if (!xy[0]) return raise_desc("IndexError", "string index out of range");
    return json_bool(xy[0] != '.' && xy[0] != '?');
}

/* _classify(tag, xy, path): dict order path,staged,unstaged,untracked,conflicted. */
static json_t *h_classify(const char *tag, const char *xy, const char *path) {
    char y = (xy[0] && xy[1]) ? xy[1] : '.';
    json_t *staged = h_entry_staged(tag, xy);
    if (wgf_raised(staged)) return staged;
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

/* _status_letter(tag, xy). */
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

/* _slugify(name): [^a-z0-9]+ -> "-", trim dashes, [:40], rstrip("-"), "work". */
static json_t *h_slugify(const json_t *name) {
    char *s;
    if (!jtruth(name)) s = strdup("");
    else s = py_str_of(name);
    char *stripped = py_strip(s ? s : "");
    free(s);
    char *lower = py_lower(stripped ? stripped : "");
    free(stripped);
    /* collapse non-[a-z0-9] runs to '-' (codepoint-aware) */
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
    /* ^-+|-+$ trim */
    char *start = slug;
    while (*start == '-') start++;
    size_t sl = strlen(start);
    while (sl > 0 && start[sl - 1] == '-') sl--;
    start[sl] = '\0';
    /* [:40] by codepoint, then rstrip('-') */
    char *cut = py_slice_left(start, 40);
    size_t cl = strlen(cut);
    while (cl > 0 && cut[cl - 1] == '-') cl--;
    cut[cl] = '\0';
    json_t *r = json_string(cut[0] ? cut : "work");
    free(cut);
    free(slug);
    return r;
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

/* ── claimed-sibling seams ─────────────────────────────────────────────── */
static json_t *h_walk_entries(wgf_io_t *io, const json_t *raw) {
    json_t *args = json_object();
    json_set(args, "raw", raw ? json_copy(raw) : json_null());
    json_t *r = wgf_call(io, "walk_entries", args);
    json_free(args);
    return r ? r : json_null();
}

static json_t *h_untracked(wgf_io_t *io, const json_t *cwd, const char *rel) {
    json_t *args = json_object();
    json_set(args, "cwd", cwd ? json_copy(cwd) : json_null());
    json_set_hdr(args, "rel", rel ? rel : "");
    json_t *r = wgf_call(io, "untracked_insertions", args);
    json_free(args);
    return r ? r : json_null();
}

/* ── _numstat ──────────────────────────────────────────────────────────── */
static json_t *h_numstat(wgf_io_t *io, const json_t *cwd, const json_t *args) {
    json_t *ga = json_array();
    json_append(ga, json_string("diff"));
    json_append(ga, json_string("--numstat"));
    if (j_is_arr(args)) {
        for (size_t i = 0; i < json_len(args); i++) json_append(ga, json_copy(json_get(args, i)));
    }
    json_t *out = h_git_out(io, cwd, ga, false);
    json_free(ga);
    if (wgf_raised(out)) return out;
    if (!j_is_str(out)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'splitlines'", py_type_name(out));
        json_free(out);
        return raise_desc("AttributeError", msg);
    }
    json_t *lines = py_split(jstr(out), 1);
    json_free(out);
    json_t *counts = json_object();
    for (size_t li = 0; li < json_len(lines); li++) {
        const char *line = jstr(json_get(lines, li));
        /* parts = line.split("\t") */
        json_t *parts = json_array();
        const char *p = line;
        for (;;) {
            const char *z = strchr(p, '\t');
            if (!z) { json_append(parts, json_string(p)); break; }
            size_t n = (size_t)(z - p);
            char *seg = malloc(n + 1);
            if (seg) { memcpy(seg, p, n); seg[n] = '\0'; json_append(parts, json_string(seg)); free(seg); }
            p = z + 1;
        }
        if (json_len(parts) >= 3) {
            double vals[2] = {0, 0};
            bool failed = false;
            json_t *err = NULL;
            for (int k = 0; k < 2; k++) {
                const char *pk = jstr(json_get(parts, k));
                if (strcmp(pk, "-") == 0) { vals[k] = 0; continue; }
                if (pk[0] == '\0') { vals[k] = 0; continue; }
                char *emsg = NULL;
                if (!py_int_parse(pk, &vals[k], &emsg)) {
                    err = raise_desc("ValueError", emsg ? emsg : "");
                    free(emsg);
                    failed = true;
                    break;
                }
            }
            if (failed) {
                json_free(parts); json_free(lines); json_free(counts);
                return err;
            }
            json_t *key = h_resolve_rename_path(json_get(parts, 2));
            json_t *pair = json_array();
            json_append(pair, json_number(vals[0]));
            json_append(pair, json_number(vals[1]));
            json_set(counts, jstr(key), pair);
            json_free(key);
        }
        json_free(parts);
    }
    json_free(lines);
    return counts;
}

/* ── _branch_base / _default_branch_name ───────────────────────────────── */
static json_t *h_branch_base(wgf_io_t *io, const json_t *cwd) {
    json_t *head = h_origin_head_abbrev(io, cwd);
    if (wgf_raised(head)) return head;
    const char *fixed[4] = { "origin/main", "origin/master", "main", "master" };
    size_t n = jtruth(head) ? 5 : 4;
    json_t *r = json_null();
    for (size_t i = 0; i < n; i++) {
        const char *ref = (i == 0 && jtruth(head)) ? jstr(head) : fixed[jtruth(head) ? i - 1 : i];
        json_t *args = json_array();
        json_append(args, json_string("merge-base"));
        json_append(args, json_string("HEAD"));
        json_append(args, json_string(ref));
        json_t *mb = h_git_line(io, cwd, args, false);
        json_free(args);
        if (wgf_raised(mb)) { json_free(head); return mb; }
        if (jtruth(mb)) { json_free(head); return mb; }
        json_free(mb);
    }
    json_free(head);
    return r;
}

static json_t *h_default_branch_name(wgf_io_t *io, const json_t *cwd) {
    json_t *head = h_origin_head_abbrev(io, cwd);
    if (wgf_raised(head)) return head;
    if (jtruth(head) && strcmp(jstr(head), "origin/HEAD") != 0) {
        const char *h = jstr(head);
        const char *slash = strchr(h, '/');
        json_t *r = json_string(slash ? slash + 1 : h);
        json_free(head);
        return r;
    }
    json_free(head);
    const char *refs[4] = { "refs/heads/main", "refs/heads/master",
                            "refs/remotes/origin/main", "refs/remotes/origin/master" };
    for (int i = 0; i < 4; i++) {
        json_t *ex = h_ref_exists(io, cwd, refs[i], false);
        if (wgf_raised(ex)) return ex;
        bool yes = ex->type == JSON_BOOL && ex->bool_val;
        json_free(ex);
        if (yes) {
            const char *last = strrchr(refs[i], '/');
            return json_string(last ? last + 1 : refs[i]);
        }
    }
    return json_null();
}

/* _has_staged(raw): any(_entry_staged(tag, xy) for ... in _walk_entries(raw)). */
static json_t *h_has_staged(wgf_io_t *io, const json_t *raw) {
    json_t *entries = h_walk_entries(io, raw);
    if (wgf_raised(entries)) return entries;
    json_t *r = json_bool(false);
    if (j_is_arr(entries)) {
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *trip = json_get(entries, i);
            const char *tag = jstr(json_get(trip, 0));
            const char *xy = jstr(json_get(trip, 1));
            json_t *es = h_entry_staged(tag, xy);
            if (wgf_raised(es)) { json_free(entries); return es; }
            bool yes = es->type == JSON_BOOL && es->bool_val;
            json_free(es);
            if (yes) { r = json_bool(true); break; }
        }
    }
    json_free(entries);
    return r;
}

/* ── repo_status ───────────────────────────────────────────────────────── */
static json_t *h_repo_status(wgf_io_t *io, const json_t *cwd) {
    json_t *isd = h_is_dir(io, cwd);
    if (wgf_raised(isd)) return isd;
    if (!jtruth(isd)) { json_free(isd); return json_null(); }
    json_free(isd);

    json_t *extra = json_array();
    json_append(extra, json_string("--branch"));
    json_t *sz = h_status_z(io, cwd, extra, false);
    json_free(extra);
    if (wgf_raised(sz)) return sz;
    json_t *code = json_get(sz, 0);
    if (!(j_is_num(code) && code->num_val == 0.0)) { json_free(sz); return json_null(); }
    json_t *raw = json_get(sz, 1) ? json_copy(json_get(sz, 1)) : json_null();
    json_free(sz);
    if (!j_is_str(raw)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'split'", py_type_name(raw));
        json_free(raw);
        return raise_desc("AttributeError", msg);
    }

    json_t *branch = json_null();
    bool detached = false;
    double ahead = 0, behind = 0;
    json_t *recs = wgf_split_records(jstr(raw));
    for (size_t i = 0; i < json_len(recs); i++) {
        const char *rec = jstr(json_get(recs, i));
        if (str_starts(rec, "# branch.head ")) {
            const char *head = rec + 14;
            detached = strcmp(head, "(detached)") == 0;
            json_free(branch);
            branch = detached ? json_null() : json_string(head);
        } else if (str_starts(rec, "# branch.ab ")) {
            json_t *toks = py_split_ws(rec);
            for (size_t k = 2; k < json_len(toks); k++) {
                const char *tok = jstr(json_get(toks, k));
                if (tok[0] == '+' || tok[0] == '-') {
                    const char *num = tok + 1;
                    double v = 0;
                    if (num[0] != '\0') {
                        char *emsg = NULL;
                        if (!py_int_parse(num, &v, &emsg)) {
                            json_t *err = raise_desc("ValueError", emsg ? emsg : "");
                            free(emsg);
                            json_free(toks); json_free(recs); json_free(raw); json_free(branch);
                            return err;
                        }
                    }
                    if (tok[0] == '+') ahead = v; else behind = v;
                }
            }
            json_free(toks);
        }
    }
    json_free(recs);

    json_t *entries = h_walk_entries(io, raw);
    if (wgf_raised(entries)) { json_free(raw); json_free(branch); return entries; }
    json_t *files = json_array();
    if (j_is_arr(entries)) {
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *trip = json_get(entries, i);
            json_t *c = h_classify(jstr(json_get(trip, 0)), jstr(json_get(trip, 1)), jstr(json_get(trip, 2)));
            if (wgf_raised(c)) {
                json_free(files); json_free(entries); json_free(raw); json_free(branch);
                return c;
            }
            json_append(files, c);
        }
    }
    json_free(entries);

    json_t *nargs = json_array();
    json_append(nargs, json_string("HEAD"));
    json_t *counts = h_numstat(io, cwd, nargs);
    json_free(nargs);
    if (wgf_raised(counts)) { json_free(files); json_free(raw); json_free(branch); return counts; }
    double added = 0, removed = 0;
    for (size_t i = 0; i < json_object_size(counts); i++) {
        json_t *pair = json_object_get_at(counts, i);
        json_t *a = json_get(pair, 0);
        json_t *r = json_get(pair, 1);
        if (j_is_num(a)) added += a->num_val;
        if (j_is_num(r)) removed += r->num_val;
    }
    size_t scan = json_len(files) < WGF_UNTRACKED_SCAN_CAP ? json_len(files) : WGF_UNTRACKED_SCAN_CAP;
    for (size_t i = 0; i < scan; i++) {
        json_t *f = json_get(files, i);
        json_t *ut = json_obj_get(f, "untracked");
        if (ut && ut->type == JSON_BOOL && ut->bool_val) {
            json_t *ui = h_untracked(io, cwd, json_get_str(f, "path", ""));
            if (wgf_raised(ui)) {
                json_free(counts); json_free(files); json_free(raw); json_free(branch);
                return ui;
            }
            if (j_is_num(ui)) added += ui->num_val;
            json_free(ui);
        }
    }
    json_free(counts);

    json_t *defb = h_default_branch_name(io, cwd);
    if (wgf_raised(defb)) { json_free(files); json_free(raw); json_free(branch); return defb; }

    double nstaged = 0, nunstaged = 0, nuntracked = 0, nconflicted = 0;
    for (size_t i = 0; i < json_len(files); i++) {
        json_t *f = json_get(files, i);
        json_t *v;
        v = json_obj_get(f, "staged");     if (v && v->type == JSON_BOOL && v->bool_val) nstaged++;
        v = json_obj_get(f, "unstaged");   if (v && v->type == JSON_BOOL && v->bool_val) nunstaged++;
        v = json_obj_get(f, "untracked");  if (v && v->type == JSON_BOOL && v->bool_val) nuntracked++;
        v = json_obj_get(f, "conflicted"); if (v && v->type == JSON_BOOL && v->bool_val) nconflicted++;
    }

    json_t *out = json_object();
    json_set(out, "branch", branch);
    json_set(out, "defaultBranch", defb);
    json_set(out, "detached", json_bool(detached));
    json_set(out, "ahead", json_number(ahead));
    json_set(out, "behind", json_number(behind));
    json_set(out, "staged", json_number(nstaged));
    json_set(out, "unstaged", json_number(nunstaged));
    json_set(out, "untracked", json_number(nuntracked));
    json_set(out, "conflicted", json_number(nconflicted));
    json_set(out, "changed", json_number((double)json_len(files)));
    json_set(out, "added", json_number(added));
    json_set(out, "removed", json_number(removed));
    json_t *files200 = json_array();
    size_t nf = json_len(files) < 200 ? json_len(files) : 200;
    for (size_t i = 0; i < nf; i++) json_append(files200, json_copy(json_get(files, i)));
    json_set(out, "files", files200);
    json_free(files);
    json_free(raw);
    return out;
}

/* ── _review_result: stable sort by path + untracked count fill ────────── */
static json_t *h_review_result(wgf_io_t *io, const json_t *cwd, json_t *files, const json_t *base) {
    size_t n = j_is_arr(files) ? json_len(files) : 0;
    /* insertion sort — STABLE (Python list.sort is stable) */
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
            json_t *ui = h_untracked(io, cwd, json_get_str(f, "path", ""));
            if (wgf_raised(ui)) { json_free(files); return ui; }
            json_set(f, "added", ui);
        }
    }
    json_t *out = json_object();
    json_set(out, "files", files);   /* adopts */
    json_set(out, "base", base ? json_copy(base) : json_null());
    return out;
}

/* ── review_list ───────────────────────────────────────────────────────── */
static json_t *empty_review(void) {
    json_t *o = json_object();
    json_set(o, "files", json_array());
    json_set(o, "base", json_null());
    return o;
}

static json_t *h_review_list(wgf_io_t *io, const json_t *cwd, const char *scope, const json_t *base_ref) {
    json_t *isd = h_is_dir(io, cwd);
    if (wgf_raised(isd)) return isd;
    if (!jtruth(isd)) { json_free(isd); return empty_review(); }
    json_free(isd);

    bool branch_scope = strcmp(scope, "branch") == 0;
    bool last_turn = strcmp(scope, "lastTurn") == 0;
    if (branch_scope || last_turn) {
        json_t *base = branch_scope ? h_branch_base(io, cwd)
                                    : (base_ref ? json_copy(base_ref) : json_null());
        if (wgf_raised(base)) return base;
        if (!jtruth(base)) { json_free(base); return empty_review(); }
        char *rng;
        if (branch_scope) {
            const char *b = jstr(base);
            size_t need = strlen(b) + 16;
            rng = malloc(need);
            if (rng) snprintf(rng, need, "%s...HEAD", b);
        } else {
            rng = strdup(jstr(base));
        }
        json_t *nargs = json_array();
        json_append(nargs, json_string(rng ? rng : ""));
        free(rng);
        json_t *num = h_numstat(io, cwd, nargs);
        json_free(nargs);
        if (wgf_raised(num)) { json_free(base); return num; }
        json_t *files = json_array();
        for (size_t i = 0; i < json_object_size(num); i++) {
            const char *path = json_object_get_key_at(num, i);
            json_t *pair = json_object_get_at(num, i);
            json_t *row = json_object();
            json_set_hdr(row, "path", path);
            json_t *a = json_get(pair, 0);
            json_t *r = json_get(pair, 1);
            json_set(row, "added", a ? json_copy(a) : json_number(0));
            json_set(row, "removed", r ? json_copy(r) : json_number(0));
            json_set_hdr(row, "status", "M");
            json_set(row, "staged", json_bool(false));
            json_append(files, row);
        }
        json_free(num);
        if (last_turn) {
            json_t *sz = h_status_z(io, cwd, NULL, false);
            if (wgf_raised(sz)) { json_free(files); json_free(base); return sz; }
            json_t *raw = json_get(sz, 1) ? json_copy(json_get(sz, 1)) : json_null();
            json_free(sz);
            json_t *entries = h_walk_entries(io, raw);
            json_free(raw);
            if (wgf_raised(entries)) { json_free(files); json_free(base); return entries; }
            if (j_is_arr(entries)) {
                for (size_t i = 0; i < json_len(entries); i++) {
                    json_t *trip = json_get(entries, i);
                    const char *tag = jstr(json_get(trip, 0));
                    const char *path = jstr(json_get(trip, 2));
                    if (strcmp(tag, "?") != 0) continue;
                    bool seen = false;
                    for (size_t k = 0; k < json_len(files); k++) {
                        if (strcmp(json_get_str(json_get(files, k), "path", ""), path) == 0) { seen = true; break; }
                    }
                    if (seen) continue;
                    json_t *row = json_object();
                    json_set_hdr(row, "path", path);
                    json_set(row, "added", json_number(0));
                    json_set(row, "removed", json_number(0));
                    json_set_hdr(row, "status", "?");
                    json_set(row, "staged", json_bool(false));
                    json_append(files, row);
                }
            }
            json_free(entries);
        }
        json_t *r = h_review_result(io, cwd, files, base);
        json_free(base);
        return r;
    }

    json_t *sz = h_status_z(io, cwd, NULL, false);
    if (wgf_raised(sz)) return sz;
    json_t *code = json_get(sz, 0);
    if (!(j_is_num(code) && code->num_val == 0.0)) { json_free(sz); return empty_review(); }
    json_t *raw = json_get(sz, 1) ? json_copy(json_get(sz, 1)) : json_null();
    json_free(sz);

    json_t *sargs = json_array();
    json_append(sargs, json_string("--cached"));
    json_t *staged = h_numstat(io, cwd, sargs);
    json_free(sargs);
    if (wgf_raised(staged)) { json_free(raw); return staged; }
    json_t *uargs = json_array();
    json_t *unstaged = h_numstat(io, cwd, uargs);
    json_free(uargs);
    if (wgf_raised(unstaged)) { json_free(staged); json_free(raw); return unstaged; }

    json_t *entries = h_walk_entries(io, raw);
    if (wgf_raised(entries)) { json_free(staged); json_free(unstaged); json_free(raw); return entries; }
    json_t *files = json_array();
    if (j_is_arr(entries)) {
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *trip = json_get(entries, i);
            const char *tag = jstr(json_get(trip, 0));
            const char *xy = jstr(json_get(trip, 1));
            const char *path = jstr(json_get(trip, 2));
            double sa = 0, sr = 0, ua = 0, ur = 0;
            json_t *sp = json_obj_get(staged, path);
            if (j_is_arr(sp)) {
                json_t *a = json_get(sp, 0); json_t *r = json_get(sp, 1);
                if (j_is_num(a)) sa = a->num_val;
                if (j_is_num(r)) sr = r->num_val;
            }
            json_t *up = json_obj_get(unstaged, path);
            if (j_is_arr(up)) {
                json_t *a = json_get(up, 0); json_t *r = json_get(up, 1);
                if (j_is_num(a)) ua = a->num_val;
                if (j_is_num(r)) ur = r->num_val;
            }
            json_t *letter = h_status_letter(tag, xy);
            if (wgf_raised(letter)) {
                json_free(files); json_free(entries); json_free(staged); json_free(unstaged); json_free(raw);
                return letter;
            }
            json_t *est = h_entry_staged(tag, xy);
            if (wgf_raised(est)) {
                json_free(letter); json_free(files); json_free(entries); json_free(staged); json_free(unstaged); json_free(raw);
                return est;
            }
            json_t *row = json_object();
            json_set_hdr(row, "path", path);
            json_set(row, "added", json_number(sa + ua));
            json_set(row, "removed", json_number(sr + ur));
            json_set(row, "status", letter);
            json_set(row, "staged", est);
            json_append(files, row);
        }
    }
    json_free(entries);
    json_free(staged);
    json_free(unstaged);
    json_free(raw);
    return h_review_result(io, cwd, files, NULL);
}


/* ── dynamic string builder ────────────────────────────────────────────── */
typedef struct { char *s; size_t len, cap; } wgf_sb;
static void sb_init(wgf_sb *b) { b->cap = 64; b->len = 0; b->s = malloc(b->cap); if (b->s) b->s[0] = '\0'; }
static void sb_add(wgf_sb *b, const char *s) {
    if (!b->s || !s) return;
    size_t n = strlen(s);
    while (b->len + n + 1 > b->cap) { b->cap *= 2; b->s = realloc(b->s, b->cap); if (!b->s) return; }
    memcpy(b->s + b->len, s, n); b->len += n; b->s[b->len] = '\0';
}
static void sb_addn(wgf_sb *b, const char *s, size_t n) {
    if (!b->s || !s) return;
    while (b->len + n + 1 > b->cap) { b->cap *= 2; b->s = realloc(b->s, b->cap); if (!b->s) return; }
    memcpy(b->s + b->len, s, n); b->len += n; b->s[b->len] = '\0';
}

/* ── _all_add_diff / _single_file_diff / review_diff ───────────────────── */
static json_t *h_all_add_diff(wgf_io_t *io, const json_t *cwd, const char *fp) {
    if (!j_is_str(cwd)) return wgf_path_type_error(cwd);
    char *joined = wgf_path_join(jstr(cwd), fp);
    json_t *args = json_object();
    json_set_hdr(args, "path", joined ? joined : "");
    free(joined);
    json_t *isf = wgf_call(io, "path.is_file", args);
    json_free(args);
    if (!isf) isf = json_null();
    if (wgf_raised(isf)) return isf;   /* no live try at this site */
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
    json_t *t = h_git(io, cwd, ga, WGF_GIT_TIMEOUT);
    json_free(ga);
    if (wgf_raised(t)) return t;
    json_t *out = json_get(t, 1);
    json_t *r = out ? json_copy(out) : json_null();
    json_free(t);
    return r;
}

static json_t *h_resolve_seam(wgf_io_t *io, const char *path) {
    json_t *args = json_object();
    json_set_hdr(args, "path", path ? path : "");
    json_t *r = wgf_call(io, "path.resolve", args);
    json_free(args);
    if (!r) r = json_null();
    if (wgf_raised(r)) return r;
    /* the oracle wraps the script back into Path(): non-str raises there */
    if (!j_is_str(r)) {
        json_t *e = wgf_path_type_error(r);
        json_free(r);
        return e;
    }
    return r;
}

static json_t *h_single_file_diff(wgf_io_t *io, const json_t *cwd, const json_t *args, const char *fp) {
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
    json_t *names = h_git_out(io, cwd, ga, false);
    json_free(ga);
    if (wgf_raised(names)) { free(literal); return names; }
    if (!jtruth(names)) { json_free(names); free(literal); return json_string(""); }

    json_t *tlargs = json_array();
    json_append(tlargs, json_string("rev-parse"));
    json_append(tlargs, json_string("--show-toplevel"));
    json_t *tl = h_git_line(io, cwd, tlargs, false);
    json_free(tlargs);
    if (wgf_raised(tl)) { json_free(names); free(literal); return tl; }
    char *root = wgf_path_norm(jstr(tl));
    json_free(tl);

    char *tjoined = wgf_path_join(jstr(cwd), fp);
    json_t *target = h_resolve_seam(io, tjoined);
    free(tjoined);
    if (wgf_raised(target)) { json_free(names); free(root); free(literal); return target; }

    json_t *recs = wgf_split_records(jstr(names));
    json_free(names);
    for (size_t i = 0; i < json_len(recs); i++) {
        const char *name = jstr(json_get(recs, i));
        if (!name[0]) continue;
        char *j = wgf_path_join(root, name);
        json_t *res = h_resolve_seam(io, j);
        free(j);
        if (wgf_raised(res)) {
            json_free(recs); json_free(target); free(root); free(literal);
            return res;
        }
        bool ne = !wgf_path_eq(jstr(res), jstr(target));
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
    return h_git_out(io, cwd, ga2, false);
}

static json_t *h_review_diff(wgf_io_t *io, const json_t *cwd, const char *fp,
                             const char *scope, const json_t *base_ref, bool staged) {
    json_t *isd = h_is_dir(io, cwd);
    if (wgf_raised(isd)) return isd;
    if (!jtruth(isd)) { json_free(isd); return json_string(""); }
    json_free(isd);

    if (strcmp(scope, "branch") == 0) {
        json_t *base = h_branch_base(io, cwd);
        if (wgf_raised(base)) return base;
        if (!jtruth(base)) { json_free(base); return json_string(""); }
        const char *b = jstr(base);
        size_t need = strlen(b) + 16;
        char *rng = malloc(need);
        if (rng) snprintf(rng, need, "%s...HEAD", b);
        json_t *args = json_array();
        json_append(args, json_string(rng ? rng : ""));
        free(rng);
        json_free(base);
        json_t *r = h_single_file_diff(io, cwd, args, fp);
        json_free(args);
        return r;
    }
    if (strcmp(scope, "lastTurn") == 0) {
        if (!jtruth(base_ref)) return json_string("");
        json_t *args = json_array();
        json_append(args, json_copy(base_ref));
        json_t *r = h_single_file_diff(io, cwd, args, fp);
        json_free(args);
        return r;
    }
    if (staged) {
        json_t *args = json_array();
        json_append(args, json_string("--cached"));
        json_t *r = h_single_file_diff(io, cwd, args, fp);
        json_free(args);
        return r;
    }
    json_t *args = json_array();
    json_t *worktree = h_single_file_diff(io, cwd, args, fp);
    json_free(args);
    if (wgf_raised(worktree)) return worktree;
    char *stripped = py_strip(jstr(worktree));
    bool nonempty = stripped && stripped[0];
    free(stripped);
    if (nonempty) return worktree;
    json_free(worktree);
    return h_all_add_diff(io, cwd, fp);
}

/* ── _review_commit_env ────────────────────────────────────────────────── */
/* re.fullmatch(r"(.*) <([^<>]*)> -?\d+ [+-]\d{4}", s) — greedy name tries
 * the RIGHTMOST " <" first; email stops at the first '<' or '>'; '.' excludes
 * \n in the name group; \d is ASCII-domain (documented). */
static bool ident_fullmatch(const char *s, char **name_out, char **email_out) {
    size_t len = strlen(s);
    /* candidate " <" positions, rightmost first */
    for (size_t i = len; i >= 2; i--) {
        size_t pos = i - 2;
        if (s[pos] != ' ' || s[pos + 1] != '<') continue;
        /* name = s[0..pos) must not contain \n */
        bool nl = false;
        for (size_t k = 0; k < pos; k++) if (s[k] == '\n') { nl = true; break; }
        if (nl) continue;
        size_t j = pos + 2;
        while (s[j] && s[j] != '<' && s[j] != '>') j++;
        if (s[j] != '>') continue;
        size_t k = j + 1;
        if (s[k] != ' ') continue;
        k++;
        if (s[k] == '-') k++;
        size_t d0 = k;
        while (s[k] >= '0' && s[k] <= '9') k++;
        if (k == d0) continue;
        if (s[k] != ' ') continue;
        k++;
        if (s[k] != '+' && s[k] != '-') continue;
        k++;
        size_t t0 = k;
        while (s[k] >= '0' && s[k] <= '9') k++;
        if (k - t0 != 4) continue;
        if (s[k] != '\0') continue;
        *name_out = malloc(pos + 1);
        if (*name_out) { memcpy(*name_out, s, pos); (*name_out)[pos] = '\0'; }
        *email_out = malloc(j - (pos + 2) + 1);
        if (*email_out) { memcpy(*email_out, s + pos + 2, j - (pos + 2)); (*email_out)[j - (pos + 2)] = '\0'; }
        return true;
    }
    return false;
}

static json_t *attr_strip_err(const json_t *v) {
    char msg[128];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", py_type_name(v));
    return raise_desc("AttributeError", msg);
}

static json_t *h_review_commit_env(wgf_io_t *io, const json_t *cwd) {
    json_t *ea0 = json_object();
    json_t *base = wgf_call(io, "environ", ea0);
    json_free(ea0);
    if (!base) base = json_null();
    if (wgf_raised(base)) return base;
    /* base = dict(os.environ): a non-mapping seam result raises TypeError
     * (domain: object or null; str/list update-sequence errors excluded). */
    if (!j_is_obj(base)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", py_type_name(base));
        json_free(base);
        return raise_desc("TypeError", msg);
    }

    json_t *ea = json_object();
    json_set(ea, "base", json_copy(base));
    json_t *env = wgf_call(io, "noninteractive_git_env", ea);
    json_free(ea);
    if (wgf_raised(env)) { json_free(base); return env; }

    json_t *probe = json_copy(env);
    const char *cfgkeys[3] = { "GIT_CONFIG_GLOBAL", "GIT_CONFIG_SYSTEM", "GIT_CONFIG_NOSYSTEM" };
    for (int k = 0; k < 3; k++) {
        json_t *bv = json_obj_get(base, cfgkeys[k]);
        if (bv) json_set(probe, cfgkeys[k], json_copy(bv));
        else json_obj_del(probe, cfgkeys[k]);
    }
    const char *roles[2] = { "AUTHOR", "COMMITTER" };
    for (int r = 0; r < 2; r++) {
        char varname[32];
        snprintf(varname, sizeof varname, "GIT_%s_IDENT", roles[r]);
        json_t *argv = json_array();
        json_append(argv, json_string("git"));
        json_append(argv, json_string("var"));
        json_append(argv, json_string(varname));
        json_t *proc = h_run(io, argv, cwd, 5, probe);
        json_free(argv);
        if (wgf_raised(proc)) { json_free(base); json_free(env); json_free(probe); return proc; }
        if (j_is_null(proc)) {
            json_free(proc); json_free(base); json_free(env); json_free(probe);
            return raise_desc("RuntimeError", "git identity lookup failed");
        }
        json_t *rc = json_obj_get(proc, "returncode");
        if (!(j_is_num(rc) && rc->num_val == 0.0)) {
            json_t *se = json_obj_get(proc, "stderr");
            json_t *err;
            if (!j_is_str(se)) {
                err = attr_strip_err(se);
            } else {
                char *stripped = py_strip(jstr(se));
                err = raise_desc("RuntimeError", (stripped && stripped[0]) ? stripped : "git identity lookup failed");
                free(stripped);
            }
            json_free(proc); json_free(base); json_free(env); json_free(probe);
            return err;
        }
        json_t *so = json_obj_get(proc, "stdout");
        if (!j_is_str(so)) {
            json_t *err = attr_strip_err(so);
            json_free(proc); json_free(base); json_free(env); json_free(probe);
            return err;
        }
        char *stripped = py_strip(jstr(so));
        char *name = NULL, *email = NULL;
        bool ok = ident_fullmatch(stripped ? stripped : "", &name, &email);
        free(stripped);
        json_free(proc);
        if (!ok) {
            free(name); free(email);
            json_free(base); json_free(env); json_free(probe);
            return raise_desc("RuntimeError", "git returned an invalid commit identity");
        }
        char key[32];
        snprintf(key, sizeof key, "GIT_%s_NAME", roles[r]);
        json_set_hdr(env, key, name ? name : "");
        snprintf(key, sizeof key, "GIT_%s_EMAIL", roles[r]);
        json_set_hdr(env, key, email ? email : "");
        free(name); free(email);
    }
    json_free(base);
    json_free(probe);
    return env;
}

/* ── _review_push / review_commit ──────────────────────────────────────── */
static json_t *h_review_push(wgf_io_t *io, const json_t *cwd) {
    json_t *args = json_array();
    json_append(args, json_string("rev-parse"));
    json_append(args, json_string("--abbrev-ref"));
    json_append(args, json_string("--symbolic-full-name"));
    json_append(args, json_string("@{u}"));
    json_t *up = h_git_line(io, cwd, args, true);
    json_free(args);
    if (wgf_raised(up)) return up;
    if (jtruth(up)) {
        json_free(up);
        json_t *pa = json_array();
        json_append(pa, json_string("push"));
        json_t *r = h_git_ok(io, cwd, pa);
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
    if (wgf_raised(branch)) return branch;
    if (jtruth(branch) && strcmp(jstr(branch), "HEAD") != 0) {
        json_t *pa = json_array();
        json_append(pa, json_string("push"));
        json_append(pa, json_string("-u"));
        json_append(pa, json_string("origin"));
        json_append(pa, json_copy(branch));
        json_free(branch);
        json_t *r = h_git_ok(io, cwd, pa);
        json_free(pa);
        return r;
    }
    json_free(branch);
    return json_null();
}

static json_t *h_review_commit(wgf_io_t *io, const json_t *cwd, const char *message, bool push) {
    json_t *env = h_review_commit_env(io, cwd);
    if (wgf_raised(env)) return env;

    json_t *sz = h_status_z(io, cwd, NULL, true);
    if (wgf_raised(sz)) { json_free(env); return sz; }
    json_t *raw = json_get(sz, 1) ? json_copy(json_get(sz, 1)) : json_null();
    json_free(sz);
    json_t *hs = h_has_staged(io, raw);
    json_free(raw);
    if (wgf_raised(hs)) { json_free(env); return hs; }
    if (!jtruth(hs)) {
        json_t *aa = json_array();
        json_append(aa, json_string("add"));
        json_append(aa, json_string("-A"));
        json_t *ok = h_git_ok(io, cwd, aa);
        json_free(aa);
        if (wgf_raised(ok)) { json_free(hs); json_free(env); return ok; }
        json_free(ok);
    }
    json_free(hs);

    json_t *cargs = json_array();
    json_append(cargs, json_string("commit"));
    json_append(cargs, json_string("-m"));
    json_append(cargs, json_string(message));
    json_t *ha = json_object();
    json_set(ha, "args", cargs);   /* adopts cargs */
    json_t *hardened = wgf_call(io, "harden_git_argv", ha);
    json_free(ha);
    if (wgf_raised(hardened)) { json_free(env); return hardened; }
    if (!j_is_arr(hardened)) {
        char msg[128];
        snprintf(msg, sizeof msg, "Value after * must be an iterable, not %s", py_type_name(hardened));
        json_free(hardened); json_free(env);
        return raise_desc("TypeError", msg);
    }
    json_t *argv = json_array();
    json_append(argv, json_string("git"));
    if (j_is_arr(hardened)) {
        for (size_t i = 0; i < json_len(hardened); i++) json_append(argv, json_copy(json_get(hardened, i)));
    }
    json_free(hardened);
    json_t *proc = h_run(io, argv, cwd, WGF_GIT_TIMEOUT, env);
    json_free(argv);
    json_free(env);
    if (wgf_raised(proc)) return proc;
    if (j_is_null(proc)) {
        json_free(proc);
        return raise_desc("RuntimeError", "git commit failed");
    }
    json_t *rc = json_obj_get(proc, "returncode");
    if (!(j_is_num(rc) && rc->num_val == 0.0)) {
        json_t *se = json_obj_get(proc, "stderr");
        json_t *err;
        if (!j_is_str(se)) {
            err = attr_strip_err(se);
        } else {
            char *stripped = py_strip(jstr(se));
            err = raise_desc("RuntimeError", (stripped && stripped[0]) ? stripped : "git commit failed");
            free(stripped);
        }
        json_free(proc);
        return err;
    }
    json_free(proc);
    if (push) {
        json_t *rp = h_review_push(io, cwd);
        if (wgf_raised(rp)) return rp;
        json_free(rp);
    }
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    return out;
}


/* ── review_commit_context ─────────────────────────────────────────────── */
static json_t *h_path_allowed(wgf_io_t *io, const char *path) {
    json_t *args = json_object();
    json_set_hdr(args, "path", path ? path : "");
    json_t *r = wgf_call(io, "path_allowed", args);
    json_free(args);
    return r ? r : json_null();
}

static json_t *ctx_empty(void) {
    json_t *o = json_object();
    json_set_hdr(o, "diff", "");
    json_set_hdr(o, "recent", "");
    return o;
}

static json_t *h_review_commit_context(wgf_io_t *io, const json_t *cwd, bool has_pa) {
    json_t *isd = h_is_dir(io, cwd);
    if (wgf_raised(isd)) return isd;
    json_t *sz;
    if (jtruth(isd)) {
        sz = h_status_z(io, cwd, NULL, false);
    } else {
        sz = json_array();
        json_append(sz, json_number(1));
        json_append(sz, json_string(""));
    }
    json_free(isd);
    if (wgf_raised(sz)) return sz;
    json_t *code = json_get(sz, 0);
    if (!(j_is_num(code) && code->num_val == 0.0)) { json_free(sz); return ctx_empty(); }
    json_t *raw = json_get(sz, 1) ? json_copy(json_get(sz, 1)) : json_null();
    json_free(sz);

    json_t *entries = h_walk_entries(io, raw);
    if (wgf_raised(entries)) { json_free(raw); return entries; }
    json_t *hs = h_has_staged(io, raw);
    if (wgf_raised(hs)) { json_free(entries); json_free(raw); return hs; }
    json_t *args = json_array();
    json_append(args, json_string("diff"));
    json_append(args, json_string(jtruth(hs) ? "--cached" : "HEAD"));
    json_free(hs);

    json_t *diff = NULL;
    if (!has_pa) {
        diff = h_git_out(io, cwd, args, false);
        if (wgf_raised(diff)) { json_free(args); json_free(entries); json_free(raw); return diff; }
    } else {
        json_t *tlargs = json_array();
        json_append(tlargs, json_string("rev-parse"));
        json_append(tlargs, json_string("--show-toplevel"));
        json_t *repo = h_git_line(io, cwd, tlargs, false);
        json_free(tlargs);
        if (wgf_raised(repo)) { json_free(args); json_free(entries); json_free(raw); return repo; }
        if (!jtruth(repo)) {
            json_free(repo); json_free(args); json_free(entries); json_free(raw);
            return ctx_empty();
        }
        json_t *na = json_copy(args);
        json_append(na, json_string("--no-renames"));
        json_append(na, json_string("--name-only"));
        json_append(na, json_string("-z"));
        json_t *names = h_git_out(io, repo, na, false);
        json_free(na);
        if (wgf_raised(names)) { json_free(repo); json_free(args); json_free(entries); json_free(raw); return names; }
        if (!j_is_str(names)) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'split'", py_type_name(names));
            json_free(names); json_free(repo); json_free(args); json_free(entries); json_free(raw);
            return raise_desc("AttributeError", msg);
        }
        json_t *recs = wgf_split_records(jstr(names));
        json_free(names);
        json_t *allowed = json_array();
        for (size_t i = 0; i < json_len(recs); i++) {
            const char *name = jstr(json_get(recs, i));
            if (!name[0]) continue;
            char *j = wgf_path_join(jstr(repo), name);
            json_t *pa = h_path_allowed(io, j);
            free(j);
            if (wgf_raised(pa)) {
                json_free(allowed); json_free(recs); json_free(repo); json_free(args); json_free(entries); json_free(raw);
                return pa;
            }
            if (jtruth(pa)) {
                size_t need = strlen(name) + 16;
                char *lit = malloc(need);
                if (lit) { snprintf(lit, need, ":(literal)%s", name); json_append(allowed, json_string(lit)); free(lit); }
            }
            json_free(pa);
        }
        json_free(recs);
        if (json_len(allowed) > 0) {
            json_t *da = json_copy(args);
            json_append(da, json_string("--no-renames"));
            json_append(da, json_string("--"));
            for (size_t i = 0; i < json_len(allowed); i++) json_append(da, json_copy(json_get(allowed, i)));
            diff = h_git_out(io, repo, da, false);
            json_free(da);
        } else {
            diff = json_string("");
        }
        json_free(allowed);
        if (wgf_raised(diff)) { json_free(repo); json_free(args); json_free(entries); json_free(raw); return diff; }
        /* entries = [entry for entry in entries if path_allowed(Path(repo)/entry[2])] */
        json_t *kept = json_array();
        if (j_is_arr(entries)) {
            for (size_t i = 0; i < json_len(entries); i++) {
                json_t *trip = json_get(entries, i);
                char *j = wgf_path_join(jstr(repo), jstr(json_get(trip, 2)));
                json_t *pa = h_path_allowed(io, j);
                free(j);
                if (wgf_raised(pa)) {
                    json_free(kept); json_free(diff); json_free(repo); json_free(args); json_free(entries); json_free(raw);
                    return pa;
                }
                if (jtruth(pa)) json_append(kept, json_copy(trip));
                json_free(pa);
            }
        }
        json_free(entries);
        entries = kept;
        json_free(repo);
    }
    json_free(args);

    /* truncation at _COMMIT_CONTEXT_DIFF_MAX_CHARS (codepoint count) */
    if (!j_is_str(diff)) {
        char msg[128];
        snprintf(msg, sizeof msg, "object of type '%s' has no len()", py_type_name(diff));
        json_free(diff); json_free(entries); json_free(raw);
        return raise_desc("TypeError", msg);
    }
    size_t dlen = py_len(jstr(diff));
    if (dlen > WGF_COMMIT_CONTEXT_DIFF_MAX_CHARS) {
        size_t omitted = dlen - WGF_COMMIT_CONTEXT_DIFF_MAX_CHARS;
        char *cut = py_slice_left(jstr(diff), WGF_COMMIT_CONTEXT_DIFF_MAX_CHARS);
        wgf_sb b; sb_init(&b);
        sb_add(&b, cut ? cut : "");
        free(cut);
        char tail[96];
        snprintf(tail, sizeof tail, "\n# diff truncated: %zu chars omitted\n", omitted);
        sb_add(&b, tail);
        json_free(diff);
        diff = json_string(b.s ? b.s : "");
        free(b.s);
    }
    /* untracked appendix */
    json_t *untracked = json_array();
    if (j_is_arr(entries)) {
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *trip = json_get(entries, i);
            if (strcmp(jstr(json_get(trip, 0)), "?") == 0) json_append(untracked, json_copy(json_get(trip, 2)));
        }
    }
    if (json_len(untracked) > 0) {
        size_t visible = json_len(untracked) < WGF_COMMIT_CONTEXT_UNTRACKED_MAX
                       ? json_len(untracked) : WGF_COMMIT_CONTEXT_UNTRACKED_MAX;
        wgf_sb b; sb_init(&b);
        sb_add(&b, jstr(diff));
        sb_add(&b, "\n# New (untracked) files:\n");
        for (size_t i = 0; i < visible; i++) {
            sb_add(&b, "#   ");
            sb_add(&b, jstr(json_get(untracked, i)));
            sb_add(&b, "\n");
        }
        if (json_len(untracked) > visible) {
            char more[96];
            snprintf(more, sizeof more, "#   ... %zu more omitted\n", json_len(untracked) - visible);
            sb_add(&b, more);
        }
        json_free(diff);
        diff = json_string(b.s ? b.s : "");
        free(b.s);
    }
    json_free(untracked);
    json_free(entries);
    json_free(raw);

    json_t *largs = json_array();
    json_append(largs, json_string("log"));
    json_append(largs, json_string("-n"));
    json_append(largs, json_string("10"));
    json_append(largs, json_string("--pretty=format:%s"));
    json_t *recent = h_git_line(io, cwd, largs, false);
    json_free(largs);
    if (wgf_raised(recent)) { json_free(diff); return recent; }

    json_t *out = json_object();
    json_set(out, "diff", diff);   /* adopts; "" stays "" (diff or "") */
    json_set(out, "recent", recent);
    return out;
}

/* ── worktree / branch decision chains ─────────────────────────────────── */
static json_t *h_worktree_list(wgf_io_t *io, const json_t *cwd, bool write) {
    json_t *args = json_array();
    json_append(args, json_string("worktree"));
    json_append(args, json_string("list"));
    json_append(args, json_string("--porcelain"));
    json_t *out = h_git_out(io, cwd, args, write);
    json_free(args);
    if (wgf_raised(out)) return out;
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
            char *repl = py_replace(v ? v : "", "refs/heads/", "");
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

static json_t *h_main_root(wgf_io_t *io, const json_t *cwd) {
    json_t *trees = h_worktree_list(io, cwd, true);
    if (wgf_raised(trees)) return trees;
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

static json_t *h_ensure_repo(wgf_io_t *io, const json_t *cwd) {
    json_t *args = json_array();
    json_append(args, json_string("rev-parse"));
    json_append(args, json_string("--is-inside-work-tree"));
    json_t *line = h_git_line(io, cwd, args, true);
    json_free(args);
    if (wgf_raised(line)) return line;
    bool needs_root;
    if (strcmp(jstr(line), "true") != 0) {
        json_t *ia = json_array();
        json_append(ia, json_string("init"));
        json_t *ok = h_git_ok(io, cwd, ia);
        json_free(ia);
        if (wgf_raised(ok)) { json_free(line); return ok; }
        json_free(ok);
        needs_root = true;
    } else {
        json_t *ex = h_ref_exists(io, cwd, "HEAD", true);
        if (wgf_raised(ex)) { json_free(line); return ex; }
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
        json_t *ok = h_git_ok(io, cwd, ca);
        json_free(ca);
        return ok;
    }
    return json_null();
}

static json_t *h_default_branch(wgf_io_t *io, const json_t *cwd, bool write) {
    json_t *oh = h_origin_head(io, cwd, write);
    if (wgf_raised(oh)) return oh;
    char *repl = py_replace(jstr(oh), "origin/", "");
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
    if (wgf_raised(cfg)) return cfg;
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
        if (wgf_raised(sr)) return sr;
        if (jtruth(sr)) { json_free(sr); return json_string(trunk[i]); }
        json_free(sr);
    }
    return json_string("");
}

static json_t *h_remote_of_ref(wgf_io_t *io, const json_t *cwd, const char *name) {
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
    json_t *t = h_git_write(io, cwd, args, WGF_GIT_TIMEOUT);
    json_free(args);
    if (wgf_raised(t)) return t;
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

static json_t *h_fetch_tracking_ref(wgf_io_t *io, const json_t *root, const char *remote, const char *branch) {
    json_t *args = json_object();
    json_set_hdr(args, "remote", remote);
    json_set_hdr(args, "branch", branch);
    json_t *ts = wgf_call(io, "tracking_refspec", args);
    json_free(args);
    if (wgf_raised(ts)) return ts;
    json_t *fa = json_array();
    json_append(fa, json_string("fetch"));
    json_append(fa, json_string(remote));
    json_append(fa, ts);   /* adopts */
    json_t *t = h_git_write(io, root, fa, WGF_GIT_TIMEOUT);
    json_free(fa);
    if (wgf_raised(t)) return t;
    json_t *code = json_get(t, 0);
    json_t *r = json_bool(j_is_num(code) && code->num_val == 0.0);
    json_free(t);
    return r;
}


/* ── _worktree_for_existing ────────────────────────────────────────────── */
static json_t *h_worktree_for_existing(wgf_io_t *io, const json_t *root, const json_t *raw_name) {
    json_t *sa = json_object();
    json_set(sa, "name", raw_name ? json_copy(raw_name) : json_null());
    json_t *req = wgf_call(io, "sanitize_branch", sa);
    json_free(sa);
    if (wgf_raised(req)) return req;
    if (!jtruth(req)) {
        json_free(req);
        return raise_desc("RuntimeError", "Branch name is required.");
    }
    char *requested = strdup(jstr(req));
    json_free(req);

    json_t *remote = h_remote_of_ref(io, root, requested);
    if (wgf_raised(remote)) { free(requested); return remote; }
    bool fetched = false;
    if (!jtruth(remote) && strchr(requested, '/')) {
        size_t need = strlen(requested) + 32;
        char *ref = malloc(need);
        if (ref) snprintf(ref, need, "refs/heads/%s", requested);
        json_t *ex = h_ref_exists(io, root, ref ? ref : "", true);
        free(ref);
        if (wgf_raised(ex)) { json_free(remote); free(requested); return ex; }
        bool exists = ex->type == JSON_BOOL && ex->bool_val;
        json_free(ex);
        if (!exists) {
            const char *slash = strchr(requested, '/');
            size_t rn = (size_t)(slash - requested);
            char *maybe_remote = malloc(rn + 1);
            if (maybe_remote) { memcpy(maybe_remote, requested, rn); maybe_remote[rn] = '\0'; }
            const char *maybe_branch = slash + 1;
            json_t *ga = json_array();
            json_append(ga, json_string("remote"));
            json_append(ga, json_string("get-url"));
            json_append(ga, json_string(maybe_remote ? maybe_remote : ""));
            json_t *gu = h_git_line(io, root, ga, true);
            json_free(ga);
            if (wgf_raised(gu)) { free(maybe_remote); json_free(remote); free(requested); return gu; }
            if (jtruth(gu)) {
                json_t *ft = h_fetch_tracking_ref(io, root, maybe_remote ? maybe_remote : "", maybe_branch);
                if (wgf_raised(ft)) {
                    json_free(gu); free(maybe_remote); json_free(remote); free(requested);
                    return ft;
                }
                if (ft->type == JSON_BOOL && ft->bool_val) {
                    json_free(remote);
                    remote = json_string(maybe_remote ? maybe_remote : "");
                    fetched = true;
                }
                json_free(ft);
            }
            json_free(gu);
            free(maybe_remote);
        }
    }
    const char *existing;
    if (jtruth(remote)) {
        const char *slash = strchr(requested, '/');
        if (!slash) {
            json_free(remote); free(requested);
            return raise_desc("IndexError", "list index out of range");
        }
        existing = slash + 1;
    } else {
        existing = requested;
    }
    if (!jtruth(remote)) {
        json_t *db = h_default_branch(io, root, true);
        if (wgf_raised(db)) { json_free(remote); free(requested); return db; }
        if (strcmp(existing, jstr(db)) == 0) {
            json_free(db);
            json_t *sw = json_array();
            json_append(sw, json_string("switch"));
            json_append(sw, json_string(existing));
            json_t *ok = h_git_ok(io, root, sw);
            json_free(sw);
            if (wgf_raised(ok)) { json_free(remote); free(requested); return ok; }
            json_free(ok);
            json_t *out = json_object();
            json_set(out, "path", json_copy(root));
            json_set_hdr(out, "branch", existing);
            json_set(out, "repoRoot", json_copy(root));
            json_free(remote); free(requested);
            return out;
        }
        json_free(db);
    }

    json_t *slug = h_slugify(json_string(existing));
    char *base_path = wgf_os_path_join3(jstr(root), ".worktrees", jstr(slug));
    json_free(slug);
    json_t *ua = json_object();
    json_set_hdr(ua, "base", base_path ? base_path : "");
    free(base_path);
    json_t *target = wgf_call(io, "unique_dir", ua);
    json_free(ua);
    if (wgf_raised(target)) { json_free(remote); free(requested); return target; }

    if (jtruth(remote)) {
        if (!fetched) {
            json_t *ft = h_fetch_tracking_ref(io, root, jstr(remote), existing);
            if (wgf_raised(ft)) { json_free(target); json_free(remote); free(requested); return ft; }
            fetched = ft->type == JSON_BOOL && ft->bool_val;
            json_free(ft);
        }
        json_t *wa = json_array();
        json_append(wa, json_string("worktree"));
        json_append(wa, json_string("add"));
        json_append(wa, json_string("--track"));
        json_append(wa, json_string("-b"));
        json_append(wa, json_string(existing));
        json_append(wa, json_copy(target));
        json_append(wa, json_string(requested));
        json_t *t = h_git_write(io, root, wa, WGF_GIT_TIMEOUT);
        json_free(wa);
        if (wgf_raised(t)) { json_free(target); json_free(remote); free(requested); return t; }
        json_t *code = json_get(t, 0);
        bool failed = !(j_is_num(code) && code->num_val == 0.0);
        json_free(t);
        if (failed) {
            json_t *wa2 = json_array();
            json_append(wa2, json_string("worktree"));
            json_append(wa2, json_string("add"));
            json_append(wa2, json_string("-b"));
            json_append(wa2, json_string(existing));
            json_append(wa2, json_copy(target));
            json_append(wa2, json_string(requested));
            json_t *ok = h_git_ok(io, root, wa2);
            json_free(wa2);
            if (wgf_raised(ok)) { json_free(target); json_free(remote); free(requested); return ok; }
            json_free(ok);
            if (fetched) {
                json_t *sb = json_array();
                json_append(sb, json_string("remote"));
                json_append(sb, json_string("set-branches"));
                json_append(sb, json_string("--add"));
                json_append(sb, json_copy(remote));
                json_append(sb, json_string(existing));
                json_t *t2 = h_git_write(io, root, sb, WGF_GIT_TIMEOUT);
                json_free(sb);
                if (wgf_raised(t2)) { json_free(target); json_free(remote); free(requested); return t2; }
                json_free(t2);
                size_t need = strlen(requested) + 32;
                char *upflag = malloc(need);
                if (upflag) snprintf(upflag, need, "--set-upstream-to=%s", requested);
                json_t *up = json_array();
                json_append(up, json_string("branch"));
                json_append(up, json_string(upflag ? upflag : ""));
                json_append(up, json_string(existing));
                free(upflag);
                json_t *t3 = h_git_write(io, root, up, WGF_GIT_TIMEOUT);
                json_free(up);
                if (wgf_raised(t3)) { json_free(target); json_free(remote); free(requested); return t3; }
                json_free(t3);
            }
        }
    } else {
        json_t *wa = json_array();
        json_append(wa, json_string("worktree"));
        json_append(wa, json_string("add"));
        json_append(wa, json_copy(target));
        json_append(wa, json_string(existing));
        json_t *ok = h_git_ok(io, root, wa);
        json_free(wa);
        if (wgf_raised(ok)) { json_free(target); json_free(remote); free(requested); return ok; }
        json_free(ok);
    }
    json_t *out = json_object();
    json_set(out, "path", target);   /* adopts */
    json_set_hdr(out, "branch", existing);
    json_set(out, "repoRoot", json_copy(root));
    json_free(remote);
    free(requested);
    return out;
}

/* ── worktree_add ──────────────────────────────────────────────────────── */
static json_t *h_worktree_add(wgf_io_t *io, const json_t *cwd, const json_t *options) {
    json_t *er = h_ensure_repo(io, cwd);
    if (wgf_raised(er)) return er;
    json_free(er);
    json_t *root = h_main_root(io, cwd);
    if (wgf_raised(root)) return root;

    json_t *empty_opts = NULL;
    const json_t *opts = options;
    if (!jtruth(opts)) {
        empty_opts = json_object();
        opts = empty_opts;
    }
    json_t *eb = json_obj_get(opts, "existingBranch");
    if (jtruth(eb)) {
        json_t *result = h_worktree_for_existing(io, root, eb);
        if (wgf_raised(result)) { json_free(root); json_free(empty_opts); return result; }
        json_t *rp = json_obj_get(result, "path");
        if (!rp || strcmp(jstr(rp), jstr(root)) != 0) {
            json_t *ra = json_object();
            json_set(ra, "root", json_copy(root));
            json_t *rec = wgf_call(io, "record_worktree_repo", ra);
            json_free(ra);
            if (wgf_raised(rec)) { json_free(result); json_free(root); json_free(empty_opts); return rec; }
            json_free(rec);
        }
        json_free(root);
        json_free(empty_opts);
        return result;
    }

    /* slug = _slugify(options.get("name") or f"work-{os.urandom(4).hex()}") */
    json_t *name = json_obj_get(opts, "name");
    json_t *slug_arg;
    if (jtruth(name)) {
        slug_arg = json_copy(name);
    } else {
        json_t *ua = json_object();
        json_set(ua, "n", json_number(4));
        json_t *ur = wgf_call(io, "os.urandom", ua);
        json_free(ua);
        if (wgf_raised(ur)) { json_free(root); json_free(empty_opts); return ur; }
        const char *hex = jstr(ur);
        size_t need = strlen(hex) + 16;
        char *buf = malloc(need);
        if (buf) snprintf(buf, need, "work-%s", hex);
        slug_arg = json_string(buf ? buf : "work-");
        free(buf);
        json_free(ur);
    }
    json_t *slug = h_slugify(slug_arg);
    json_free(slug_arg);

    /* branch = _sanitize_branch(options.get("branch") or "") or f"hermes/{slug}" */
    json_t *bo = json_obj_get(opts, "branch");
    json_t *sa = json_object();
    json_set(sa, "name", jtruth(bo) ? json_copy(bo) : json_string(""));
    json_t *san = wgf_call(io, "sanitize_branch", sa);
    json_free(sa);
    if (wgf_raised(san)) { json_free(slug); json_free(root); json_free(empty_opts); return san; }
    json_t *branch;
    if (jtruth(san)) {
        branch = san;
    } else {
        json_free(san);
        size_t need = strlen(jstr(slug)) + 16;
        char *buf = malloc(need);
        if (buf) snprintf(buf, need, "hermes/%s", jstr(slug));
        branch = json_string(buf ? buf : "hermes/");
        free(buf);
    }

    char *base_path = wgf_os_path_join3(jstr(root), ".worktrees", jstr(slug));
    json_free(slug);
    json_t *ua = json_object();
    json_set_hdr(ua, "base", base_path ? base_path : "");
    free(base_path);
    json_t *target = wgf_call(io, "unique_dir", ua);
    json_free(ua);
    if (wgf_raised(target)) { json_free(branch); json_free(root); json_free(empty_opts); return target; }

    json_t *args = json_array();
    json_append(args, json_string("worktree"));
    json_append(args, json_string("add"));
    json_append(args, json_string("-b"));
    json_append(args, json_copy(branch));
    json_append(args, json_copy(target));
    json_t *base = json_obj_get(opts, "base");
    if (jtruth(base)) {
        char *bs = py_str_of(base);
        if (str_starts(bs ? bs : "", "origin/")) {
            const char *rb = bs + 7;
            json_t *cr = json_array();
            json_append(cr, json_string("check-ref-format"));
            json_append(cr, json_string("--branch"));
            json_append(cr, json_string(rb));
            json_t *t = h_git_write(io, root, cr, WGF_GIT_TIMEOUT);
            json_free(cr);
            if (wgf_raised(t)) { free(bs); json_free(args); json_free(target); json_free(branch); json_free(root); json_free(empty_opts); return t; }
            json_t *code = json_get(t, 0);
            bool ok = j_is_num(code) && code->num_val == 0.0;
            json_free(t);
            if (ok) {
                json_t *ft = h_fetch_tracking_ref(io, root, "origin", rb);
                if (wgf_raised(ft)) { free(bs); json_free(args); json_free(target); json_free(branch); json_free(root); json_free(empty_opts); return ft; }
                json_free(ft);
            }
            json_append(args, json_string("--no-track"));
        }
        json_append(args, json_string(bs ? bs : ""));
        free(bs);
    }
    json_t *t = h_git_write(io, root, args, WGF_GIT_TIMEOUT);
    json_free(args);
    if (wgf_raised(t)) { json_free(target); json_free(branch); json_free(root); json_free(empty_opts); return t; }
    json_t *code = json_get(t, 0);
    if (!(j_is_num(code) && code->num_val == 0.0)) {
        json_t *err = json_get(t, 2);
        char *lower;
        if (j_is_null(err)) {
            lower = strdup("");
        } else if (j_is_str(err)) {
            lower = py_lower(jstr(err));
        } else {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'lower'", py_type_name(err));
            json_free(t); json_free(target); json_free(branch); json_free(root); json_free(empty_opts);
            return raise_desc("AttributeError", msg);
        }
        if (!str_contains(lower ? lower : "", "already exists")) {
            json_t *r;
            if (!j_is_str(err)) {
                r = attr_strip_err(err);
            } else {
                char *stripped = py_strip(jstr(err));
                r = raise_desc("RuntimeError", (stripped && stripped[0]) ? stripped : "git worktree add failed");
                free(stripped);
            }
            free(lower);
            json_free(t); json_free(target); json_free(branch); json_free(root); json_free(empty_opts);
            return r;
        }
        free(lower);
        json_t *wa = json_array();
        json_append(wa, json_string("worktree"));
        json_append(wa, json_string("add"));
        json_append(wa, json_copy(target));
        json_append(wa, json_copy(branch));
        json_t *ok = h_git_ok(io, root, wa);
        json_free(wa);
        if (wgf_raised(ok)) { json_free(t); json_free(target); json_free(branch); json_free(root); json_free(empty_opts); return ok; }
        json_free(ok);
    }
    json_free(t);

    json_t *ra = json_object();
    json_set(ra, "root", json_copy(root));
    json_t *rec = wgf_call(io, "record_worktree_repo", ra);
    json_free(ra);
    if (wgf_raised(rec)) { json_free(target); json_free(branch); json_free(root); json_free(empty_opts); return rec; }
    json_free(rec);

    json_t *out = json_object();
    json_set(out, "path", target);   /* adopts */
    json_set(out, "branch", branch); /* adopts */
    json_set(out, "repoRoot", root); /* adopts */
    json_free(empty_opts);
    return out;
}

/* ── PoP-claimed targets (json in -> json out) ─────────────────────────── */

/* PoP: wgf__run @ hermes_cli/web_git.py:_run */
json_t *wgf__run(wgf_io_t *io, json_t *in) {
    return h_run(io, json_obj_get(in, "argv"), json_obj_get(in, "cwd"),
                 json_get_num(in, "timeout", WGF_GIT_TIMEOUT), json_obj_get(in, "env"));
}

/* PoP: wgf__git_in @ hermes_cli/web_git.py:_git_in */
json_t *wgf__git_in(wgf_io_t *io, json_t *in) {
    return h_git_in(io, json_obj_get(in, "cwd"), json_obj_get(in, "args"),
                    json_get_num(in, "timeout", WGF_GIT_TIMEOUT), json_obj_get(in, "env"));
}

/* PoP: wgf__git @ hermes_cli/web_git.py:_git */
json_t *wgf__git(wgf_io_t *io, json_t *in) {
    return h_git(io, json_obj_get(in, "cwd"), json_obj_get(in, "args"),
                 json_get_num(in, "timeout", WGF_GIT_TIMEOUT));
}

/* PoP: wgf__git_write @ hermes_cli/web_git.py:_git_write */
json_t *wgf__git_write(wgf_io_t *io, json_t *in) {
    return h_git_write(io, json_obj_get(in, "cwd"), json_obj_get(in, "args"),
                       json_get_num(in, "timeout", WGF_GIT_TIMEOUT));
}

/* PoP: wgf__git_out @ hermes_cli/web_git.py:_git_out */
json_t *wgf__git_out(wgf_io_t *io, json_t *in) {
    bool write = strcmp(json_get_str(in, "run", "read"), "write") == 0;
    return h_git_out(io, json_obj_get(in, "cwd"), json_obj_get(in, "args"), write);
}

/* PoP: wgf__git_ok @ hermes_cli/web_git.py:_git_ok */
json_t *wgf__git_ok(wgf_io_t *io, json_t *in) {
    return h_git_ok(io, json_obj_get(in, "cwd"), json_obj_get(in, "args"));
}

/* PoP: wgf_resolve_rename_path @ hermes_cli/web_git.py:resolve_rename_path */
json_t *wgf_resolve_rename_path(wgf_io_t *io, json_t *in) {
    (void)io;
    return h_resolve_rename_path(json_obj_get(in, "raw"));
}

/* PoP: wgf__numstat @ hermes_cli/web_git.py:_numstat */
json_t *wgf__numstat(wgf_io_t *io, json_t *in) {
    return h_numstat(io, json_obj_get(in, "cwd"), json_obj_get(in, "args"));
}

/* PoP: wgf__branch_base @ hermes_cli/web_git.py:_branch_base */
json_t *wgf__branch_base(wgf_io_t *io, json_t *in) {
    return h_branch_base(io, json_obj_get(in, "cwd"));
}

/* PoP: wgf__default_branch_name @ hermes_cli/web_git.py:_default_branch_name */
json_t *wgf__default_branch_name(wgf_io_t *io, json_t *in) {
    return h_default_branch_name(io, json_obj_get(in, "cwd"));
}

/* PoP: wgf_repo_status @ hermes_cli/web_git.py:repo_status */
json_t *wgf_repo_status(wgf_io_t *io, json_t *in) {
    return h_repo_status(io, json_obj_get(in, "cwd"));
}

/* PoP: wgf_review_list @ hermes_cli/web_git.py:review_list */
json_t *wgf_review_list(wgf_io_t *io, json_t *in) {
    return h_review_list(io, json_obj_get(in, "cwd"), json_get_str(in, "scope", "working"),
                         json_obj_get(in, "base_ref"));
}

/* PoP: wgf_review_diff @ hermes_cli/web_git.py:review_diff */
json_t *wgf_review_diff(wgf_io_t *io, json_t *in) {
    bool staged = jtruth(json_obj_get(in, "staged"));
    return h_review_diff(io, json_obj_get(in, "cwd"), json_get_str(in, "file_path", ""),
                         json_get_str(in, "scope", "working"), json_obj_get(in, "base_ref"), staged);
}

/* PoP: wgf__review_commit_env @ hermes_cli/web_git.py:_review_commit_env */
json_t *wgf__review_commit_env(wgf_io_t *io, json_t *in) {
    return h_review_commit_env(io, json_obj_get(in, "cwd"));
}

/* PoP: wgf_review_commit @ hermes_cli/web_git.py:review_commit */
json_t *wgf_review_commit(wgf_io_t *io, json_t *in) {
    bool push = jtruth(json_obj_get(in, "push"));
    return h_review_commit(io, json_obj_get(in, "cwd"), json_get_str(in, "message", ""), push);
}

/* PoP: wgf_review_commit_context @ hermes_cli/web_git.py:review_commit_context */
json_t *wgf_review_commit_context(wgf_io_t *io, json_t *in) {
    json_t *pa = json_obj_get(in, "path_allowed");
    bool has_pa = pa && pa->type == JSON_BOOL && pa->bool_val;
    return h_review_commit_context(io, json_obj_get(in, "cwd"), has_pa);
}

/* PoP: wgf_worktree_add @ hermes_cli/web_git.py:worktree_add */
json_t *wgf_worktree_add(wgf_io_t *io, json_t *in) {
    return h_worktree_add(io, json_obj_get(in, "cwd"), json_obj_get(in, "options"));
}

/* PoP: wgf__worktree_for_existing @ hermes_cli/web_git.py:_worktree_for_existing */
json_t *wgf__worktree_for_existing(wgf_io_t *io, json_t *in) {
    return h_worktree_for_existing(io, json_obj_get(in, "root"), json_obj_get(in, "raw_name"));
}
