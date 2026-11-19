/*
 * platforms_helpers_flow.c — platform-helper FLOW cores of
 * gateway/platforms/helpers.py (hermes-agent): message formatting/chunking
 * decision chains (clarify numbering, choice shortening, link-target keep,
 * table-aware split points, UTF-8 reply-file parts), platform capability
 * gates (Discord obfuscated channels, reply-document sender selection,
 * reply message caps) and media upload budget branches.
 *
 * Decision sequences only: every live I/O / injected callable (escape,
 * len_fn, fits, reply_message_count, os.stat, os.fstat) enters through ONE
 * universal seam: phf_call(ctx, name, args). The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle
 * installs recording stubs emitting the identical event shape
 * {"name", "args", "ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and phf_-prefixed.
 *
 * Representation contract (shared with tests/oracle_platforms_helpers_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc)} object
 *   (OSError adds errno, message "[Errno N] msg").
 * - A Python float in an isinstance/type-sensitive slot travels as
 *   {"__float__": true, "v": <number>}; plain JSON numbers are ints, JSON
 *   booleans are bools (bool IS an int instance, but type() is not int).
 * - bytes returns (reply_file_parts) project to latin-1 strings: byte b is
 *   emitted as the UTF-8 encoding of codepoint U+00b.
 * - Bound-method returns (reply_document_sender) project to the fn name.
 * - len_fn/escape/fits params: the literal "seam" selects the recording
 *   stub; null/absent selects builtin codepoint len / identity.
 * - Adapter doubles: {"hooks": {...}, "attrs": {...}}; send_document
 *   "base" (default) models inheriting BasePlatformAdapter.send_document
 *   (the identity gate's negative), "own" a native override, "null" None.
 * - Media sources: JSON string = path; {"kind":"pathlike","path":...};
 *   {"kind":"file","fileno":N|"fileno_raise":spec}; anything else has no
 *   .fileno (AttributeError -> 0). stat/fstat scripts return {"st_size":N};
 *   a null return models the real .st_size AttributeError (propagates for
 *   paths, caught for file handles).
 * - logger calls are silent on both sides (not traced).
 * - Fixture corpus: strip/lstrip/rstrip fold ASCII whitespace only; str()
 *   of non-string choices covers ints/bools/None only; reply_file_parts
 *   limits are >= 4 for non-ASCII text (live loops forever when a forced
 *   cut backs up to 0 mid-character).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHF_ELL "\xe2\x80\xa6" /* U+2026 HORIZONTAL ELLIPSIS */
#define PHF_MIN_MEDIA_UPLOAD_RATE 131072.0 /* 128 * 1024 B/s */
#define PHF_MEDIA_UPLOAD_DEADLINE_MAX 3600.0
#define PHF_DISCORD_OBFUSCATED_FLAG (1LL << 17)
#define PHF_DISCORD_HIDDEN_NAME "___hidden___"

/* ── universal seam context ── */
typedef struct phf_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} phf_ctx_t;

/* ── basic helpers ── */
static bool phf_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool phf_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *phf_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool phf_str_eq(const json_t *v, const char *s) {
    const char *t = phf_str(v);
    return t && s && strcmp(t, s) == 0;
}

static bool phf_is_oserror_type(const char *t) {
    static const char *fam[] = {
        "OSError", "IOError", "TimeoutError", "PermissionError",
        "FileNotFoundError", "FileExistsError", "BlockingIOError",
        "ConnectionError", "ConnectionResetError", "BrokenPipeError",
        "InterruptedError", "NotADirectoryError", "IsADirectoryError",
        "ChildProcessError", "ProcessLookupError",
    };
    for (size_t i = 0; i < sizeof(fam) / sizeof(fam[0]); i++)
        if (strcmp(t, fam[i]) == 0) return true;
    return false;
}

static bool phf_is_oserror(const json_t *r) {
    return phf_is_raise(r) && phf_is_oserror_type(json_get_str(r, "__raise__", ""));
}

static bool phf_is_valueerror(const json_t *r) {
    return phf_is_raise(r) && strcmp(json_get_str(r, "__raise__", ""), "ValueError") == 0;
}

static bool phf_is_attributeerror(const json_t *r) {
    return phf_is_raise(r) && strcmp(json_get_str(r, "__raise__", ""), "AttributeError") == 0;
}

static json_t *phf_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *phf_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && phf_is_oserror_type(type)) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = phf_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return phf_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return
 * is a fresh owned node (scripted value copy or raise descriptor). */
static json_t *phf_call(phf_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = phf_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = phf_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = phf_make_exc(raise);
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* ── string helpers (Python str semantics on the fixture corpus) ── */
static char *phf_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static bool phf_is_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.strip(): ASCII whitespace both ends (fixtures fold ASCII only). */
static char *phf_strip(const char *s) {
    if (!s) s = "";
    while (*s && phf_is_ascii_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && phf_is_ascii_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* str.lstrip(): drop leading ASCII whitespace. */
static char *phf_lstrip(const char *s) {
    if (!s) s = "";
    while (*s && phf_is_ascii_ws((unsigned char)*s)) s++;
    return phf_strdup(s);
}

/* str.rstrip(): drop trailing ASCII whitespace. */
static char *phf_rstrip(const char *s) {
    if (!s) s = "";
    const char *end = s + strlen(s);
    while (end > s && phf_is_ascii_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* str.rstrip('\n'): drop trailing newlines ONLY. */
static char *phf_rstrip_nl(const char *s) {
    if (!s) s = "";
    const char *end = s + strlen(s);
    while (end > s && end[-1] == '\n') end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* str.lower(): ASCII fold (fixture targets fold ASCII only). */
static char *phf_lower(const char *s) {
    char *o = phf_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* UTF-8 codepoint count (Python len counts characters). */
static size_t phf_utf8_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xc0) != 0x80) n++;
    return n;
}

/* byte pointer at codepoint index i (clamped to [0, len]). */
static const char *phf_cp_ptr(const char *s, long long i) {
    if (!s) return "";
    if (i < 0) i = 0;
    const char *p = s;
    while (i > 0 && *p) {
        p++;
        while (*p && ((unsigned char)*p & 0xc0) == 0x80) p++;
        i--;
    }
    return p;
}

/* Python s[a:b] on codepoints (clamped; a > b -> ""). Caller frees. */
static char *phf_slice(const char *s, long long a, long long b) {
    if (!s) s = "";
    long long n = (long long)phf_utf8_len(s);
    if (a < 0) a = 0;
    if (a > n) a = n;
    if (b < a) b = a;
    if (b > n) b = n;
    const char *pa = phf_cp_ptr(s, a);
    const char *pb = phf_cp_ptr(s, b);
    size_t len = (size_t)(pb - pa);
    char *o = malloc(len + 1);
    if (!o) return NULL;
    memcpy(o, pa, len);
    o[len] = '\0';
    return o;
}

/* Python s[i:i+1]: one codepoint, or "" at/past the end. Caller frees. */
static char *phf_slice1(const char *s, long long i) {
    return phf_slice(s, i, i + 1);
}

/* decode the codepoint at byte p (valid UTF-8 fixture corpus). */
static unsigned int phf_decode_cp(const unsigned char *p) {
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xe0) == 0xc0)
        return ((unsigned)(p[0] & 0x1f) << 6) | (p[1] & 0x3f);
    if ((p[0] & 0xf0) == 0xe0)
        return ((unsigned)(p[0] & 0x0f) << 12) | ((unsigned)(p[1] & 0x3f) << 6) | (p[2] & 0x3f);
    return ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3f) << 12)
        | ((unsigned)(p[2] & 0x3f) << 6) | (p[3] & 0x3f);
}

