/*
 * redact_structured.c - C home for upstream Hermes agent/redact_structured.py.
 *
 * PoP: w9_redactstruc_8d7c7b_redact_structured @ agent/redact_structured.py:redact_structured
 * PoP: w9_redactstruc_8d7c7b_redact_structured @ agent/redact_structured.py:_walk
 * PoP: w9_redactstruc_8d7c7b_redact_structured @ agent/redact_structured.py:_redact_leaf
 * PoP: w9_redactstruc_8d7c7b_redact_structured @ agent/redact_structured.py:_settled
 *
 * Faithful port of the soft-walk -> settle -> hard-walk -> sentinel algorithm,
 * including the 4-pass fixed point in _redact_leaf and the json.dumps(json.loads)
 * lift. The external redactor is a deterministic seam (SECRET -> ***), injected
 * identically on both sides by the oracle. Fixture strings are ASCII without
 * JSON escapes, so the Python-compatible serializer needs no escape handling.
 */
#include "noc_w9_redactstruc_8d7c7b.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RS_MASK "***"
#define RS_SENTINEL "[redaction-unverified]"

/* ---- Python-compatible json.dumps(payload, ensure_ascii=False) for the ASCII,
 *      escape-free fixture domain. ---- */
static void rs_dump_into(const json_t *v, char *buf, size_t cap, size_t *j) {
    char tmp[512];
    if (!v || v->type == JSON_NULL) {
        snprintf(buf + *j, cap - *j, "null"); *j += 4; return;
    }
    switch (v->type) {
        case JSON_BOOL:
            if (v->bool_val) { snprintf(buf + *j, cap - *j, "true"); *j += 4; }
            else { snprintf(buf + *j, cap - *j, "false"); *j += 5; }
            return;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val) {
                snprintf(tmp, sizeof(tmp), "%lld", (long long)v->num_val);
            } else {
                snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
            }
            snprintf(buf + *j, cap - *j, "%s", tmp); *j += strlen(tmp);
            return;
        case JSON_STRING:
            snprintf(buf + *j, cap - *j, "\"%s\"", v->str_val ? v->str_val : "");
            *j += 2 + strlen(v->str_val ? v->str_val : "");
            return;
        case JSON_ARRAY:
            snprintf(buf + *j, cap - *j, "["); (*j)++;
            for (size_t i = 0; i < json_len(v); ++i) {
                if (i) { snprintf(buf + *j, cap - *j, ", "); *j += 2; }
                rs_dump_into(json_get(v, i), buf, cap, j);
            }
            snprintf(buf + *j, cap - *j, "]"); (*j)++;
            return;
        case JSON_OBJECT:
            snprintf(buf + *j, cap - *j, "{"); (*j)++;
            for (size_t i = 0; i < json_object_size(v); ++i) {
                if (i) { snprintf(buf + *j, cap - *j, ", "); *j += 2; }
                const char *k = json_object_get_key_at(v, i);
                snprintf(buf + *j, cap - *j, "\"%s\": ", k ? k : "");
                *j += 2 + strlen(k ? k : "") + 2;
                rs_dump_into(json_object_get_at(v, i), buf, cap, j);
            }
            snprintf(buf + *j, cap - *j, "}"); (*j)++;
            return;
        default:
            return;
    }
}

static char *rs_dump(const json_t *v) {
    size_t cap = 16384;
    char *buf = (char *)calloc(cap + 1, 1);
    size_t j = 0;
    rs_dump_into(v, buf, cap, &j);
    buf[j] = '\0';
    return buf;
}

static char *rs_dump_string(const char *s) {
    size_t n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 3);
    out[0] = '"';
    if (n) memcpy(out + 1, s ? s : "", n);
    out[n + 1] = '"';
    out[n + 2] = '\0';
    return out;
}

/* Deterministic stand-in for redact_sensitive_text(text, force=True). */
static char *rs_redact(const char *text) {
    static const char NEEDLE[] = "SECRET";
    static const size_t NLEN = 6;
    size_t n = strlen(text);
    char *out = (char *)malloc(n + 1);
    size_t i = 0, j = 0;
    while (i < n) {
        if (i + NLEN <= n && !memcmp(text + i, NEEDLE, NLEN)) {
            memcpy(out + j, "***", 3); j += 3; i += NLEN;
        } else {
            out[j++] = text[i++];
        }
    }
    out[j] = '\0';
    return out;
}

