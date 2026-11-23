/*
 * hosted_rooms_flow.c — FLOW + leaf port of gateway/hosted_rooms.py
 * room-lifecycle decision chains: the create/list facades, peer-obligation and
 * grant-revoke/reservation/receipt facades, row ownership fences, legacy-column
 * migration ordering and the event-capacity chain.
 *
 * Decision sequences only: every live I/O (the store verbs, the wall clock,
 * get_hermes_home, sqlite conn.execute, the claimed prune helper) runs through
 * ONE universal seam hrf_call(ctx, name, args). The harness wires it to a
 * recording stub driven by the case's scripted results (in.io); the oracle
 * installs recording monkeypatch stubs fed the SAME scripts.
 *
 * Seam names (-> live call):
 *   hosted_rooms.create_room                 store.hosted_rooms.create_room
 *   hosted_rooms.list_rooms                  store.hosted_rooms.list_rooms
 *   hosted_room_peers.room_has_peer_obligations
 *   hosted_room_peers.retire_room_link_records
 *   hosted_room_peers.due_room_grant_revokes
 *   hosted_room_peers.settle_room_grant_revoke
 *   hosted_room_peers.drop_remote_run_receipt
 *   get_hermes_home                          hermes_constants.get_hermes_home
 *   clock                                    time.time (via common.clock)
 *   conn.execute                             sqlite3.Connection.execute
 *   _prune_disbanded_rooms_locked            module helper (claimed elsewhere)
 *
 * Representation contract (shared with tests/oracle_hosted_rooms_flow.py):
 * - None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
 *   KeyError carries str(exc) == repr(key). No flow here has an except site,
 *   so every scripted raise (BaseException included) propagates verbatim.
 * - A conn.execute script is the ROWS list (fetchone = rows[0] or None,
 *   fetchall/iteration = the list); scripts are queues of row-lists.
 * - placement travels as its Literal value string; owner as its mapping
 *   projection; dataclass results (_typed/_grant_claims/_reservation_grant)
 *   project via asdict; tuples project as lists; frozenset(revoked_digests)
 *   projects as a sorted-unique list (sorted by JSON-serialized element, the
 *   oracle canon's frozenset order).
 * - None-valued seam arg keys are dropped on both sides (owner,
 *   revoked_digests, retry_at, prune now).
 * - libjson stores every number as a double: integral doubles serialize
 *   int-like (%.0f). bounded_int positions (limit/offset) therefore carry int
 *   literals only in the fixture corpus (a Python float 500.0 is rejected by
 *   isinstance(int) but is indistinguishable from 500 after JSON parse);
 *   int()/float() coercion positions accept floats faithfully. str()/repr()
 *   of an integral float reads int-like here (Python "5.0"); the corpus keeps
 *   integral floats out of str-coerced claim values and canonical-JSON byte
 *   boundaries. Container repr/str is ASCII-only in the corpus. int()/float()
 *   string parsing accepts ASCII digits only (Python also admits Unicode Nd
 *   digits such as '١٢'; those are outside the corpus).
 * - default_db_path models PurePosixPath lexically (no drive/UNC/double-slash
 *   roots in the corpus); the oracle compares Path.as_posix().
 * - The canonical-JSON byte bound in _validate_members is checked with a
 *   sorted-key compact serializer (ensure_ascii=False); its result is
 *   discarded by the live code, only the bound matters.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include "hermes_json.h"

typedef struct hrf_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} hrf_ctx_t;

/* ── basic helpers ── */
static bool hrf_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool hrf_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool hrf_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *hrf_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool hrf_str_eq(const json_t *v, const char *s) {
    const char *t = hrf_str(v);
    return t && s && strcmp(t, s) == 0;
}
static char *hrf_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static json_t *hrf_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
/* str(KeyError(k)) == repr(k); the keys here are strings. */
static json_t *hrf_keyerror(const char *key) {
    size_t n = strlen(key ? key : "");
    char *buf = malloc(n + 3);
    if (!buf) return hrf_err("KeyError", "''");
    buf[0] = '\'';
    memcpy(buf + 1, key ? key : "", n);
    buf[n + 1] = '\'';
    buf[n + 2] = '\0';
    json_t *e = hrf_err("KeyError", buf);
    free(buf);
    return e;
}
/* Python type name for TypeError messages (integral doubles read as int). */
static const char *hrf_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER:
            return (v->num_val == trunc(v->num_val) && fabs(v->num_val) < 9.2e18)
                       ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
static json_t *hrf_not_iterable(const json_t *v) {
    char buf[128];
    snprintf(buf, sizeof buf, "'%s' object is not iterable", hrf_type_name(v));
    return hrf_err("TypeError", buf);
}
static json_t *hrf_not_subscriptable(const json_t *v) {
    char buf[128];
    snprintf(buf, sizeof buf, "'%s' object is not subscriptable", hrf_type_name(v));
    return hrf_err("TypeError", buf);
}
/* Propagate a raise result unless it is NULL (success). */
#define HRF_CHK(v) do { if (hrf_is_raise(v)) return (v); } while (0)

/* ── seam dispatch (mirrors the oracle Ctx.call exactly) ── */
static json_t *hrf_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    if (strcmp(type, "KeyError") == 0) {
        /* the spec message is the RAW key; str(KeyError(k)) == repr(k) */
        return hrf_keyerror(msg);
    }
    return hrf_err(type, msg);
}
static json_t *hrf_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return hrf_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}
/* Record one seam event and answer its script. args is OWNED (moved into the
 * trace event). The result is a NEW node the caller frees; a raise object
 * propagates (no flow here catches). */
static json_t *hrf_call(hrf_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    const json_t *entry = c->io ? json_obj_get(c->io, name) : NULL;
    json_t *res;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        res = hrf_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            res = hrf_err("RuntimeError", msg);
        } else {
            json_set(c->cursors, name, json_number(cur + 1));
            res = hrf_entry_result(json_get(entry, idx));
        }
    } else {
        res = hrf_entry_result(entry);
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}
/* args builder: drop None-valued keys on both sides. */
static void hrf_arg(json_t *args, const char *key, const json_t *v) {
    if (!v || v->type == JSON_NULL) return;
    json_set(args, key, json_copy(v));
}
static void hrf_arg_owned(json_t *args, const char *key, json_t *v) {
    if (!v || v->type == JSON_NULL) { json_free(v); return; }
    json_set(args, key, v);
}