/* os.path.commonprefix([a, b]): codepoint count of the shared prefix. */
static size_t phf_commonprefix_len(const char *a, const char *b) {
    size_t n = 0;
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    while (*pa && *pb) {
        unsigned int ca = phf_decode_cp(pa), cb = phf_decode_cp(pb);
        if (ca != cb) break;
        size_t la = 1, lb = 1;
        while (pa[la] && (pa[la] & 0xc0) == 0x80) la++;
        while (pb[lb] && (pb[lb] & 0xc0) == 0x80) lb++;
        if (la != lb || memcmp(pa, pb, la) != 0) break;
        pa += la;
        pb += lb;
        n++;
    }
    return n;
}

/* str.split('\n'): "" -> [""], trailing newline keeps a final "". */
typedef struct {
    char **lines;
    size_t count;
} phf_lines_t;

static phf_lines_t phf_split_nl(const char *s) {
    phf_lines_t out = {NULL, 0};
    if (!s) s = "";
    size_t cap = 8;
    out.lines = malloc(cap * sizeof(char *));
    if (!out.lines) return out;
    const char *start = s;
    for (const char *p = s; ; p++) {
        if (*p == '\n' || *p == '\0') {
            if (out.count == cap) {
                cap *= 2;
                char **nl = realloc(out.lines, cap * sizeof(char *));
                if (!nl) return out;
                out.lines = nl;
            }
            size_t len = (size_t)(p - start);
            char *line = malloc(len + 1);
            if (!line) return out;
            memcpy(line, start, len);
            line[len] = '\0';
            out.lines[out.count++] = line;
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    return out;
}

static void phf_lines_free(phf_lines_t *ls) {
    if (!ls) return;
    for (size_t i = 0; i < ls->count; i++) free(ls->lines[i]);
    free(ls->lines);
    ls->lines = NULL;
    ls->count = 0;
}

static char *phf_join_nl(char *const *lines, size_t n) {
    size_t total = 0;
    for (size_t i = 0; i < n; i++) total += strlen(lines[i]) + 1;
    char *o = malloc(total + 1);
    if (!o) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (i) o[k++] = '\n';
        size_t len = strlen(lines[i]);
        memcpy(o + k, lines[i], len);
        k += len;
    }
    o[k] = '\0';
    return o;
}

/* str.rfind('\n', 0, end): last codepoint index < end, else -1. */
static long long phf_rfind_nl(const char *s, long long end) {
    long long n = (long long)phf_utf8_len(s);
    if (end > n) end = n;
    if (end < 0) end = 0;
    const char *p = s;
    long long idx = 0, found = -1;
    while (idx < end && *p) {
        if (*p == '\n') found = idx;
        p++;
        while (*p && ((unsigned char)*p & 0xc0) == 0x80) p++;
        idx++;
    }
    return found;
}

/* str.find('\n', from): first codepoint index >= from, else -1. */
static long long phf_find_nl(const char *s, long long from) {
    long long n = (long long)phf_utf8_len(s);
    if (from < 0) from = 0;
    const char *p = phf_cp_ptr(s, from);
    long long idx = from;
    while (idx < n && *p) {
        if (*p == '\n') return idx;
        p++;
        while (*p && ((unsigned char)*p & 0xc0) == 0x80) p++;
        idx++;
    }
    return -1;
}

/* Python floor division for b > 0 (C / truncates toward zero). */
static long long phf_fdiv(long long a, long long b) {
    if (a >= 0) return a / b;
    return -((-a + b - 1) / b);
}

/* str() projection for choice entries: strings verbatim, integral numbers
 * as ints, bools as True/False, None as "None" (fixture corpus). */
static char *phf_str_of_json(const json_t *v) {
    if (!v || v->type == JSON_NULL) return phf_strdup("None");
    switch (v->type) {
        case JSON_STRING: return phf_strdup(v->str_val);
        case JSON_BOOL: return phf_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return phf_strdup(buf);
        }
        default: return NULL; /* lists/dicts: outside the fixture corpus */
    }
}

/* ── East-Asian wide/fullwidth table (unicodedata 16.0.0, W+F ranges) ── */
static const struct { unsigned int lo, hi; } PHF_EAW_WF[] = {
    {0x1100, 0x115F}, {0x231A, 0x231B}, {0x2329, 0x232A},
    {0x23E9, 0x23EC}, {0x23F0, 0x23F0}, {0x23F3, 0x23F3},
    {0x25FD, 0x25FE}, {0x2614, 0x2615}, {0x2630, 0x2637},
    {0x2648, 0x2653}, {0x267F, 0x267F}, {0x268A, 0x268F},
    {0x2693, 0x2693}, {0x26A1, 0x26A1}, {0x26AA, 0x26AB},
    {0x26BD, 0x26BE}, {0x26C4, 0x26C5}, {0x26CE, 0x26CE},
    {0x26D4, 0x26D4}, {0x26EA, 0x26EA}, {0x26F2, 0x26F3},
    {0x26F5, 0x26F5}, {0x26FA, 0x26FA}, {0x26FD, 0x26FD},
    {0x2705, 0x2705}, {0x270A, 0x270B}, {0x2728, 0x2728},
    {0x274C, 0x274C}, {0x274E, 0x274E}, {0x2753, 0x2755},
    {0x2757, 0x2757}, {0x2795, 0x2797}, {0x27B0, 0x27B0},
    {0x27BF, 0x27BF}, {0x2B1B, 0x2B1C}, {0x2B50, 0x2B50},
    {0x2B55, 0x2B55}, {0x2E80, 0x2E99}, {0x2E9B, 0x2EF3},
    {0x2F00, 0x2FD5}, {0x2FF0, 0x303E}, {0x3041, 0x3096},
    {0x3099, 0x30FF}, {0x3105, 0x312F}, {0x3131, 0x318E},
    {0x3190, 0x31E5}, {0x31EF, 0x321E}, {0x3220, 0x3247},
    {0x3250, 0xA48C}, {0xA490, 0xA4C6}, {0xA960, 0xA97C},
    {0xAC00, 0xD7A3}, {0xF900, 0xFAFF}, {0xFE10, 0xFE19},
    {0xFE30, 0xFE52}, {0xFE54, 0xFE66}, {0xFE68, 0xFE6B},
    {0xFF01, 0xFF60}, {0xFFE0, 0xFFE6}, {0x16FE0, 0x16FE4},
    {0x16FF0, 0x16FF1}, {0x17000, 0x187F7}, {0x18800, 0x18CD5},
    {0x18CFF, 0x18D08}, {0x1AFF0, 0x1AFF3}, {0x1AFF5, 0x1AFFB},
    {0x1AFFD, 0x1AFFE}, {0x1B000, 0x1B122}, {0x1B132, 0x1B132},
    {0x1B150, 0x1B152}, {0x1B155, 0x1B155}, {0x1B164, 0x1B167},
    {0x1B170, 0x1B2FB}, {0x1D300, 0x1D356}, {0x1D360, 0x1D376},
    {0x1F004, 0x1F004}, {0x1F0CF, 0x1F0CF}, {0x1F18E, 0x1F18E},
    {0x1F191, 0x1F19A}, {0x1F200, 0x1F202}, {0x1F210, 0x1F23B},
    {0x1F240, 0x1F248}, {0x1F250, 0x1F251}, {0x1F260, 0x1F265},
    {0x1F300, 0x1F320}, {0x1F32D, 0x1F335}, {0x1F337, 0x1F37C},
    {0x1F37E, 0x1F393}, {0x1F3A0, 0x1F3CA}, {0x1F3CF, 0x1F3D3},
    {0x1F3E0, 0x1F3F0}, {0x1F3F4, 0x1F3F4}, {0x1F3F8, 0x1F43E},
    {0x1F440, 0x1F440}, {0x1F442, 0x1F4FC}, {0x1F4FF, 0x1F53D},
    {0x1F54B, 0x1F54E}, {0x1F550, 0x1F567}, {0x1F57A, 0x1F57A},
    {0x1F595, 0x1F596}, {0x1F5A4, 0x1F5A4}, {0x1F5FB, 0x1F64F},
    {0x1F680, 0x1F6C5}, {0x1F6CC, 0x1F6CC}, {0x1F6D0, 0x1F6D2},
    {0x1F6D5, 0x1F6D7}, {0x1F6DC, 0x1F6DF}, {0x1F6EB, 0x1F6EC},
    {0x1F6F4, 0x1F6FC}, {0x1F7E0, 0x1F7EB}, {0x1F7F0, 0x1F7F0},
    {0x1F90C, 0x1F93A}, {0x1F93C, 0x1F945}, {0x1F947, 0x1F9FF},
    {0x1FA70, 0x1FA7C}, {0x1FA80, 0x1FA89}, {0x1FA8F, 0x1FAC6},
    {0x1FACE, 0x1FADC}, {0x1FADF, 0x1FAE9}, {0x1FAF0, 0x1FAF8},
    {0x20000, 0x2FFFD}, {0x30000, 0x3FFFD},
};
#define PHF_EAW_N (sizeof(PHF_EAW_WF) / sizeof(PHF_EAW_WF[0]))

