/*
 * slack_adapter_flow.c -- FLOW cores + pure leaves of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's module-level decision chains (link-preview bool coercion, GFM
 * table wrap/align, Block-Kit mention recovery + inline rendering, dedupe
 * unescape, audio-extension routing, standalone post kwargs, response
 * payload normalization) and its seamed flows (i18n expired notice, bang
 * command rewrite, dedup TTL secret parse, proxy resolution, DM-cache trim,
 * saved bot-token loading).
 *
 * Decision sequences only: every live I/O / injected callable (t,
 * is_gateway_known_command, get_scoped_secret, resolve_proxy_url,
 * safe_proxy_url_for_log, is_host_excluded_by_no_proxy, get_hermes_home,
 * tokens_file.exists / read_text, warn_if_credential_file_broadly_readable,
 * logger.info / logger.warning) enters through ONE universal seam:
 * sa_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and sa_-prefixed.
 *
 * Representation contract (shared with tests/oracle_slack_adapter_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc)} object.
 * - Plain JSON numbers are ints (integral doubles serialize %.0f); JSON
 *   booleans are bools. Non-integral floats appear only as float() RESULTS
 *   (dedup TTL), never in str()-computed positions.
 * - Object doubles (non-dict attribute carriers) travel as
 *   {"__obj__": {attrs}}; a plain JSON object IS a Python dict.
 * - logger seams record {"msg": <format literal>, "args": [<raw args>]} --
 *   %r/%s apply only at emission, args are recorded VERBATIM.
 * - Path modeling is POSIX: get_hermes_home returns a POSIX path string;
 *   tokens_file = home / "slack_tokens.json" is a lexical POSIX join; the
 *   oracle's FileDouble.__str__ projects the same POSIX string.
 *   _resolve_slack_audio_ext's os.path.splitext follows ntpath._splitext
 *   (both '/' and '\\' separators, leading-dot scan from sepIndex+1).
 * - read_text(encoding="utf-8-sig") strips ONE leading U+FEFF BOM AFTER the
 *   seam call (the trace ret carries the raw scripted text).
 * - Fixture corpus (ASCII scope): strip/lower fold ASCII whitespace
 *   (0x09-0x0d, 0x1c-0x1f, 0x20) only; int()/float() string parses accept
 *   ASCII digits/sign/underscore/exponent (no Unicode digits); str() of
 *   non-strings covers str/int/bool/None only (containers excluded);
 *   float() raw strings parse finite or nan (inf results excluded -- JSON
 *   cannot carry them); malformed saved-token JSON is limited to "" and
 *   whitespace-only text (probed JSONDecodeError messages); _disp_width
 *   uses the full generated East-Asian W/F range table (all Unicode).
 * - BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 *   CancelledError) PASS THROUGH every except-Exception site.
 * - Fail-closed: a missing/exhausted seam script is a RuntimeError raise
 *   spec recorded in the trace before propagation.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SA_DM_CACHE_MAX 5000
#define SA_DEDUP_TTL_DEFAULT 3600.0

/* -- universal seam context -- */
typedef struct sa_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sa_ctx_t;

/* -- basic helpers -- */
static bool sa_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static bool sa_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0 ||
                    strcmp(kind, "KeyboardInterrupt") == 0 ||
                    strcmp(kind, "GeneratorExit") == 0 ||
                    strcmp(kind, "CancelledError") == 0);
}

/* A raise spec that an except-Exception site must PASS THROUGH. */
static bool sa_raise_passes(const json_t *r) {
    return sa_is_raise(r) && sa_is_base_exc(json_get_str(r, "__raise__", ""));
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool sa_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *sa_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool sa_str_eq(const json_t *v, const char *s) {
    const char *t = sa_str(v);
    return t && s && strcmp(t, s) == 0;
}

static json_t *sa_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *sa_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return sa_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return
 * is a fresh owned node (scripted value copy or raise descriptor). */
static json_t *sa_call(sa_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sa_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sa_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sa_make_exc(raise);
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

/* -- string helpers (Python str semantics on the ASCII fixture corpus) -- */
static char *sa_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static bool sa_is_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f) || c == ' ';
}

/* str.strip(): ASCII whitespace both ends (fixtures fold ASCII only). */
static char *sa_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && sa_is_ws((unsigned char)s[a])) a++;
    while (b > a && sa_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}

static char *sa_lower(const char *s) {
    char *o = sa_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* Python repr() of a str (ASCII corpus): prefer single quotes; switch to
 * double quotes when the text contains ' but no "; escape \ and the active
 * quote plus \n \r \t; other C0 controls as \xNN. */
static char *sa_py_repr_str(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *o = malloc(cap);
    if (!o) return NULL;
    size_t k = 0;
    o[k++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\' || c == (unsigned char)q) { o[k++] = '\\'; o[k++] = (char)c; }
        else if (c == '\n') { o[k++] = '\\'; o[k++] = 'n'; }
        else if (c == '\r') { o[k++] = '\\'; o[k++] = 'r'; }
        else if (c == '\t') { o[k++] = '\\'; o[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            static const char hex[] = "0123456789abcdef";
            o[k++] = '\\'; o[k++] = 'x';
            o[k++] = hex[c >> 4]; o[k++] = hex[c & 15];
        } else o[k++] = (char)c;
    }
    o[k++] = q;
    o[k] = '\0';
    return o;
}

/* str() projection for the fixture domain (str/int/bool/None). Containers
 * are OUT OF DOMAIN (Python repr divergence) and return NULL. */
static char *sa_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa_strdup("None");
    switch (v->type) {
        case JSON_STRING: return sa_strdup(v->str_val);
        case JSON_BOOL: return sa_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return sa_strdup(buf);
        }
        default: return NULL; /* containers: excluded from the corpus */
    }
}

/* Python type name for a JSON value (attribute-error messages). */
static const char *sa_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER:
            return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* Deep JSON equality mirroring Python == on the fixture domain (bool is an
 * int instance: True == 1). */