/* ── Python float repr (shortest round-trip, Python formatting rules) ── */
static char *hrf_float_repr(double x) {
    char buf[64];
    if (isnan(x)) return hrf_strdup("nan");
    if (isinf(x)) return hrf_strdup(x < 0 ? "-inf" : "inf");
    bool neg = signbit(x);
    double ax = neg ? -x : x;
    int prec;
    for (prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*e", prec - 1, ax);
        if (strtod(buf, NULL) == ax) break;
    }
    char digits[32];
    int nd = 0;
    digits[nd++] = buf[0];
    for (const char *p = buf + 2; *p && *p != 'e'; p++) digits[nd++] = *p;
    digits[nd] = '\0';
    int exp = atoi(strchr(buf, 'e') + 1);
    while (nd > 1 && digits[nd - 1] == '0') nd--;
    digits[nd] = '\0';
    char out[80];
    int k = 0;
    if (neg) out[k++] = '-';
    if (exp < -4 || exp >= 16) {
        out[k++] = digits[0];
        if (nd > 1) {
            out[k++] = '.';
            memcpy(out + k, digits + 1, (size_t)(nd - 1));
            k += nd - 1;
        }
        k += snprintf(out + k, sizeof out - (size_t)k, "e%c%02d",
                      exp < 0 ? '-' : '+', exp < 0 ? -exp : exp);
    } else if (exp >= 0) {
        int ip = exp + 1;
        if (nd <= ip) {
            memcpy(out + k, digits, (size_t)nd);
            k += nd;
            for (int i = nd; i < ip; i++) out[k++] = '0';
            out[k++] = '.';
            out[k++] = '0';
        } else {
            memcpy(out + k, digits, (size_t)ip);
            k += ip;
            out[k++] = '.';
            memcpy(out + k, digits + ip, (size_t)(nd - ip));
            k += nd - ip;
        }
    } else {
        out[k++] = '0';
        out[k++] = '.';
        for (int i = 0; i < -exp - 1; i++) out[k++] = '0';
        memcpy(out + k, digits, (size_t)nd);
        k += nd;
    }
    out[k] = '\0';
    return hrf_strdup(out);
}
/* str() of a JSON number: integral doubles read int-like (libjson identity). */
static char *hrf_num_str(double x) {
    if (x == trunc(x) && fabs(x) < 9.2e18) {
        char buf[64];
        snprintf(buf, sizeof buf, "%.0f", x);
        return hrf_strdup(buf);
    }
    return hrf_float_repr(x);
}
/* ── Python str()/repr() on the JSON corpus ── */
static void hrf_buf_putc(char **buf, size_t *len, size_t *cap, char ch) {
    if (*len + 2 > *cap) {
        *cap = *cap ? *cap * 2 : 64;
        *buf = realloc(*buf, *cap);
    }
    (*buf)[(*len)++] = ch;
    (*buf)[*len] = '\0';
}
static void hrf_buf_puts(char **buf, size_t *len, size_t *cap, const char *s) {
    while (*s) hrf_buf_putc(buf, len, cap, *s++);
}
/* repr(str): single quotes unless the string has ' and no "; ASCII corpus. */
static void hrf_repr_str(char **buf, size_t *len, size_t *cap, const char *s) {
    char quote = '\'';
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    hrf_buf_putc(buf, len, cap, quote);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char ch = *p;
        if (ch == (unsigned char)quote || ch == '\\') {
            hrf_buf_putc(buf, len, cap, '\\');
            hrf_buf_putc(buf, len, cap, (char)ch);
        } else if (ch == '\n') hrf_buf_puts(buf, len, cap, "\\n");
        else if (ch == '\r') hrf_buf_puts(buf, len, cap, "\\r");
        else if (ch == '\t') hrf_buf_puts(buf, len, cap, "\\t");
        else if (ch < 0x20 || ch == 0x7f) {
            char esc[8];
            snprintf(esc, sizeof esc, "\\x%02x", ch);
            hrf_buf_puts(buf, len, cap, esc);
        } else {
            hrf_buf_putc(buf, len, cap, (char)ch);
        }
    }
    hrf_buf_putc(buf, len, cap, quote);
}
static void hrf_repr_node(char **buf, size_t *len, size_t *cap, const json_t *v) {
    if (!v || v->type == JSON_NULL) { hrf_buf_puts(buf, len, cap, "None"); return; }
    switch (v->type) {
        case JSON_BOOL:
            hrf_buf_puts(buf, len, cap, v->bool_val ? "True" : "False");
            return;
        case JSON_NUMBER: {
            char *s = hrf_num_str(v->num_val);
            hrf_buf_puts(buf, len, cap, s);
            free(s);
            return;
        }
        case JSON_STRING:
            hrf_repr_str(buf, len, cap, v->str_val);
            return;
        case JSON_ARRAY:
            hrf_buf_putc(buf, len, cap, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) hrf_buf_puts(buf, len, cap, ", ");
                hrf_repr_node(buf, len, cap, v->c.items[i]);
            }
            hrf_buf_putc(buf, len, cap, ']');
            return;
        case JSON_OBJECT:
            hrf_buf_putc(buf, len, cap, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) hrf_buf_puts(buf, len, cap, ", ");
                hrf_repr_str(buf, len, cap, v->c.keys[i]);
                hrf_buf_puts(buf, len, cap, ": ");
                hrf_repr_node(buf, len, cap, v->c.items[i]);
            }
            hrf_buf_putc(buf, len, cap, '}');
            return;
        default:
            hrf_buf_puts(buf, len, cap, "None");
            return;
    }
}
static char *hrf_py_repr(const json_t *v) {
    char *buf = NULL;
    size_t len = 0, cap = 0;
    hrf_repr_node(&buf, &len, &cap, v);
    return buf ? buf : hrf_strdup("");
}
/* str(value): strings verbatim, containers via repr. */
static char *hrf_py_str(const json_t *v) {
    if (v && v->type == JSON_STRING) return hrf_strdup(v->str_val);
    return hrf_py_repr(v);
}

/* ── Python int()/float() string parsing ── */
/* digits with single underscores BETWEEN digits; >= 1 digit required. */
static bool hrf_scan_digits(const char *s, size_t *i) {
    size_t start = *i;
    if (!isdigit((unsigned char)s[*i])) return false;
    while (isdigit((unsigned char)s[*i])) (*i)++;
    while (s[*i] == '_' && isdigit((unsigned char)s[*i + 1])) {
        (*i)++;
        while (isdigit((unsigned char)s[*i])) (*i)++;
    }
    return *i > start;
}
static bool hrf_parse_int_str(const char *s, long long *out) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    size_t i = a;
    bool neg = false;
    if (i < b && (s[i] == '+' || s[i] == '-')) { neg = s[i] == '-'; i++; }
    if (i >= b || !hrf_scan_digits(s, &i)) return false;
    if (i != b) return false;
    /* accumulate (corpus stays inside int64) */
    long long v = 0;
    for (size_t k = a; k < b; k++) {
        if (s[k] == '_' || s[k] == '+' || s[k] == '-' || isspace((unsigned char)s[k])) continue;
        v = v * 10 + (s[k] - '0');
    }
    *out = neg ? -v : v;
    return true;
}
static bool hrf_parse_float_str(const char *s, double *out) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    size_t len = b - a;
    char *tmp = malloc(len + 1);
    if (!tmp) return false;
    memcpy(tmp, s + a, len);
    tmp[len] = '\0';
    /* inf / infinity / nan (case-insensitive, optional sign) */
    char *body = tmp;
    bool neg = false;
    if (*body == '+' || *body == '-') { neg = *body == '-'; body++; }
    for (char *p = body; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (strcmp(body, "inf") == 0 || strcmp(body, "infinity") == 0) {
        free(tmp);
        *out = neg ? -INFINITY : INFINITY;
        return true;
    }
    if (strcmp(body, "nan") == 0) { free(tmp); *out = NAN; return true; }
    free(tmp);
    /* numeric: [+-]digits[.digits][e[+-]digits], underscores between digits */
    size_t i = a;
    if (i < b && (s[i] == '+' || s[i] == '-')) i++;
    bool any = false;
    if (i < b && isdigit((unsigned char)s[i])) { any = hrf_scan_digits(s, &i) || any; }
    if (i < b && s[i] == '.') {
        i++;
        if (i < b && isdigit((unsigned char)s[i])) { any = hrf_scan_digits(s, &i) || any; }
    }
    if (!any) return false;
    if (i < b && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        if (i < b && (s[i] == '+' || s[i] == '-')) i++;
        if (i >= b || !hrf_scan_digits(s, &i)) return false;
    }
    if (i != b) return false;
    /* build a clean numeric string (drop underscores) and strtod */
    char *clean = malloc(len + 1);
    if (!clean) return false;
    size_t k = 0;
    for (size_t j = a; j < b; j++) if (s[j] != '_') clean[k++] = s[j];
    clean[k] = '\0';
    *out = strtod(clean, NULL);
    free(clean);
    return true;
}
/* int(value): bool -> 1/0, number -> trunc, string -> parse, else TypeError. */
static json_t *hrf_py_int(const json_t *v, long long *out) {
    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return NULL; }
    if (v && v->type == JSON_NUMBER) {
        double x = v->num_val;
        if (isnan(x)) return hrf_err("ValueError", "cannot convert float NaN to integer");
        if (isinf(x)) return hrf_err("OverflowError", "cannot convert float infinity to integer");
        if (fabs(x) >= 9.2e18) return hrf_err("OverflowError", "int too large for C long long");
        *out = (long long)trunc(x);
        return NULL;
    }
    if (v && v->type == JSON_STRING) {
        long long r;
        if (hrf_parse_int_str(v->str_val, &r)) { *out = r; return NULL; }
        char *rp = hrf_py_repr(v);
        size_t n = strlen(rp) + 64;
        char *msg = malloc(n);
        snprintf(msg, n, "invalid literal for int() with base 10: %s", rp);
        free(rp);
        json_t *e = hrf_err("ValueError", msg);
        free(msg);
        return e;
    }
    char buf[160];
    snprintf(buf, sizeof buf,
             "int() argument must be a string, a bytes-like object or a real number, not '%s'",
             hrf_type_name(v));
    return hrf_err("TypeError", buf);
}
/* float(value): bool -> 1.0/0.0, number verbatim, string -> parse. */
static json_t *hrf_py_float(const json_t *v, double *out) {
    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return NULL; }
    if (v && v->type == JSON_NUMBER) { *out = v->num_val; return NULL; }
    if (v && v->type == JSON_STRING) {
        double r;
        if (hrf_parse_float_str(v->str_val, &r)) { *out = r; return NULL; }
        char *rp = hrf_py_repr(v);
        size_t n = strlen(rp) + 64;
        char *msg = malloc(n);
        snprintf(msg, n, "could not convert string to float: %s", rp);
        free(rp);
        json_t *e = hrf_err("ValueError", msg);
        free(msg);
        return e;
    }
    char buf[160];
    snprintf(buf, sizeof buf, "float() argument must be a string or a real number, not '%s'",
             hrf_type_name(v));
    return hrf_err("TypeError", buf);
}