static bool phf_eaw_wf(unsigned int cp) {
    size_t lo = 0, hi = PHF_EAW_N;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (cp < PHF_EAW_WF[mid].lo) hi = mid;
        else if (cp > PHF_EAW_WF[mid].hi) lo = mid + 1;
        else return true;
    }
    return false;
}

/* internal core helper; PoP credit lives on the dispatched case wrapper */
static size_t phf_display_width_str(const char *s) {
    size_t w = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ) {
        unsigned int cp = phf_decode_cp(p);
        w += phf_eaw_wf(cp) ? 2 : 1;
        p++;
        while (*p && (*p & 0xc0) == 0x80) p++;
    }
    return w;
}

/* ── table leaves ── */

/* is_table_row leaf ('|' in line.strip()); the named fn is already claimed
 * by platforms_helpers.c — inlined here for self-containment, no re-port. */
static bool phf_is_table_row(const char *line) {
    char *s = phf_strip(line);
    bool has = s && strchr(s, '|') != NULL;
    free(s);
    return has;
}

/* TABLE_SEPARATOR_RE: ^\s*\|?\s*:?-+:?\s*(?:\|\s*:?-+:?\s*){1,}\|?\s*$
 * Hand-matched: optional outer pipes, >= 2 dash cells (optional alignment
 * colons), at least one internal '|'. ASCII whitespace corpus. */
static bool phf_sep_cell_ok(const char *cell) {
    char *s = phf_strip(cell);
    if (!s) return false;
    size_t i = 0, n = strlen(s);
    if (i < n && s[i] == ':') i++;
    size_t d0 = i;
    while (i < n && s[i] == '-') i++;
    if (i == d0) { free(s); return false; }
    if (i < n && s[i] == ':') i++;
    bool ok = (i == n);
    free(s);
    return ok;
}

static bool phf_sep_match(const char *line) {
    char *s = phf_strip(line);
    if (!s) return false;
    size_t n = strlen(s);
    size_t start = (s[0] == '|') ? 1 : 0;
    size_t end = n;
    if (end > start && s[end - 1] == '|') end--; /* optional trailing pipe */
    size_t cells = 0;
    bool ok = true;
    size_t cs = start;
    for (size_t i = start; i <= end; i++) {
        if (i == end || s[i] == '|') {
            char *cell = malloc(i - cs + 1);
            if (!cell) { free(s); return false; }
            memcpy(cell, s + cs, i - cs);
            cell[i - cs] = '\0';
            if (!phf_sep_cell_ok(cell)) ok = false;
            free(cell);
            cells++;
            cs = i + 1;
            if (i == end) break;
        }
    }
    free(s);
    return ok && cells >= 2;
}

/* PoP: phf_table_header_above @ gateway/platforms/helpers.py:_table_header_above */
static long long phf_table_header_above(char *const *lines, size_t n, long long i) {
    while (i >= 1 && (size_t)i < n && phf_is_table_row(lines[i])) {
        if (phf_sep_match(lines[i]) && phf_is_table_row(lines[i - 1]))
            return i - 1;
        i--;
    }
    return -1;
}

/* PoP: phf_is_pipe_row @ gateway/platforms/helpers.py:_is_pipe_row */
static bool phf_is_pipe_row(const char *line) {
    char *s = phf_strip(line);
    bool ok = false;
    if (s && s[0] == '|') {
        size_t n = strlen(s);
        ok = n > 0 && s[n - 1] == '|';
    }
    free(s);
    return ok;
}

/* PoP: phf_split_inside_table @ gateway/platforms/helpers.py:_split_inside_table */
static bool phf_split_inside_table(const char *head, const char *tail) {
    char *h = phf_rstrip_nl(head);
    phf_lines_t hl = phf_split_nl(h);
    bool head_row = hl.count > 0 && phf_is_table_row(hl.lines[hl.count - 1]);
    phf_lines_free(&hl);
    free(h);
    if (!head_row) return false;
    char *t = phf_lstrip(tail);
    phf_lines_t tl = phf_split_nl(t);
    bool tail_row = tl.count > 0 && phf_is_table_row(tl.lines[0]);
    phf_lines_free(&tl);
    free(t);
    return tail_row;
}

/* ── injected callable seams (len_fn / escape) ── */

/* len_fn(text): "seam" marker -> traced seam; else builtin codepoint len.
 * Returns an owned number node, or a raise node to propagate. */
static json_t *phf_measure(phf_ctx_t *ctx, const json_t *args, const char *text) {
    if (phf_str_eq(json_obj_get(args, "len_fn"), "seam")) {
        json_t *a = json_object();
        json_set_hdr(a, "text", text);
        json_t *r = phf_call(ctx, "len_fn", a);
        if (phf_is_raise(r)) return r;
        double v = (r && r->type == JSON_NUMBER) ? r->num_val : 0.0;
        json_free(r);
        return json_number(v);
    }
    return json_number((double)phf_utf8_len(text));
}

/* escape(text): "seam" marker -> traced seam; else identity. seam_allowed
 * is false for clarify's gate call (live passes no escape there).
 * Returns an owned string node, or a raise node to propagate. */
static json_t *phf_escape(phf_ctx_t *ctx, const json_t *args, const char *text,
                          bool seam_allowed) {
    if (seam_allowed && phf_str_eq(json_obj_get(args, "escape"), "seam")) {
        json_t *a = json_object();
        json_set_hdr(a, "text", text);
        json_t *r = phf_call(ctx, "escape", a);
        if (phf_is_raise(r)) return r;
        const char *s = phf_str(r);
        json_t *o = json_string(s ? s : "");
        json_free(r);
        return o;
    }
    return json_string(text);
}