static bool sa_json_eq(const json_t *a, const json_t *b) {
    bool an = !a || a->type == JSON_NULL, bn = !b || b->type == JSON_NULL;
    if (an || bn) return an && bn;
    bool anum = a->type == JSON_NUMBER || a->type == JSON_BOOL;
    bool bnum = b->type == JSON_NUMBER || b->type == JSON_BOOL;
    if (anum && bnum) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type != b->type) return false;
    if (a->type == JSON_STRING) return strcmp(a->str_val, b->str_val) == 0;
    if (a->type == JSON_ARRAY) {
        if (json_len(a) != json_len(b)) return false;
        for (size_t i = 0; i < json_len(a); i++)
            if (!sa_json_eq(json_get(a, i), json_get(b, i))) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !sa_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* -- UTF-8 decode + East-Asian display width -- */
static unsigned int sa_utf8_next(const char **ps) {
    const unsigned char *p = (const unsigned char *)*ps;
    unsigned int c = *p++;
    unsigned int cp;
    int extra;
    if (c < 0x80) { cp = c; extra = 0; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else { *ps = (const char *)p; return 0xFFFD; }
    for (int i = 0; i < extra; i++) {
        unsigned int b = *p;
        if ((b & 0xC0) != 0x80) { *ps = (const char *)p; return 0xFFFD; }
        cp = (cp << 6) | (b & 0x3F);
        p++;
    }
    *ps = (const char *)p;
    return cp;
}
/* East-Asian Wide/Fullwidth codepoint ranges (unicodedata 15.x, py3.14.9).
 * Generated from live unicodedata.east_asian_width; W and F count as 2. */
static const unsigned int sa_eaw_wf[][2] = {
    {0x1100,0x115F},{0x231A,0x231B},{0x2329,0x232A},{0x23E9,0x23EC},
    {0x23F0,0x23F0},{0x23F3,0x23F3},{0x25FD,0x25FE},{0x2614,0x2615},
    {0x2630,0x2637},{0x2648,0x2653},{0x267F,0x267F},{0x268A,0x268F},
    {0x2693,0x2693},{0x26A1,0x26A1},{0x26AA,0x26AB},{0x26BD,0x26BE},
    {0x26C4,0x26C5},{0x26CE,0x26CE},{0x26D4,0x26D4},{0x26EA,0x26EA},
    {0x26F2,0x26F3},{0x26F5,0x26F5},{0x26FA,0x26FA},{0x26FD,0x26FD},
    {0x2705,0x2705},{0x270A,0x270B},{0x2728,0x2728},{0x274C,0x274C},
    {0x274E,0x274E},{0x2753,0x2755},{0x2757,0x2757},{0x2795,0x2797},
    {0x27B0,0x27B0},{0x27BF,0x27BF},{0x2B1B,0x2B1C},{0x2B50,0x2B50},
    {0x2B55,0x2B55},{0x2E80,0x2E99},{0x2E9B,0x2EF3},{0x2F00,0x2FD5},
    {0x2FF0,0x303E},{0x3041,0x3096},{0x3099,0x30FF},{0x3105,0x312F},
    {0x3131,0x318E},{0x3190,0x31E5},{0x31EF,0x321E},{0x3220,0x3247},
    {0x3250,0xA48C},{0xA490,0xA4C6},{0xA960,0xA97C},{0xAC00,0xD7A3},
    {0xF900,0xFAFF},{0xFE10,0xFE19},{0xFE30,0xFE52},{0xFE54,0xFE66},
    {0xFE68,0xFE6B},{0xFF01,0xFF60},{0xFFE0,0xFFE6},{0x16FE0,0x16FE4},
    {0x16FF0,0x16FF1},{0x17000,0x187F7},{0x18800,0x18CD5},{0x18CFF,0x18D08},
    {0x1AFF0,0x1AFF3},{0x1AFF5,0x1AFFB},{0x1AFFD,0x1AFFE},{0x1B000,0x1B122},
    {0x1B132,0x1B132},{0x1B150,0x1B152},{0x1B155,0x1B155},{0x1B164,0x1B167},
    {0x1B170,0x1B2FB},{0x1D300,0x1D356},{0x1D360,0x1D376},{0x1F004,0x1F004},
    {0x1F0CF,0x1F0CF},{0x1F18E,0x1F18E},{0x1F191,0x1F19A},{0x1F200,0x1F202},
    {0x1F210,0x1F23B},{0x1F240,0x1F248},{0x1F250,0x1F251},{0x1F260,0x1F265},
    {0x1F300,0x1F320},{0x1F32D,0x1F335},{0x1F337,0x1F37C},{0x1F37E,0x1F393},
    {0x1F3A0,0x1F3CA},{0x1F3CF,0x1F3D3},{0x1F3E0,0x1F3F0},{0x1F3F4,0x1F3F4},
    {0x1F3F8,0x1F43E},{0x1F440,0x1F440},{0x1F442,0x1F4FC},{0x1F4FF,0x1F53D},
    {0x1F54B,0x1F54E},{0x1F550,0x1F567},{0x1F57A,0x1F57A},{0x1F595,0x1F596},
    {0x1F5A4,0x1F5A4},{0x1F5FB,0x1F64F},{0x1F680,0x1F6C5},{0x1F6CC,0x1F6CC},
    {0x1F6D0,0x1F6D2},{0x1F6D5,0x1F6D7},{0x1F6DC,0x1F6DF},{0x1F6EB,0x1F6EC},
    {0x1F6F4,0x1F6FC},{0x1F7E0,0x1F7EB},{0x1F7F0,0x1F7F0},{0x1F90C,0x1F93A},
    {0x1F93C,0x1F945},{0x1F947,0x1F9FF},{0x1FA70,0x1FA7C},{0x1FA80,0x1FA89},
    {0x1FA8F,0x1FAC6},{0x1FACE,0x1FADC},{0x1FADF,0x1FAE9},{0x1FAF0,0x1FAF8},
    {0x20000,0x2FFFD},{0x30000,0x3FFFD},
};
#define SA_EAW_WF_COUNT (sizeof(sa_eaw_wf)/sizeof(sa_eaw_wf[0]))

/* _disp_width: W/F codepoints count 2, everything else 1 (binary search). */
static int sa_disp_width(const char *s) {
    if (!s) return 0;
    int w = 0;
    const char *p = s;
    while (*p) {
        unsigned int cp = sa_utf8_next(&p);
        size_t lo = 0, hi = SA_EAW_WF_COUNT;
        bool wide = false;
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (cp < sa_eaw_wf[mid][0]) hi = mid;
            else if (cp > sa_eaw_wf[mid][1]) lo = mid + 1;
            else { wide = true; break; }
        }
        w += wide ? 2 : 1;
    }
    return w;
}

/* os.path.splitext extension part (ntpath _splitext: both separators,
 * unified leading-dot scan starting at sepIndex+1). */
static char *sa_splitext(const char *p) {
    if (!p) p = "";
    const char *bs = strrchr(p, '\\');
    const char *fs = strrchr(p, '/');
    long sepIndex = -1;
    if (bs) sepIndex = (long)(bs - p);
    if (fs && (long)(fs - p) > sepIndex) sepIndex = (long)(fs - p);
    const char *dot = strrchr(p, '.');
    long dotIndex = dot ? (long)(dot - p) : -1;
    if (dotIndex > sepIndex) {
        long fi = sepIndex + 1;
        while (fi < dotIndex) {
            if (p[fi] != '.') return sa_strdup(p + dotIndex);
            fi++;
        }
    }
    return sa_strdup("");
}

/* -- int()/float() string parsers (ASCII corpus; ValueError == !ok) -- */
/* Underscores must sit BETWEEN two digits of the post-sign buffer. */
static bool sa_clean_digits(const char *s, char *out, size_t cap) {
    size_t n = strlen(s), k = 0;
    if (n == 0) return false;
    bool any_digit = false;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            if (k + 1 >= cap) return false;
            out[k++] = c;
            any_digit = true;
        } else if (c == '_') {
            if (i == 0 || i + 1 >= n) return false;
            if (!(s[i - 1] >= '0' && s[i - 1] <= '9')) return false;
            if (!(s[i + 1] >= '0' && s[i + 1] <= '9')) return false;
        } else return false;
    }
    out[k] = '\0';
    return any_digit;
}

/* int(str): strip, optional sign, base-10 digits with neighbor underscores.
 * Returns false on ValueError. Overflow is out of domain (|v| < 2^53). */
static bool sa_parse_int(const char *raw, long long *out) {
    char *s = sa_strip(raw);
    if (!s) return false;
    size_t n = strlen(s), i = 0;
    bool neg = false;
    if (n > 0 && (s[0] == '+' || s[0] == '-')) { neg = s[0] == '-'; i = 1; }
    char clean[512];
    bool ok = sa_clean_digits(s + i, clean, sizeof clean);
    free(s);
    if (!ok) return false;
    *out = strtoll(clean, NULL, 10);
    if (neg) *out = -*out;
    return true;
}

/* float(str): strip, optional sign, inf/infinity/nan specials, then
 * (digits[.digits] | .digits)[eE[+-]digits] with neighbor underscores.
 * Returns false on ValueError. */
