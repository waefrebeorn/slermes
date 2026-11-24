/*
 * adapter_reactions_context.c -- real C11 port of the emoji-reaction marker
 * upstream Hermes renders into Slack thread context
 *   plugins/platforms/slack/adapter_reactions_context.py.
 *
 * PoP: w21_adapterrea_e0609e_slack_reactions_marker @ plugins/platforms/slack/adapter_reactions_context.py:slack_reactions_marker
 * PoP: w21_adapterrea_e0609e_sanitize_name @ plugins/platforms/slack/adapter_reactions_context.py:_UNSAFE_NAME_CHARS
 * PoP: w21_adapterrea_e0609e_marker_max @ plugins/platforms/slack/adapter_reactions_context.py:REACTION_MARKER_MAX
 *
 * str(value or "") is Python's coercion (containers use their repr); the
 * _UNSAFE_NAME_CHARS substitution keeps [A-Za-z0-9_+'-:], every byte >= 0x80
 * (Python's \w is unicode-aware, so a UTF-8 emoji name must survive) and drops
 * the rest, then .strip(":") trims colons from both ends only.
 */
#include "noc_w21_adapterrea_e0609e.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W21_ADAPTERREA_E0609E_MARKER_MAX 8

/* --- string builder ------------------------------------------------------- */

typedef struct {
    char *p;
    size_t n;
    size_t cap;
} ar_sb;

static int ar_sb_grow(ar_sb *b, size_t extra) {
    size_t want = b->n + extra + 1;
    char *np;
    if (want <= b->cap) return 1;
    if (want < 32) want = 32;
    np = (char *)realloc(b->p, want);
    if (!np) return 0;
    b->p = np;
    b->cap = want;
    return 1;
}

static int ar_sb_raw(ar_sb *b, const char *s, size_t len) {
    if (!ar_sb_grow(b, len)) return 0;
    if (len) memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
    return 1;
}

static int ar_sb_puts(ar_sb *b, const char *s) {
    return ar_sb_raw(b, s ? s : "", s ? strlen(s) : 0);
}

static int ar_sb_putc(ar_sb *b, char c) {
    return ar_sb_raw(b, &c, 1);
}

static char *ar_sb_take(ar_sb *b) {
    char *p;
    if (!ar_sb_grow(b, 0)) {
        free(b->p);
        b->p = NULL;
        b->n = b->cap = 0;
        return NULL;
    }
    p = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return p;
}

static char *ar_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int ar_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* `isinstance(x, int)`: bools ARE ints in Python; integral floats are not
 * (the wire cannot tell 2 from 2.0, so the gate convention is that integral
 * JSON numbers are ints -- fixtures never write "2.0"). */
