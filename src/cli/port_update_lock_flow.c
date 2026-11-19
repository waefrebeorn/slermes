/* port_update_lock_flow.c — FLOW-core port of hermes_cli/update_lock.py.
 * See include/port_update_lock_flow.h for the seam table and the
 * representation contract. libjson-only; no real I/O inside the decision core.
 */
#include "port_update_lock_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#define ULF_U32_MAX 4294967295ULL
#define ULF_U64_MAX 18446744073709551615ULL
#define ULF_MAX_AGE 1200.0
#define ULF_CT_TOL 2.0
#define ULF_OWN_EPS 0.005
#define ULF_WHOLE_CT 1.0
#define ULF_EMPTY_GRACE 5.0
#define ULF_BIGINT_SAFE 9007199254740992ULL  /* 2^53 */

/* ── seam dispatch ─────────────────────────────────────────────────────── */
json_t *ulf_call(ulf_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_append(io->trace, ev);
    if (!io->call) return json_null();
    return io->call(io, name, args);
}

bool ulf_raised(const json_t *result) {
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
static bool j_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

/* u64 projection: numbers past 2^53 become {"__bigint__": decimal} on both
 * sides (libjson doubles cannot hold them exactly). */
static json_t *u64_node(unsigned long long v) {
    if (v <= ULF_BIGINT_SAFE) return json_number((double)v);
    json_t *o = json_object();
    char buf[32];
    snprintf(buf, sizeof buf, "%" PRIu64, v);
    json_set_hdr(o, "__bigint__", buf);
    return o;
}

static bool node_to_u64(const json_t *v, unsigned long long *out) {
    if (j_is_num(v)) {
        if (v->num_val < 0 || v->num_val > 1.8446744073709552e19) return false;
        *out = (unsigned long long)v->num_val;
        return true;
    }
    if (j_is_obj(v)) {
        const char *s = json_get_str(v, "__bigint__", NULL);
        if (!s) return false;
        char *end = NULL;
        unsigned long long x = strtoull(s, &end, 10);
        if (!end || *end != '\0') return false;
        *out = x;
        return true;
    }
    return false;
}

/* ── exception classification (OSError family as the live except clauses) ─ */
static const char *exc_type(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static bool exc_is_oserror(const json_t *r) {
    const char *t = exc_type(r);
    return strcmp(t, "OSError") == 0 || strcmp(t, "PermissionError") == 0
        || strcmp(t, "FileNotFoundError") == 0 || strcmp(t, "MarkerBusy") == 0
        || strcmp(t, "BlockingIOError") == 0 || strcmp(t, "FileExistsError") == 0
        || strcmp(t, "IsADirectoryError") == 0 || strcmp(t, "NotADirectoryError") == 0
        || strcmp(t, "InterruptedError") == 0 || strcmp(t, "TimeoutError") == 0;
}
static bool exc_is_fnf(const json_t *r) {
    return strcmp(exc_type(r), "FileNotFoundError") == 0;
}
static bool exc_is_markerbusy(const json_t *r) {
    return strcmp(exc_type(r), "MarkerBusy") == 0;
}
/* str(exc) parity: OSError(errno, msg) -> "[Errno N] msg"; OSError(msg) -> msg */
static char *exc_str(const json_t *r) {
    const char *msg = json_get_str(r, "message", "");
    json_t *en = json_obj_get(r, "errno");
    char *out = malloc(strlen(msg) + 48);
    if (j_is_num(en)) snprintf(out, strlen(msg) + 48, "[Errno %.0f] %s", en->num_val, msg);
    else snprintf(out, strlen(msg) + 48, "%s", msg);
    return out;
}

/* ── _bounded_int: ASCII digits, significant-digit length gate, u64 cap ── */
static bool bounded_int_scan(const char *text, unsigned long long limit, unsigned long long *out) {
    if (!text || !text[0]) return false;
    for (const char *p = text; *p; p++) {
        if (*p < '0' || *p > '9') return false;
    }
    const char *sig = text;
    while (*sig == '0') sig++;
    if (!*sig) sig = "0";
    char limbuf[32];
    snprintf(limbuf, sizeof limbuf, "%" PRIu64, limit);
    if (strlen(sig) > strlen(limbuf)) return false;
    /* exact u64 parse with overflow detection */
    unsigned long long v = 0;
    for (const char *p = sig; *p; p++) {
        unsigned d = (unsigned)(*p - '0');
        if (v > (ULF_U64_MAX - d) / 10) return false;
        v = v * 10 + d;
    }
    if (v > limit) return false;
    *out = v;
    return true;
}

/* PoP: ulf_bounded_int @ hermes_cli/update_lock.py:_bounded_int */
json_t *ulf_bounded_int(ulf_io_t *io, json_t *in) {
    (void)io;
    const char *text = json_get_str(in, "text", "");
    unsigned long long limit = ULF_U32_MAX;
    const char *limstr = json_get_str(in, "limit_str", NULL);
    if (limstr) {
        char *end = NULL;
        limit = strtoull(limstr, &end, 10);
        if (!end || *end != '\0') return raise_desc("ValueError", "bad limit_str");
    } else {
        json_t *lim = json_obj_get(in, "limit");
        if (j_is_num(lim)) limit = (unsigned long long)lim->num_val;
    }
    unsigned long long v;
    if (!bounded_int_scan(text, limit, &v)) return json_null();
    return u64_node(v);
}

/* ── _as_ct ────────────────────────────────────────────────────────────── */
static bool py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}
static char *py_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && py_isspace((unsigned char)s[a])) a++;
    while (b > a && py_isspace((unsigned char)s[b - 1])) b--;
    char *out = malloc(b - a + 1);
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* _CT_VALUE fullmatch: [0-9]+(?:\.[0-9]+)? — returns the matched text */
static bool ct_value_fullmatch(const char *s, const char **end_out) {
    const char *p = s;
    if (!(*p >= '0' && *p <= '9')) return false;
    while (*p >= '0' && *p <= '9') p++;
    if (*p == '.') {
        p++;
        if (!(*p >= '0' && *p <= '9')) return false;
        while (*p >= '0' && *p <= '9') p++;
    }
    if (*p != '\0') return false;
    if (end_out) *end_out = p;
    return true;
}