static bool sa_parse_float(const char *raw, double *out) {
    char *s = sa_strip(raw);
    if (!s) return false;
    size_t n = strlen(s), i = 0;
    bool neg = false;
    if (n > 0 && (s[0] == '+' || s[0] == '-')) { neg = s[0] == '-'; i = 1; }
    char *body = s + i;
    /* specials (case-insensitive) */
    char lb[32];
    size_t bl = strlen(body);
    if (bl < sizeof lb) {
        for (size_t k = 0; k < bl; k++) lb[k] = (char)tolower((unsigned char)body[k]);
        lb[bl] = '\0';
        if (strcmp(lb, "inf") == 0 || strcmp(lb, "infinity") == 0) {
            *out = neg ? -INFINITY : INFINITY;
            free(s);
            return true;
        }
        if (strcmp(lb, "nan") == 0) { *out = NAN; free(s); return true; }
    }
    /* mantissa: digit run, optional . digit run; at least one digit total.
     * Each run is copied into a scratch buffer (no mutation of body). */
    char clean[1024];
    size_t k = 0;
    size_t j = 0;
    bool any = false;
    char tmp[512];
    /* integer part */
    size_t start = j;
    while (j < bl && ((body[j] >= '0' && body[j] <= '9') || body[j] == '_')) j++;
    if (j > start) {
        if (j - start >= sizeof tmp) { free(s); return false; }
        memcpy(tmp, body + start, j - start); tmp[j - start] = '\0';
        char dig[512];
        if (!sa_clean_digits(tmp, dig, sizeof dig)) { free(s); return false; }
        size_t tl = strlen(dig);
        if (k + tl >= sizeof clean) { free(s); return false; }
        memcpy(clean + k, dig, tl); k += tl;
        any = true;
    }
    if (j < bl && body[j] == '.') {
        j++;
        clean[k++] = '.';
        start = j;
        while (j < bl && ((body[j] >= '0' && body[j] <= '9') || body[j] == '_')) j++;
        if (j > start) {
            if (j - start >= sizeof tmp) { free(s); return false; }
            memcpy(tmp, body + start, j - start); tmp[j - start] = '\0';
            char dig[512];
            if (!sa_clean_digits(tmp, dig, sizeof dig)) { free(s); return false; }
            size_t tl = strlen(dig);
            if (k + tl >= sizeof clean) { free(s); return false; }
            memcpy(clean + k, dig, tl); k += tl;
            any = true;
        }
    }
    if (!any) { free(s); return false; }
    /* exponent */
    if (j < bl && (body[j] == 'e' || body[j] == 'E')) {
        j++;
        clean[k++] = 'e';
        if (j < bl && (body[j] == '+' || body[j] == '-')) clean[k++] = body[j++];
        start = j;
        while (j < bl && ((body[j] >= '0' && body[j] <= '9') || body[j] == '_')) j++;
        if (j == start) { free(s); return false; }
        if (j - start >= sizeof tmp) { free(s); return false; }
        memcpy(tmp, body + start, j - start); tmp[j - start] = '\0';
        char dig[512];
        if (!sa_clean_digits(tmp, dig, sizeof dig)) { free(s); return false; }
        size_t tl = strlen(dig);
        if (k + tl >= sizeof clean) { free(s); return false; }
        memcpy(clean + k, dig, tl); k += tl;
    }
    if (j != bl) { free(s); return false; }
    clean[k] = '\0';
    char full[1100];
    snprintf(full, sizeof full, "%s%s", neg ? "-" : "", clean);
    free(s);
    *out = strtod(full, NULL);
    return true;
}

/* _TABLE_SEPARATOR_RE: ^\s*\|?\s*:?-+:?\s*(?:\|\s*:?-+:?\s*){1,}\|?\s*$
 * Deterministic linear matcher (ASCII \s corpus); the greedy runs never
 * need to backtrack because '|' and ':' are unambiguous separators. */