static int ar_py_int_value(const json_t *v, long long *out) {
    double d;
    if (!v) return 0;
    if (v->type == JSON_BOOL) {
        *out = v->bool_val ? 1 : 0;
        return 1;
    }
    if (v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    *out = (long long)d;
    return 1;
}

static const char *ar_py_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return "float";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* str(float): Python uses the shortest round-tripping repr. */
static void ar_fmt_float(double d, char *buf, size_t n) {
    int prec;
    for (prec = 15; prec <= 17; prec++) {
        snprintf(buf, n, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) return;
    }
}

/* repr(str): single quotes, backslash and quote escaped, like CPython. */
static int ar_repr_str(ar_sb *b, const char *s) {
    const char *p;
    if (!ar_sb_putc(b, '\'')) return 0;
    for (p = s; *p; p++) {
        if (*p == '\\' || *p == '\'') {
            if (!ar_sb_putc(b, '\\') || !ar_sb_putc(b, *p)) return 0;
        } else if (*p == '\n') {
            if (!ar_sb_puts(b, "\\n")) return 0;
        } else if (*p == '\t') {
            if (!ar_sb_puts(b, "\\t")) return 0;
        } else if (*p == '\r') {
            if (!ar_sb_puts(b, "\\r")) return 0;
        } else if (!ar_sb_putc(b, *p)) {
            return 0;
        }
    }
    return ar_sb_putc(b, '\'');
}

static int ar_repr(ar_sb *b, const json_t *v);

/* str() of a container is its repr(); scalars use their plain forms. */
static int ar_emit_str(ar_sb *b, const json_t *v) {
    char num[48];
    if (!v) return ar_sb_puts(b, "None");
    switch (v->type) {
    case JSON_NULL:
        return ar_sb_puts(b, "None");
    case JSON_BOOL:
        return ar_sb_puts(b, v->bool_val ? "True" : "False");
    case JSON_STRING:
        return ar_sb_puts(b, v->str_val ? v->str_val : "");
    case JSON_NUMBER: {
        long long i;
        if (ar_py_int_value(v, &i)) {
            snprintf(num, sizeof(num), "%lld", i);
        } else {
            ar_fmt_float(v->num_val, num, sizeof(num));
        }
        return ar_sb_puts(b, num);
    }
    case JSON_ARRAY:
    case JSON_OBJECT:
        return ar_repr(b, v);
    }
    return ar_sb_puts(b, "None");
}

static int ar_repr(ar_sb *b, const json_t *v) {
    size_t i;
    if (!v) return ar_sb_puts(b, "None");
    switch (v->type) {
    case JSON_STRING:
        return ar_repr_str(b, v->str_val ? v->str_val : "");
    case JSON_ARRAY:
        if (!ar_sb_putc(b, '[')) return 0;
        for (i = 0; i < v->c.count; i++) {
            if (i && !ar_sb_puts(b, ", ")) return 0;
            if (!ar_repr(b, v->c.items[i])) return 0;
        }
        return ar_sb_putc(b, ']');
    case JSON_OBJECT:
        if (!ar_sb_putc(b, '{')) return 0;
        for (i = 0; i < v->c.count; i++) {
            const char *k = v->c.keys ? v->c.keys[i] : "";
            if (i && !ar_sb_puts(b, ", ")) return 0;
            if (!ar_repr_str(b, k ? k : "")) return 0;
            if (!ar_sb_puts(b, ": ")) return 0;
            if (!ar_repr(b, v->c.items[i])) return 0;
        }
        return ar_sb_putc(b, '}');
    default:
        return ar_emit_str(b, v);
    }
}

/* `str(value or "")` */
static char *ar_str_or_empty(const json_t *v) {
    ar_sb b = {NULL, 0, 0};
    char *out;
    if (!ar_truthy(v)) return ar_dup("");
    if (!ar_emit_str(&b, v)) {
        free(b.p);
        return NULL;
    }
    out = ar_sb_take(&b);
    return out ? out : ar_dup("");
}

/* --- _UNSAFE_NAME_CHARS = re.compile(r"[^\w+'\-:]+") ---------------------- */

static int ar_name_byte_kept(unsigned char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return 1;
    if (c == '_' || c == '+' || c == '\'' || c == '-' || c == ':') return 1;
    if (c >= 0x80) return 1;   /* Python \w is unicode-aware */
    return 0;
}

/* re.sub(...) then .strip(":") -- in place, NUL-terminated after shortening. */
static void ar_sanitize_inplace(char *s) {
    size_t r = 0, w = 0;
    if (!s) return;
    while (s[r]) {
        unsigned char c = (unsigned char)s[r];
        if (ar_name_byte_kept(c)) s[w++] = s[r];
        r++;
    }
    s[w] = '\0';
    while (w && s[w - 1] == ':') s[--w] = '\0';
    r = 0;
    while (s[r] == ':') r++;
    if (r) memmove(s, s + r, strlen(s + r) + 1);
}

json_t *w21_adapterrea_e0609e_sanitize_name(const json_t *value) {
    char *s = ar_str_or_empty(value);
    json_t *out;
    if (!s) return json_null();
    ar_sanitize_inplace(s);
    out = json_string(s);
    free(s);
    return out;
}

int w21_adapterrea_e0609e_marker_max(void) {
    return W21_ADAPTERREA_E0609E_MARKER_MAX;
}

json_t *w21_adapterrea_e0609e_more_label(int total_parts) {
    int extra = total_parts - W21_ADAPTERREA_E0609E_MARKER_MAX;
    char buf[48];
    if (extra <= 0) return json_string("");
    snprintf(buf, sizeof(buf), "+%d more", extra);
    return json_string(buf);
}

json_t *w21_adapterrea_e0609e_slack_reactions_marker(const json_t *msg) {
    const json_t *reactions;
    char **parts = NULL;
    size_t nparts = 0, caps = 0, i;
    json_t *out = NULL;
    ar_sb b = {NULL, 0, 0};

    if (!msg || msg->type != JSON_OBJECT) {
        char buf[256];
        json_t *err = json_object();
        snprintf(buf, sizeof(buf), "'%s' object has no attribute 'get'", ar_py_type_name(msg));
        json_set(err, "status", json_string("error"));
        json_set(err, "type", json_string("AttributeError"));
        json_set(err, "message", json_string(buf));
        return err;
    }

    reactions = json_obj_get(msg, "reactions");
    if (!reactions || reactions->type != JSON_ARRAY) return json_string("");

    for (i = 0; i < reactions->c.count; i++) {
        const json_t *reaction = reactions->c.items[i];
        const json_t *raw_name;
        long long count;
        char *name;
        ar_sb part = {NULL, 0, 0};
        char num[48];

        if (!reaction || reaction->type != JSON_OBJECT) continue;
        raw_name = json_obj_get(reaction, "name");
        name = ar_str_or_empty(raw_name);
        if (!name) continue;
        ar_sanitize_inplace(name);
        if (!name[0]) {
            free(name);
            continue;
        }
        if (!ar_py_int_value(json_obj_get(reaction, "count"), &count)) count = 0;

        ar_sb_putc(&part, ':');
        ar_sb_puts(&part, name);
        ar_sb_putc(&part, ':');
        if (count > 1) {
            ar_sb_puts(&part, "\xC3\x97");   /* U+00D7 MULTIPLICATION SIGN, UTF-8 */
            snprintf(num, sizeof(num), "%lld", count);
            ar_sb_puts(&part, num);
        }
        free(name);

        if (nparts == caps) {
            size_t ncap = caps ? caps * 2 : 8;
            char **np = (char **)realloc(parts, ncap * sizeof(*np));
            if (!np) {
                free(part.p);
                break;
            }
            parts = np;
            caps = ncap;
        }
        parts[nparts] = ar_sb_take(&part);
        if (!parts[nparts]) break;
        nparts++;
    }

    if (nparts == 0) {
        free(parts);
        return json_string("");
    }

    ar_sb_puts(&b, "[reactions: ");
    for (i = 0; i < nparts && i < (size_t)W21_ADAPTERREA_E0609E_MARKER_MAX; i++) {
        if (i) ar_sb_putc(&b, ' ');
        ar_sb_puts(&b, parts[i] ? parts[i] : "");
    }
    if (nparts > (size_t)W21_ADAPTERREA_E0609E_MARKER_MAX) {
        char num[48];
        snprintf(num, sizeof(num), "+%llu more",
                 (unsigned long long)(nparts - W21_ADAPTERREA_E0609E_MARKER_MAX));
        ar_sb_putc(&b, ' ');
        ar_sb_puts(&b, num);
    }
    ar_sb_putc(&b, ']');

    for (i = 0; i < nparts; i++) free(parts[i]);
    free(parts);
    out = json_string(b.p ? b.p : "");
    free(b.p);
    return out;
}


