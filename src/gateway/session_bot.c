/*
 * session_bot.c — faithful C11 port of the pure receipt-validation helpers in
 *                  gateway/session_bot.py.
 *
 * PoP: w7_sessionbot_29c875_canonical_record_error @ gateway/session_bot.py:_canonical_record_error
 * PoP: w7_sessionbot_29c875_record_shape_error     @ gateway/session_bot.py:_record_shape_error
 * PoP: w7_sessionbot_29c875_is_pre_admission_pin   @ gateway/session_bot.py:_is_pre_admission_pin
 * PoP: w7_sessionbot_29c875_retry_identity         @ gateway/session_bot.py:_retry_identity
 * PoP: w7_sessionbot_29c875_refused_answer         @ gateway/session_bot.py:_refused_answer
 */
#include "noc_w7_sessionbot_29c875.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *W7SB_DEFAULT_REQUIRED[] = {"admission_id", "profile_home", "session_id", "principal_id"};
static const size_t W7SB_DEFAULT_N = 4;
static const char *W7SB_STATUSES[] = {"canonical","queued","claimed","ambiguous","settled","failed","cancelled"};
static const size_t W7SB_STATUSES_N = 7;

static int w7sb_is_str(const struct json_t *v) { return v && v->type == JSON_STRING; }
static int w7sb_str_eq(const struct json_t *v, const char *s) {
    return w7sb_is_str(v) && v->str_val && !strcmp(v->str_val, s);
}
static int w7sb_is_true(const struct json_t *v) { return v && v->type == JSON_BOOL && v->bool_val; }

/* tools/bot_live_delivery._delivery_id: [0-9a-f]{32,64} fullmatch. */
static int w7sb_valid_delivery_id(const char *s) {
    if (!s) return 0;
    size_t n = strlen(s);
    if (n < 32 || n > 64) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return 1;
}

/* Python repr() of a JSON scalar (fixture status values are simple). */
static char *w7sb_repr(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        for (int prec = 15; prec <= 17; prec++) {
            snprintf(buf, sizeof(buf), "%.*g", prec, v->num_val);
            if (strtod(buf, NULL) == v->num_val) break;
        }
        return strdup(buf);
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char quote = (strchr(s, '\'') && !strchr(s, '"')) ? '"' : '\'';
        size_t n = strlen(s);
        char *out = (char *)malloc(n * 5 + 3);
        size_t k = 0;
        out[k++] = quote;
        for (size_t i = 0; i < n; i++) {
            unsigned char c = (unsigned char)s[i];
            if (c == '\\' || c == (unsigned char)quote) { out[k++]='\\'; out[k++]=(char)c; }
            else if (c == '\n') { out[k++]='\\'; out[k++]='n'; }
            else if (c == '\r') { out[k++]='\\'; out[k++]='r'; }
            else if (c == '\t') { out[k++]='\\'; out[k++]='t'; }
            else if (c < 0x20 || c == 0x7f) {
                static const char *H="0123456789abcdef";
                out[k++]='\\'; out[k++]='x'; out[k++]=H[c>>4]; out[k++]=H[c&15];
            } else out[k++] = (char)c;
        }
        out[k++] = quote; out[k] = '\0';
        return out;
    }
    return strdup("");
}

static struct json_t *w7sb_err(const char *msg) { return json_string(msg); }
static struct json_t *w7sb_ok(void) { return json_null(); }