static bool sa_is_ascii_ws_re(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static const char *sa_skip_ws(const char *p) {
    while (*p && sa_is_ascii_ws_re((unsigned char)*p)) p++;
    return p;
}

/* one :?-+:? cell; returns end pointer or NULL */
static const char *sa_sep_cell(const char *p) {
    if (*p == ':') p++;
    const char *d = p;
    while (*p == '-') p++;
    if (p == d) return NULL;
    if (*p == ':') p++;
    return p;
}

static bool sa_table_sep_match(const char *line) {
    if (!line) return false;
    const char *p = sa_skip_ws(line);
    if (*p == '|') p++;
    p = sa_skip_ws(p);
    p = sa_sep_cell(p);
    if (!p) return false;
    p = sa_skip_ws(p);
    int reps = 0;
    while (*p == '|') {
        const char *q = sa_skip_ws(p + 1);
        q = sa_sep_cell(q);
        if (!q) break;
        q = sa_skip_ws(q);
        p = q;
        reps++;
    }
    if (reps < 1) return false;
    if (*p == '|') p++;
    p = sa_skip_ws(p);
    return *p == '\0';
}
/* ================================================================
 *  Pure leaves
 * ================================================================ */

/* PoP: sa_slack_unfurl_kwargs @ plugins/platforms/slack/adapter.py:_slack_unfurl_kwargs */
static json_t *sa_slack_unfurl_kwargs(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *extra = json_obj_get(args, "extra");
    const json_t *settings = sa_truthy(extra) ? extra : NULL;
    if (settings && settings->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", sa_type_name(settings));
        return sa_err("AttributeError", msg);
    }
    json_t *out = json_object();
    static const char *keys[] = {"unfurl_links", "unfurl_media"};
    static const char *bool_words[] = {"1", "0", "true", "false", "yes", "no", "on", "off"};
    static const char *true_words[] = {"1", "true", "yes", "on"};
    for (size_t ki = 0; ki < 2; ki++) {
        const json_t *val = settings ? json_obj_get(settings, keys[ki]) : NULL;
        if (val && val->type == JSON_BOOL) {
            json_set(out, keys[ki], json_bool(val->bool_val));
        } else if (val && val->type == JSON_STRING) {
            char *t = sa_strip(val->str_val);
            char *l = sa_lower(t);
            free(t);
            bool in_words = false, is_true = false;
            for (size_t i = 0; i < 8; i++) if (strcmp(l, bool_words[i]) == 0) in_words = true;
            for (size_t i = 0; i < 4; i++) if (strcmp(l, true_words[i]) == 0) is_true = true;
            if (in_words) json_set(out, keys[ki], json_bool(is_true));
            free(l);
        }
    }
    return out;
}

/* PoP: sa_is_table_row @ plugins/platforms/slack/adapter.py:_is_table_row */
static json_t *sa_is_table_row(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *line = json_get_str(args, "line", "");
    char *s = sa_strip(line);
    bool r = s && s[0] != '\0' && strchr(s, '|') != NULL;
    free(s);
    return json_bool(r);
}

/* PoP: sa_disp_width @ plugins/platforms/slack/adapter.py:_disp_width */
static json_t *sa_disp_width_case(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return json_number((double)sa_disp_width(json_get_str(args, "s", "")));
}

/* PoP: sa_pad @ plugins/platforms/slack/adapter.py:_pad */
static json_t *sa_pad(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *cell = json_get_str(args, "cell", "");
    int width = (int)json_get_num(args, "width", 0);
    int pad = width - sa_disp_width(cell);
    if (pad < 0) pad = 0;
    size_t n = strlen(cell);
    char *o = malloc(n + (size_t)pad + 1);
    if (!o) return json_null();
    memcpy(o, cell, n);
    memset(o + n, ' ', (size_t)pad);
    o[n + (size_t)pad] = '\0';
    json_t *r = json_string(o);
    free(o);
    return r;
}

/* PoP: sa_split_table_row @ plugins/platforms/slack/adapter.py:_split_table_row */
static json_t *sa_split_table_row(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *line = json_get_str(args, "line", "");
    char *s = sa_strip(line);
    size_t n = strlen(s);
    size_t a = 0, b = n;
    if (b > a && s[a] == '|') a++;           /* removeprefix("|") */
    if (b > a && s[b - 1] == '|') b--;       /* removesuffix("|") */
    json_t *out = json_array();
    size_t start = a;
    for (size_t i = a; i <= b; i++) {
        if (i == b || s[i] == '|') {
            char *cell = malloc(i - start + 1);
            if (cell) {
                memcpy(cell, s + start, i - start);
                cell[i - start] = '\0';
                char *t = sa_strip(cell);
                free(cell);
                json_append(out, json_string(t ? t : ""));
                free(t);
            }
            start = i + 1;
        }
    }
    free(s);
    return out;
}

/* _align_table core on a json array of row strings. */
static json_t *sa_align_table_rows(const json_t *rows) {
    size_t nrows = json_len(rows);
    if (nrows < 2) return json_copy(rows);
    /* parsed cells per row */
    json_t **parsed = calloc(nrows, sizeof(json_t *));
    if (!parsed) return json_null();
    size_t n_cols = 0;
    for (size_t i = 0; i < nrows; i++) {
        json_t *a = json_object();
        json_set(a, "line", json_copy(json_get(rows, i)));
        parsed[i] = sa_split_table_row(NULL, a);
        json_free(a);
        if (json_len(parsed[i]) > n_cols) n_cols = json_len(parsed[i]);
    }
    for (size_t i = 0; i < nrows; i++)
        while (json_len(parsed[i]) < n_cols) json_append(parsed[i], json_string(""));
    /* parsed[1] = ["---"] * n_cols */
    json_free(parsed[1]);
    parsed[1] = json_array();
    for (size_t c = 0; c < n_cols; c++) json_append(parsed[1], json_string("---"));
    size_t *widths = calloc(n_cols, sizeof(size_t));
    for (size_t c = 0; c < n_cols; c++) {
        int w = 0;
        for (size_t i = 0; i < nrows; i++) {
            const json_t *cell = json_get(parsed[i], c);
            int cw = sa_disp_width(sa_str(cell));
            if (cw > w) w = cw;
        }
        widths[c] = (size_t)w;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < nrows; i++) {
        /* "| " + " | ".join(cells) + " |" */
        size_t cap = 4;
        for (size_t c = 0; c < n_cols; c++) cap += widths[c] + 3;
        char *line = malloc(cap + 1);
        if (!line) continue;
        size_t k = 0;
        line[k++] = '|'; line[k++] = ' ';
        for (size_t c = 0; c < n_cols; c++) {
            if (c) { line[k++] = ' '; line[k++] = '|'; line[k++] = ' '; }
            const char *cell = sa_str(json_get(parsed[i], c));
            if (!cell) cell = "";
            if (i == 1) {
                memset(line + k, '-', widths[c]);
                k += widths[c];
            } else {
                size_t cl = strlen(cell);
                memcpy(line + k, cell, cl); k += cl;
                int pad = (int)widths[c] - sa_disp_width(cell);
                if (pad < 0) pad = 0;
                memset(line + k, ' ', (size_t)pad); k += (size_t)pad;
            }
        }
        line[k++] = ' '; line[k++] = '|'; line[k] = '\0';
        json_append(out, json_string(line));
        free(line);
    }
    free(widths);
    for (size_t i = 0; i < nrows; i++) json_free(parsed[i]);
    free(parsed);
    return out;
}

/* PoP: sa_align_table @ plugins/platforms/slack/adapter.py:_align_table */
static json_t *sa_align_table(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *rows = json_obj_get(args, "rows");
    if (!rows || rows->type != JSON_ARRAY) return json_array();
    return sa_align_table_rows(rows);
}

/* PoP: sa_wrap_markdown_tables @ plugins/platforms/slack/adapter.py:_wrap_markdown_tables */
static json_t *sa_wrap_markdown_tables(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = json_get_str(args, "text", "");
    if (!text || !text[0] || !strchr(text, '|') || !strchr(text, '-'))
        return json_string(text ? text : "");
    /* split on '\n' */
    json_t *lines = json_array();
    const char *start = text;
    for (const char *p = text; ; p++) {
        if (*p == '\n' || *p == '\0') {
            char *ln = malloc((size_t)(p - start) + 1);
            if (ln) { memcpy(ln, start, (size_t)(p - start)); ln[p - start] = '\0'; }
            json_append(lines, json_string(ln ? ln : ""));
            free(ln);
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    size_t n = json_len(lines);
    json_t *out = json_array();
    bool in_fence = false;
    size_t i = 0;
    while (i < n) {
        const char *line = sa_str(json_get(lines, i));
        char *ls = sa_strip(line); /* lstrip for the ``` test */
        bool fence = ls && strncmp(ls, "```", 3) == 0;
        free(ls);
        if (fence) {
            in_fence = !in_fence;
        } else if (!in_fence && strchr(line, '|') && i + 1 < n &&
                   sa_table_sep_match(sa_str(json_get(lines, i + 1)))) {
            json_t *block = json_array();
            json_append(block, json_copy(json_get(lines, i)));
            json_append(block, json_copy(json_get(lines, i + 1)));
            size_t j = i + 2;
            while (j < n) {
                const char *lj = sa_str(json_get(lines, j));
                char *sj = sa_strip(lj);
                bool row = sj && sj[0] && strchr(sj, '|');
                free(sj);
                if (!row) break;
                json_append(block, json_copy(json_get(lines, j)));
                j++;
            }
            json_append(out, json_string("```"));
            json_t *aligned = sa_align_table_rows(block);
            for (size_t a = 0; a < json_len(aligned); a++)
                json_append(out, json_copy(json_get(aligned, a)));
            json_free(aligned);
            json_free(block);
            json_append(out, json_string("```"));
            i = j;
            continue;
        }
        json_append(out, json_copy(json_get(lines, i)));
        i++;
    }
    /* "\n".join(out) */
    size_t cap = 1;
    for (size_t a = 0; a < json_len(out); a++) cap += strlen(sa_str(json_get(out, a))) + 1;
    char *joined = malloc(cap);
    if (joined) {
        size_t k = 0;
        for (size_t a = 0; a < json_len(out); a++) {
            if (a) joined[k++] = '\n';
            const char *s = sa_str(json_get(out, a));
            size_t sl = strlen(s);
            memcpy(joined + k, s, sl); k += sl;
        }
        joined[k] = '\0';
    }
    json_t *r = json_string(joined ? joined : "");
    free(joined);
    json_free(out);
    json_free(lines);
    return r;
}

/* _collect_slack_block_mentions recursive walker. */
static void sa_collect_walk(const json_t *node, bool in_quote, json_t *mentions) {
    if (!node) return;
    if (node->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(node); i++)
            sa_collect_walk(json_get(node, i), in_quote, mentions);
        return;
    }
    if (node->type != JSON_OBJECT) return;
    const char *ntype = sa_str(json_obj_get(node, "type"));
    bool quoted = in_quote || (ntype && strcmp(ntype, "rich_text_quote") == 0);
    if (ntype && strcmp(ntype, "user") == 0 && !quoted) {
        const json_t *uid = json_obj_get(node, "user_id");
        if (sa_truthy(uid)) {
            char *us = sa_str_of(uid);
            if (us) {
                size_t n = strlen(us);
                char *m = malloc(n + 4);
                if (m) { snprintf(m, n + 4, "<@%s>", us); json_append(mentions, json_string(m)); free(m); }
                free(us);
            }
        }
    }
    static const char *child_keys[] = {"elements", "element"};
    for (size_t i = 0; i < 2; i++) {
        const json_t *child = json_obj_get(node, child_keys[i]);
        if (child) sa_collect_walk(child, quoted, mentions);
    }
}

/* PoP: sa_collect_slack_block_mentions @ plugins/platforms/slack/adapter.py:_collect_slack_block_mentions */
static json_t *sa_collect_slack_block_mentions(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *blocks = json_obj_get(args, "blocks");
    json_t *mentions = json_array();
    sa_collect_walk(blocks, false, mentions);
    return mentions;
}

/* PoP: sa_slack_mention_detection_text @ plugins/platforms/slack/adapter.py:_slack_mention_detection_text */
static json_t *sa_slack_mention_detection_text(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *event = json_obj_get(args, "event");
    bool ev_ok = event && event->type == JSON_OBJECT;
    const json_t *textn = ev_ok ? json_obj_get(event, "text") : NULL;
    char *flat = sa_truthy(textn) ? sa_str_of(textn) : sa_strdup("");
    if (!flat) flat = sa_strdup("");
    const json_t *blocks = ev_ok ? json_obj_get(event, "blocks") : NULL;
    json_t *extra = json_array();
    if (sa_truthy(blocks)) {
        json_t *mentions = json_array();
        sa_collect_walk(blocks, false, mentions);
        for (size_t i = 0; i < json_len(mentions); i++) {
            const char *m = sa_str(json_get(mentions, i));
            if (m && strstr(flat, m) == NULL) json_append(extra, json_string(m));
        }
        json_free(mentions);
    }
    json_t *r;
    if (json_len(extra) > 0) {
        char *fs = sa_strip(flat);
        size_t cap = strlen(fs) + 2;
        for (size_t i = 0; i < json_len(extra); i++) cap += strlen(sa_str(json_get(extra, i))) + 1;
        char *buf = malloc(cap);
        if (buf) {
            size_t k = 0;
            size_t fl = strlen(fs);
            memcpy(buf + k, fs, fl); k += fl;
            buf[k++] = '\n';
            for (size_t i = 0; i < json_len(extra); i++) {
                if (i) buf[k++] = ' ';
                const char *m = sa_str(json_get(extra, i));
                size_t ml = strlen(m);
                memcpy(buf + k, m, ml); k += ml;
            }
            buf[k] = '\0';
        }
        free(fs);
        char *t = sa_strip(buf ? buf : "");
        r = json_string(t ? t : "");
        free(t);
        free(buf);
    } else {
        r = json_string(flat);
    }
    free(flat);
    json_free(extra);
    return r;
}

/* PoP: sa_slack_permalink_path @ plugins/platforms/slack/adapter.py:_slack_permalink_path */
static json_t *sa_slack_permalink_path(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *ch = json_obj_get(args, "channel_id");
    const json_t *ts = json_obj_get(args, "message_ts");
    if (!sa_truthy(ch) || !sa_truthy(ts)) return json_string("");
    char *cs = sa_str_of(ch);
    char *tss = sa_str_of(ts);
    if (!cs || !tss) { free(cs); free(tss); return json_string(""); }
    /* str(ts).replace('.', '') */
    char *clean = malloc(strlen(tss) + 1);
    size_t k = 0;
    if (clean) {
        for (const char *p = tss; *p; p++) if (*p != '.') clean[k++] = *p;
        clean[k] = '\0';
    }
    size_t cap = strlen(cs) + (clean ? strlen(clean) : 0) + 32;
    char *buf = malloc(cap);
    if (buf) snprintf(buf, cap, "archives/%s/p%s", cs, clean ? clean : "");
    json_t *r = json_string(buf ? buf : "");
    free(buf); free(clean); free(cs); free(tss);
    return r;
}

/* PoP: sa_str_or_empty @ plugins/platforms/slack/adapter.py:_str_or_empty */
static json_t *sa_str_or_empty(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *v = json_obj_get(args, "value");
    if (!sa_truthy(v)) return json_string("");
    char *s = sa_str_of(v);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* PoP: sa_int_or_zero @ plugins/platforms/slack/adapter.py:_int_or_zero */
static json_t *sa_int_or_zero(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *v = json_obj_get(args, "value");
    if (v && v->type == JSON_NUMBER)
        return json_number((double)(long long)v->num_val); /* int() truncates */
    if (v && v->type == JSON_BOOL)
        return json_number(v->bool_val ? 1.0 : 0.0);
    if (v && v->type == JSON_STRING) {
        long long out;
        if (sa_parse_int(v->str_val, &out)) return json_number((double)out);
        return json_number(0.0); /* ValueError swallowed */
    }
    char msg[160];
    snprintf(msg, sizeof msg,
             "int() argument must be a string, a bytes-like object or a real number, not '%s'",
             sa_type_name(v));
    return sa_err("TypeError", msg);
}

/* PoP: sa_first_truthy @ plugins/platforms/slack/adapter.py:_first_truthy */
static json_t *sa_first_truthy(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *mapping = json_obj_get(args, "mapping");
    const json_t *keys = json_obj_get(args, "keys");
    if (mapping && mapping->type == JSON_OBJECT && keys && keys->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(keys); i++) {
            const char *key = sa_str(json_get(keys, i));
            if (!key) continue;
            const json_t *value = json_obj_get(mapping, key);
            if (sa_truthy(value)) return json_copy(value);
        }
    }
    return json_null();
}

/* PoP: sa_slack_str_field @ plugins/platforms/slack/adapter.py:_slack_str_field */
static json_t *sa_slack_str_field(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *el = json_obj_get(args, "el");
    const char *name = json_get_str(args, "name", "");
    const json_t *value = (el && el->type == JSON_OBJECT) ? json_obj_get(el, name) : NULL;
    return json_string(sa_str(value) ? sa_str(value) : "");
}

/* _slack_permalink_path on raw nodes (shared with the inline renderer). */
static char *sa_permalink_of(const json_t *ch, const json_t *ts) {
    json_t *a = json_object();
    json_set(a, "channel_id", ch ? json_copy(ch) : json_null());
    json_set(a, "message_ts", ts ? json_copy(ts) : json_null());
    json_t *r = sa_slack_permalink_path(NULL, a);
    json_free(a);
    char *s = sa_strdup(sa_str(r));
    json_free(r);
    return s;
}

/* _slack_str_field on a raw element node (shared with the inline renderer). */
static const char *sa_strfield(const json_t *el, const char *nm) {
    const json_t *v = (el && el->type == JSON_OBJECT) ? json_obj_get(el, nm) : NULL;
    const char *s = sa_str(v);
    return s ? s : "";
}

/* PoP: sa_render_slack_inline_element @ plugins/platforms/slack/adapter.py:_render_slack_inline_element */
static json_t *sa_render_slack_inline_element(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *el = json_obj_get(args, "el");
    if (!el || el->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", sa_type_name(el));
        return sa_err("AttributeError", msg);
    }
    const char *el_type = sa_str(json_obj_get(el, "type"));
    if (!el_type) el_type = ""; /* absent -> ""; non-str types match nothing */
    if (strcmp(el_type, "text") == 0) return json_string(sa_strfield(el, "text"));
    if (strcmp(el_type, "color") == 0) return json_string(sa_strfield(el, "value"));
    static const struct { const char *type, *pre, *post, *key, *def; } ents[] = {
        {"channel", "<#", ">", "channel_id", ""},
        {"user", "<@", ">", "user_id", ""},
        {"usergroup", "<!subteam^", ">", "usergroup_id", ""},
        {"team", "<!team^", ">", "team_id", ""},
        {"emoji", ":", ":", "name", ""},
        {"broadcast", "<!", ">", "range", "here"},
    };
    for (size_t i = 0; i < sizeof ents / sizeof ents[0]; i++) {
        if (strcmp(el_type, ents[i].type) == 0) {
            const json_t *v = json_obj_get(el, ents[i].key);
            char *vs = v ? sa_str_of(v) : sa_strdup(ents[i].def);
            if (!vs) vs = sa_strdup("");
            size_t cap = strlen(ents[i].pre) + strlen(vs) + strlen(ents[i].post) + 1;
            char *buf = malloc(cap);
            if (buf) snprintf(buf, cap, "%s%s%s", ents[i].pre, vs, ents[i].post);
            json_t *r = json_string(buf ? buf : "");
            free(buf); free(vs);
            return r;
        }
    }
    if (strcmp(el_type, "date") == 0) {
        const char *fb = sa_strfield(el, "fallback");
        if (fb[0]) return json_string(fb);
    }
    const char *url = sa_strfield(el, "url");
    const char *label = sa_strfield(el, "text");
    if (!label[0]) label = sa_strfield(el, "fallback");
    char *url_buf = NULL;
    if (!url[0] && strcmp(el_type, "message_mention") == 0) {
        url_buf = sa_permalink_of(json_obj_get(el, "channel_id"), json_obj_get(el, "message_ts"));
        url = url_buf;
    }
    json_t *r;
    if (url[0]) {
        if (label[0] && strcmp(label, url) != 0) {
            size_t cap = strlen(label) + strlen(url) + 4;
            char *buf = malloc(cap);
            if (buf) snprintf(buf, cap, "%s (%s)", label, url);
            r = json_string(buf ? buf : "");
            free(buf);
        } else {
            r = json_string(url);
        }
    } else {
        r = json_string(label);
    }
    free(url_buf);
    return r;
}

/* PoP: sa_render_inline_elements @ plugins/platforms/slack/adapter.py:_render_inline_elements */
static json_t *sa_render_inline_elements(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *elements = json_obj_get(args, "elements");
    size_t n = (elements && elements->type == JSON_ARRAY) ? json_len(elements) : 0;
    size_t cap = 1;
    json_t *parts = json_array();
    for (size_t i = 0; i < n; i++) {
        json_t *a = json_object();
        json_set(a, "el", json_copy(json_get(elements, i)));
        json_t *r = sa_render_slack_inline_element(NULL, a);
        json_free(a);
        if (sa_is_raise(r)) { json_free(parts); return r; }
        const char *s = sa_str(r);
        cap += s ? strlen(s) : 0;
        json_append(parts, r);
    }
    char *buf = malloc(cap);
    if (buf) {
        size_t k = 0;
        for (size_t i = 0; i < json_len(parts); i++) {
            const char *s = sa_str(json_get(parts, i));
            size_t sl = s ? strlen(s) : 0;
            if (s) memcpy(buf + k, s, sl);
            k += sl;
        }
        buf[k] = '\0';
    }
    json_t *r = json_string(buf ? buf : "");
    free(buf);
    json_free(parts);
    return r;
}

/* PoP: sa_unescape_slack_entities @ plugins/platforms/slack/adapter.py:_unescape_slack_entities */
static json_t *sa_unescape_slack_entities(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = json_get_str(args, "text", "");
    if (!text) text = "";
    size_t n = strlen(text);
    char *o = malloc(n + 1);
    if (!o) return json_string("");
    size_t k = 0;
    for (size_t i = 0; i < n; ) {
        if (text[i] == '&' && i + 1 < n) {
            if (strncmp(text + i, "&amp;", 5) == 0) { o[k++] = '&'; i += 5; continue; }
            if (strncmp(text + i, "&lt;", 4) == 0) { o[k++] = '<'; i += 4; continue; }
            if (strncmp(text + i, "&gt;", 4) == 0) { o[k++] = '>'; i += 4; continue; }
        }
        o[k++] = text[i++];
    }
    o[k] = '\0';
    json_t *r = json_string(o);
    free(o);
    return r;
}

/* PoP: sa_resolve_slack_audio_ext @ plugins/platforms/slack/adapter.py:_resolve_slack_audio_ext */
static json_t *sa_resolve_slack_audio_ext(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *fo = json_obj_get(args, "file_obj");
    const json_t *namen = (fo && fo->type == JSON_OBJECT) ? json_obj_get(fo, "name") : NULL;
    char *name = sa_truthy(namen) ? sa_str_of(namen) : sa_strdup("");
    if (!name) name = sa_strdup("");
    char *ns = sa_strip(name);
    free(name);
    char *ext = sa_splitext(ns);
    free(ns);
    char *lext = sa_lower(ext);
    free(ext);
    static const char *stt[] = {".mp3", ".mp4", ".mpeg", ".mpga", ".m4a", ".wav",
                                ".webm", ".ogg", ".aac", ".flac"};
    for (size_t i = 0; i < sizeof stt / sizeof stt[0]; i++) {
        if (strcmp(lext, stt[i]) == 0) {
            json_t *r = json_string(lext);
            free(lext);
            return r;
        }
    }
    free(lext);
    const json_t *mimen = json_obj_get(args, "mimetype");
    char *mime = sa_truthy(mimen) ? sa_str_of(mimen) : sa_strdup("");
    if (!mime) mime = sa_strdup("");
    /* split(";", 1)[0].strip().lower() */
    char *semi = strchr(mime, ';');
    if (semi) *semi = '\0';
    char *ms = sa_strip(mime);
    free(mime);
    char *key = sa_lower(ms);
    free(ms);
    static const struct { const char *mime, *ext; } map[] = {
        {"audio/ogg", ".ogg"}, {"audio/opus", ".ogg"}, {"audio/mpeg", ".mp3"},
        {"audio/mp3", ".mp3"}, {"audio/wav", ".wav"}, {"audio/x-wav", ".wav"},
        {"audio/webm", ".webm"}, {"audio/mp4", ".m4a"}, {"audio/x-m4a", ".m4a"},
        {"audio/m4a", ".m4a"}, {"audio/aac", ".m4a"}, {"audio/flac", ".flac"},
        {"audio/x-flac", ".flac"},
    };
    const char *res = ".m4a";
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
        if (strcmp(key, map[i].mime) == 0) { res = map[i].ext; break; }
    free(key);
    return json_string(res);
}

/* PoP: sa_is_slack_voice_clip @ plugins/platforms/slack/adapter.py:_is_slack_voice_clip */
static json_t *sa_is_slack_voice_clip(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *fo = json_obj_get(args, "file_obj");
    bool fo_ok = fo && fo->type == JSON_OBJECT;
    const json_t *sub = fo_ok ? json_obj_get(fo, "subtype") : NULL;
    char *s1 = sa_truthy(sub) ? sa_str_of(sub) : sa_strdup("");
    if (!s1) s1 = sa_strdup("");
    char *s1s = sa_strip(s1); free(s1);
    char *s1l = sa_lower(s1s); free(s1s);
    bool r = strcmp(s1l, "slack_audio") == 0;
    free(s1l);
    if (!r) {
        const json_t *nm = json_obj_get(fo, "name");
        char *s2 = sa_truthy(nm) ? sa_str_of(nm) : sa_strdup("");
        if (!s2) s2 = sa_strdup("");
        char *s2s = sa_strip(s2); free(s2);
        char *s2l = sa_lower(s2s); free(s2s);
        r = strncmp(s2l, "audio_message", 13) == 0;
        free(s2l);
    }
    return json_bool(r);
}

/* PoP: sa_attachment_label @ plugins/platforms/slack/adapter.py:_attachment_label */
static json_t *sa_attachment_label(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *fo = json_obj_get(args, "file_obj");
    if (!sa_truthy(fo) || fo->type != JSON_OBJECT) fo = NULL;
    const json_t *name = fo ? json_obj_get(fo, "name") : NULL;
    const json_t *id = fo ? json_obj_get(fo, "id") : NULL;
    const json_t *pick = sa_truthy(name) ? name : (sa_truthy(id) ? id : NULL);
    if (!pick) return json_string("this attachment");
    char *s = sa_str_of(pick);
    json_t *r = json_string(s ? s : "this attachment");
    free(s);
    return r;
}

/* PoP: sa_standalone_post_kwargs @ plugins/platforms/slack/adapter.py:_standalone_post_kwargs */
static json_t *sa_standalone_post_kwargs(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *out = json_object();
    const json_t *chat = json_obj_get(args, "chat_id");
    const json_t *text = json_obj_get(args, "text");
    json_set(out, "channel", chat ? json_copy(chat) : json_null());
    json_set(out, "text", text ? json_copy(text) : json_null());
    json_set(out, "mrkdwn", json_bool(true));
    const json_t *unfurl = json_obj_get(args, "unfurl_kwargs");
    if (unfurl && unfurl->type == JSON_OBJECT) {
        for (size_t i = 0; i < unfurl->c.count; i++)
            json_set(out, unfurl->c.keys[i], json_copy(unfurl->c.items[i]));
    }
    const json_t *thread = json_obj_get(args, "thread_id");
    if (sa_truthy(thread)) json_set(out, "thread_ts", json_copy(thread));
    return out;
}

/* PoP: sa_slack_response_payload @ plugins/platforms/slack/adapter.py:_slack_response_payload */
static json_t *sa_slack_response_payload(sa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *resp = json_obj_get(args, "response");
    if (resp && resp->type == JSON_OBJECT) {
        const json_t *tag = json_obj_get(resp, "__obj__");
        if (tag && tag->type == JSON_OBJECT) {
            const json_t *data = json_obj_get(tag, "data");
            if (data && data->type == JSON_OBJECT) return json_copy(data);
            return json_object();
        }
        return json_copy(resp); /* isinstance(response, dict) */
    }
    return json_object(); /* getattr(..., "data", None) -> None -> {} */
}
/* ================================================================
 *  Seamed flows
 * ================================================================ */

/* PoP: sa_model_picker_expired_notice @ plugins/platforms/slack/adapter.py:_model_picker_expired_notice */
static json_t *sa_model_picker_expired_notice(sa_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set(a, "key", json_string("platform.shared.model_picker_expired"));
    return sa_call(ctx, "t", a);
}

/* PoP: sa_rewrite_known_bang_command @ plugins/platforms/slack/adapter.py:_rewrite_known_bang_command */
static json_t *sa_rewrite_known_bang_command(sa_ctx_t *ctx, const json_t *args) {
    const json_t *textn = json_obj_get(args, "text");
    const char *text = sa_str(textn);
    if (!text) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'startswith'", sa_type_name(textn));
        return sa_err("AttributeError", msg);
    }
    if (text[0] != '!') return json_string(text);
    const char *rest = text + 1;
    /* first_token = rest.split(maxsplit=1)[0]; IndexError -> swallowed */
    const char *p = rest;
    while (*p && sa_is_ws((unsigned char)*p)) p++;
    if (!*p) return json_string(text); /* empty split -> IndexError -> except */
    const char *tok_start = p;
    while (*p && !sa_is_ws((unsigned char)*p)) p++;
    size_t tok_len = (size_t)(p - tok_start);
    /* cmd_name = first_token.split("@", 1)[0].lower() */
    size_t at = 0;
    while (at < tok_len && tok_start[at] != '@') at++;
    char *cmd = malloc(at + 1);
    if (!cmd) return json_string(text);
    for (size_t i = 0; i < at; i++) cmd[i] = (char)tolower((unsigned char)tok_start[i]);
    cmd[at] = '\0';
    json_t *r = NULL;
    if (cmd[0] && !strchr(cmd, '/')) {
        json_t *a = json_object();
        json_set(a, "name", json_string(cmd));
        json_t *known = sa_call(ctx, "is_gateway_known_command", a);
        if (sa_is_raise(known)) {
            if (sa_raise_passes(known)) { free(cmd); return known; }
            json_free(known); /* except Exception: pass */
        } else if (sa_truthy(known)) {
            size_t n = strlen(rest);
            char *buf = malloc(n + 2);
            if (buf) { buf[0] = '/'; memcpy(buf + 1, rest, n); buf[n + 1] = '\0'; }
            r = json_string(buf ? buf : text);
            free(buf);
            json_free(known);
            free(cmd);
            return r;
        } else {
            json_free(known);
        }
    }
    free(cmd);
    return json_string(text);
}

/* PoP: sa_slack_dedup_ttl_seconds @ plugins/platforms/slack/adapter.py:_slack_dedup_ttl_seconds */
static json_t *sa_slack_dedup_ttl_seconds(sa_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set(a, "key", json_string("SLACK_DEDUP_TTL_SECONDS"));
    json_set(a, "default", json_string(""));
    json_t *raw = sa_call(ctx, "get_scoped_secret", a);
    if (sa_is_raise(raw)) return raw;
    if (sa_truthy(raw)) {
        double value = 0.0;
        bool valueerror = false;
        if (raw->type == JSON_STRING) {
            if (!sa_parse_float(raw->str_val, &value)) valueerror = true;
        } else if (raw->type == JSON_NUMBER) {
            value = raw->num_val;
        } else if (raw->type == JSON_BOOL) {
            value = raw->bool_val ? 1.0 : 0.0;
        } else {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "float() argument must be a string or a real number, not '%s'",
                     sa_type_name(raw));
            json_free(raw);
            return sa_err("TypeError", msg);
        }
        if (valueerror) {
            /* logger.warning(fmt, raw) -- args recorded VERBATIM (%r at emission) */
            json_t *la = json_object();
            json_set(la, "msg", json_string("[Slack] Invalid SLACK_DEDUP_TTL_SECONDS=%r; using default"));
            json_t *largs = json_array();
            json_append(largs, json_copy(raw));
            json_set(la, "args", largs);
            json_t *w = sa_call(ctx, "logger.warning", la);
            json_free(raw);
            if (sa_is_raise(w)) return w;
            json_free(w);
            return json_number(SA_DEDUP_TTL_DEFAULT);
        }
        json_free(raw);
        if (value > 0.0) return json_number(value); /* NaN > 0 is false */
        return json_number(SA_DEDUP_TTL_DEFAULT);
    }
    json_free(raw);
    return json_number(SA_DEDUP_TTL_DEFAULT);
}