/* PoP: ulf_as_ct @ hermes_cli/update_lock.py:_as_ct */
json_t *ulf_as_ct(ulf_io_t *io, json_t *in) {
    (void)io;
    json_t *rec = json_obj_get(in, "recorded");
    if (!rec || rec->type == JSON_NULL) return json_null();
    if (rec->type == JSON_BOOL) return json_number(rec->bool_val ? 1.0 : 0.0);
    if (j_is_num(rec)) return json_number(rec->num_val);
    if (j_is_str(rec)) {
        char *stripped = py_strip(rec->str_val);
        const char *body = stripped;
        if (strncmp(body, "ct:", 3) == 0) body += 3;
        json_t *r = json_null();
        if (ct_value_fullmatch(body, NULL)) r = json_number(strtod(body, NULL));
        free(stripped);
        return r;
    }
    /* containers: str(recorded) is a repr that never fullmatches (fixture
     * scope excludes container reprs; outcome is None either way) */
    return json_null();
}

/* ── world injection ───────────────────────────────────────────────────── */
typedef struct { const json_t *w; json_t *owned; } world_ref_t;

static bool world_get(ulf_io_t *io, const json_t *world, world_ref_t *out, json_t **err) {
    out->w = NULL; out->owned = NULL;
    if (world) { out->w = world; return true; }
    json_t *r = ulf_call(io, "real_world", NULL);
    if (ulf_raised(r)) { *err = r; return false; }
    if (!j_is_obj(r)) { json_free(r); *err = raise_desc("TypeError", "real_world must return a world object"); return false; }
    out->owned = r; out->w = r;
    return true;
}
static void world_put(world_ref_t *r) { if (r->owned) json_free(r->owned); }

/* ── _incarnation: THE identity rule (A7 rule 4) ───────────────────────── */
enum { ULF_V_FALSE = 0, ULF_V_TRUE = 1, ULF_V_NONE = 2 };

static int incarnation_flow(ulf_io_t *io, long long pid, double recorded, bool has_rec,
                            const json_t *world, json_t **err) {
    world_ref_t wr;
    if (!world_get(io, world, &wr, err)) return ULF_V_FALSE;
    int verdict = ULF_V_FALSE;
    long long wpid = (long long)json_get_num(wr.w, "pid", 0);
    json_t *wct = json_obj_get(wr.w, "ct");
    if (pid <= 0) { verdict = ULF_V_FALSE; goto done; }
    if (pid == wpid) {
        if (!j_is_num(wct)) { verdict = has_rec ? ULF_V_FALSE : ULF_V_TRUE; goto done; }
        if (!has_rec) { verdict = ULF_V_FALSE; goto done; }
        double d = fabs(wct->num_val - recorded);
        if (d <= ULF_OWN_EPS) { verdict = ULF_V_TRUE; goto done; }
        bool is_int = isfinite(recorded) && floor(recorded) == recorded;
        double delta = wct->num_val - recorded;
        verdict = (is_int && delta >= 0.0 && delta < ULF_WHOLE_CT) ? ULF_V_TRUE : ULF_V_FALSE;
        goto done;
    }
    {
        json_t *args = json_object();
        json_set(args, "pid", json_number((double)pid));
        json_t *alive = ulf_call(io, "world.alive", args);
        json_free(args);
        if (ulf_raised(alive)) { *err = alive; goto done; }
        bool is_alive = jtruth(alive);
        json_free(alive);
        if (!is_alive) { verdict = ULF_V_FALSE; goto done; }
        if (!has_rec) { verdict = ULF_V_NONE; goto done; }
        json_t *a2 = json_object();
        json_set(a2, "pid", json_number((double)pid));
        json_t *actual = ulf_call(io, "world.ct_of", a2);
        json_free(a2);
        if (ulf_raised(actual)) { *err = actual; goto done; }
        if (!actual || actual->type == JSON_NULL) { json_free(actual); verdict = ULF_V_NONE; goto done; }
        if (!j_is_num(actual)) { json_free(actual); *err = raise_desc("TypeError", "ct_of must return a number or None"); goto done; }
        verdict = fabs(actual->num_val - recorded) <= ULF_CT_TOL ? ULF_V_TRUE : ULF_V_FALSE;
        json_free(actual);
    }
done:
    world_put(&wr);
    return verdict;
}

/* PoP: ulf_incarnation @ hermes_cli/update_lock.py:_incarnation */
json_t *ulf_incarnation(ulf_io_t *io, json_t *in) {
    long long pid = (long long)json_get_num(in, "pid", 0);
    json_t *rec = json_obj_get(in, "recorded");
    bool has_rec = j_is_num(rec);
    double recorded = has_rec ? rec->num_val : 0.0;
    json_t *world = json_obj_get(in, "world");
    json_t *err = NULL;
    int v = incarnation_flow(io, pid, recorded, has_rec, world, &err);
    if (err) return err;
    if (v == ULF_V_NONE) return json_null();
    return json_bool(v == ULF_V_TRUE);
}

/* ── _identity_live: incarnation + the v1 age ceiling ──────────────────── */
static bool identity_live_flow(ulf_io_t *io, long long pid, double ct, bool has_ct,
                               double age, const json_t *world, json_t **err) {
    int v = incarnation_flow(io, pid, ct, has_ct, world, err);
    if (*err) return false;
    if (v == ULF_V_NONE) return age <= ULF_MAX_AGE;
    return v == ULF_V_TRUE;
}

/* PoP: ulf_identity_live @ hermes_cli/update_lock.py:_identity_live */
json_t *ulf_identity_live(ulf_io_t *io, json_t *in) {
    long long pid = (long long)json_get_num(in, "pid", 0);
    json_t *ctn = json_obj_get(in, "create_time");
    bool has_ct = j_is_num(ctn);
    double ct = has_ct ? ctn->num_val : 0.0;
    double age = json_get_num(in, "age", 0.0);
    json_t *world = json_obj_get(in, "world");
    json_t *err = NULL;
    bool live = identity_live_flow(io, pid, ct, has_ct, age, world, &err);
    if (err) return err;
    return json_bool(live);
}/* ── the marker: parse state machine + identity methods ────────────────── */
typedef struct {
    unsigned char *raw; size_t raw_len;
    long long pid;
    unsigned long long started_at; bool has_started;
    double create_time; bool has_ct;
    long long delegate_pid; bool has_delegate;
    double delegate_ct; bool has_dct;
    bool in_flight;
    char *ct_text; char *dct_text;
    char **runs; size_t nruns;
} ulf_marker_t;