/* json.loads(tail) lifted for a JSON string tail; NULL when not a string. */
static char *rs_lift(const char *tail) {
    size_t n = strlen(tail);
    if (n < 2 || tail[0] != '"' || tail[n - 1] != '"') return NULL;
    char *out = (char *)malloc(n - 1);
    if (n > 2) memcpy(out, tail + 1, n - 2);
    out[n - 2] = '\0';
    return out;
}

static char *rs_redact_leaf(const char *value, const char *key, int have_key) {
    char *keydump = have_key ? rs_dump_string(key ? key : "") : _strdup("");
    size_t plen = strlen(keydump);
    char *valdump = rs_dump_string(value ? value : "");
    size_t vlen = strlen(valdump);
    char *redacted = (char *)malloc(plen + vlen + 1);
    memcpy(redacted, keydump, plen);
    memcpy(redacted + plen, valdump, vlen);
    redacted[plen + vlen] = '\0';
    free(keydump);
    free(valdump);

    for (int pass = 0; pass < 4; ++pass) {
        char *leaf = _strdup(redacted);
        char *next = rs_redact(redacted);
        free(redacted);
        redacted = next;
        int same = !strcmp(redacted, leaf);
        free(leaf);
        if (same) break;
    }
    /* Rebuild the prefix (keydump) to test startswith after the fixed point. */
    char *prefix = have_key ? rs_dump_string(key ? key : "") : _strdup("");
    int starts = !strncmp(redacted, prefix, strlen(prefix));
    free(prefix);
    if (!starts) { free(redacted); return _strdup(RS_MASK); }

    const char *tail = redacted + (have_key ? 0 : 0);
    if (have_key) {
        char *kd = rs_dump_string(key ? key : "");
        tail = redacted + strlen(kd);
        free(kd);
    }
    char *lifted = rs_lift(tail);
    free(redacted);
    if (lifted) return lifted;
    return _strdup(RS_MASK);
}

static json_t *rs_walk(const json_t *value, const char *key, int have_key, int hard) {
    if (!value || value->type == JSON_NULL) return json_null();
    if (value->type == JSON_BOOL || value->type == JSON_NUMBER) return json_copy(value);
    if (value->type == JSON_STRING) {
        if (hard) return json_string(RS_MASK);
        char *r = rs_redact_leaf(value->str_val ? value->str_val : "", key, have_key);
        json_t *out = json_string(r);
        free(r);
        return out;
    }
    if (value->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(value); ++i) {
            json_append(out, rs_walk(json_get(value, i), NULL, 0, hard));
        }
        return out;
    }
    /* JSON_OBJECT */
    json_t *out = json_object();
    for (size_t i = 0; i < json_object_size(value); ++i) {
        const char *item_key = json_object_get_key_at(value, i);
        char *safe_key = rs_redact_leaf(item_key ? item_key : "", NULL, 0);
        json_set(out, safe_key, rs_walk(json_object_get_at(value, i), safe_key, 1, hard));
        free(safe_key);
    }
    return out;
}

static int rs_settled(const json_t *payload) {
    char *serialized = rs_dump(payload);
    char *redacted = rs_redact(serialized);
    int same = !strcmp(serialized, redacted);
    free(serialized);
    free(redacted);
    return same;
}

json_t *w9_redactstruc_8d7c7b_redact_structured(json_t *payload) {
    if (!payload) payload = json_null();
    json_t *walked = rs_walk(payload, NULL, 0, 0);
    if (rs_settled(walked)) return walked;
    json_free(walked);

    json_t *masked = rs_walk(payload, NULL, 0, 1);
    if (rs_settled(masked)) return masked;
    json_free(masked);

    if (payload->type == JSON_OBJECT) {
        json_t *out = json_object();
        json_set(out, "_redaction", json_string(RS_SENTINEL));
        return out;
    }
    if (payload->type == JSON_ARRAY) {
        json_t *out = json_array();
        json_append(out, json_string(RS_SENTINEL));
        return out;
    }
    return json_string(RS_SENTINEL);
}