/* PoP: sa_resolve_slack_proxy_url @ plugins/platforms/slack/adapter.py:_resolve_slack_proxy_url */
static json_t *sa_resolve_slack_proxy_url(sa_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *p = sa_call(ctx, "resolve_proxy_url", json_object());
    if (sa_is_raise(p)) return p;
    if (!sa_truthy(p)) { json_free(p); return json_null(); }
    const char *ps = sa_str(p);
    if (!ps) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'lower'", sa_type_name(p));
        json_free(p);
        return sa_err("AttributeError", msg);
    }
    char *norm = sa_lower(ps);
    bool http = strncmp(norm, "http://", 7) == 0 || strncmp(norm, "https://", 8) == 0;
    free(norm);
    if (!http) {
        json_t *sa = json_object();
        json_set(sa, "url", json_copy(p));
        json_t *s = sa_call(ctx, "safe_proxy_url_for_log", sa);
        if (sa_is_raise(s)) { json_free(p); return s; }
        json_t *la = json_object();
        json_set(la, "msg", json_string("[Slack] Ignoring unsupported proxy scheme for Slack transport: %s"));
        json_t *largs = json_array();
        json_append(largs, s); /* owned transfer */
        json_set(la, "args", largs);
        json_t *w = sa_call(ctx, "logger.info", la);
        json_free(p);
        if (sa_is_raise(w)) return w;
        json_free(w);
        return json_null();
    }
    static const char *hosts[] = {"slack.com", "files.slack.com", "wss-primary.slack.com"};
    for (size_t i = 0; i < 3; i++) {
        json_t *ha = json_object();
        json_set(ha, "host", json_string(hosts[i]));
        json_t *r = sa_call(ctx, "is_host_excluded_by_no_proxy", ha);
        if (sa_is_raise(r)) { json_free(p); return r; }
        bool excl = sa_truthy(r);
        json_free(r);
        if (excl) {
            json_t *la = json_object();
            json_set(la, "msg", json_string("[Slack] NO_PROXY bypasses Slack proxy configuration"));
            json_set(la, "args", json_array());
            json_t *w = sa_call(ctx, "logger.info", la);
            json_free(p);
            if (sa_is_raise(w)) return w;
            json_free(w);
            return json_null();
        }
    }
    return p; /* original-case URL */
}