/* "\n".join(f"{i}. {esc(t)}") — the _render closure. */
static json_t *phf_render(phf_ctx_t *ctx, const json_t *args, char *const *texts,
                          size_t n, bool seam_allowed) {
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    if (!buf) return phf_err("MemoryError", "render alloc");
    buf[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        json_t *e = phf_escape(ctx, args, texts[i], seam_allowed);
        if (phf_is_raise(e)) { free(buf); return e; }
        const char *esc = e->str_val ? e->str_val : "";
        char num[32];
        snprintf(num, sizeof num, "%zu. ", i + 1);
        size_t need = len + strlen(num) + strlen(esc) + 2;
        if (need > cap) {
            while (cap < need) cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { json_free(e); free(buf); return phf_err("MemoryError", "render alloc"); }
            buf = nb;
        }
        if (i) buf[len++] = '\n';
        memcpy(buf + len, num, strlen(num));
        len += strlen(num);
        memcpy(buf + len, esc, strlen(esc));
        len += strlen(esc);
        buf[len] = '\0';
        json_free(e);
    }
    json_t *o = json_string(buf);
    free(buf);
    return o;
}

/* internal core helper; PoP credit lives on the dispatched case wrapper */
static char *phf_shorten_choice(const char *text, long long each, long long anchor) {
    long long n = (long long)phf_utf8_len(text);
    if (n <= each) return phf_strdup(text);
    long long head = (each + 1) / 2, tail = each / 2;
    char *a, *b, *o;
    if (anchor < head || anchor >= n - tail) {
        a = phf_slice(text, 0, head);
        b = phf_slice(text, n - tail, n);
        char *ar = phf_rstrip(a), *bl = phf_lstrip(b);
        free(a); free(b);
        size_t len = strlen(ar) + strlen(bl) + 3;
        o = malloc(len + 1);
        if (o) snprintf(o, len + 1, "%s" PHF_ELL "%s", ar, bl);
        free(ar); free(bl);
        return o;
    }
    head = tail = each / 4;
    long long width = each - head - tail;
    long long start = anchor - width / 2;
    if (start < head + 1) start = head + 1;
    long long cap = n - tail - width - 1;
    if (start > cap) start = cap;
    if (start <= head) {
        a = phf_slice(text, 0, (each + 1) / 2);
        b = phf_slice(text, n - each / 2, n);
        char *ar = phf_rstrip(a), *bl = phf_lstrip(b);
        free(a); free(b);
        size_t len = strlen(ar) + strlen(bl) + 3;
        o = malloc(len + 1);
        if (o) snprintf(o, len + 1, "%s" PHF_ELL "%s", ar, bl);
        free(ar); free(bl);
        return o;
    }
    a = phf_slice(text, 0, head);
    char *w = phf_slice(text, start, start + width);
    b = phf_slice(text, n - tail, n);
    char *ar = phf_rstrip(a), *ws = phf_strip(w), *bl = phf_lstrip(b);
    free(a); free(w); free(b);
    size_t len = strlen(ar) + strlen(ws) + strlen(bl) + 6;
    o = malloc(len + 1);
    if (o) snprintf(o, len + 1, "%s" PHF_ELL "%s" PHF_ELL "%s", ar, ws, bl);
    free(ar); free(ws); free(bl);
    return o;
}

/* internal core helper; PoP credit lives on the dispatched case wrapper */
static char **phf_distinct_shortenings(char *const *texts, size_t n, long long each,
                                       size_t *prefix) {
    char **out = malloc(n * sizeof(char *));
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        long long anchor = 0;
        bool any = false;
        for (size_t j = 0; j < n; j++) {
            if (j == i) continue;
            long long p = (long long)prefix[i * n + j];
            if (!any || p > anchor) { anchor = p; any = true; }
        }
        out[i] = phf_shorten_choice(texts[i], each, anchor);
    }
    for (size_t iter = 0; iter < n; iter++) {
        /* groups: first-appearance order of the shortened lines */
        char **glines = malloc(n * sizeof(char *));
        size_t *gidx = malloc(n * n * sizeof(size_t));
        size_t *gcount = calloc(n, sizeof(size_t));
        size_t ng = 0;
        if (!glines || !gidx || !gcount) {
            free(glines); free(gidx); free(gcount);
            break;
        }
        for (size_t i = 0; i < n; i++) {
            size_t g = 0;
            while (g < ng && strcmp(glines[g], out[i]) != 0) g++;
            if (g == ng) { glines[ng] = out[i]; ng++; }
            gidx[g * n + gcount[g]++] = i;
        }
        bool clashed = false;
        for (size_t g = 0; g < ng; g++) {
            if (gcount[g] < 2) continue;
            /* > 1 distinct ORIGINAL text in the group */
            bool distinct = false;
            for (size_t a = 0; a < gcount[g] && !distinct; a++)
                for (size_t b = a + 1; b < gcount[g]; b++)
                    if (strcmp(texts[gidx[g * n + a]], texts[gidx[g * n + b]]) != 0) {
                        distinct = true;
                        break;
                    }
            if (!distinct) continue;
            clashed = true;
            long long shared = -1;
            for (size_t a = 0; a < gcount[g]; a++)
                for (size_t b = 0; b < gcount[g]; b++) {
                    size_t ia = gidx[g * n + a], ib = gidx[g * n + b];
                    if (strcmp(texts[ia], texts[ib]) == 0) continue;
                    long long p = (long long)prefix[ia * n + ib];
                    if (shared < 0 || p < shared) shared = p;
                }
            for (size_t a = 0; a < gcount[g]; a++) {
                size_t i = gidx[g * n + a];
                free(out[i]);
                out[i] = phf_shorten_choice(texts[i], each, shared);
            }
        }
        free(glines); free(gidx); free(gcount);
        if (!clashed) break;
    }
    return out;
}

/* numbered_clarify_choices core. use_seams=false models the gate call inside
 * clarify_text_with_choices (builtin len, identity escape, no max_chars). */
static json_t *phf_numbered_core(phf_ctx_t *ctx, const json_t *args,
                                 const json_t *choices, double mlc,
                                 bool use_seams, bool has_max_chars, double max_chars) {
    size_t n = choices ? json_len(choices) : 0;
    char **texts = malloc((n ? n : 1) * sizeof(char *));
    if (!texts) return phf_err("MemoryError", "texts alloc");
    for (size_t i = 0; i < n; i++) {
        char *s = phf_str_of_json(json_get(choices, i));
        if (!s) {
            for (size_t j = 0; j < i; j++) free(texts[j]);
            free(texts);
            return phf_err("TypeError", "choice str() projection outside fixture corpus");
        }
        texts[i] = phf_strip(s);
        free(s);
    }
    json_t *result = NULL;
    bool all_fit = true;
    for (size_t i = 0; i < n; i++)
        if ((double)phf_display_width_str(texts[i]) > mlc) { all_fit = false; break; }
    if (all_fit) { result = json_null(); goto done; }

    long long each = 0;
    for (size_t i = 0; i < n; i++) {
        long long l = (long long)phf_utf8_len(texts[i]);
        if (l > each) each = l;
    }
    size_t *prefix = malloc(n * n * sizeof(size_t));
    if (!prefix) { result = phf_err("MemoryError", "prefix alloc"); goto done; }
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++)
            prefix[i * n + j] = phf_commonprefix_len(texts[i], texts[j]);

    json_t *out = phf_render(ctx, args, texts, n, use_seams);
    if (phf_is_raise(out)) { free(prefix); result = out; goto done; }
    /* live order: max_chars is not None and len_fn(out) > max_chars and
     * each > 1 — len_fn(out) is evaluated EVEN when each has reached 1. */
    while (use_seams && has_max_chars) {
        json_t *m = phf_measure(ctx, args, out->str_val);
        if (phf_is_raise(m)) { json_free(out); free(prefix); result = m; goto done; }
        double olen = m->num_val;
        json_free(m);
        if (olen <= max_chars) break;
        if (each <= 1) break;
        long long shrink = each * 4 / 5;
        if (each - 1 < shrink) shrink = each - 1;
        each = shrink ? shrink : 1;
        char **shortened = phf_distinct_shortenings(texts, n, each, prefix);
        if (!shortened) { json_free(out); free(prefix); result = phf_err("MemoryError", "shorten alloc"); goto done; }
        json_t *next = phf_render(ctx, args, shortened, n, use_seams);
        for (size_t i = 0; i < n; i++) free(shortened[i]);
        free(shortened);
        json_free(out);
        if (phf_is_raise(next)) { free(prefix); result = next; goto done; }
        out = next;
    }
    free(prefix);
    result = out;