/* ── UTF-8 helpers + str.strip() (Unicode White_Space) ── */
static size_t hrf_utf8_count(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xc0) != 0x80) n++;
    return n;
}
static bool hrf_ws_cp(uint32_t cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0d) || (cp >= 0x1c && cp <= 0x1f)) return true;
    if (cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return true;
    return false;
}
static size_t hrf_utf8_decode(const char *s, size_t i, uint32_t *cp) {
    const unsigned char *p = (const unsigned char *)s + i;
    if (p[0] < 0x80) { *cp = p[0]; return 1; }
    if ((p[0] & 0xe0) == 0xc0 && p[1]) { *cp = ((p[0] & 0x1f) << 6) | (p[1] & 0x3f); return 2; }
    if ((p[0] & 0xf0) == 0xe0 && p[1] && p[2]) {
        *cp = ((p[0] & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f);
        return 3;
    }
    if ((p[0] & 0xf8) == 0xf0 && p[1] && p[2] && p[3]) {
        *cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f);
        return 4;
    }
    *cp = p[0];
    return 1;
}
static char *hrf_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    uint32_t cp;
    while (a < b) {
        size_t l = hrf_utf8_decode(s, a, &cp);
        if (!hrf_ws_cp(cp)) break;
        a += l;
    }
    while (b > a) {
        size_t back = b - 1;
        while (back > a && ((unsigned char)s[back] & 0xc0) == 0x80) back--;
        size_t l = hrf_utf8_decode(s, back, &cp);
        if (!hrf_ws_cp(cp) || back + l != b) break;
        b = back;
    }
    char *o = malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}

/* ── identifier / bounded_int (hosted_rooms_common semantics) ── */
#define HRF_P_IDENT 0   /* ^[A-Za-z0-9][A-Za-z0-9._:-]*$ */
#define HRF_P_KIND  1   /* ^[a-z][a-z0-9_.-]*$ */
#define HRF_P_NONE  2
static bool hrf_ident_char(unsigned char ch, bool first, int pattern) {
    if (pattern == HRF_P_IDENT) {
        if (first) return isalnum(ch) != 0;
        return isalnum(ch) || ch == '.' || ch == '_' || ch == ':' || ch == '-';
    }
    if (first) return ch >= 'a' && ch <= 'z';
    return (ch >= 'a' && ch <= 'z') || isdigit(ch) || ch == '_' || ch == '.' || ch == '-';
}
static bool hrf_pattern_match(const char *s, int pattern) {
    if (pattern == HRF_P_NONE) return true;
    if (!s[0]) return false;
    for (size_t i = 0; s[i]; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch >= 0x80) return false; /* both patterns are ASCII-only */
        if (!hrf_ident_char(ch, i == 0, pattern)) return false;
    }
    return true;
}
/* identifier(value, label=..., error=HostedRoomError, max_chars=..., pattern=..., invalid=...) */
static char *hrf_identifier(const json_t *v, const char *label, int max_chars,
                            int pattern, const char *invalid, json_t **err) {
    *err = NULL;
    if (!v || v->type != JSON_STRING) {
        char buf[160];
        snprintf(buf, sizeof buf, "%s must be a string", label);
        *err = hrf_err("HostedRoomError", buf);
        return NULL;
    }
    char *s = hrf_strip(v->str_val);
    size_t count = hrf_utf8_count(s);
    if (count == 0 || count > (size_t)max_chars || !hrf_pattern_match(s, pattern)) {
        char buf[160];
        if (invalid) snprintf(buf, sizeof buf, "%s", invalid);
        else snprintf(buf, sizeof buf, "invalid %s", label);
        *err = hrf_err("HostedRoomError", buf);
        free(s);
        return NULL;
    }
    return s;
}
static char *hrf_room_id(const json_t *v, json_t **err) {
    return hrf_identifier(v, "room_id", 128, HRF_P_IDENT, NULL, err);
}
static char *hrf_actor_id(const json_t *v, const char *label, json_t **err) {
    return hrf_identifier(v, label, 128, HRF_P_IDENT, NULL, err);
}
static char *hrf_room_name(const json_t *v, json_t **err) {
    return hrf_identifier(v, "name", 200, HRF_P_NONE, "invalid room name", err);
}
/* bounded_int: reject bools, non-ints and out-of-range. Integral doubles model
 * int literals (fixture corpus); non-integral doubles are floats -> rejected. */
static json_t *hrf_bounded_int(const json_t *v, const char *message,
                               long long low, long long high, bool has_high, long long *out) {
    bool ok = v && v->type == JSON_NUMBER && v->num_val == trunc(v->num_val)
              && fabs(v->num_val) < 9.2e18;
    if (ok) {
        long long x = (long long)v->num_val;
        if (x >= low && (!has_high || x <= high)) { *out = x; return NULL; }
    }
    return hrf_err("HostedRoomError", message);
}
/* ── canonical_json (sorted-key compact, ensure_ascii=False, UTF-8 bound) ── */
static void hrf_canon_str(char **buf, size_t *len, size_t *cap, const char *s) {
    hrf_buf_putc(buf, len, cap, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char ch = *p;
        switch (ch) {
            case '"': hrf_buf_puts(buf, len, cap, "\\\""); break;
            case '\\': hrf_buf_puts(buf, len, cap, "\\\\"); break;
            case '\n': hrf_buf_puts(buf, len, cap, "\\n"); break;
            case '\r': hrf_buf_puts(buf, len, cap, "\\r"); break;
            case '\t': hrf_buf_puts(buf, len, cap, "\\t"); break;
            case '\b': hrf_buf_puts(buf, len, cap, "\\b"); break;
            case '\f': hrf_buf_puts(buf, len, cap, "\\f"); break;
            default:
                if (ch < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof esc, "\\u%04x", ch);
                    hrf_buf_puts(buf, len, cap, esc);
                } else {
                    hrf_buf_putc(buf, len, cap, (char)ch);
                }
        }
    }
    hrf_buf_putc(buf, len, cap, '"');
}
static int hrf_key_cmp(const void *a, const void *b) {
    const char *ka = *(const char *const *)a;
    const char *kb = *(const char *const *)b;
    return strcmp(ka, kb);
}
static void hrf_canon_node(char **buf, size_t *len, size_t *cap, const json_t *v) {
    if (!v || v->type == JSON_NULL) { hrf_buf_puts(buf, len, cap, "null"); return; }
    switch (v->type) {
        case JSON_BOOL:
            hrf_buf_puts(buf, len, cap, v->bool_val ? "true" : "false");
            return;
        case JSON_NUMBER: {
            char *s = hrf_num_str(v->num_val);
            hrf_buf_puts(buf, len, cap, s);
            free(s);
            return;
        }
        case JSON_STRING:
            hrf_canon_str(buf, len, cap, v->str_val);
            return;
        case JSON_ARRAY:
            hrf_buf_putc(buf, len, cap, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) hrf_buf_putc(buf, len, cap, ',');
                hrf_canon_node(buf, len, cap, v->c.items[i]);
            }
            hrf_buf_putc(buf, len, cap, ']');
            return;
        case JSON_OBJECT: {
            hrf_buf_putc(buf, len, cap, '{');
            size_t n = v->c.count;
            if (n) {
                const char **keys = malloc(n * sizeof(char *));
                for (size_t i = 0; i < n; i++) keys[i] = v->c.keys[i];
                qsort(keys, n, sizeof(char *), hrf_key_cmp);
                for (size_t i = 0; i < n; i++) {
                    if (i) hrf_buf_putc(buf, len, cap, ',');
                    hrf_canon_str(buf, len, cap, keys[i]);
                    hrf_buf_putc(buf, len, cap, ':');
                    hrf_canon_node(buf, len, cap, json_obj_get(v, keys[i]));
                }
                free(keys);
            }
            hrf_buf_putc(buf, len, cap, '}');
            return;
        }
        default:
            hrf_buf_puts(buf, len, cap, "null");
            return;
    }
}
static char *hrf_canonical(const json_t *v, const char *label, size_t max_bytes, json_t **err) {
    *err = NULL;
    char *buf = NULL;
    size_t len = 0, cap = 0;
    hrf_canon_node(&buf, &len, &cap, v);
    if (!buf) buf = hrf_strdup("");
    if (strlen(buf) > max_bytes) {
        char msg[160];
        snprintf(msg, sizeof msg, "%s is too large", label);
        *err = hrf_err("HostedRoomError", msg);
        free(buf);
        return NULL;
    }
    return buf;
}