/* PoP: sa_trim_slack_dm_cache @ plugins/platforms/slack/adapter.py:_trim_slack_dm_cache */
static json_t *sa_trim_slack_dm_cache(sa_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *cache = json_obj_get(ctx->state, "dm_cache");
    if (!cache || cache->type != JSON_OBJECT) {
        cache = json_object();
        json_set(ctx->state, "dm_cache", cache);
    }
    while (cache->c.count > SA_DM_CACHE_MAX)
        json_obj_del(cache, cache->c.keys[0]); /* oldest-insertion-first */
    return json_null();
}

/* POSIX home / "slack_tokens.json" (Path join projection). */
static char *sa_posix_join(const char *home, const char *name) {
    size_t hn = strlen(home);
    while (hn > 0 && home[hn - 1] == '/') hn--;
    if (hn == 0) {
        if (home[0] == '/') {
            char *o = malloc(strlen(name) + 2);
            if (o) snprintf(o, strlen(name) + 2, "/%s", name);
            return o;
        }
        return sa_strdup(name);
    }
    size_t cap = hn + strlen(name) + 2;
    char *o = malloc(cap);
    if (o) snprintf(o, cap, "%.*s/%s", (int)hn, home, name);
    return o;
}

static bool sa_tokens_contains(const json_t *tokens, const json_t *tok) {
    for (size_t i = 0; i < json_len(tokens); i++)
        if (sa_json_eq(json_get(tokens, i), tok)) return true;
    return false;
}