done:
    for (size_t i = 0; i < n; i++) free(texts[i]);
    free(texts);
    return result;
}

/* ── capability-gate leaves (adapter doubles) ── */

static const json_t *phf_adapter_hooks(const json_t *args) {
    const json_t *ad = json_obj_get(args, "adapter");
    return ad ? json_obj_get(ad, "hooks") : NULL;
}

static const json_t *phf_adapter_attr(const json_t *args, const char *key) {
    const json_t *ad = json_obj_get(args, "adapter");
    const json_t *attrs = ad ? json_obj_get(ad, "attrs") : NULL;
    return attrs ? json_obj_get(attrs, key) : NULL;
}

/* A plain JSON number is a Python int; {"__float__": true} is a float. */
static bool phf_is_float_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__float__", false);
}

/* internal core helper; PoP credit lives on the dispatched case wrapper */
static const char *phf_reply_document_sender(const json_t *args) {
    const json_t *hooks = phf_adapter_hooks(args);
    const json_t *srd = hooks ? json_obj_get(hooks, "send_reply_document") : NULL;
    /* callable(getattr(type(adapter), "send_reply_document", None)) */
    if (phf_str_eq(srd, "own")) return "send_reply_document";
    const json_t *sd = hooks ? json_obj_get(hooks, "send_document") : NULL;
    /* native = getattr(type(adapter), "send_document", None) not in
     * (None, BasePlatformAdapter.send_document): "own" overrides the base
     * identity; "base"/absent inherits it; "null" is None. */
    if (phf_str_eq(sd, "own")) return "send_document";
    return NULL;
}

/* internal core helper; PoP credit lives on the dispatched case wrapper */
static json_t *phf_reply_message_cap(const json_t *args) {
    const json_t *cap = phf_adapter_attr(args, "MAX_SPLIT_MESSAGES");
    /* type(cap) is not int: bools and tagged floats fail, plain numbers pass */
    if (!cap || cap->type != JSON_NUMBER || phf_is_float_tag(cap)) return json_null();
    if (cap->num_val <= 1) return json_null();
    if (!phf_reply_document_sender(args)) return json_null();
    return json_number(cap->num_val);
}

/* int(x) for the MAX_MESSAGE_LENGTH fast path (x truthy). */
static json_t *phf_int_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return json_number(0);
    if (phf_is_float_tag(v))
        return json_number((double)(long long)json_get_num(v, "v", 0));
    if (v->type == JSON_NUMBER) return json_number((double)(long long)v->num_val);
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        char *end = NULL;
        long long val = strtoll(s, &end, 10);
        while (end && *end && phf_is_ascii_ws((unsigned char)*end)) end++;
        bool digits = end && end > s;
        /* strtoll skips leading ws and accepts a sign; require >= 1 digit */
        const char *p = s;
        while (*p && phf_is_ascii_ws((unsigned char)*p)) p++;
        if (*p == '+' || *p == '-') p++;
        if (!(*p >= '0' && *p <= '9')) digits = false;
        if (digits && end && *end == '\0') return json_number((double)val);
        char msg[512];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: '%s'", s);
        return phf_err("ValueError", msg);
    }
    return phf_err("TypeError", "int() argument outside fixture corpus");
}

/* ── media leaves ── */

/* type name for the .st_size AttributeError projection (the oracle's build
 * wraps scripted dicts in SimpleNamespace). */
static const char *phf_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "SimpleNamespace";
        default: return "object";
    }
}

static json_t *phf_attrerror_st_size(const json_t *v) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'st_size'", phf_type_name(v));
    return phf_err("AttributeError", msg);
}

/* read .st_size off a scripted stat/fstat result: number -> owned number;
 * missing attribute -> AttributeError node; null value -> null node (the
 * sum's TypeError surfaces in media_upload_seconds). */
static json_t *phf_st_size(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        json_t *sz = json_obj_get(r, "st_size");
        if (sz) return json_copy(sz);
    }
    return phf_attrerror_st_size(r);
}

/* PoP: phf_on_disk_size @ gateway/platforms/helpers.py:_on_disk_size */
static json_t *phf_on_disk_size(phf_ctx_t *ctx, const json_t *source) {
    const char *path = NULL;
    char *path_buf = NULL;
    if (source && source->type == JSON_STRING) {
        path = source->str_val;
    } else if (source && source->type == JSON_OBJECT
               && phf_str_eq(json_obj_get(source, "kind"), "pathlike")) {
        path_buf = phf_strdup(json_get_str(source, "path", ""));
        path = path_buf;
    }
    if (path) {
        /* str / os.PathLike branch: except (OSError, ValueError) -> 0 */
        json_t *a = json_object();
        json_set_hdr(a, "path", path);
        free(path_buf);
        json_t *r = phf_call(ctx, "os.stat", a);
        if (phf_is_raise(r)) {
            if (phf_is_oserror(r) || phf_is_valueerror(r)) {
                json_free(r);
                return json_number(0);
            }
            return r;
        }
        json_t *sz = phf_st_size(r);
        json_free(r);
        return sz; /* AttributeError PROPAGATES on the path branch */
    }
    /* open-file branch: except (AttributeError, OSError, ValueError) -> 0 */
    bool is_file = source && source->type == JSON_OBJECT
        && phf_str_eq(json_obj_get(source, "kind"), "file");
    if (!is_file) return json_number(0); /* no .fileno -> AttributeError -> 0 */
    json_t *fr = json_obj_get(source, "fileno_raise");
    if (fr && fr->type == JSON_OBJECT) {
        json_t *exc = phf_make_exc(fr); /* silent attribute raise, not traced */
        const char *t = json_get_str(exc, "__raise__", "");
        if (phf_is_oserror_type(t) || strcmp(t, "ValueError") == 0
                || strcmp(t, "AttributeError") == 0) {
            json_free(exc);
            return json_number(0);
        }
        return exc;
    }
    double fd = json_get_num(source, "fileno", 0);
    json_t *a = json_object();
    json_set(a, "fd", json_number(fd));
    json_t *r = phf_call(ctx, "os.fstat", a);
    if (phf_is_raise(r)) {
        const char *t = json_get_str(r, "__raise__", "");
        if (phf_is_oserror_type(t) || strcmp(t, "ValueError") == 0
                || strcmp(t, "AttributeError") == 0) {
            json_free(r);
            return json_number(0);
        }
        return r;
    }
    json_t *sz = phf_st_size(r);
    json_free(r);
    if (phf_is_attributeerror(sz)) {
        json_free(sz);
        return json_number(0); /* AttributeError IS caught on the file branch */
    }
    return sz;
}