/* ── _validate_members ── */
static json_t *hrf_validate_members(const json_t *value) {
    if (!value || value->type != JSON_ARRAY)
        return hrf_err("HostedRoomError", "members must be a list");
    if (value->c.count > 128)
        return hrf_err("HostedRoomError", "too many room members");
    for (size_t i = 0; i < value->c.count; i++) {
        const json_t *m = value->c.items[i];
        if (!m || m->type != JSON_OBJECT)
            return hrf_err("HostedRoomError", "each room member must be an object");
    }
    json_t *members = json_copy(value); /* [dict(member) for member in value] */
    json_t *err = NULL;
    char *canon = hrf_canonical(members, "members", 128 * 1024, &err);
    free(canon);
    if (err) { json_free(members); return err; }
    return members;
}

/* ── _claim_values / _typed / _grant_claims / _reservation_* ── */
static json_t *hrf_claim_values(const json_t *claims, const char *const *keys, size_t n) {
    if (!claims || claims->type != JSON_OBJECT) {
        char buf[128];
        snprintf(buf, sizeof buf, "'%s' object has no attribute 'get'", hrf_type_name(claims));
        return hrf_err("AttributeError", buf);
    }
    json_t *out = json_object();
    for (size_t i = 0; i < n; i++) {
        const json_t *v = json_obj_get(claims, keys[i]);
        char *s = hrf_truthy(v) ? hrf_py_str(v) : hrf_strdup("");
        json_set(out, keys[i], json_string(s));
        free(s);
    }
    return out;
}
static const char *HRF_RRI_FIELDS[] = {
    "room_id", "home_install_id", "authority_gateway_id", "authority_epoch", "member_id",
    "target_install_id", "target_profile", "task_id", "execution_generation",
};
static const char *HRF_RGC_FIELDS[] = {
    "room_id", "home_install_id", "authority_gateway_id", "authority_epoch", "member_id",
    "target_install_id", "target_profile", "issued_at",
};
static const char *HRF_RRR_FIELDS[] = {
    "room_id", "home_install_id", "authority_gateway_id", "authority_epoch", "member_id",
    "target_install_id", "target_profile", "task_id", "execution_generation", "run_id", "session_id",
};
static const char *HRF_RLR_FIELDS[] = {
    "room_id", "member_id", "target_url", "target_profile", "grant", "catalog_json",
    "cancellation_scope_id", "trace_id", "transport_security", "status", "updated_at",
};
typedef struct { const char *name; const char *const *fields; size_t n; } hrf_record_type_t;
static const hrf_record_type_t HRF_RECORD_TYPES[] = {
    {"RemoteRunIdentity", HRF_RRI_FIELDS, 9},
    {"RoomGrantClaims", HRF_RGC_FIELDS, 8},
    {"RemoteRunReceipt", HRF_RRR_FIELDS, 11},
    {"RoomLinkRecord", HRF_RLR_FIELDS, 11},
};
/* _typed impl: record_type(**{f.name: record[f.name] for f in fields(type)}). */
static json_t *hrf_typed_impl(const char *type_name, const json_t *record) {
    const hrf_record_type_t *rt = NULL;
    for (size_t i = 0; i < sizeof(HRF_RECORD_TYPES) / sizeof(HRF_RECORD_TYPES[0]); i++)
        if (strcmp(HRF_RECORD_TYPES[i].name, type_name) == 0) rt = &HRF_RECORD_TYPES[i];
    if (!rt) return hrf_err("ValueError", "unknown record type");
    if (!record || record->type != JSON_OBJECT) {
        if (!record || record->type == JSON_NULL)
            return hrf_err("TypeError", "'NoneType' object is not subscriptable");
        if (record->type == JSON_ARRAY)
            return hrf_err("TypeError", "list indices must be integers or slices, not str");
        if (record->type == JSON_STRING)
            return hrf_err("TypeError", "string indices must be integers");
        return hrf_not_subscriptable(record);
    }
    json_t *out = json_object();
    for (size_t i = 0; i < rt->n; i++) {
        const json_t *v = json_obj_get(record, rt->fields[i]);
        if (!v) { json_free(out); return hrf_keyerror(rt->fields[i]); }
        json_set(out, rt->fields[i], json_copy(v));
    }
    return out;
}
static const char *HRF_GRANT_STR_KEYS[] = {
    "room_id", "home_install_id", "authority_gateway_id", "member_id",
    "target_install_id", "target_profile",
};
/* _grant_claims impl: RoomGrantClaims(**values, authority_epoch=int(... or 0),
 * issued_at=float(... or 0)); projected as its field dict. */
static json_t *hrf_grant_claims_impl(const json_t *claims) {
    json_t *values = hrf_claim_values(claims, HRF_GRANT_STR_KEYS, 6);
    HRF_CHK(values);
    const json_t *epoch_node = claims->type == JSON_OBJECT ? json_obj_get(claims, "authority_epoch") : NULL;
    long long epoch = 0;
    if (hrf_truthy(epoch_node)) {
        json_t *e = hrf_py_int(epoch_node, &epoch);
        if (e) { json_free(values); return e; }
    }
    const json_t *issued_node = claims->type == JSON_OBJECT ? json_obj_get(claims, "issued_at") : NULL;
    double issued = 0.0;
    if (hrf_truthy(issued_node)) {
        json_t *e = hrf_py_float(issued_node, &issued);
        if (e) { json_free(values); return e; }
    }
    json_t *out = json_object();
    json_set(out, "room_id", json_copy(json_obj_get(values, "room_id")));
    json_set(out, "home_install_id", json_copy(json_obj_get(values, "home_install_id")));
    json_set(out, "authority_gateway_id", json_copy(json_obj_get(values, "authority_gateway_id")));
    json_set(out, "authority_epoch", json_number((double)epoch));
    json_set(out, "member_id", json_copy(json_obj_get(values, "member_id")));
    json_set(out, "target_install_id", json_copy(json_obj_get(values, "target_install_id")));
    json_set(out, "target_profile", json_copy(json_obj_get(values, "target_profile")));
    json_set(out, "issued_at", json_number(issued));
    json_free(values);
    return out;
}
/* _reservation_claims impl: ordered identifier fences then epoch positivity. */
static json_t *hrf_reservation_claims_impl(const json_t *claims) {
    if (!claims || claims->type != JSON_OBJECT) {
        char buf[128];
        snprintf(buf, sizeof buf, "'%s' object has no attribute 'get'", hrf_type_name(claims));
        return hrf_err("AttributeError", buf);
    }
    json_t *err = NULL;
    char *rid = hrf_room_id(json_obj_get(claims, "room_id"), &err);
    if (err) return err;
    char *mid = hrf_actor_id(json_obj_get(claims, "member_id"), "member_id", &err);
    if (err) { free(rid); return err; }
    char *tp = hrf_actor_id(json_obj_get(claims, "target_profile"), "target_profile", &err);
    if (err) { free(rid); free(mid); return err; }
    char *gid = hrf_actor_id(json_obj_get(claims, "authority_gateway_id"), "authority_gateway_id", &err);
    if (err) { free(rid); free(mid); free(tp); return err; }
    const json_t *epoch_node = json_obj_get(claims, "authority_epoch");
    long long epoch = 0;
    if (hrf_truthy(epoch_node)) {
        json_t *e = hrf_py_int(epoch_node, &epoch);
        if (e) { free(rid); free(mid); free(tp); free(gid); return e; }
    }
    if (epoch < 1) {
        free(rid); free(mid); free(tp); free(gid);
        return hrf_err("HostedRoomError", "authority_epoch must be positive");
    }
    json_t *out = json_array();
    json_append(out, json_string(rid));
    json_append(out, json_string(mid));
    json_append(out, json_string(tp));
    json_append(out, json_string(gid));
    json_append(out, json_number((double)epoch));
    free(rid); free(mid); free(tp); free(gid);
    return out;
}
/* _reservation_grant impl: replace(_grant_claims(claims), ...validated key). */
static json_t *hrf_reservation_grant_impl(const json_t *claims) {
    json_t *rc = hrf_reservation_claims_impl(claims);
    HRF_CHK(rc);
    json_t *gc = hrf_grant_claims_impl(claims);
    if (hrf_is_raise(gc)) { json_free(rc); return gc; }
    json_set(gc, "room_id", json_copy(json_get(rc, 0)));
    json_set(gc, "member_id", json_copy(json_get(rc, 1)));
    json_set(gc, "target_profile", json_copy(json_get(rc, 2)));
    json_set(gc, "authority_gateway_id", json_copy(json_get(rc, 3)));
    json_set(gc, "authority_epoch", json_copy(json_get(rc, 4)));
    json_free(rc);
    return gc;
}

