/*
 * refs.c -- real C11 port of agent/checkpoints/refs.py.
 *
 * PoP: w22_refs_0a533f_resolve @ agent/checkpoints/refs.py:resolve_checkpoint_ref
 * PoP: w22_refs_0a533f_min_hash @ agent/checkpoints/refs.py:_MIN_HASH
 *
 * `resolve_checkpoint_ref` is fully pure: it never touches the store, the
 * filesystem or the clock, so the whole function is ported (no seam).
 */
#include "hermes_json.h"
#include "noc_w22_refs_0a533f.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python str.strip() with no argument: ASCII whitespace on both ends. */
static int refs_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *refs_strip(const char *s) {
    size_t n, a = 0;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    while (a < n && refs_ws((unsigned char)s[a])) a++;
    while (n > a && refs_ws((unsigned char)s[n - 1])) n--;
    out = (char *)malloc(n - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, n - a);
    out[n - a] = '\0';
    return out;
}

/* str.isdigit(): every character is a decimal digit; "" is not. */
static int refs_all_digits(const char *s) {
    size_t i;
    if (!s || !s[0]) return 0;
    for (i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
    }
    return 1;
}

/* int(s) for a pure-digit string; reports overflow instead of wrapping. */
static int refs_parse_digits(const char *s, unsigned long long *out) {
    unsigned long long v = 0;
    size_t i;
    if (!out) return 0;
    *out = 0;
    if (!refs_all_digits(s)) return 0;
    for (i = 0; s[i]; i++) {
        unsigned d = (unsigned)(s[i] - '0');
        if (v > (18446744073709551615ULL - d) / 10ULL) return 0; /* int() has no such limit */
        v = v * 10ULL + d;
    }
    *out = v;
    return 1;
}

static json_t *refs_ok(json_t *value) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(1));
    json_set(out, "value", value ? value : json_null());
    return out;
}

static json_t *refs_fail(const char *message, const char *type) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(0));
    json_set(out, "error", json_string(message ? message : ""));
    json_set(out, "type", json_string(type ? type : "ValueError"));
    return out;
}

/* Python truthiness of a decoded JSON value; absent (NULL) reads as None. */
static int refs_truthy(const json_t *v) {
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

/* type(x).__name__ for the shapes a JSON request can carry. */
static const char *refs_py_type(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: {
        double d = v->num_val;
        return (d == (double)(long long)d) ? "int" : "float";
    }
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

json_t *w22_refs_0a533f_resolve(const json_t *checkpoints, const json_t *ref) {
    char *text;
    size_t count = 0;
    unsigned long long number = 0;
    int digits, parsed;
    json_t *out;

    /* `ref = (ref or "").strip()` -- truthiness, then str, then strip. */
    if (!refs_truthy(ref)) {
        text = refs_strip("");
    } else if (ref->type != JSON_STRING) {
        char message[96];
        snprintf(message, sizeof(message), "'%s' object has no attribute 'strip'",
                 refs_py_type(ref));
        return refs_fail(message, "AttributeError");
    } else {
        text = refs_strip(ref->str_val);
    }
    if (!text) return refs_fail("", "ValueError");
    count = (checkpoints && checkpoints->type == JSON_ARRAY) ? checkpoints->c.count : 0;
    digits = refs_all_digits(text);
    parsed = digits ? refs_parse_digits(text, &number) : 0;

    /* `if ref.isdigit() and not ref.startswith("0")` */
    if (digits && text[0] != '0' && parsed && number >= 1ULL &&
        number <= (unsigned long long)count) {
        json_t *entry = checkpoints->c.items[number - 1];
        json_t *hash = (entry && entry->type == JSON_OBJECT) ? json_obj_get(entry, "hash") : NULL;
        out = refs_ok(hash ? json_copy(hash) : json_string(text));
        free(text);
        return out;
    }
    /* `if ref.isdigit() and len(ref) < _MIN_HASH: raise ValueError(...)` */
    if (digits && strlen(text) < (size_t)W22_REFS_0A533F_MIN_HASH) {
        char message[64];
        snprintf(message, sizeof(message),
                 "Invalid checkpoint number. Use 1-%llu.", (unsigned long long)count);
        out = refs_fail(message, "ValueError");
        free(text);
        return out;
    }
    out = refs_ok(json_string(text));
    free(text);
    return out;
}