/* ── the 18 case functions ── */

/* PoP: phf_display_width @ gateway/platforms/helpers.py:display_width */
static json_t *phf_display_width(phf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return json_number((double)phf_display_width_str(json_get_str(args, "text", "")));
}

/* PoP: phf_clip_text @ gateway/platforms/helpers.py:_clip_text */
static json_t *phf_clip_text(phf_ctx_t *ctx, const json_t *args) {
    const char *text = json_get_str(args, "text", "");
    long long limit = (long long)json_get_num(args, "limit", 0);
    json_t *m = phf_measure(ctx, args, text);
    if (phf_is_raise(m)) return m;
    double len = m->num_val;
    json_free(m);
    if (len <= (double)limit) return json_string(text);
    long long cut_n = limit - 1;
    if (cut_n < 0) cut_n = 0;
    char *cut = phf_slice(text, 0, cut_n);
    while (cut && cut[0]) {
        json_t *cm = phf_measure(ctx, args, cut);
        if (phf_is_raise(cm)) { free(cut); return cm; }
        double clen = cm->num_val;
        json_free(cm);
        if (clen <= (double)(limit - 1)) break;
        long long cl = (long long)phf_utf8_len(cut);
        char *next = phf_slice(cut, 0, cl - 1);
        free(cut);
        cut = next;
    }
    size_t olen = strlen(cut ? cut : "") + 3;
    char *o = malloc(olen + 1);
    if (o) snprintf(o, olen + 1, "%s" PHF_ELL, cut ? cut : "");
    free(cut);
    json_t *r = json_string(o ? o : PHF_ELL);
    free(o);
    return r;
}