/* ── PurePosixPath lexical model (corpus: no drive/UNC/double-slash roots) ── */
static char *hrf_path_normalize(const char *s) {
    if (!s || !s[0]) return hrf_strdup(".");
    /* Windows Path folds '\\' into '/': normalize backslashes before the
     * lexical composition below (device/UNC prefix '\\\\?' -> '/?'). */
    size_t slen = strlen(s);
    char *folded = malloc(slen + 1);
    if (!folded) return NULL;
    for (size_t i = 0; i < slen; i++) folded[i] = (s[i] == '\\') ? '/' : s[i];
    folded[slen] = '\0';
    s = folded;

    bool absolute = s[0] == '/';
    char *out = malloc(slen + 2);
    if (!out) { free(folded); return NULL; }
    size_t k = 0;
    if (absolute) out[k++] = '/';
    const char *p = s;
    bool first = true;
    while (*p) {
        while (*p == '/') p++;
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t n = (size_t)(p - start);
        if (n == 0) break;
        if (n == 1 && start[0] == '.') continue;
        if (!first) out[k++] = '/';
        memcpy(out + k, start, n);
        k += n;
        first = false;
    }
    if (k == 0) out[k++] = absolute ? '/' : '.';
    out[k] = '\0';
    free(folded);
    return out;
}
static char *hrf_path_parent(const char *p) {
    if (strcmp(p, "/") == 0 || strcmp(p, ".") == 0) return hrf_strdup(p);
    const char *slash = strrchr(p, '/');
    if (!slash) return hrf_strdup(".");
    if (slash == p) return hrf_strdup("/");
    size_t n = (size_t)(slash - p);
    char *o = malloc(n + 1);
    memcpy(o, p, n);
    o[n] = '\0';
    return o;
}
static const char *hrf_path_name(const char *p) {
    if (strcmp(p, "/") == 0 || strcmp(p, ".") == 0) return "";
    const char *slash = strrchr(p, '/');
    return slash ? slash + 1 : p;
}
static char *hrf_path_join(const char *a, const char *b) {
    if (b[0] == '/') return hrf_path_normalize(b);
    size_t na = strlen(a), nb = strlen(b);
    char *tmp = malloc(na + nb + 2);
    if (!tmp) return NULL;
    memcpy(tmp, a, na);
    size_t k = na;
    if (k == 0 || tmp[k - 1] != '/') tmp[k++] = '/';
    memcpy(tmp + k, b, nb + 1);
    char *o = hrf_path_normalize(tmp);
    free(tmp);
    return o;
}
/* ── SQL constants (byte-stable, extracted from the quarry) ── */
static const char HRF_SUM_EVENT_BYTES[] = "SELECT COALESCE(SUM(event_bytes), 0) FROM hosted_rooms";
static const char HRF_INSERT_EVENT[] =
    "INSERT INTO hosted_room_events (room_id, seq, event_id, kind, actor_json, authority_epoch, "
    "payload_json, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
static const char HRF_EVENT_BYTES_BACKFILL[] =
    "UPDATE hosted_rooms\n"
    "                  SET event_bytes=COALESCE((\n"
    "                      SELECT SUM(\n"
    "                          length(CAST(event_id AS BLOB)) +\n"
    "                          length(CAST(kind AS BLOB)) +\n"
    "                          length(CAST(actor_json AS BLOB)) +\n"
    "                          length(CAST(payload_json AS BLOB))\n"
    "                      )\n"
    "                      FROM hosted_room_events\n"
    "                      WHERE hosted_room_events.room_id=hosted_rooms.room_id\n"
    "                  ), 0) WHERE 1";
/* _LEGACY_ACTOR_JSON = _system_actor_json("legacy").replace("'", "''") */
static const char HRF_LEGACY_ACTOR_DEFAULT[] = "'{\"id\":\"legacy\",\"kind\":\"system\"}'";
typedef struct {
    const char *table, *column, *declaration, *default_lit; /* default_lit NULL == no DEFAULT */
} hrf_legacy_column_t;
static const hrf_legacy_column_t HRF_LEGACY_COLUMNS[] = {
    {"hosted_rooms", "authority_gateway_id", "TEXT NOT NULL", "'legacy'"},
    {"hosted_rooms", "authority_epoch", "INTEGER NOT NULL", "1"},
    {"hosted_rooms", "event_bytes", "INTEGER NOT NULL", "0"},
    {"hosted_room_events", "actor_json", "TEXT NOT NULL", HRF_LEGACY_ACTOR_DEFAULT},
    {"hosted_room_events", "authority_epoch", "INTEGER", NULL},
};

/* ── conn.execute seam + cursor projections ── */
static json_t *hrf_execute(hrf_ctx_t *c, const char *sql, json_t *params) {
    json_t *a = json_object();
    json_set_hdr(a, "sql", sql);
    if (params) json_set(a, "params", params); /* owned */
    return hrf_call(c, "conn.execute", a);
}
/* fetchone: the script is the rows list; rows[0] or None. A non-list script
 * fails exactly where live iterates the cursor (TypeError). */
static json_t *hrf_fetchone(hrf_ctx_t *c, const char *sql, json_t *params) {
    json_t *res = hrf_execute(c, sql, params);
    HRF_CHK(res);
    if (res->type != JSON_ARRAY) { json_t *e = hrf_not_iterable(res); json_free(res); return e; }
    json_t *row = json_len(res) ? json_copy(json_get(res, 0)) : NULL;
    json_free(res);
    return row;
}
static bool hrf_arr_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (hrf_str_eq(arr->c.items[i], s)) return true;
    return false;
}
/* row[key] guard for the dict-modelled sqlite rows. */
static json_t *hrf_subscript_check(const json_t *row) {
    if (row && row->type == JSON_OBJECT) return NULL;
    if (!row || row->type == JSON_NULL)
        return hrf_err("TypeError", "'NoneType' object is not subscriptable");
    if (row->type == JSON_ARRAY)
        return hrf_err("TypeError", "list indices must be integers or slices, not str");
    if (row->type == JSON_STRING)
        return hrf_err("TypeError", "string indices must be integers");
    return hrf_not_subscriptable(row);
}

/* ── _is_retired ── */
static json_t *hrf_is_retired_impl(hrf_ctx_t *c, const json_t *room_id) {
    json_t *params = json_array();
    json_append(params, room_id ? json_copy(room_id) : json_null());
    json_t *row = hrf_fetchone(c, "SELECT 1 FROM hosted_room_retired_ids WHERE room_id=?", params);
    HRF_CHK(row);
    bool retired = row != NULL;
    json_free(row);
    return json_bool(retired);
}
/* PoP: hrf_is_retired @ gateway/hosted_rooms.py:_is_retired */
static json_t *hrf_is_retired(hrf_ctx_t *c, const json_t *args) {
    return hrf_is_retired_impl(c, json_obj_get(args, "room_id"));
}

/* ── _room_row: precise not-found / expired decision chain ── */
/* PoP: hrf_room_row @ gateway/hosted_rooms.py:_room_row */
static json_t *hrf_room_row(hrf_ctx_t *c, const json_t *args) {
    const char *sql = json_get_str(args, "sql", "");
    const json_t *room_id = json_obj_get(args, "room_id");
    json_t *params = json_obj_get(args, "params");
    json_t *row = hrf_fetchone(c, sql, params ? json_copy(params) : NULL);
    HRF_CHK(row);
    if (row) return row;
    json_t *p2 = json_array();
    json_append(p2, room_id ? json_copy(room_id) : json_null());
    json_t *retained = hrf_fetchone(c, "SELECT 1 FROM hosted_rooms WHERE room_id=?", p2);
    HRF_CHK(retained);
    if (!retained) {
        json_t *ret = hrf_is_retired_impl(c, room_id);
        HRF_CHK(ret);
        bool retired = ret->type == JSON_BOOL && ret->bool_val;
        json_free(ret);
        if (retired)
            return hrf_err("RoomHistoryExpiredError",
                           "Group Chat history expired; room_id remains permanently retired");
    }
    json_free(retained);
    return hrf_err("RoomNotFoundError", "hosted room not found");
}

/* ── _require_authority: str/int ownership fence ── */
/* PoP: hrf_require_authority @ gateway/hosted_rooms.py:_require_authority */
static json_t *hrf_require_authority(hrf_ctx_t *c, const json_t *args) {
    (void)c;
    const json_t *room = json_obj_get(args, "room");
    const json_t *gateway_id = json_obj_get(args, "gateway_id");
    const json_t *epoch = json_obj_get(args, "epoch");
    const char *message = json_get_str(args, "message", "");
    json_t *guard = hrf_subscript_check(room);
    HRF_CHK(guard);
    const json_t *g = json_obj_get(room, "authority_gateway_id");
    if (!g) return hrf_keyerror("authority_gateway_id");
    const json_t *e = json_obj_get(room, "authority_epoch");
    if (!e) return hrf_keyerror("authority_epoch");
    char *gs = hrf_py_str(g);
    bool mismatch_g = !gateway_id || gateway_id->type != JSON_STRING
                      || strcmp(gs, gateway_id->str_val) != 0;
    free(gs);
    long long ei = 0;
    json_t *ie = hrf_py_int(e, &ei);
    HRF_CHK(ie);
    bool mismatch_e;
    if (epoch && epoch->type == JSON_NUMBER) mismatch_e = (double)ei != epoch->num_val;
    else if (epoch && epoch->type == JSON_BOOL) mismatch_e = ei != (epoch->bool_val ? 1 : 0);
    else mismatch_e = true; /* int != non-number is always True */
    if (mismatch_g || mismatch_e) return hrf_err("AuthorityConflictError", message);
    return json_null();
}