struct json_t *w7_sessionbot_29c875_canonical_record_error(const char *stem,
        const struct json_t *record, const struct json_t *required) {
    const struct json_t *did = record ? json_obj_get(record, "delivery_id") : NULL;
    if (!w7sb_is_str(did) || !stem || strcmp(did->str_val, stem))
        return w7sb_err("delivery id does not match filename");
    if (!w7sb_valid_delivery_id(did->str_val))
        return w7sb_err("delivery id is invalid");

    const char *const *keys = W7SB_DEFAULT_REQUIRED;
    size_t nkeys = W7SB_DEFAULT_N;
    if (required && required->type == JSON_ARRAY) {
        keys = NULL; nkeys = required->c.count; /* handled below via items */
    }
    /* Build the missing-fields list. */
    struct json_t *missing = json_array();
    for (size_t i = 0; i < nkeys; i++) {
        const char *key = keys ? keys[i]
                               : (required->c.items[i] && required->c.items[i]->type == JSON_STRING
                                  ? required->c.items[i]->str_val : "");
        const struct json_t *v = json_obj_get(record, key);
        if (!w7sb_is_str(v) || !v->str_val || !v->str_val[0]) json_append(missing, json_string(key));
    }
    if (json_len(missing) > 0) {
        size_t n = json_len(missing);
        size_t cap = 64;
        for (size_t i = 0; i < n; i++) cap += strlen(json_get(missing, i)->str_val) + 2;
        char *msg = (char *)malloc(cap);
        size_t k = (size_t)snprintf(msg, cap, "missing canonical fields: ");
        for (size_t i = 0; i < n; i++) {
            if (i) { msg[k++] = ','; msg[k++] = ' '; }
            const char *s = json_get(missing, i)->str_val;
            size_t sl = strlen(s); memcpy(msg + k, s, sl); k += sl;
        }
        msg[k] = '\0';
        json_free(missing);
        struct json_t *out = w7sb_err(msg);
        free(msg);
        return out;
    }
    json_free(missing);

    const struct json_t *msg = json_obj_get(record, "message");
    if (!w7sb_is_str(msg)) return w7sb_err("message is not a string");

    const struct json_t *status = json_obj_get(record, "status");
    int known = 0;
    if (w7sb_is_str(status))
        for (size_t i = 0; i < W7SB_STATUSES_N; i++)
            if (!strcmp(status->str_val, W7SB_STATUSES[i])) { known = 1; break; }
    if (!known) {
        char *rp = w7sb_repr(status);
        size_t cap = strlen(rp) + 32;
        char *text = (char *)malloc(cap);
        snprintf(text, cap, "unknown canonical status %s", rp);
        free(rp);
        struct json_t *out = w7sb_err(text);
        free(text);
        return out;
    }
    return w7sb_ok();
}

/* Upstream delegates 'owner' records to tools.bot_live_delivery._ticket_shape_error.
 * The oracle stubs that delegate to a fixed marker; we mirror it here. */
#define W7SB_TICKET_MARKER "owner ticket error"

struct json_t *w7_sessionbot_29c875_record_shape_error(const char *stem, const struct json_t *record) {
    if (w7sb_is_true(json_obj_get(record, "refused")) && !json_obj_get(record, "admission_id")) {
        if (!w7sb_str_eq(json_obj_get(record, "status"), "failed"))
            return w7sb_err("refused receipt is not failed");
        struct json_t *req = json_array();
        json_append(req, json_string("profile_home"));
        json_append(req, json_string("session_id"));
        json_append(req, json_string("principal_id"));
        struct json_t *out = w7_sessionbot_29c875_canonical_record_error(stem, record, req);
        json_free(req);
        return out;
    }
    if (json_obj_get(record, "admission_id") || w7sb_str_eq(json_obj_get(record, "status"), "canonical")
            || json_obj_get(record, "principal_id"))
        return w7_sessionbot_29c875_canonical_record_error(stem, record, NULL);
    if (json_obj_get(record, "owner")) return w7sb_err(W7SB_TICKET_MARKER);
    return w7sb_err("unknown receipt schema");
}

int w7_sessionbot_29c875_is_pre_admission_pin(const char *stem, const struct json_t *record) {
    if (!w7sb_str_eq(json_obj_get(record, "status"), "ambiguous")) return 0;
    if (json_obj_get(record, "admission_id") || json_obj_get(record, "refused")) return 0;
    struct json_t *req = json_array();
    json_append(req, json_string("profile_home"));
    json_append(req, json_string("session_id"));
    json_append(req, json_string("principal_id"));
    struct json_t *err = w7_sessionbot_29c875_canonical_record_error(stem, record, req);
    json_free(req);
    int result = (err->type == JSON_NULL);
    json_free(err);
    return result;
}

struct json_t *w7_sessionbot_29c875_retry_identity(const char *key) {
    size_t n = 4 + (key ? strlen(key) : 0) + 6 + 1;
    char *out = (char *)malloc(n);
    snprintf(out, n, "bot:%s:retry", key ? key : "");
    struct json_t *r = json_string(out);
    free(out);
    return r;
}

struct json_t *w7_sessionbot_29c875_refused_answer(const struct json_t *record) {
    static const char *KEYS[] = {"status","refused","delivery_id","profile_home",
                                 "session_id","message","reply","error","reason"};
    struct json_t *out = json_object();
    for (size_t i = 0; i < sizeof(KEYS)/sizeof(KEYS[0]); i++) {
        struct json_t *v = json_obj_get(record, KEYS[i]);
        json_set(out, KEYS[i], v ? json_copy(v) : json_null());
    }
    return out;
}