/* PoP: phf_shorten_choice_case @ gateway/platforms/helpers.py:_shorten_choice */
static json_t *phf_shorten_choice_case(phf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *s = phf_shorten_choice(json_get_str(args, "text", ""),
                                 (long long)json_get_num(args, "each", 0),
                                 (long long)json_get_num(args, "anchor", 0));
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* PoP: phf_distinct_shortenings_case @ gateway/platforms/helpers.py:_distinct_shortenings */
static json_t *phf_distinct_shortenings_case(phf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *texts = json_obj_get(args, "texts");
    const json_t *prefix = json_obj_get(args, "prefix");
    size_t n = texts ? json_len(texts) : 0;
    long long each = (long long)json_get_num(args, "each", 0);
    char **ts = malloc((n ? n : 1) * sizeof(char *));
    size_t *px = malloc((n ? n : 1) * (n ? n : 1) * sizeof(size_t));
    if (!ts || !px) { free(ts); free(px); return phf_err("MemoryError", "alloc"); }
    for (size_t i = 0; i < n; i++) {
        const char *t = phf_str(json_get(texts, i));
        ts[i] = phf_strdup(t ? t : "");
    }
    for (size_t i = 0; i < n; i++) {
        const json_t *row = json_get(prefix, i);
        for (size_t j = 0; j < n; j++) {
            const json_t *p = json_get(row, j);
            px[i * n + j] = (p && p->type == JSON_NUMBER) ? (size_t)p->num_val : 0;
        }
    }
    char **out = phf_distinct_shortenings(ts, n, each, px);
    json_t *r = json_array();
    if (out) {
        for (size_t i = 0; i < n; i++) {
            json_append(r, json_string(out[i] ? out[i] : ""));
            free(out[i]);
        }
        free(out);
    }
    for (size_t i = 0; i < n; i++) free(ts[i]);
    free(ts); free(px);
    return r;
}

/* PoP: phf_numbered_clarify_choices @ gateway/platforms/helpers.py:numbered_clarify_choices */
static json_t *phf_numbered_clarify_choices(phf_ctx_t *ctx, const json_t *args) {
    const json_t *mc = json_obj_get(args, "max_chars");
    bool has_mc = mc && mc->type == JSON_NUMBER;
    return phf_numbered_core(ctx, args, json_obj_get(args, "choices"),
                             json_get_num(args, "max_label_cols", 0),
                             true, has_mc, has_mc ? mc->num_val : 0);
}

/* _clip_text(head, lim, len_fn) via a scratch args object (clarify's args
 * carry "head", not "text"). */
static json_t *phf_clip_head(phf_ctx_t *ctx, const json_t *args, const char *head,
                             long long lim) {
    json_t *clip_args = json_object();
    json_set_hdr(clip_args, "text", head);
    json_set(clip_args, "limit", json_number((double)lim));
    const json_t *lf = json_obj_get(args, "len_fn");
    if (lf) json_set(clip_args, "len_fn", json_copy(lf));
    json_t *clipped = phf_clip_text(ctx, clip_args);
    json_free(clip_args);
    return clipped;
}

/* PoP: phf_clarify_text_with_choices @ gateway/platforms/helpers.py:clarify_text_with_choices */
static json_t *phf_clarify_text_with_choices(phf_ctx_t *ctx, const json_t *args) {
    const char *head = json_get_str(args, "head", "");
    const json_t *choices = json_obj_get(args, "choices");
    long long limit = (long long)json_get_num(args, "limit", 0);
    double mlc = json_get_num(args, "max_label_cols", 0);
    json_t *result = NULL;
    char *listed_buf = NULL;

    /* gate call: builtin len, identity escape, no max_chars (no seam events) */
    json_t *gate = phf_numbered_core(ctx, args, choices, mlc, false, false, 0);
    if (phf_is_raise(gate)) return gate;
    if (gate->type == JSON_NULL) {
        json_free(gate);
        json_t *clipped = phf_clip_head(ctx, args, head, limit);
        if (phf_is_raise(clipped)) return clipped;
        result = json_array();
        json_append(result, clipped);
        json_append(result, json_bool(false));
        return result;
    }
    json_free(gate);

    /* room = max(limit - 2 - len_fn(head), limit // 2) */
    json_t *hm = phf_measure(ctx, args, head);
    if (phf_is_raise(hm)) return hm;
    double head_len = hm->num_val;
    json_free(hm);
    long long room_a = limit - 2 - (long long)head_len;
    long long room_b = phf_fdiv(limit, 2);
    long long room = room_a > room_b ? room_a : room_b;

    /* listed = numbered(choices, max_label_cols=0, escape, max_chars=room, len_fn) */
    json_t *listed = phf_numbered_core(ctx, args, choices, 0, true, true, (double)room);
    if (phf_is_raise(listed)) return listed;
    if (listed->type != JSON_STRING) {
        /* all-empty choices with a negative gate mlc: live's len_fn(None)
         * raises before the f-string (builtin len corpus). */
        json_free(listed);
        return phf_err("TypeError", "object of type 'NoneType' has no len()");
    }
    listed_buf = phf_strdup(listed->str_val);
    json_free(listed);

    /* head clip limit = max(0, limit - 2 - len_fn(listed)) */
    json_t *lm = phf_measure(ctx, args, listed_buf);
    if (phf_is_raise(lm)) { free(listed_buf); return lm; }
    double listed_len = lm->num_val;
    json_free(lm);
    long long head_limit = limit - 2 - (long long)listed_len;
    if (head_limit < 0) head_limit = 0;

    json_t *clipped = phf_clip_head(ctx, args, head, head_limit);
    if (phf_is_raise(clipped)) { free(listed_buf); return clipped; }

    const char *cs = clipped->str_val ? clipped->str_val : "";
    size_t len = strlen(cs) + strlen(listed_buf) + 2;
    char *o = malloc(len + 1);
    if (o) snprintf(o, len + 1, "%s\n\n%s", cs, listed_buf);
    json_free(clipped);
    free(listed_buf);
    result = json_array();
    json_append(result, json_string(o ? o : ""));
    json_append(result, json_bool(true));
    free(o);
    return result;
}

/* PoP: phf_keep_link_target @ gateway/platforms/helpers.py:_keep_link_target */
static json_t *phf_keep_link_target(phf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *g1 = json_get_str(args, "group1", "");
    const char *g2 = json_get_str(args, "group2", "");
    char *label = phf_strip(g1);
    char *target = phf_strip(g2);
    char *lower = phf_lower(target);
    bool http = strncmp(lower, "http://", 7) == 0 || strncmp(lower, "https://", 8) == 0;
    json_t *r;
    if (!http) {
        r = json_string(g1); /* the RAW (unstripped) label */
    } else if (strcmp(label, target) == 0) {
        r = json_string(target);
    } else {
        size_t len = strlen(label) + strlen(target) + 1;
        char *o = malloc(len + 1);
        if (o) snprintf(o, len + 1, "%s\n%s", label, target);
        r = json_string(o ? o : "");
        free(o);
    }
    free(label); free(target); free(lower);
    return r;
}

/* PoP: phf_bounded_put @ gateway/platforms/helpers.py:bounded_put */
static json_t *phf_bounded_put(phf_ctx_t *ctx, const json_t *args) {
    const json_t *store_in = json_obj_get(args, "store");
    json_t *store = (store_in && store_in->type == JSON_OBJECT)
        ? json_copy(store_in) : json_object();
    json_set(ctx->state, "store", store);
    const char *key = json_get_str(args, "key", "");
    const json_t *value = json_obj_get(args, "value");
    json_obj_del(store, key);
    json_set(store, key, value ? json_copy(value) : json_null());
    long long cap = (long long)json_get_num(args, "cap", 0);
    while ((long long)json_object_size(store) > cap) {
        const char *k0 = json_object_get_key_at(store, 0);
        if (!k0) {
            /* live: next(iter({})) raises StopIteration (negative cap) */
            return phf_err("StopIteration", "");
        }
        char *dup = phf_strdup(k0);
        json_obj_del(store, dup);
        free(dup);
    }
    return json_null();
}

/* PoP: phf_table_row_start @ gateway/platforms/helpers.py:table_row_start */
static json_t *phf_table_row_start(phf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = json_get_str(args, "text", "");
    long long split_at = (long long)json_get_num(args, "split_at", 0);
    long long n = (long long)phf_utf8_len(text);
    long long newline = phf_rfind_nl(text, split_at);
    char *ch = phf_slice1(text, split_at);
    bool at_boundary = !ch || ch[0] == '\0' || (ch[0] == '\n' && ch[1] == '\0');
    free(ch);
    if (newline < 1 || at_boundary) return json_number((double)split_at);
    long long line_end = phf_find_nl(text, split_at);
    char *prefix = phf_slice(text, 0, line_end >= 0 ? line_end : n);
    phf_lines_t ls = phf_split_nl(prefix);
    free(prefix);
    long long hdr = phf_table_header_above(ls.lines, ls.count, (long long)ls.count - 1);
    phf_lines_free(&ls);
    return json_number((double)(hdr >= 0 ? newline : split_at));
}

/* PoP: phf_table_rollback_point @ gateway/platforms/helpers.py:table_rollback_point */
static json_t *phf_table_rollback_point(phf_ctx_t *ctx, const json_t *args) {
    const char *text = json_get_str(args, "text", "");
    long long split_at = (long long)json_get_num(args, "split_at", 0);
    long long n = (long long)phf_utf8_len(text);
    char *head = phf_slice(text, 0, split_at);
    char *tail = phf_slice(text, split_at, n);
    bool inside = phf_split_inside_table(head, tail);
    if (!inside) {
        free(head); free(tail);
        return json_number((double)split_at);
    }
    phf_lines_t head_lines = phf_split_nl(head);
    free(head);
    char *tl = phf_lstrip(tail);
    free(tail);
    phf_lines_t tl_lines = phf_split_nl(tl);
    free(tl);
    /* lines = head_lines + [first line of tail.lstrip()] */
    size_t hn = head_lines.count;
    char **lines = malloc((hn + 1) * sizeof(char *));
    if (!lines) {
        phf_lines_free(&head_lines); phf_lines_free(&tl_lines);
        return phf_err("MemoryError", "lines alloc");
    }
    for (size_t i = 0; i < hn; i++) lines[i] = head_lines.lines[i];
    lines[hn] = tl_lines.count > 0 ? tl_lines.lines[0] : "";
    long long header = phf_table_header_above(lines, hn + 1, (long long)hn);
    if (header <= 0) {
        free(lines);
        phf_lines_free(&head_lines); phf_lines_free(&tl_lines);
        return json_number((double)split_at);
    }
    char *joined = phf_join_nl(lines, (size_t)header);
    long long start = (long long)phf_utf8_len(joined) + 1;
    free(joined);
    bool no_rows = header >= (long long)hn - 2;
    free(lines);
    phf_lines_free(&head_lines);
    phf_lines_free(&tl_lines);
    if (no_rows) return json_number((double)start);
    long long end = start;
    char *from_start = phf_slice(text, start, n);
    phf_lines_t rest = phf_split_nl(from_start);
    free(from_start);
    for (size_t i = 0; i < rest.count; i++) {
        if (!phf_is_table_row(rest.lines[i])) break;
        end += (long long)phf_utf8_len(rest.lines[i]) + 1;
    }
    phf_lines_free(&rest);
    char *fits_arg = phf_slice(text, start, end);
    char *trimmed = phf_rstrip_nl(fits_arg);
    free(fits_arg);
    if (!phf_str_eq(json_obj_get(args, "fits"), "seam")) {
        free(trimmed);
        return phf_err("TypeError", "table_rollback_point requires the fits seam");
    }
    json_t *a = json_object();
    json_set_hdr(a, "text", trimmed);
    free(trimmed);
    json_t *r = phf_call(ctx, "fits", a);
    if (phf_is_raise(r)) return r;
    bool fits = phf_truthy(r);
    json_free(r);
    return json_number((double)(fits ? start : split_at));
}

/* PoP: phf_table_header_to_repeat @ gateway/platforms/helpers.py:table_header_to_repeat */
static json_t *phf_table_header_to_repeat(phf_ctx_t *ctx, const json_t *args) {
    const char *chunk = json_get_str(args, "chunk", "");
    const char *rest = json_get_str(args, "rest", "");
    double max_units = json_get_num(args, "max_units", 0);
    if (!phf_split_inside_table(chunk, rest)) return json_null();
    char *trimmed = phf_rstrip_nl(chunk);
    phf_lines_t ls = phf_split_nl(trimmed);
    free(trimmed);
    long long header = phf_table_header_above(ls.lines, ls.count, (long long)ls.count - 1);
    if (header < 0) { phf_lines_free(&ls); return json_null(); }
    size_t len = strlen(ls.lines[header]) + strlen(ls.lines[header + 1]) + 1;
    char *block = malloc(len + 1);
    if (block) snprintf(block, len + 1, "%s\n%s", ls.lines[header], ls.lines[header + 1]);
    phf_lines_free(&ls);
    json_t *m = phf_measure(ctx, args, block ? block : "");
    if (phf_is_raise(m)) { free(block); return m; }
    double blen = m->num_val;
    json_free(m);
    json_t *r = (blen <= max_units) ? json_string(block ? block : "") : json_null();
    free(block);
    return r;
}

/* PoP: phf_is_discord_channel_obfuscated @ gateway/platforms/helpers.py:is_discord_channel_obfuscated */
static json_t *phf_is_discord_channel_obfuscated(phf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *channel = json_obj_get(args, "channel");
    const json_t *flags = (channel && channel->type == JSON_OBJECT)
        ? json_obj_get(channel, "flags") : NULL;
    /* try: channel.flags.value except AttributeError -> None */
    const json_t *flag_value = (flags && flags->type == JSON_OBJECT)
        ? json_obj_get(flags, "value") : NULL;
    if (flag_value) {
        bool is_int = false;
        long long v = 0;
        if (flag_value->type == JSON_BOOL) {
            is_int = true; /* bool IS an int instance */
            v = flag_value->bool_val ? 1 : 0;
        } else if (flag_value->type == JSON_NUMBER && !phf_is_float_tag(flag_value)) {
            is_int = true;
            v = (long long)flag_value->num_val;
        }
        if (is_int && (v & PHF_DISCORD_OBFUSCATED_FLAG)) return json_bool(true);
    }
    const json_t *name = (channel && channel->type == JSON_OBJECT)
        ? json_obj_get(channel, "name") : NULL;
    return json_bool(phf_str_eq(name, PHF_DISCORD_HIDDEN_NAME));
}

/* PoP: phf_reply_document_sender_case @ gateway/platforms/helpers.py:reply_document_sender */
static json_t *phf_reply_document_sender_case(phf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *name = phf_reply_document_sender(args);
    return name ? json_string(name) : json_null();
}

/* PoP: phf_reply_message_cap_case @ gateway/platforms/helpers.py:reply_message_cap */
static json_t *phf_reply_message_cap_case(phf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return phf_reply_message_cap(args);
}

/* PoP: phf_over_reply_cap @ gateway/platforms/helpers.py:_over_reply_cap */
static json_t *phf_over_reply_cap(phf_ctx_t *ctx, const json_t *args) {
    json_t *cap_node = phf_reply_message_cap(args);
    const json_t *hooks = phf_adapter_hooks(args);
    bool count_hook = phf_str_eq(hooks ? json_obj_get(hooks, "reply_message_count") : NULL, "own");
    const char *content = json_get_str(args, "content", "");
    if (cap_node->type == JSON_NULL || !count_hook || !content[0]) {
        json_free(cap_node);
        return json_bool(false);
    }
    long long cap = (long long)cap_node->num_val;
    json_free(cap_node);
    /* fast path: len(content) * 8 <= cap * int(MAX_MESSAGE_LENGTH or 0) */
    const json_t *mml = phf_adapter_attr(args, "MAX_MESSAGE_LENGTH");
    json_t *mml_int;
    if (phf_truthy(mml)) {
        mml_int = phf_int_of(mml);
        if (phf_is_raise(mml_int)) return mml_int;
    } else {
        mml_int = json_number(0);
    }
    long long mml_val = (long long)mml_int->num_val;
    json_free(mml_int);
    if ((long long)phf_utf8_len(content) * 8 <= cap * mml_val) return json_bool(false);
    json_t *a = json_object();
    json_set_hdr(a, "content", content);
    const json_t *md = json_obj_get(args, "metadata");
    json_set(a, "metadata", md ? json_copy(md) : json_null());
    json_t *r = phf_call(ctx, "reply_message_count", a);
    if (phf_is_raise(r)) return r;
    double count = (r && r->type == JSON_NUMBER) ? r->num_val : 0;
    json_free(r);
    return json_bool(count > (double)cap);
}

/* bytes -> latin-1 projection: byte b as the UTF-8 encoding of U+00b. */
static json_t *phf_bytes_node(const unsigned char *data, size_t n) {
    char *o = malloc(2 * n + 1);
    if (!o) return json_string("");
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char b = data[i];
        if (b < 0x80) {
            o[k++] = (char)b;
        } else {
            o[k++] = (char)(0xc0 | (b >> 6));
            o[k++] = (char)(0x80 | (b & 0x3f));
        }
    }
    o[k] = '\0';
    json_t *r = json_string(o);
    free(o);
    return r;
}

