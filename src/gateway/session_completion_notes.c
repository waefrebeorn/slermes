/*
 * session_completion_notes.c - C11 port of upstream
 * gateway/session_completion_notes.py (gate w26_sessioncom_fd19d0).
 *
 * PoP: w26_sessioncom_fd19d0_producer @ gateway/session_completion_notes.py:_producer
 * PoP: w26_sessioncom_fd19d0_noted_completion @ gateway/session_completion_notes.py:noted_completion
 * PoP: w26_sessioncom_fd19d0_wakes @ gateway/session_completion_notes.py:_wakes_unviewed_parent
 * PoP: w26_sessioncom_fd19d0_constants @ gateway/session_completion_notes.py:<producer tables>
 *
 * The admission queue, the store append, the close registry and the viewer set
 * are seams: whether a chat was closed by its user arrives here as one fixed
 * boolean, and the wake counter is a plain integer.  What is ported is the part
 * that is a pure function of values: the producer an encoded identity names,
 * whether a completion row is note-eligible, and whether an unviewed resume has
 * budget left to run as a turn.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_sessioncom_fd19d0.h"

#define W26_SC_UNATTENDED_WAKE_LIMIT 3

/* _NOTED_PRODUCERS */
static const char *const W26_SC_NOTED_PRODUCERS[] = {
    "async_delegation", "completion", "watch_disabled", "watch_match",
};
/* _ALWAYS_NOTED */
static const char *const W26_SC_ALWAYS_NOTED[] = {"cron"};

#define W26_SC_NOTED_N (sizeof(W26_SC_NOTED_PRODUCERS) / sizeof(*W26_SC_NOTED_PRODUCERS))
#define W26_SC_ALWAYS_N (sizeof(W26_SC_ALWAYS_NOTED) / sizeof(*W26_SC_ALWAYS_NOTED))

static int sc_truthy(const json_t *v) {
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

/* json.loads(identity)[0]: a list's first element, a string's first code point;
 * anything else raises upstream (TypeError/KeyError/IndexError) -> None. */
json_t *w26_sessioncom_fd19d0_producer(const json_t *identity) {
    json_t *parsed, *out = NULL;
    if (!identity || identity->type != JSON_STRING) return json_null();
    parsed = json_parse(identity->str_val ? identity->str_val : "", NULL);
    if (!parsed) return json_null();
    if (parsed->type == JSON_ARRAY) {
        if (json_len(parsed) > 0) out = json_copy(json_get(parsed, 0));
    } else if (parsed->type == JSON_STRING) {
        const char *s = parsed->str_val ? parsed->str_val : "";
        size_t n = 0;
        if (s[0]) {
            unsigned char c = (unsigned char)s[0];
            n = 1;
            if ((c & 0xE0) == 0xC0) n = 2;
            else if ((c & 0xF0) == 0xE0) n = 3;
            else if ((c & 0xF8) == 0xF0) n = 4;
            if (strlen(s) < n) n = strlen(s);
        }
        if (n > 0) {
            char *buf = (char *)malloc(n + 1);
            if (buf) {
                memcpy(buf, s, n);
                buf[n] = '\0';
                out = json_string(buf);
                free(buf);
            }
        } else {
            out = json_string("");
        }
    }
    json_free(parsed);
    return out ? out : json_null();
}

/* descriptor identity is a member of _NOTED_PRODUCERS | _ALWAYS_NOTED */
static int sc_in_table(const json_t *producer, int with_cron) {
    size_t i;
    if (!producer || producer->type != JSON_STRING) return 0;
    for (i = 0; i < W26_SC_NOTED_N; i++)
        if (!strcmp(producer->str_val ? producer->str_val : "", W26_SC_NOTED_PRODUCERS[i])) return 1;
    if (with_cron)
        for (i = 0; i < W26_SC_ALWAYS_N; i++)
            if (!strcmp(producer->str_val ? producer->str_val : "", W26_SC_ALWAYS_NOTED[i])) return 1;
    return 0;
}

static const json_t *sc_dict_get(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT ? json_obj_get(obj, key) : NULL;
}

json_t *w26_sessioncom_fd19d0_noted_completion(const json_t *row) {
    const json_t *payload, *descriptor, *producer;
    json_t *prod;
    int noted;
    if (!row || row->type != JSON_OBJECT) return json_bool(0);
    payload = sc_dict_get(row, "payload");
    if (!payload || payload->type != JSON_OBJECT) return json_bool(0);
    descriptor = sc_dict_get(payload, "local_automation_v1");
    if (!descriptor || descriptor->type == JSON_NULL) return json_bool(0);
    /* descriptor['identity'] on a non-mapping descriptor raises upstream */
    producer = sc_dict_get(descriptor, "identity");
    prod = w26_sessioncom_fd19d0_producer(producer);
    noted = sc_in_table(prod, 1);
    json_free(prod);
    return json_bool(noted ? 1 : 0);
}

json_t *w26_sessioncom_fd19d0_wakes(const json_t *req) {
    const json_t *descriptor = sc_dict_get(req, "descriptor");
    const json_t *failed_v, *resumes_v;
    int failed, resumes, closed, wakes = 0;
    long long counter = 0;
    json_t *out;

    if (descriptor && descriptor->type != JSON_OBJECT) descriptor = NULL;
    failed_v = sc_dict_get(descriptor, "wakes_parent");
    resumes_v = sc_dict_get(descriptor, "resumes_parent");
    failed = sc_truthy(failed_v);
    resumes = sc_truthy(resumes_v);
    closed = sc_truthy(sc_dict_get(req, "closed"));
    {
        const json_t *c = sc_dict_get(req, "unattended_wakes");
        if (c && c->type == JSON_NUMBER) counter = (long long)c->num_val;
        else if (c && c->type == JSON_BOOL) counter = c->bool_val ? 1 : 0;
    }
    if ((failed || resumes) && !closed) {
        if (!failed) {
            wakes = 1;
        } else if (counter >= W26_SC_UNATTENDED_WAKE_LIMIT) {
            wakes = 0;
        } else {
            counter += 1;
            wakes = 1;
        }
    }
    out = json_object();
    json_set(out, "wakes", json_bool(wakes ? 1 : 0));
    json_set(out, "unattended_wakes", json_number((double)counter));
    return out;
}

json_t *w26_sessioncom_fd19d0_constants(void) {
    json_t *o = json_object();
    json_t *noted = json_array(), *always = json_array();
    size_t i;
    for (i = 0; i < W26_SC_NOTED_N; i++) json_append(noted, json_string(W26_SC_NOTED_PRODUCERS[i]));
    for (i = 0; i < W26_SC_ALWAYS_N; i++) json_append(always, json_string(W26_SC_ALWAYS_NOTED[i]));
    json_set(o, "noted_producers", noted);
    json_set(o, "always_noted", always);
    json_set(o, "unattended_wake_limit", json_number((double)W26_SC_UNATTENDED_WAKE_LIMIT));
    return o;
}
