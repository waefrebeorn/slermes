/*
 * update_admission.c - C11 port of upstream
 * plugins/platforms/telegram/update_admission.py (gate w23_updateadmi_517547).
 *
 * PoP: w23_updateadmi_517547_field_names @ plugins/platforms/telegram/update_admission.py:_field_names
 * PoP: w23_updateadmi_517547_topic_lane @ plugins/platforms/telegram/update_admission.py:_topic_lane
 * PoP: w23_updateadmi_517547_lane_keys @ plugins/platforms/telegram/update_admission.py:PerChatUpdateProcessor._lane_keys
 * PoP: w23_updateadmi_517547_update_limit @ plugins/platforms/telegram/update_admission.py:build_update_processor
 * PoP: w23_updateadmi_517547_defaults @ plugins/platforms/telegram/update_admission.py:<module constants>
 *
 * The PTB Application, the asyncio tails and the receipt file are seams. What is
 * ported is what those machinery pieces ask of a value: which update fields a
 * log line may name, which FIFO lane an update waits in, and how a
 * max_concurrent_updates setting is sanitized.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_updateadmi_517547.h"

/* Python truthiness of a JSON value. */
static int ua_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* a growable string: the joined length is not known up front */
typedef struct {
    char *buf;
    size_t len, cap;
    int oom;
} ua_buf;

static void ua_put(ua_buf *b, const char *txt) {
    size_t n;
    char *grown;
    if (b->oom || !txt) return;
    n = strlen(txt);
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        while (cap < b->len + n + 1) cap *= 2;
        grown = (char *)realloc(b->buf, cap);
        if (!grown) { b->oom = 1; return; }
        b->buf = grown;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, txt, n + 1);
    b->len += n;
}