/* PoP: phf_reply_file_parts @ gateway/platforms/helpers.py:reply_file_parts */
static json_t *phf_reply_file_parts(phf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const unsigned char *data = (const unsigned char *)json_get_str(args, "text", "");
    long long dlen = (long long)strlen((const char *)data);
    long long limit = (long long)json_get_num(args, "limit_bytes", 0);
    json_t *parts = json_array();
    size_t nparts = 0;
    while (dlen > limit) {
        long long cut = -1;
        for (long long i = limit - 1; i >= 0; i--)
            if (data[i] == '\n') { cut = i; break; }
        cut += 1;
        if (cut < phf_fdiv(limit, 2)) {
            cut = limit;
            while (cut && (data[cut] & 0xC0) == 0x80) cut--;
        }
        if (cut <= 0) {
            /* live loops forever here; fail closed instead of hanging */
            json_free(parts);
            return phf_err("RuntimeError", "reply_file_parts: zero cut (fixture outside terminating domain)");
        }
        json_append(parts, phf_bytes_node(data, (size_t)cut));
        nparts++;
        data += cut;
        dlen -= cut;
    }
    if (dlen > 0 || nparts == 0) json_append(parts, phf_bytes_node(data, (size_t)dlen));
    return parts;
}

/* PoP: phf_scaled_upload_deadline @ gateway/platforms/helpers.py:scaled_upload_deadline */
static json_t *phf_scaled_upload_deadline(phf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    double deadline = json_get_num(args, "deadline", 0);
    double upload_seconds = json_get_num(args, "upload_seconds", 0);
    if (upload_seconds <= 0) return json_number(deadline);
    double widened = deadline + 1.5 * upload_seconds;
    if (widened > PHF_MEDIA_UPLOAD_DEADLINE_MAX) widened = PHF_MEDIA_UPLOAD_DEADLINE_MAX;
    return json_number(deadline > widened ? deadline : widened);
}

/* PoP: phf_media_upload_seconds @ gateway/platforms/helpers.py:media_upload_seconds */
static json_t *phf_media_upload_seconds(phf_ctx_t *ctx, const json_t *args) {
    const json_t *sources = json_obj_get(args, "sources");
    size_t n = sources ? json_len(sources) : 0;
    long long total = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *sz = phf_on_disk_size(ctx, json_get(sources, i));
        if (phf_is_raise(sz)) return sz;
        if (!sz || sz->type != JSON_NUMBER) {
            char msg[256];
            snprintf(msg, sizeof msg, "unsupported operand type(s) for +: 'int' and '%s'",
                     phf_type_name(sz));
            json_free(sz);
            return phf_err("TypeError", msg);
        }
        total += (long long)sz->num_val;
        json_free(sz);
    }
    return json_number((double)total / PHF_MIN_MEDIA_UPLOAD_RATE);
}
