/*
 * hosted_rooms_common.c — faithful C11 port of the pure validation leaves in
 * gateway/hosted_rooms_common.py (gate w14_hostedroom_818b6c).
 * PoP: w14_hostedroom_818b6c_identifier @ gateway/hosted_rooms_common.py:identifier
 * PoP: w14_hostedroom_818b6c_bounded_int @ gateway/hosted_rooms_common.py:bounded_int
 * PoP: w14_hostedroom_818b6c_exact_fields @ gateway/hosted_rooms_common.py:exact_fields
 * PoP: w14_hostedroom_818b6c_text @ gateway/hosted_rooms_common.py:text
 * PoP: w14_hostedroom_818b6c_utf8_len @ gateway/hosted_rooms_common.py:utf8_len
 */
#include "noc_w14_hostedroom_818b6c.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python `error(...)` is surfaced as {"ok":false,"error":<msg>}; success as
 * {"ok":true,"value":<v>,"error":null}. */
static json_t *res_ok(json_t *val) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "value", val ? val : json_null());
    json_set(o, "error", json_null());
    return o;
}
static json_t *res_err(const char *msg) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "value", json_null());
    json_set(o, "error", json_string(msg));
    return o;
}

/* POSIX dupstr (never name a helper `dup`: collision). */
static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "");
    char *p = (char *)malloc(n + 1);
    memcpy(p, s ? s : "", n);
    p[n] = '\0';
    return p;
}

/* ASCII whitespace strip, mirroring str.strip() for the ASCII fixtures.
 * Always returns a fresh buffer; caller frees. */
static char *strip_copy(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    size_t a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* IDENTIFIER_RE = ^[A-Za-z0-9][A-Za-z0-9._:-]*$ (fullmatch). */
static int identifier_shape_ok(const char *s) {
    if (!(s[0] && isalnum((unsigned char)s[0]))) return 0;
    for (const char *p = s + 1; *p; ++p) {
        if (!(isalnum((unsigned char)*p) || *p == '.' || *p == '_' || *p == ':' || *p == '-'))
            return 0;
    }
    return 1;
}

json_t *w14_hostedroom_818b6c_identifier(json_t *value, const char *label,
                                         int max_chars, int skip_pattern,
                                         const char *invalid) {
    char msg[512];
    if (!value || value->type != JSON_STRING) {
        snprintf(msg, sizeof(msg), "%s must be a string", label ? label : "");
        return res_err(msg);
    }
    /* Copy out before any mutation; strip_copy already makes its own buffer. */
    char *v = strip_copy(value->str_val ? value->str_val : "");
    size_t len = strlen(v);
    int bad = (!*v || (int)len > max_chars || (!skip_pattern && !identifier_shape_ok(v)));
    json_t *ret;
    if (bad) {
        if (invalid && *invalid) snprintf(msg, sizeof(msg), "%s", invalid);
        else snprintf(msg, sizeof(msg), "invalid %s", label ? label : "");
        ret = res_err(msg);
    } else {
        ret = res_ok(json_string(v));
    }
    free(v);
    return ret;
}

json_t *w14_hostedroom_818b6c_bounded_int(json_t *value, const char *message,
                                         long low, int has_high, long high) {
    int bad = 0;
    long v = 0;
    if (!value || value->type != JSON_NUMBER) {
        bad = 1;
    } else {
        double d = value->num_val;
        if (d != (double)(long long)d) bad = 1; /* floats are not ints */
        else v = (long)d;
    }
    if (!bad && (v < low || (has_high && v > high))) bad = 1;
    if (bad) return res_err(message ? message : "");
    return res_ok(json_number((double)v));
}

/* Replace {label} and {fields} in a format template. */
static void fmt_msg(const char *tmpl, const char *label, const char *fields,
                    char *out, size_t cap) {
    size_t n = 0;
    if (!tmpl) tmpl = "";
    for (const char *p = tmpl; *p && n + 1 < cap;) {
        if (!strncmp(p, "{label}", 7)) {
            n += (size_t)snprintf(out + n, cap - n, "%s", label ? label : "");
            p += 7;
        } else if (!strncmp(p, "{fields}", 8)) {
            n += (size_t)snprintf(out + n, cap - n, "%s", fields ? fields : "");
            p += 8;
        } else {
            out[n++] = *p++;
        }
    }
    out[n] = '\0';
}

static int json_str_in(json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); ++i) {
        json_t *it = json_get(arr, i);
        if (it && it->type == JSON_STRING && it->str_val && !strcmp(it->str_val, s)) return 1;
    }
    return 0;
}

/* Sorted ", "-joined set difference of object keys vs a string-array whitelist.
 * whitelist_minus_keys=1 -> whitelist - keys ; 0 -> keys - whitelist. */