/* ── _gateway_event_bytes (internal helper; runs real on both sides) ── */
static json_t *hrf_gateway_event_bytes(hrf_ctx_t *c) {
    json_t *row = hrf_fetchone(c, HRF_SUM_EVENT_BYTES, NULL);
    HRF_CHK(row);
    if (!row) return hrf_err("TypeError", "'NoneType' object is not subscriptable");
    const json_t *v;
    if (row->type == JSON_ARRAY) {
        if (json_len(row) == 0) { json_free(row); return hrf_err("IndexError", "list index out of range"); }
        v = json_get(row, 0);
    } else if (row->type == JSON_OBJECT) {
        json_free(row);
        return hrf_err("KeyError", "0");
    } else {
        json_t *e = hrf_not_subscriptable(row);
        json_free(row);
        return e;
    }
    long long n = 0;
    json_t *ie = hrf_py_int(v, &n);
    if (ie) { json_free(row); return ie; }
    json_free(row);
    return json_number((double)n);
}

/* ── table_columns (internal helper; common.table_columns runs real) ── */
static json_t *hrf_table_columns(hrf_ctx_t *c, const char *table) {
    char sql[256];
    snprintf(sql, sizeof sql, "PRAGMA table_info(%s)", table);
    json_t *res = hrf_execute(c, sql, NULL);
    HRF_CHK(res);
    if (res->type != JSON_ARRAY) { json_t *e = hrf_not_iterable(res); json_free(res); return e; }
    json_t *out = json_array();
    for (size_t i = 0; i < res->c.count; i++) {
        const json_t *row = res->c.items[i];
        if (!row || row->type != JSON_ARRAY) {
            json_free(out);
            json_t *e = row && row->type == JSON_OBJECT ? hrf_err("KeyError", "1")
                                                        : hrf_not_subscriptable(row);
            json_free(res);
            return e;
        }
        if (row->c.count < 2) {
            json_free(out);
            json_free(res);
            return hrf_err("IndexError", "list index out of range");
        }
        json_append(out, json_copy(row->c.items[1]));
    }
    json_free(res);
    return out;
}

/* ── _migrate_legacy_columns: ordered ALTERs, cached PRAGMAs, backfill ── */
/* PoP: hrf_migrate_legacy_columns @ gateway/hosted_rooms.py:_migrate_legacy_columns */
static json_t *hrf_migrate_legacy_columns(hrf_ctx_t *c, const json_t *args) {
    (void)args;
    json_t *columns = json_object(); /* table -> column list, read once each */
    for (size_t i = 0; i < sizeof(HRF_LEGACY_COLUMNS) / sizeof(HRF_LEGACY_COLUMNS[0]); i++) {
        const hrf_legacy_column_t *lc = &HRF_LEGACY_COLUMNS[i];
        json_t *cols = json_obj_get(columns, lc->table);
        if (!cols) {
            cols = hrf_table_columns(c, lc->table);
            if (hrf_is_raise(cols)) { json_free(columns); return cols; }
            json_set(columns, lc->table, cols);
        }
        if (!hrf_arr_has_str(cols, lc->column)) {
            char sql[512];
            if (lc->default_lit)
                snprintf(sql, sizeof sql, "ALTER TABLE %s ADD COLUMN %s %s DEFAULT %s",
                         lc->table, lc->column, lc->declaration, lc->default_lit);
            else
                snprintf(sql, sizeof sql, "ALTER TABLE %s ADD COLUMN %s %s",
                         lc->table, lc->column, lc->declaration);
            json_t *res = hrf_execute(c, sql, NULL);
            if (hrf_is_raise(res)) { json_free(columns); return res; }
            json_free(res);
        }
    }
    if (!hrf_arr_has_str(json_obj_get(columns, "hosted_rooms"), "event_bytes")) {
        json_t *res = hrf_execute(c, HRF_EVENT_BYTES_BACKFILL, NULL);
        if (hrf_is_raise(res)) { json_free(columns); return res; }
        json_free(res);
    }
    json_free(columns);
    return json_null();
}

/* ── _prepare_event: capacity decision chain ── */
static json_t *hrf_prepare_event_impl(hrf_ctx_t *c, const json_t *room,
                                      const json_t *event_id, const json_t *kind,
                                      const json_t *actor_json, const json_t *payload_json,
                                      bool allow_control) {
    const json_t *parts[4] = {event_id, kind, actor_json, payload_json};
    size_t additional = 0;
    for (int i = 0; i < 4; i++) {
        if (!parts[i] || parts[i]->type != JSON_STRING) {
            char buf[160];
            snprintf(buf, sizeof buf, "sequence item %d: expected str instance, %s found",
                     i, hrf_type_name(parts[i]));
            return hrf_err("TypeError", buf);
        }
        additional += strlen(parts[i]->str_val); /* utf8_len: bytes of the join */
    }
    long long count_reserve = allow_control ? 64 : 0;
    long long byte_reserve = allow_control ? 1024 * 1024 : 0;
    long long gateway_byte_limit = 16 * 1024 * 1024 + byte_reserve;
    json_t *guard = hrf_subscript_check(room);
    HRF_CHK(guard);
    const json_t *ns_node = json_obj_get(room, "next_seq");
    if (!ns_node) return hrf_keyerror("next_seq");
    long long next_seq = 0;
    json_t *ie = hrf_py_int(ns_node, &next_seq);
    HRF_CHK(ie);
    if (next_seq - 1 >= 50000 + count_reserve)
        return hrf_err("HostedRoomError",
                       "This Group Chat reached its history limit. Start a new Group Chat to continue.");
    const json_t *eb_node = json_obj_get(room, "event_bytes");
    if (!eb_node) return hrf_keyerror("event_bytes");
    long long event_bytes = 0;
    ie = hrf_py_int(eb_node, &event_bytes);
    HRF_CHK(ie);
    if (event_bytes + (long long)additional > 256 * 1024 * 1024 + byte_reserve)
        return hrf_err("HostedRoomError",
                       "This Group Chat reached its storage limit. Start a new Group Chat to continue.");
    json_t *gb_node = hrf_gateway_event_bytes(c);
    HRF_CHK(gb_node);
    long long gateway_bytes = (long long)gb_node->num_val;
    json_free(gb_node);
    if (gateway_bytes + (long long)additional > gateway_byte_limit) {
        long long target = gateway_byte_limit - (long long)additional;
        if (target < 0) target = 0;
        json_t *pa = json_object();
        json_set(pa, "max_gateway_event_bytes", json_number((double)target));
        json_t *pr = hrf_call(c, "_prune_disbanded_rooms_locked", pa);
        if (hrf_is_raise(pr)) return pr;
        json_free(pr);
        gb_node = hrf_gateway_event_bytes(c);
        HRF_CHK(gb_node);
        gateway_bytes = (long long)gb_node->num_val;
        json_free(gb_node);
    }
    if (gateway_bytes + (long long)additional > gateway_byte_limit)
        return hrf_err("HostedRoomError",
                       "Group Chat storage is full on this host. Delete an old Group Chat and try again.");
    return json_number((double)additional);
}
/* PoP: hrf_prepare_event @ gateway/hosted_rooms.py:_prepare_event */
static json_t *hrf_prepare_event(hrf_ctx_t *c, const json_t *args) {
    return hrf_prepare_event_impl(c, json_obj_get(args, "room"),
                                  json_obj_get(args, "event_id"), json_obj_get(args, "kind"),
                                  json_obj_get(args, "actor_json"), json_obj_get(args, "payload_json"),
                                  json_get_bool(args, "allow_control", false));
}