static int ua_cmp_key(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* sorted key names of one dict, with Python's `value is not False` filter. */
static size_t ua_names(const json_t *obj, const char **out, size_t cap) {
    size_t i, n = 0;
    if (!obj || obj->type != JSON_OBJECT) return 0;
    for (i = 0; i < json_object_size(obj) && n < cap; i++) {
        const char *k = json_object_get_key_at(obj, i);
        const json_t *v = json_object_get_at(obj, i);
        if (!k) continue;
        if (v && v->type == JSON_BOOL && !v->bool_val) continue;
        out[n++] = k;
    }
    qsort(out, n, sizeof(out[0]), ua_cmp_key);
    return n;
}

json_t *w23_updateadmi_517547_field_names(const json_t *data) {
    const char **keys;
    size_t n, i, j, cap;
    ua_buf b;
    json_t *out;

    memset(&b, 0, sizeof(b));
    if (!data || data->type != JSON_OBJECT) return json_string("");
    cap = json_object_size(data);
    if (cap == 0) return json_string("");
    keys = (const char **)calloc(cap, sizeof(*keys));
    if (!keys) return json_string("");
    n = ua_names(data, keys, cap);
    for (i = 0; i < n; i++) {
        const json_t *v;
        if (i) ua_put(&b, ", ");
        ua_put(&b, keys[i]);
        v = json_obj_get(data, keys[i]);
        if (v && v->type == JSON_OBJECT) {
            size_t kcap = json_object_size(v);
            const char **kids = kcap ? (const char **)calloc(kcap, sizeof(*kids)) : NULL;
            size_t m = kids ? ua_names(v, kids, kcap) : 0;
            ua_put(&b, "(");
            for (j = 0; j < m; j++) {
                if (j) ua_put(&b, ",");
                ua_put(&b, kids[j]);
            }
            ua_put(&b, ")");
            free(kids);
        }
    }
    out = json_string(b.buf ? b.buf : "");
    free(b.buf);
    free(keys);
    return out;
}

json_t *w23_updateadmi_517547_topic_lane(const json_t *message) {
    const json_t *is_topic, *created, *thread;
    if (!message || message->type != JSON_OBJECT) return json_null();
    /* getattr(message, "is_topic_message", False) is not True -> no lane. */
    is_topic = json_obj_get(message, "is_topic_message");
    if (!is_topic || is_topic->type != JSON_BOOL || !is_topic->bool_val) return json_null();
    /* a forum_topic_created service message stays on the chat's shared lane. */
    created = json_obj_get(message, "forum_topic_created");
    if (created && ua_truthy(created)) return json_null();
    thread = json_obj_get(message, "message_thread_id");
    if (!thread || thread->type == JSON_NULL) return json_null();
    return json_copy(thread);
}

/* str() of the message_thread_id: an int prints as an integer. */
static const char *ua_lane_text(const json_t *v, char *buf, size_t cap) {
    double d;
    if (!v) return NULL;
    if (v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    if (v->type == JSON_BOOL) return v->bool_val ? "True" : "False";
    if (v->type != JSON_NUMBER) return NULL;
    d = v->num_val;
    if ((double)(long long)d == d) { snprintf(buf, cap, "%lld", (long long)d); return buf; }
    snprintf(buf, cap, "%g", d);
    return buf;
}

json_t *w23_updateadmi_517547_lane_keys(const json_t *chat_id, const json_t *message,
                                        const json_t *adopt) {
    json_t *out = json_array();
    json_t *lane = w23_updateadmi_517547_topic_lane(message);
    int has_message = message && message->type != JSON_NULL;
    int has_adopt = has_message && adopt && adopt->type != JSON_NULL;
    char thread_buf[64];
    const char *thread = NULL;

    if (has_adopt) {
        json_t *chat_lane = json_array();
        json_t *topic_lane = json_array();
        json_append(chat_lane, chat_id ? json_copy(chat_id) : json_null());
        json_append(chat_lane, json_null());
        json_append(topic_lane, chat_id ? json_copy(chat_id) : json_null());
        json_append(topic_lane, json_copy(adopt));
        json_append(out, chat_lane);
        json_append(out, topic_lane);
        json_free(lane);
        return out;
    }
    if (lane && lane->type != JSON_NULL) thread = ua_lane_text(lane, thread_buf, sizeof(thread_buf));
    {
        json_t *row = json_array();
        json_append(row, chat_id ? json_copy(chat_id) : json_null());
        json_append(row, thread ? json_string(thread) : json_null());
        json_append(out, row);
    }
    json_free(lane);
    return out;
}

/* int(value): bool, truncating float, or a whitespace-padded int literal. */
static int ua_py_int(const json_t *v, long long *out) {
    const char *p;
    int neg = 0, digits = 0;
    long long acc = 0;

    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d != d) return 0;                                  /* nan */
        if (d >= 9.2233720368547758e18 || d <= -9.2233720368547758e18) return 0;
        *out = (long long)d;                                   /* trunc toward 0 */
        return 1;
    }
    if (v->type != JSON_STRING || !v->str_val) return 0;
    p = v->str_val;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    while (*p >= '0' && *p <= '9') {
        if (acc > 922337203685477580LL) return 0;
        acc = acc * 10 + (*p - '0');
        digits++;
        p++;
    }
    if (!digits) return 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p) return 0;
    *out = neg ? -acc : acc;
    return 1;
}

json_t *w23_updateadmi_517547_update_limit(const json_t *raw) {
    long long n = 0;
    if (!ua_py_int(raw, &n)) n = 0;                /* coerce_port(raw, 0) */
    if (n < 1) n = W23_UPDATEADMI_517547_DEFAULT_MAX_CONCURRENT;
    return json_number((double)n);
}

json_t *w23_updateadmi_517547_defaults(void) {
    json_t *o = json_object();
    json_set(o, "max_concurrent_updates",
             json_number((double)W23_UPDATEADMI_517547_DEFAULT_MAX_CONCURRENT));
    json_set(o, "seen_cap", json_number((double)W23_UPDATEADMI_517547_SEEN_CAP));
    json_set(o, "receipt_ttl_seconds",
             json_number((double)W23_UPDATEADMI_517547_RECEIPT_TTL_SECONDS));
    return o;
}