static char *set_diff_joined(json_t *obj, json_t *whitelist, int whitelist_minus_keys) {
    char keys[256][256];
    size_t nk = 0;
    if (obj && obj->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(obj) && nk < 256; ++i) {
            const char *k = json_object_get_key_at(obj, i);
            if (!k) continue;
            size_t j = 0;
            for (; j < nk; ++j) if (!strcmp(keys[j], k)) break;
            if (j == nk) { snprintf(keys[nk], sizeof(keys[j]), "%s", k); nk++; }
        }
    }
    const char *items[512];
    size_t ni = 0;
    if (whitelist_minus_keys) {
        if (whitelist && whitelist->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(whitelist) && ni < 512; ++i) {
                json_t *it = json_get(whitelist, i);
                if (!it || it->type != JSON_STRING) continue;
                int present = 0;
                for (size_t j = 0; j < nk; ++j) if (!strcmp(keys[j], it->str_val)) { present = 1; break; }
                if (!present) items[ni++] = it->str_val;
            }
        }
    } else {
        for (size_t j = 0; j < nk && ni < 512; ++j) {
            if (!json_str_in(whitelist, keys[j])) items[ni++] = keys[j];
        }
    }
    for (size_t i = 0; i < ni; ++i)
        for (size_t j = i + 1; j < ni; ++j)
            if (strcmp(items[i], items[j]) > 0) {
                const char *t = items[i]; items[i] = items[j]; items[j] = t;
            }
    size_t cap = 1;
    for (size_t i = 0; i < ni; ++i) cap += strlen(items[i]) + 2;
    char *out = (char *)malloc(cap);
    size_t n = 0;
    out[0] = '\0';
    for (size_t i = 0; i < ni; ++i) {
        if (n) { out[n++] = ','; out[n++] = ' '; }
        n += (size_t)snprintf(out + n, cap - n, "%s", items[i]);
    }
    out[n] = '\0';
    return out;
}

json_t *w14_hostedroom_818b6c_exact_fields(json_t *value, const char *label,
                                          json_t *required, json_t *optional,
                                          const char *not_object,
                                          const char *missing_fmt,
                                          const char *unknown_fmt) {
    if (!value || value->type != JSON_OBJECT) {
        char msg[512];
        if (not_object && *not_object) snprintf(msg, sizeof(msg), "%s", not_object);
        else snprintf(msg, sizeof(msg), "%s must be an object", label ? label : "");
        return res_err(msg);
    }
    json_t *white = json_array();
    if (required && required->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(required); ++i) json_append(white, json_copy(json_get(required, i)));
    if (optional && optional->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(optional); ++i) json_append(white, json_copy(json_get(optional, i)));

    char *missing = set_diff_joined(value, required, 1);
    if (missing && *missing) {
        char msg[1024];
        fmt_msg(missing_fmt, label, missing, msg, sizeof(msg));
        free(missing); json_free(white);
        return res_err(msg);
    }
    char *unknown = set_diff_joined(value, white, 0);
    if (unknown && *unknown) {
        char msg[1024];
        fmt_msg(unknown_fmt, label, unknown, msg, sizeof(msg));
        free(unknown); json_free(white);
        return res_err(msg);
    }
    free(missing); free(unknown); json_free(white);
    /* exact_fields returns the validated Mapping itself on success. */
    return res_ok(json_copy(value));
}

json_t *w14_hostedroom_818b6c_text(json_t *value, const char *label,
                                  int max_bytes, int strip) {
    char msg[512];
    if (!value || value->type != JSON_STRING) {
        snprintf(msg, sizeof(msg), "%s must be a string", label ? label : "");
        return res_err(msg);
    }
    const char *src = value->str_val ? value->str_val : "";
    char *blank = strip_copy(src);
    if (!*blank) {
        free(blank);
        snprintf(msg, sizeof(msg), "%s must not be empty", label ? label : "");
        return res_err(msg);
    }
    free(blank);
    char *v = strip ? strip_copy(src) : dupstr(src);
    if ((int)strlen(v) > max_bytes) {
        free(v);
        snprintf(msg, sizeof(msg), "%s is too large", label ? label : "");
        return res_err(msg);
    }
    json_t *ret = res_ok(json_string(v));
    free(v);
    return ret;
}

json_t *w14_hostedroom_818b6c_utf8_len(json_t *parts) {
    size_t total = 0;
    if (parts && parts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(parts); ++i) {
            json_t *it = json_get(parts, i);
            if (it && it->type == JSON_STRING && it->str_val) total += strlen(it->str_val);
        }
    }
    return res_ok(json_number((double)total));
}