/* ── _insert_event: capacity check THEN the INSERT ── */
/* PoP: hrf_insert_event @ gateway/hosted_rooms.py:_insert_event */
static json_t *hrf_insert_event(hrf_ctx_t *c, const json_t *args) {
    json_t *bytes = hrf_prepare_event_impl(c, json_obj_get(args, "room"),
                                           json_obj_get(args, "event_id"), json_obj_get(args, "kind"),
                                           json_obj_get(args, "actor_json"),
                                           json_obj_get(args, "payload_json"),
                                           json_get_bool(args, "allow_control", false));
    HRF_CHK(bytes);
    static const char *param_keys[] = {
        "room_id", "seq", "event_id", "kind", "actor_json", "epoch", "payload_json", "now",
    };
    json_t *params = json_array();
    for (size_t i = 0; i < 8; i++) {
        const json_t *v = json_obj_get(args, param_keys[i]);
        json_append(params, v ? json_copy(v) : json_null());
    }
    json_t *res = hrf_execute(c, HRF_INSERT_EVENT, params);
    if (hrf_is_raise(res)) { json_free(bytes); return res; }
    json_free(res);
    return bytes;
}
/* ── clock seam (common.clock: time.time() only when now is None) ── */
static json_t *hrf_now(hrf_ctx_t *c, const json_t *now_node) {
    if (hrf_is_null(now_node)) return hrf_call(c, "clock", json_object());
    double t = 0.0;
    json_t *e = hrf_py_float(now_node, &t);
    if (e) return e;
    return json_number(t);
}

/* ── frozenset projection: sorted-unique by JSON-serialized element ── */
static int hrf_node_cmp(const void *a, const void *b) {
    const json_t *na = *(const json_t *const *)a;
    const json_t *nb = *(const json_t *const *)b;
    char *sa = json_serialize(na);
    char *sb = json_serialize(nb);
    int r = strcmp(sa ? sa : "", sb ? sb : "");
    free(sa);
    free(sb);
    return r;
}
static json_t *hrf_sorted_unique(json_t *items) {
    if (items->c.count > 1) {
        qsort(items->c.items, items->c.count, sizeof(json_t *), hrf_node_cmp);
        size_t w = 1;
        for (size_t i = 1; i < items->c.count; i++) {
            char *sa = json_serialize(items->c.items[w - 1]);
            char *sb = json_serialize(items->c.items[i]);
            bool dup = sa && sb && strcmp(sa, sb) == 0;
            free(sa);
            free(sb);
            if (dup) json_free(items->c.items[i]);
            else items->c.items[w++] = items->c.items[i];
        }
        items->c.count = w;
    }
    return items;
}
static json_t *hrf_frozenset_sorted(const json_t *v) {
    json_t *items = json_array();
    if (v->type == JSON_ARRAY) {
        for (size_t i = 0; i < v->c.count; i++) {
            const json_t *el = v->c.items[i];
            if (el && (el->type == JSON_ARRAY || el->type == JSON_OBJECT)) {
                json_free(items);
                char buf[96];
                snprintf(buf, sizeof buf, "cannot use '%s' as a set element (unhashable type: '%s')",
                         hrf_type_name(el), hrf_type_name(el));
                return hrf_err("TypeError", buf);
            }
            json_append(items, json_copy(el));
        }
    } else if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        size_t i = 0;
        while (s[i]) {
            uint32_t cp;
            size_t l = hrf_utf8_decode(s, i, &cp);
            char *ch = malloc(l + 1);
            memcpy(ch, s + i, l);
            ch[l] = '\0';
            json_append(items, json_string(ch));
            free(ch);
            i += l;
        }
    } else {
        json_free(items);
        return hrf_not_iterable(v);
    }
    return hrf_sorted_unique(items);
}

/* ── dict(row) mapping for the due-revokes row projection ── */
static json_t *hrf_dict_row(const json_t *item);
/* iterate a container the way Python for-loops do (keys for dicts). */
static json_t *hrf_iter_items(const json_t *v) {
    json_t *out = json_array();
    if (v && v->type == JSON_ARRAY) {
        for (size_t i = 0; i < v->c.count; i++) json_append(out, json_copy(v->c.items[i]));
        return out;
    }
    if (v && v->type == JSON_OBJECT) {
        for (size_t i = 0; i < v->c.count; i++) json_append(out, json_string(v->c.keys[i]));
        return out;
    }
    if (v && v->type == JSON_STRING) {
        const char *s = v->str_val;
        size_t i = 0;
        while (s[i]) {
            uint32_t cp;
            size_t l = hrf_utf8_decode(s, i, &cp);
            char *ch = malloc(l + 1);
            memcpy(ch, s + i, l);
            ch[l] = '\0';
            json_append(out, json_string(ch));
            free(ch);
            i += l;
        }
        return out;
    }
    json_free(out);
    return hrf_not_iterable(v);
}
/* JSON object key coercion for non-string hashable pair keys (json.dumps). */
static char *hrf_key_string(const json_t *k) {
    if (k && k->type == JSON_STRING) return hrf_strdup(k->str_val);
    if (!k || k->type == JSON_NULL) return hrf_strdup("null");
    if (k->type == JSON_BOOL) return hrf_strdup(k->bool_val ? "true" : "false");
    if (k->type == JSON_NUMBER) return hrf_num_str(k->num_val);
    return NULL;
}
static json_t *hrf_merge_pairs(json_t *out, const json_t *seq) {
    /* seq is a list/str of pairs; build one dict from it. */
    json_t *items = hrf_iter_items(seq);
    if (hrf_is_raise(items)) return items;
    for (size_t i = 0; i < items->c.count; i++) {
        const json_t *pair = items->c.items[i];
        if (pair && pair->type == JSON_OBJECT) {
            for (size_t k = 0; k < pair->c.count; k++)
                json_set(out, pair->c.keys[k], json_copy(pair->c.items[k]));
            continue;
        }
        const json_t *chars = NULL;
        bool chars_owned = false;
        size_t len = 0;
        if (pair && pair->type == JSON_ARRAY) { chars = pair; len = pair->c.count; }
        else if (pair && pair->type == JSON_STRING) {
            chars = hrf_iter_items(pair);
            chars_owned = true;
            len = chars->c.count;
        } else {
            json_free(items);
            char buf[128];
            snprintf(buf, sizeof buf, "cannot convert dictionary update sequence element #%zu to a sequence", i);
            return hrf_err("TypeError", buf);
        }
        if (len != 2) {
            if (chars_owned) json_free((json_t *)chars);
            json_free(items);
            char buf[128];
            snprintf(buf, sizeof buf, "dictionary update sequence element #%zu has length %zu; 2 is required", i, len);
            return hrf_err("ValueError", buf);
        }
        const json_t *key = json_get(chars, 0);
        const json_t *val = json_get(chars, 1);
        if (key && (key->type == JSON_ARRAY || key->type == JSON_OBJECT)) {
            if (chars_owned) json_free((json_t *)chars);
            json_free(items);
            char buf[96];
            snprintf(buf, sizeof buf, "cannot use '%s' as a dict key (unhashable type: '%s')",
                     hrf_type_name(key), hrf_type_name(key));
            return hrf_err("TypeError", buf);
        }
        char *ks = hrf_key_string(key);
        json_set(out, ks, json_copy(val));
        free(ks);
        if (chars_owned) json_free((json_t *)chars);
    }
    json_free(items);
    return NULL;
}
static json_t *hrf_dict_row(const json_t *item) {
    if (item && item->type == JSON_OBJECT) return json_copy(item);
    if (item && (item->type == JSON_ARRAY || item->type == JSON_STRING)) {
        json_t *out = json_object();
        json_t *e = hrf_merge_pairs(out, item);
        if (e) { json_free(out); return e; }
        return out;
    }
    return hrf_not_iterable(item);
}

/* ── create_room: validation chain THEN the store verb ── */
/* PoP: hrf_create_room @ gateway/hosted_rooms.py:create_room */
static json_t *hrf_create_room(hrf_ctx_t *c, const json_t *args) {
    json_t *err = NULL;
    char *rid = hrf_room_id(json_obj_get(args, "room_id"), &err);
    if (err) return err;
    char *name = hrf_room_name(json_obj_get(args, "name"), &err);
    if (err) { free(rid); return err; }
    json_t *members = hrf_validate_members(json_obj_get(args, "members"));
    if (hrf_is_raise(members)) { free(rid); free(name); return members; }
    char *agid = hrf_actor_id(json_obj_get(args, "authority_gateway_id"), "authority_gateway_id", &err);
    if (err) { free(rid); free(name); json_free(members); return err; }
    json_t *now = hrf_now(c, json_obj_get(args, "now"));
    if (hrf_is_raise(now)) { free(rid); free(name); json_free(members); free(agid); return now; }
    json_t *a = json_object();
    hrf_arg(a, "placement", json_obj_get(args, "placement"));
    json_set(a, "room_id", json_string(rid));
    json_set(a, "name", json_string(name));
    json_set(a, "members", members);
    json_set(a, "authority_gateway_id", json_string(agid));
    hrf_arg(a, "owner", json_obj_get(args, "owner"));
    json_set(a, "now", now);
    free(rid); free(name); free(agid);
    return hrf_call(c, "hosted_rooms.create_room", a);
}