/* PoP: sa_load_slack_bot_tokens @ plugins/platforms/slack/adapter.py:_load_slack_bot_tokens */
static json_t *sa_load_slack_bot_tokens(sa_ctx_t *ctx, const json_t *args) {
    const json_t *rtn = json_obj_get(args, "raw_token");
    const char *raw_token = sa_str(rtn);
    if (!raw_token) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'split'", sa_type_name(rtn));
        return sa_err("AttributeError", msg);
    }
    bool quiet = json_get_bool(args, "quiet", false);
    json_t *tokens = json_array();
    /* [t.strip() for t in raw_token.split(",") if t.strip()] */
    const char *start = raw_token;
    for (const char *p = raw_token; ; p++) {
        if (*p == ',' || *p == '\0') {
            char *piece = malloc((size_t)(p - start) + 1);
            if (piece) {
                memcpy(piece, start, (size_t)(p - start));
                piece[p - start] = '\0';
                char *t = sa_strip(piece);
                if (t && t[0]) json_append(tokens, json_string(t));
                free(t);
                free(piece);
            }
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    /* try: home / file / exists -- except Exception: quiet ? tokens : raise */
    char *path = NULL;
    json_t *err1 = NULL;
    json_t *present = NULL;
    json_t *home = sa_call(ctx, "get_hermes_home", json_object());
    if (sa_is_raise(home)) {
        err1 = home;
    } else if (sa_str(home)) {
        path = sa_posix_join(sa_str(home), "slack_tokens.json");
        json_t *ea = json_object();
        json_set(ea, "path", json_string(path ? path : ""));
        present = sa_call(ctx, "tokens_file.exists", ea);
        if (sa_is_raise(present)) { err1 = present; present = NULL; }
        json_free(home);
    } else {
        char msg[160];
        snprintf(msg, sizeof msg, "unsupported operand type(s) for /: '%s' and 'str'",
                 sa_type_name(home));
        err1 = sa_err("TypeError", msg);
        json_free(home);
    }
    if (err1) {
        if (sa_raise_passes(err1)) { free(path); json_free(tokens); return err1; }
        if (quiet) { json_free(err1); free(path); return tokens; }
        free(path);
        return err1;
    }
    if (!sa_truthy(present)) {
        json_free(present);
        free(path);
        return tokens;
    }
    json_free(present);
    /* try: warn / read / parse / merge -- except Exception as e: warn log */
    json_t *err2 = NULL;
    if (!quiet) {
        json_t *wa = json_object();
        json_set(wa, "path", json_string(path ? path : ""));
        json_set(wa, "label", json_string("[Slack]"));
        json_t *w = sa_call(ctx, "warn_if_credential_file_broadly_readable", wa);
        if (sa_is_raise(w)) {
            if (sa_raise_passes(w)) { free(path); json_free(tokens); return w; }
            err2 = w;
        }
    }
    if (!err2) {
        json_t *ra = json_object();
        json_set(ra, "path", json_string(path ? path : ""));
        json_set(ra, "encoding", json_string("utf-8-sig"));
        json_t *text = sa_call(ctx, "tokens_file.read_text", ra);
        if (sa_is_raise(text)) {
            if (sa_raise_passes(text)) { free(path); json_free(tokens); return text; }
            err2 = text;
        } else if (!sa_str(text)) {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "the JSON object must be str, bytes or bytearray, not %s",
                     sa_type_name(text));
            err2 = sa_err("TypeError", msg);
            json_free(text);
        } else {
            const char *t = sa_str(text);
            if (strncmp(t, "\xef\xbb\xbf", 3) == 0) t += 3; /* utf-8-sig BOM */
            json_t *saved = json_parse(t, NULL);
            if (!saved) {
                /* probed JSONDecodeError messages: "" and whitespace-only */
                size_t tl = strlen(t);
                size_t ws = 0;
                while (ws < tl && sa_is_ws((unsigned char)t[ws])) ws++;
                char msg[128];
                if (ws == tl)
                    snprintf(msg, sizeof msg,
                             "Expecting value: line 1 column %zu (char %zu)", tl + 1, tl);
                else
                    snprintf(msg, sizeof msg,
                             "Expecting value: line 1 column %zu (char %zu)", ws + 1, ws);
                err2 = sa_err("JSONDecodeError", msg);
            } else if (saved->type != JSON_OBJECT) {
                char msg[128];
                snprintf(msg, sizeof msg, "'%s' object has no attribute 'items'",
                         sa_type_name(saved));
                err2 = sa_err("AttributeError", msg);
                json_free(saved);
            } else {
                for (size_t i = 0; i < saved->c.count && !err2; i++) {
                    const char *team_id = saved->c.keys[i];
                    const json_t *entry = saved->c.items[i];
                    bool entry_obj = entry && entry->type == JSON_OBJECT;
                    const json_t *tok = entry_obj ? json_obj_get(entry, "token") : NULL;
                    if (sa_truthy(tok) && !sa_tokens_contains(tokens, tok)) {
                        json_append(tokens, json_copy(tok));
                        if (!quiet) {
                            const json_t *tn = entry_obj ? json_obj_get(entry, "team_name") : NULL;
                            json_t *label = tn ? json_copy(tn) : json_string(team_id);
                            json_t *la = json_object();
                            json_set(la, "msg", json_string("[Slack] Loaded saved token for workspace %s"));
                            json_t *largs = json_array();
                            json_append(largs, label);
                            json_set(la, "args", largs);
                            json_t *l = sa_call(ctx, "logger.info", la);
                            if (sa_is_raise(l)) {
                                if (sa_raise_passes(l)) {
                                    json_free(saved); json_free(text);
                                    free(path); json_free(tokens);
                                    return l;
                                }
                                err2 = l;
                            } else {
                                json_free(l);
                            }
                        }
                    }
                }
                json_free(saved);
            }
            json_free(text);
        }
    }
    if (err2) {
        if (sa_raise_passes(err2)) { free(path); json_free(tokens); return err2; }
        if (!quiet) {
            json_t *la = json_object();
            json_set(la, "msg", json_string("[Slack] Failed to read %s: %s"));
            json_t *largs = json_array();
            json_append(largs, json_string(path ? path : ""));
            json_append(largs, json_string(json_get_str(err2, "message", "")));
            json_set(la, "args", largs);
            json_t *w = sa_call(ctx, "logger.warning", la);
            if (sa_is_raise(w)) { json_free(err2); free(path); json_free(tokens); return w; }
            json_free(w);
        }
        json_free(err2);
    }
    free(path);
    return tokens;
}