static void marker_free(ulf_marker_t *m) {
    if (!m) return;
    free(m->raw); free(m->ct_text); free(m->dct_text);
    for (size_t i = 0; i < m->nruns; i++) free(m->runs[i]);
    free(m->runs);
    memset(m, 0, sizeof *m);
}

/* UTF-8 decode with per-byte U+FFFD replacement (errors="replace"; fixtures
 * avoid truncated multi-byte sequences where Python groups replacements). */
static char *utf8_decode_replace(const unsigned char *raw, size_t len) {
    char *out = malloc(len * 3 + 1);
    size_t o = 0;
    size_t i = 0;
    while (i < len) {
        unsigned char c = raw[i];
        unsigned cp = 0; int need = 0; unsigned min_cp = 0;
        if (c < 0x80) { cp = c; need = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; need = 1; min_cp = 0x80; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; need = 2; min_cp = 0x800; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; need = 3; min_cp = 0x10000; }
        else { need = -1; }
        bool ok = need >= 0 && (i + (size_t)need) < len;
        /* need continuation bytes available */
        if (ok && need > 0) {
            for (int k = 1; k <= need; k++) {
                if (i + (size_t)k >= len || (raw[i + k] & 0xC0) != 0x80) { ok = false; break; }
            }
        }
        if (ok) {
            for (int k = 1; k <= need; k++) cp = (cp << 6) | (raw[i + k] & 0x3F);
            if (cp < min_cp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) ok = false;
        }
        if (!ok) {
            /* one replacement per invalid byte */
            out[o++] = (char)0xEF; out[o++] = (char)0xBF; out[o++] = (char)0xBD;
            i++;
            continue;
        }
        if (cp < 0x80) out[o++] = (char)cp;
        else if (cp < 0x800) {
            out[o++] = (char)(0xC0 | (cp >> 6));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out[o++] = (char)(0xE0 | (cp >> 12));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        } else {
            out[o++] = (char)(0xF0 | (cp >> 18));
            out[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        }
        i += (size_t)need + 1;
    }
    out[o] = '\0';
    return out;
}

static unsigned char *hex_decode(const char *hex, size_t *len_out) {
    size_t n = strlen(hex) / 2;
    unsigned char *out = malloc(n + 1);
    for (size_t i = 0; i < n; i++) {
        char b[3] = {hex[2 * i], hex[2 * i + 1], 0};
        out[i] = (unsigned char)strtoul(b, NULL, 16);
    }
    out[n] = 0;
    *len_out = n;
    return out;
}

/* in.raw (UTF-8 string) or in.raw_hex (exact bytes) */
static unsigned char *input_bytes(const json_t *in, size_t *len_out) {
    const char *hex = json_get_str(in, "raw_hex", NULL);
    if (hex) return hex_decode(hex, len_out);
    const char *raw = json_get_str(in, "raw", "");
    size_t n = strlen(raw);
    unsigned char *out = malloc(n + 1);
    memcpy(out, raw, n + 1);
    *len_out = n;
    return out;
}

/* line normalization: removesuffix("\r") then strip(" \t") */
static char *normalize_line(const char *line) {
    size_t n = strlen(line);
    if (n > 0 && line[n - 1] == '\r') n--;
    size_t a = 0, b = n;
    while (a < b && (line[a] == ' ' || line[a] == '\t')) a++;
    while (b > a && (line[b - 1] == ' ' || line[b - 1] == '\t')) b--;
    char *out = malloc(b - a + 1);
    memcpy(out, line + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* _CT_LINE fullmatch: ct:([0-9]+(?:\.[0-9]+)?) — group text via strdup */
static char *ct_line_fullmatch(const char *line) {
    if (strncmp(line, "ct:", 3) != 0) return NULL;
    const char *body = line + 3;
    if (!ct_value_fullmatch(body, NULL)) return NULL;
    return strdup(body);
}

/* _DELEGATE_LINE fullmatch: delegate:([0-9]+) ct:([0-9]+(?:\.[0-9]+)?) */
static bool delegate_line_fullmatch(const char *line, char **pid_out, char **ct_out) {
    if (strncmp(line, "delegate:", 9) != 0) return false;
    const char *p = line + 9;
    const char *ds = p;
    if (!(*p >= '0' && *p <= '9')) return false;
    while (*p >= '0' && *p <= '9') p++;
    size_t dn = (size_t)(p - ds);
    if (strncmp(p, " ct:", 4) != 0) return false;
    p += 4;
    if (!ct_value_fullmatch(p, NULL)) return false;
    char *pid = malloc(dn + 1);
    memcpy(pid, ds, dn); pid[dn] = '\0';
    *pid_out = pid;
    *ct_out = strdup(p);
    return true;
}

/* _RUN_LINE fullmatch: run:([A-Za-z0-9._-]{1,128}) */
static char *run_line_fullmatch(const char *line) {
    if (strncmp(line, "run:", 4) != 0) return NULL;
    const char *p = line + 4;
    size_t n = 0;
    while (p[n]) {
        char c = p[n];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (!ok) return NULL;
        n++;
    }
    if (n < 1 || n > 128) return NULL;
    return strdup(p);
}

/* _parse_marker flow. mtime present => the empty-marker grace clock read. */
static bool parse_marker_flow(ulf_io_t *io, const unsigned char *raw, size_t raw_len,
                              double mtime, bool has_mtime, ulf_marker_t *m, json_t **err) {
    memset(m, 0, sizeof *m);
    m->raw = malloc(raw_len + 1);
    memcpy(m->raw, raw, raw_len);
    m->raw[raw_len] = 0;
    m->raw_len = raw_len;
    m->pid = -1;
    char *text = utf8_decode_replace(raw, raw_len);
    char *body = text;
    if (strncmp(body, "\xef\xbb\xbf", 3) == 0) body += 3;  /* removeprefix BOM */
    /* split("\n") — every segment, including trailing empties */
    size_t cap = 8, nlines = 0;
    char **lines = malloc(cap * sizeof(char *));
    const char *seg = body;
    for (;;) {
        const char *nl = strchr(seg, '\n');
        size_t len = nl ? (size_t)(nl - seg) : strlen(seg);
        char *line = malloc(len + 1);
        memcpy(line, seg, len); line[len] = '\0';
        char *norm = normalize_line(line);
        free(line);
        if (nlines == cap) { cap *= 2; lines = realloc(lines, cap * sizeof(char *)); }
        lines[nlines++] = norm;
        if (!nl) break;
        seg = nl + 1;
    }
    while (nlines < 3) {
        if (nlines == cap) { cap *= 2; lines = realloc(lines, cap * sizeof(char *)); }
        lines[nlines++] = strdup("");
    }
    unsigned long long pid_v;
    if (bounded_int_scan(lines[0], ULF_U32_MAX, &pid_v)) {
        m->pid = (long long)pid_v;
        unsigned long long st;
        if (bounded_int_scan(lines[1], ULF_U64_MAX, &st)) {
            m->started_at = st;
            m->has_started = true;
        }
    }
    char *ct = ct_line_fullmatch(lines[2]);
    if (ct) {
        m->ct_text = ct;
        m->create_time = strtod(ct, NULL);
        m->has_ct = true;
    }
    for (size_t i = 3; i < nlines; i++) {
        char *dpid = NULL, *dct = NULL;
        if (!m->has_delegate && delegate_line_fullmatch(lines[i], &dpid, &dct)) {
            unsigned long long dv;
            if (bounded_int_scan(dpid, ULF_U32_MAX, &dv)) {
                m->delegate_pid = (long long)dv;
                m->has_delegate = true;
                m->delegate_ct = strtod(dct, NULL);
                m->has_dct = true;
                m->dct_text = dct;
                free(dpid);
            } else {
                free(dpid); free(dct);
            }
        }
        char *run = run_line_fullmatch(lines[i]);
        if (run) {
            m->runs = realloc(m->runs, (m->nruns + 1) * sizeof(char *));
            m->runs[m->nruns++] = run;
        }
    }
    /* in_flight = not raw and mtime is not None and time.time() - mtime < grace */
    if (raw_len == 0 && has_mtime) {
        json_t *t = ulf_call(io, "time.time", NULL);
        if (ulf_raised(t)) {
            *err = t;
            for (size_t i = 0; i < nlines; i++) free(lines[i]);
            free(lines); free(text);
            return false;
        }
        double now = j_is_num(t) ? t->num_val : 0.0;
        json_free(t);
        m->in_flight = (now - mtime) < ULF_EMPTY_GRACE;
    }
    for (size_t i = 0; i < nlines; i++) free(lines[i]);
    free(lines); free(text);
    return true;
}

/* ── marker methods (world NULL => the real_world/time.time seams) ─────── */
static bool marker_age(ulf_io_t *io, const ulf_marker_t *m, const json_t *world,
                       double *age_out, json_t **err) {
    if (!m->has_started) { *age_out = INFINITY; return true; }
    double now;
    if (world) {
        now = json_get_num(world, "now", 0.0);
    } else {
        json_t *t = ulf_call(io, "time.time", NULL);
        if (ulf_raised(t)) { *err = t; return false; }
        now = j_is_num(t) ? t->num_val : 0.0;
        json_free(t);
    }
    *age_out = now - (double)m->started_at;
    return true;
}

static bool marker_owner_live(ulf_io_t *io, const ulf_marker_t *m, const json_t *world, json_t **err) {
    if (!m->has_started) return false;
    double age;
    if (!marker_age(io, m, world, &age, err)) return false;
    return identity_live_flow(io, m->pid, m->create_time, m->has_ct, age, world, err);
}

static bool marker_delegate_live(ulf_io_t *io, const ulf_marker_t *m, const json_t *world, json_t **err) {
    if (!m->has_delegate || !m->has_started) return false;
    double age;
    if (!marker_age(io, m, world, &age, err)) return false;
    return identity_live_flow(io, m->delegate_pid, m->delegate_ct, m->has_dct, age, world, err);
}

static bool marker_live_pid(ulf_io_t *io, const ulf_marker_t *m, const json_t *world,
                            long long *pid_out, json_t **err) {
    if (m->in_flight) { *pid_out = 0; return true; }
    bool own = marker_owner_live(io, m, world, err);
    if (*err) return false;
    if (own) { *pid_out = m->pid; return true; }
    bool del = marker_delegate_live(io, m, world, err);
    if (*err) return false;
    if (del) { *pid_out = m->delegate_pid; return true; }
    return false;
}

/* canonical(pid, ct_text): lines 1-3 rewritten + run lines kept */
static char *marker_canonical(const ulf_marker_t *m, long long pid, const char *ct_text) {
    size_t cap = 128;
    for (size_t i = 0; i < m->nruns; i++) cap += strlen(m->runs[i]) + 8;
    if (ct_text) cap += strlen(ct_text) + 8;
    char *out = malloc(cap);
    int n = snprintf(out, cap, "%lld\n%" PRIu64 "\n", pid, m->started_at);
    if (ct_text && ct_text[0]) n += snprintf(out + n, cap - (size_t)n, "ct:%s\n", ct_text);
    for (size_t i = 0; i < m->nruns; i++) {
        n += snprintf(out + n, cap - (size_t)n, "run:%s\n", m->runs[i]);
    }
    return out;
}

/* base: first 3 byte-lines with ends (bytes splitlines: \r\n, \n, \r) */
static char *marker_base_hex(const ulf_marker_t *m) {
    size_t i = 0, lines = 0, end = 0;
    while (i < m->raw_len && lines < 3) {
        unsigned char c = m->raw[i];
        if (c == '\n') { i++; lines++; end = i; }
        else if (c == '\r') {
            i++;
            if (i < m->raw_len && m->raw[i] == '\n') i++;
            lines++; end = i;
        } else i++;
    }
    if (lines < 3) end = m->raw_len;
    char *hex = malloc(end * 2 + 1);
    static const char digits[] = "0123456789abcdef";
    for (size_t k = 0; k < end; k++) {
        hex[2 * k] = digits[m->raw[k] >> 4];
        hex[2 * k + 1] = digits[m->raw[k] & 0xF];
    }
    hex[end * 2] = '\0';
    return hex;
}

static json_t *marker_proj(const ulf_marker_t *m) {
    json_t *o = json_object();
    json_set(o, "pid", json_number((double)m->pid));
    json_set(o, "started_at", m->has_started ? u64_node(m->started_at) : json_null());
    json_set(o, "create_time", m->has_ct ? json_number(m->create_time) : json_null());
    json_set(o, "delegate_pid", m->has_delegate ? json_number((double)m->delegate_pid) : json_null());
    json_set(o, "delegate_create_time", m->has_dct ? json_number(m->delegate_ct) : json_null());
    json_set(o, "in_flight", json_bool(m->in_flight));
    json_set(o, "ct_text", m->ct_text ? json_string(m->ct_text) : json_null());
    json_set(o, "delegate_ct_text", m->dct_text ? json_string(m->dct_text) : json_null());
    json_t *runs = json_array();
    for (size_t i = 0; i < m->nruns; i++) json_append(runs, json_string(m->runs[i]));
    json_set(o, "runs", runs);
    char *base = marker_base_hex(m);
    json_set_hdr(o, "base_hex", base);
    free(base);
    json_set(o, "run", m->nruns > 0 ? json_string(m->runs[0]) : json_null());
    return o;
}

/* scripted marker spec -> internal marker (the _read_marker seam result) */
static bool spec_to_marker(const json_t *spec, ulf_marker_t *m) {
    memset(m, 0, sizeof *m);
    if (!j_is_obj(spec)) return false;
    size_t raw_len = 0;
    unsigned char *raw = input_bytes(spec, &raw_len);
    m->raw = raw; m->raw_len = raw_len;
    m->pid = (long long)json_get_num(spec, "pid", -1);
    json_t *st = json_obj_get(spec, "started_at");
    if (st && st->type != JSON_NULL) {
        unsigned long long v;
        if (!node_to_u64(st, &v)) { marker_free(m); return false; }
        m->started_at = v; m->has_started = true;
    }
    json_t *ct = json_obj_get(spec, "create_time");
    if (j_is_num(ct)) { m->create_time = ct->num_val; m->has_ct = true; }
    json_t *dp = json_obj_get(spec, "delegate_pid");
    if (j_is_num(dp)) { m->delegate_pid = (long long)dp->num_val; m->has_delegate = true; }
    json_t *dct = json_obj_get(spec, "delegate_create_time");
    if (j_is_num(dct)) { m->delegate_ct = dct->num_val; m->has_dct = true; }
    m->in_flight = json_get_bool(spec, "in_flight", false);
    const char *ctt = json_get_str(spec, "ct_text", NULL);
    if (ctt) m->ct_text = strdup(ctt);
    const char *dctt = json_get_str(spec, "delegate_ct_text", NULL);
    if (dctt) m->dct_text = strdup(dctt);
    json_t *runs = json_obj_get(spec, "runs");
    if (runs && runs->type == JSON_ARRAY) {
        for (size_t i = 0; i < runs->c.count; i++) {
            json_t *r = runs->c.items[i];
            if (j_is_str(r)) {
                m->runs = realloc(m->runs, (m->nruns + 1) * sizeof(char *));
                m->runs[m->nruns++] = strdup(r->str_val);
            }
        }
    }
    return true;
}

/* POSIX parent (PurePosixPath semantics; fixtures use clean paths) */
static char *posix_parent(const char *p) {
    if (!p || !p[0]) return strdup(".");
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/') n--;
    if (n == 1 && p[0] == '/') return strdup("/");
    size_t last = 0;
    bool found = false;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == '/') { last = i; found = true; }
    }
    if (!found) return strdup(".");
    if (last == 0) return strdup("/");
    char *out = malloc(last + 1);
    memcpy(out, p, last);
    out[last] = '\0';
    return out;
}

/* PoP: ulf_parse_marker @ hermes_cli/update_lock.py:_parse_marker */
json_t *ulf_parse_marker(ulf_io_t *io, json_t *in) {
    size_t raw_len = 0;
    unsigned char *raw = input_bytes(in, &raw_len);
    json_t *mt = json_obj_get(in, "mtime");
    bool has_mtime = j_is_num(mt);
    double mtime = has_mtime ? mt->num_val : 0.0;
    ulf_marker_t m;
    json_t *err = NULL;
    bool ok = parse_marker_flow(io, raw, raw_len, mtime, has_mtime, &m, &err);
    free(raw);
    if (!ok) { marker_free(&m); return err; }
    json_t *r = marker_proj(&m);
    marker_free(&m);
    return r;
}

/* PoP: ulf_judge_marker @ hermes_cli/update_lock.py:judge_marker */
json_t *ulf_judge_marker(ulf_io_t *io, json_t *in) {
    json_t *world = json_obj_get(in, "world");
    size_t raw_len = 0;
    unsigned char *raw = input_bytes(in, &raw_len);
    ulf_marker_t m;
    json_t *err = NULL;
    bool ok = parse_marker_flow(io, raw, raw_len, 0.0, false, &m, &err);
    free(raw);
    if (!ok) { marker_free(&m); return err; }
    json_t *r = json_object();
    if (!m.has_started) {
        json_set_hdr(r, "verdict", "malformed");
        json_set(r, "owner", json_null());
        json_set(r, "run", json_null());
        marker_free(&m);
        return r;
    }
    long long owner = 0;
    bool has_owner = marker_live_pid(io, &m, world, &owner, &err);
    if (err) { marker_free(&m); json_free(r); return err; }
    if (!has_owner) {
        json_set_hdr(r, "verdict", "dead");
        json_set(r, "owner", json_null());
        json_set(r, "run", m.nruns > 0 ? json_string(m.runs[0]) : json_null());
        marker_free(&m);
        return r;
    }
    long long wpid = (long long)json_get_num(world, "pid", 0);
    /* ours = (pid == w.pid and owner_live(w)) or (delegate_pid == w.pid and delegate_live(w)) */
    bool ours = false;
    if (m.pid == wpid) {
        ours = marker_owner_live(io, &m, world, &err);
        if (err) { marker_free(&m); json_free(r); return err; }
    }
    if (!ours && m.has_delegate && m.delegate_pid == wpid) {
        ours = marker_delegate_live(io, &m, world, &err);
        if (err) { marker_free(&m); json_free(r); return err; }
    }
    json_set_hdr(r, "verdict", ours ? "ours" : "live");
    json_set(r, "owner", json_number((double)owner));
    json_set(r, "run", m.nruns > 0 ? json_string(m.runs[0]) : json_null());
    marker_free(&m);
    return r;
}

/* PoP: ulf_release_decision @ hermes_cli/update_lock.py:_release_decision */
json_t *ulf_release_decision(ulf_io_t *io, json_t *in) {
    json_t *world = json_obj_get(in, "world");
    size_t raw_len = 0;
    unsigned char *raw = input_bytes(in, &raw_len);
    ulf_marker_t m;
    json_t *err = NULL;
    bool ok = parse_marker_flow(io, raw, raw_len, 0.0, false, &m, &err);
    free(raw);
    if (!ok) { marker_free(&m); return err; }
    json_t *r = json_object();
    const char *action = "keep";
    char *new_bytes = NULL;
    long long wpid = (long long)json_get_num(world, "pid", 0);
    if (!m.has_started) {
        goto emit;
    }
    if (m.pid == wpid) {
        bool own = marker_owner_live(io, &m, world, &err);
        if (err) goto fail;
        if (own) {
            if (m.has_delegate && m.delegate_pid != wpid) {
                bool del = marker_delegate_live(io, &m, world, &err);
                if (err) goto fail;
                if (del) {
                    action = "rewrite";
                    new_bytes = marker_canonical(&m, m.delegate_pid, m.dct_text);
                    goto emit;
                }
            }
            action = "delete";
            goto emit;
        }
    }
    if (m.has_delegate && m.delegate_pid == wpid) {
        bool del = marker_delegate_live(io, &m, world, &err);
        if (err) goto fail;
        if (del) {
            bool own = marker_owner_live(io, &m, world, &err);
            if (err) goto fail;
            if (own) {
                action = "rewrite";
                new_bytes = marker_canonical(&m, m.pid, m.ct_text);
            } else {
                action = "delete";
            }
            goto emit;
        }
    }
emit:
    json_set_hdr(r, "action", action);
    if (new_bytes) {
        json_set_hdr(r, "new", new_bytes);
        free(new_bytes);
    } else {
        json_set(r, "new", json_null());
    }
    marker_free(&m);
    return r;
fail:
    marker_free(&m);
    json_free(r);
    free(new_bytes);
    return err;
}

/* PoP: ulf_live_partners @ hermes_cli/update_lock.py:_live_partners */
json_t *ulf_live_partners(ulf_io_t *io, json_t *in) {
    json_t *spec = json_obj_get(in, "marker");
    ulf_marker_t m;
    if (!spec_to_marker(spec, &m)) {
        return raise_desc("TypeError", "marker spec must be an object");
    }
    json_t *r = json_array();
    json_t *err = NULL;
    /* world=None call sites: age() reads time.time, identity reads real_world */
    if (m.in_flight) {
        json_append(r, json_number(0));
        marker_free(&m);
        return r;
    }
    bool own = marker_owner_live(io, &m, NULL, &err);
    if (err) { marker_free(&m); json_free(r); return err; }
    if (own) json_append(r, json_number((double)m.pid));
    bool del = marker_delegate_live(io, &m, NULL, &err);
    if (err) { marker_free(&m); json_free(r); return err; }
    if (del) json_append(r, json_number((double)m.delegate_pid));
    marker_free(&m);
    return r;
}/* ── holder construction / projection (UpdateHolder) ───────────────────── */
static json_t *holder_make(const json_t *spec) {
    json_t *h = json_object();
    json_set(h, "pid", json_number(json_get_num(spec, "pid", 0)));
    json_set(h, "age_seconds", json_number(json_get_num(spec, "age_seconds", 0.0)));
    json_t *reason = json_obj_get(spec, "reason");
    json_set(h, "reason", (reason && reason->type != JSON_NULL) ? json_copy(reason) : json_null());
    json_set(h, "held", json_bool(json_get_bool(spec, "held", false)));
    return h;
}

static json_t *holder_default(void) {
    json_t *h = json_object();
    json_set(h, "pid", json_number(0));
    json_set(h, "age_seconds", json_number(0.0));
    json_set(h, "reason", json_null());
    json_set(h, "held", json_bool(false));
    return h;
}

/* _marker_unwritable(path, exc) — the live f-string, byte-exact */
static json_t *holder_unwritable(const char *path, const json_t *exc) {
    char *es = exc_str(exc);
    size_t n = strlen(path) + strlen(es) + 160;
    char *reason = malloc(n);
    snprintf(reason, n, "%s is not writable (%s); the update records itself there so the Desktop app, "
             "gateways, installers and other updaters wait for it", path, es);
    free(es);
    json_t *h = holder_default();
    json_set(h, "reason", json_string(reason));
    free(reason);
    return h;
}

/* PoP: ulf_reclaim_dead @ hermes_cli/update_lock.py:_reclaim_dead */
json_t *ulf_reclaim_dead(ulf_io_t *io, json_t *in) {
    const char *path = json_get_str(in, "path", "");
    json_t *install_root = json_obj_get(in, "install_root");
    json_t *args = json_object();
    json_set_hdr(args, "path", path);
    json_t *enter = ulf_call(io, "mutex.enter", args);
    json_free(args);
    if (ulf_raised(enter)) {
        if (exc_is_oserror(enter)) { json_free(enter); return json_string("busy"); }
        return enter;
    }
    json_free(enter);
    const char *verdict = NULL;
    json_t *body_err = NULL;
    json_t *a2 = json_object();
    json_set_hdr(a2, "path", path);
    json_t *current = ulf_call(io, "_read_marker", a2);
    json_free(a2);
    if (ulf_raised(current)) {
        body_err = current;
    } else if (!current || current->type == JSON_NULL) {
        json_free(current);
        verdict = "absent";
    } else {
        ulf_marker_t m;
        if (!spec_to_marker(current, &m)) {
            body_err = raise_desc("TypeError", "marker spec must be an object");
        } else {
            long long live = 0;
            bool has_live = marker_live_pid(io, &m, NULL, &live, &body_err);
            if (!body_err && has_live) {
                verdict = "live";
            } else if (!body_err) {
                json_t *a3 = json_object();
                json_set(a3, "install_root", install_root ? json_copy(install_root) : json_null());
                json_t *held = ulf_call(io, "checkout_lock_held", a3);
                json_free(a3);
                if (ulf_raised(held)) {
                    body_err = held;
                } else if (jtruth(held)) {
                    json_free(held);
                    verdict = "held";
                } else {
                    json_free(held);
                    json_t *a4 = json_object();
                    json_set_hdr(a4, "path", path);
                    json_t *un = ulf_call(io, "path.unlink", a4);
                    json_free(a4);
                    if (ulf_raised(un)) {
                        if (exc_is_fnf(un)) json_free(un);  /* suppress(FileNotFoundError) */
                        else body_err = un;
                    } else {
                        json_free(un);
                    }
                    if (!body_err) verdict = "reclaimed";
                }
            }
            marker_free(&m);
        }
        json_free(current);
    }
    /* mutex exit on EVERY body exit (never after a failed enter) */
    json_t *a5 = json_object();
    json_set_hdr(a5, "path", path);
    json_t *exit_r = ulf_call(io, "mutex.exit", a5);
    json_free(a5);
    if (ulf_raised(exit_r)) {
        if (body_err) json_free(body_err);
        return exit_r;
    }
    json_free(exit_r);
    if (body_err) {
        if (exc_is_oserror(body_err)) { json_free(body_err); return json_string("busy"); }
        return body_err;
    }
    return json_string(verdict);
}

/* ── UpdateLock._claim_marker: the acquire state machine ───────────────── */
enum { CLAIM_CONTINUE = 0, CLAIM_RET_TRUE, CLAIM_RET_FALSE };

/* NOTE: the scripted self._adopt_or_refuse "holder" state effect is applied
 * by the harness dispatcher (it owns the script entry), mirroring the
 * oracle's recording stub. */

/* PoP: ulf_claim_marker @ hermes_cli/update_lock.py:UpdateLock._claim_marker */
json_t *ulf_claim_marker(ulf_io_t *io, json_t *in) {
    json_t *self = json_obj_get(in, "self");
    json_t *claims = json_obj_get(in, "claims");
    if (!j_is_obj(self) || !j_is_obj(claims)) {
        return raise_desc("TypeError", "self and claims must be objects");
    }
    const char *path = json_get_str(self, "path", "");
    char *parent = posix_parent(path);
    json_t *err = NULL;
    json_t *r = NULL;

    /* try: self.path.parent.mkdir(parents=True, exist_ok=True) */
    json_t *args = json_object();
    json_set_hdr(args, "path", parent);
    json_set(args, "parents", json_bool(true));
    json_set(args, "exist_ok", json_bool(true));
    r = ulf_call(io, "path.parent.mkdir", args);
    json_free(args);
    if (ulf_raised(r)) {
        if (exc_is_oserror(r)) {
            json_set(self, "holder", holder_unwritable(parent, r));
            json_free(r);
            free(parent);
            return json_bool(false);
        }
        free(parent);
        return r;
    }
    json_free(r);

    /* _sweep_dead_tmp_siblings(self.path) — outside the try: raises escape */
    args = json_object();
    json_set_hdr(args, "path", path);
    r = ulf_call(io, "_sweep_dead_tmp_siblings", args);
    json_free(args);
    if (ulf_raised(r)) { free(parent); return r; }
    json_free(r);

    /* if _CLAIMS.get(str(self.path)) is None: legacy profile claims gate */
    json_t *claim_entry = json_obj_get(claims, path);
    if (!claim_entry || claim_entry->type == JSON_NULL) {
        args = json_object();
        json_set_hdr(args, "root", parent);
        json_t *legacy = ulf_call(io, "legacy_profile_claims", args);
        json_free(args);
        if (ulf_raised(legacy)) { free(parent); return legacy; }
        json_t *first_kept = NULL;
        if (legacy && legacy->type == JSON_ARRAY) {
            for (size_t i = 0; i < legacy->c.count; i++) {
                json_t *claim = legacy->c.items[i];
                json_t *pa = json_object();
                json_set(pa, "pid", json_number(json_get_num(claim, "pid", 0)));
                json_t *partner = ulf_call(io, "self._is_partner", pa);
                json_free(pa);
                if (ulf_raised(partner)) {
                    if (first_kept) json_free(first_kept);
                    json_free(legacy);
                    free(parent);
                    return partner;
                }
                if (!jtruth(partner) && !first_kept) first_kept = holder_make(claim);
                json_free(partner);
            }
        }
        json_free(legacy);
        if (first_kept) {
            json_set(self, "holder", first_kept);
            free(parent);
            return json_bool(false);
        }
    }

    /* body = f"{os.getpid()}\n{int(time.time())}\n{_identity_line()}\n" */
    json_t *pid_r = ulf_call(io, "os.getpid", NULL);
    if (ulf_raised(pid_r)) { free(parent); return pid_r; }
    long long mypid = j_is_num(pid_r) ? (long long)pid_r->num_val : 0;
    json_free(pid_r);
    json_t *t_r = ulf_call(io, "time.time", NULL);
    if (ulf_raised(t_r)) { free(parent); return t_r; }
    long long started = j_is_num(t_r) ? (long long)t_r->num_val : 0;
    json_free(t_r);
    json_t *il_r = ulf_call(io, "_identity_line", NULL);
    if (ulf_raised(il_r)) { free(parent); return il_r; }
    const char *identity_line = j_is_str(il_r) ? il_r->str_val : "";
    char body[256];
    snprintf(body, sizeof body, "%lld\n%lld\n%s\n", mypid, started, identity_line);
    json_free(il_r);

    /* try: for _attempt in range(3): publish / mutex / judge / reclaim */
    for (int attempt = 0; attempt < 3; attempt++) {
        args = json_object();
        json_set_hdr(args, "path", path);
        json_set_hdr(args, "body", body);
        json_t *pub = ulf_call(io, "_publish_exclusive", args);
        json_free(args);
        if (ulf_raised(pub)) { err = pub; goto try_exc; }
        if (jtruth(pub)) {
            json_free(pub);
            json_set(self, "acquired", json_bool(true));
            free(parent);
            return json_bool(true);
        }
        json_free(pub);

        args = json_object();
        json_set_hdr(args, "path", path);
        json_t *enter = ulf_call(io, "mutex.enter", args);
        json_free(args);
        if (ulf_raised(enter)) { err = enter; goto try_exc; }
        json_free(enter);

        int action = CLAIM_CONTINUE;
        json_t *inner_err = NULL;
        args = json_object();
        json_set_hdr(args, "path", path);
        json_t *existing = ulf_call(io, "_read_marker", args);
        json_free(args);
        if (ulf_raised(existing)) {
            inner_err = existing;
        } else if (!existing || existing->type == JSON_NULL) {
            json_free(existing);
            action = CLAIM_CONTINUE;  /* vanished between publish and read */
        } else {
            ulf_marker_t m;
            if (!spec_to_marker(existing, &m)) {
                inner_err = raise_desc("TypeError", "marker spec must be an object");
            } else {
                json_t *mproj = marker_proj(&m);
                args = json_object();
                json_set(args, "marker", json_copy(mproj));
                json_t *partners = ulf_call(io, "_live_partners", args);
                json_free(args);
                if (ulf_raised(partners)) {
                    inner_err = partners;
                } else if (jtruth(partners)) {
                    json_free(partners);
                    args = json_object();
                    json_set(args, "marker", mproj);  /* ownership transferred */
                    mproj = NULL;
                    json_t *adopt = ulf_call(io, "self._adopt_or_refuse", args);
                    json_free(args);
                    if (ulf_raised(adopt)) {
                        inner_err = adopt;
                    } else {
                        action = jtruth(adopt) ? CLAIM_RET_TRUE : CLAIM_RET_FALSE;
                        json_free(adopt);
                    }
                } else {
                    json_free(partners);
                    json_t *elsewhere = ulf_call(io, "self._checkout_held_elsewhere", NULL);
                    if (ulf_raised(elsewhere)) {
                        inner_err = elsewhere;
                    } else if (jtruth(elsewhere)) {
                        json_free(elsewhere);
                        /* R6 holder: dead owner, checkout still held */
                        double age = 0.0;
                        if (m.has_started) {
                            json_t *t = ulf_call(io, "time.time", NULL);
                            if (ulf_raised(t)) {
                                inner_err = t;
                            } else {
                                double now = j_is_num(t) ? t->num_val : 0.0;
                                json_free(t);
                                age = now - (double)m.started_at;
                                if (age < 0.0) age = 0.0;
                            }
                        }
                        if (!inner_err) {
                            json_t *h = holder_default();
                            json_set(h, "held", json_bool(true));
                            json_set(h, "age_seconds", json_number(age));
                            json_set(self, "holder", h);
                            action = CLAIM_RET_FALSE;
                        }
                    } else {
                        json_free(elsewhere);
                        args = json_object();
                        json_set_hdr(args, "path", path);
                        json_t *un = ulf_call(io, "path.unlink", args);
                        json_free(args);
                        if (ulf_raised(un)) {
                            if (exc_is_fnf(un)) json_free(un);
                            else inner_err = un;
                        } else {
                            json_free(un);
                        }
                        if (!inner_err) {
                            args = json_object();
                            json_set_hdr(args, "path", path);
                            json_set_hdr(args, "body", body);
                            json_t *pub2 = ulf_call(io, "_publish_exclusive", args);
                            json_free(args);
                            if (ulf_raised(pub2)) {
                                inner_err = pub2;
                            } else if (jtruth(pub2)) {
                                json_free(pub2);
                                json_set(self, "acquired", json_bool(true));
                                action = CLAIM_RET_TRUE;
                            } else {
                                json_free(pub2);
                            }
                        }
                    }
                }
                marker_free(&m);
                if (mproj) json_free(mproj);
            }
            json_free(existing);
        }
        /* mutex exit on every body exit */
        args = json_object();
        json_set_hdr(args, "path", path);
        json_t *exit_r = ulf_call(io, "mutex.exit", args);
        json_free(args);
        if (ulf_raised(exit_r)) {
            if (inner_err) json_free(inner_err);
            inner_err = exit_r;
        } else {
            json_free(exit_r);
        }
        if (inner_err) { err = inner_err; goto try_exc; }
        if (action == CLAIM_RET_TRUE) { free(parent); return json_bool(true); }
        if (action == CLAIM_RET_FALSE) { free(parent); return json_bool(false); }
        /* CLAIM_CONTINUE: next attempt */
    }

    /* loop exhausted: holder = read_live_update(...) or UpdateHolder(0, 0.0) */
    args = json_object();
    json_set_hdr(args, "path", path);
    json_t *ir = json_obj_get(self, "install_root");
    json_set(args, "install_root", ir ? json_copy(ir) : json_null());
    json_t *rlu = ulf_call(io, "read_live_update", args);
    json_free(args);
    if (ulf_raised(rlu)) { free(parent); return rlu; }
    if (!rlu || rlu->type == JSON_NULL) {
        json_free(rlu);
        json_set(self, "holder", holder_default());
    } else {
        json_set(self, "holder", holder_make(rlu));
        json_free(rlu);
    }
    free(parent);
    return json_bool(false);

try_exc:
    /* except MarkerBusy / except OSError — inside the claim loop only */
    free(parent);
    if (exc_is_markerbusy(err)) {
        json_free(err);
        json_set(self, "holder", holder_default());
        return json_bool(false);
    }
    if (exc_is_oserror(err)) {
        json_set(self, "holder", holder_unwritable(path, err));
        json_free(err);
        return json_bool(false);
    }
    return err;
}