/* ── list_rooms: bounded page fence THEN the store verb ── */
/* PoP: hrf_list_rooms @ gateway/hosted_rooms.py:list_rooms */
static json_t *hrf_list_rooms(hrf_ctx_t *c, const json_t *args) {
    const json_t *ln = json_obj_get(args, "limit");
    json_t *ldef = ln ? NULL : json_number(500); /* live default MAX_ROOM_LIST_LIMIT */
    long long limit = 0;
    json_t *e = hrf_bounded_int(ln ? ln : ldef, "limit must be between 1 and 500", 1, 500, true, &limit);
    json_free(ldef);
    HRF_CHK(e);
    const json_t *on = json_obj_get(args, "offset");
    json_t *odef = on ? NULL : json_number(0);
    long long offset = 0;
    e = hrf_bounded_int(on ? on : odef, "offset must be a non-negative integer", 0, 0, false, &offset);
    json_free(odef);
    HRF_CHK(e);
    const json_t *inc = json_obj_get(args, "include_disbanded");
    json_t *a = json_object();
    hrf_arg(a, "placement", json_obj_get(args, "placement"));
    json_set(a, "include_disbanded", inc ? json_copy(inc) : json_bool(false));
    json_set(a, "limit", json_number((double)limit));
    json_set(a, "offset", json_number((double)offset));
    return hrf_call(c, "hosted_rooms.list_rooms", a);
}

/* ── room_has_peer_obligations ── */
/* PoP: hrf_room_has_peer_obligations @ gateway/hosted_rooms.py:room_has_peer_obligations */
static json_t *hrf_room_has_peer_obligations(hrf_ctx_t *c, const json_t *args) {
    json_t *err = NULL;
    char *rid = hrf_room_id(json_obj_get(args, "room_id"), &err);
    if (err) return err;
    json_t *now = hrf_now(c, json_obj_get(args, "now"));
    if (hrf_is_raise(now)) { free(rid); return now; }
    json_t *a = json_object();
    hrf_arg(a, "placement", json_obj_get(args, "placement"));
    json_set(a, "room_id", json_string(rid));
    json_set(a, "now", now);
    free(rid);
    return hrf_call(c, "hosted_room_peers.room_has_peer_obligations", a);
}

/* ── retire_room_link_records: clock fires BEFORE the frozenset coercion ── */
/* PoP: hrf_retire_room_link_records @ gateway/hosted_rooms.py:retire_room_link_records */
static json_t *hrf_retire_room_link_records(hrf_ctx_t *c, const json_t *args) {
    json_t *now = hrf_now(c, json_obj_get(args, "now"));
    HRF_CHK(now);
    const json_t *rd = json_obj_get(args, "revoked_digests");
    json_t *rds = NULL;
    if (!hrf_is_null(rd)) {
        rds = hrf_frozenset_sorted(rd);
        if (hrf_is_raise(rds)) { json_free(now); return rds; }
    }
    const json_t *owe = json_obj_get(args, "owe_routes");
    json_t *a = json_object();
    hrf_arg(a, "placement", json_obj_get(args, "placement"));
    hrf_arg(a, "room_id", json_obj_get(args, "room_id"));
    json_set(a, "owe_routes", owe ? json_copy(owe) : json_bool(false));
    json_set(a, "now", now);
    hrf_arg_owned(a, "revoked_digests", rds);
    return hrf_call(c, "hosted_room_peers.retire_room_link_records", a);
}

/* ── due_room_grant_revokes: int(limit), clock, verb, dict(row) mapping ── */
/* PoP: hrf_due_room_grant_revokes @ gateway/hosted_rooms.py:due_room_grant_revokes */
static json_t *hrf_due_room_grant_revokes(hrf_ctx_t *c, const json_t *args) {
    long long limit = 0;
    json_t *e = hrf_py_int(json_obj_get(args, "limit"), &limit);
    HRF_CHK(e);
    json_t *now = hrf_now(c, json_obj_get(args, "now"));
    HRF_CHK(now);
    json_t *a = json_object();
    hrf_arg(a, "placement", json_obj_get(args, "placement"));
    json_set(a, "limit", json_number((double)limit));
    json_set(a, "now", now);
    json_t *res = hrf_call(c, "hosted_room_peers.due_room_grant_revokes", a);
    HRF_CHK(res);
    json_t *items = hrf_iter_items(res);
    json_free(res);
    HRF_CHK(items);
    json_t *out = json_array();
    for (size_t i = 0; i < items->c.count; i++) {
        json_t *row = hrf_dict_row(items->c.items[i]);
        if (hrf_is_raise(row)) { json_free(out); json_free(items); return row; }
        json_append(out, row);
    }
    json_free(items);
    return out;
}

/* ── settle_room_grant_revoke: retry_at None stays None, else float() ── */
/* PoP: hrf_settle_room_grant_revoke @ gateway/hosted_rooms.py:settle_room_grant_revoke */
static json_t *hrf_settle_room_grant_revoke(hrf_ctx_t *c, const json_t *args) {
    const json_t *retry = json_obj_get(args, "retry_at");
    json_t *retry_out = NULL;
    if (!hrf_is_null(retry)) {
        double t = 0.0;
        json_t *e = hrf_py_float(retry, &t);
        HRF_CHK(e);
        retry_out = json_number(t);
    }
    json_t *a = json_object();
    hrf_arg(a, "placement", json_obj_get(args, "placement"));
    hrf_arg(a, "room_id", json_obj_get(args, "room_id"));
    hrf_arg(a, "member_id", json_obj_get(args, "member_id"));
    hrf_arg(a, "grant_digest", json_obj_get(args, "grant_digest"));
    hrf_arg_owned(a, "retry_at", retry_out);
    return hrf_call(c, "hosted_room_peers.settle_room_grant_revoke", a);
}

/* ── drop_remote_run_receipt: _typed identity fence THEN the verb ── */
/* PoP: hrf_drop_remote_run_receipt @ gateway/hosted_rooms.py:drop_remote_run_receipt */
static json_t *hrf_drop_remote_run_receipt(hrf_ctx_t *c, const json_t *args) {
    json_t *identity = hrf_typed_impl("RemoteRunIdentity", json_obj_get(args, "record"));
    HRF_CHK(identity);
    json_t *a = json_object();
    hrf_arg(a, "placement", json_obj_get(args, "placement"));
    json_set(a, "identity", identity);
    hrf_arg(a, "run_id", json_obj_get(args, "run_id"));
    return hrf_call(c, "hosted_room_peers.drop_remote_run_receipt", a);
}

/* ── pure leaf entry points ── */
/* PoP: hrf_typed @ gateway/hosted_rooms.py:_typed */
static json_t *hrf_typed(hrf_ctx_t *c, const json_t *args) {
    (void)c;
    return hrf_typed_impl(json_get_str(args, "type", ""), json_obj_get(args, "record"));
}
/* PoP: hrf_grant_claims @ gateway/hosted_rooms.py:_grant_claims */
static json_t *hrf_grant_claims(hrf_ctx_t *c, const json_t *args) {
    (void)c;
    return hrf_grant_claims_impl(json_obj_get(args, "claims"));
}
/* PoP: hrf_reservation_claims @ gateway/hosted_rooms.py:_reservation_claims */
static json_t *hrf_reservation_claims(hrf_ctx_t *c, const json_t *args) {
    (void)c;
    return hrf_reservation_claims_impl(json_obj_get(args, "claims"));
}
/* PoP: hrf_reservation_grant @ gateway/hosted_rooms.py:_reservation_grant */
static json_t *hrf_reservation_grant(hrf_ctx_t *c, const json_t *args) {
    (void)c;
    return hrf_reservation_grant_impl(json_obj_get(args, "claims"));
}

/* ── default_db_path: profile-gateway shared-store resolution ── */
/* PoP: hrf_default_db_path @ gateway/hosted_rooms.py:default_db_path */
static json_t *hrf_default_db_path(hrf_ctx_t *c, const json_t *args) {
    const json_t *home = json_obj_get(args, "home");
    json_t *home_str = NULL;
    if (hrf_is_null(home)) {
        home_str = hrf_call(c, "get_hermes_home", json_object());
        HRF_CHK(home_str);
    } else {
        home_str = json_copy(home);
    }
    if (!home_str || home_str->type != JSON_STRING) {
        char buf[192];
        snprintf(buf, sizeof buf,
                 "argument should be a str or an os.PathLike object where __fspath__ returns a str, "
                 "not '%s'", hrf_type_name(home_str));
        json_free(home_str);
        return hrf_err("TypeError", buf);
    }
    char *p = hrf_path_normalize(home_str->str_val);
    json_free(home_str);
    char *par = hrf_path_parent(p);
    char *base;
    if (strcmp(hrf_path_name(par), "profiles") == 0) base = hrf_path_parent(par);
    else base = hrf_strdup(p);
    char *result = hrf_path_join(base, "shared-state.db");
    free(p); free(par); free(base);
    json_t *out = json_string(result);
    free(result);
    return out;
}
