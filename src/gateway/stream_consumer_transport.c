/*
 * stream_consumer_transport.c - C11 port of upstream Hermes
 * gateway/stream_consumer_transport.py (gates w31_streamcons_1c03c4_p1 / _p2).
 *
 * PoP: cn_1c03c4c0d9_constants @ gateway/stream_consumer_transport.py:<module-constants>
 * PoP: w31_streamcons_1c03c4_p1_constants                @ gateway/stream_consumer_transport.py:<module constants + StreamTransportMixin._MIN_NEW_MSG_CHARS>
 * PoP: w31_streamcons_1c03c4_p1_draft_metadata          @ gateway/stream_consumer_transport.py:StreamTransportMixin._draft_metadata
 * PoP: w31_streamcons_1c03c4_p1_stale_preview_ids       @ gateway/stream_consumer_transport.py:StreamTransportMixin._stale_preview_ids
 * PoP: w31_streamcons_1c03c4_p1_has_real_preview        @ gateway/stream_consumer_transport.py:StreamTransportMixin._has_real_preview
 * PoP: w31_streamcons_1c03c4_p1_adopt_message_id        @ gateway/stream_consumer_transport.py:StreamTransportMixin._adopt_message_id
 * PoP: w31_streamcons_1c03c4_p1_close_native_state      @ gateway/stream_consumer_transport.py:StreamTransportMixin._close_native_state
 * PoP: w31_streamcons_1c03c4_p1_enter_fallback_mode     @ gateway/stream_consumer_transport.py:StreamTransportMixin._enter_fallback_mode
 * PoP: w31_streamcons_1c03c4_p1_track_preview_id        @ gateway/stream_consumer_transport.py:StreamTransportMixin._track_preview_id
 * PoP: w31_streamcons_1c03c4_p2_should_push_draft       @ gateway/stream_consumer_transport.py:StreamTransportMixin._should_push_draft
 * PoP: w31_streamcons_1c03c4_p2_defer_draft_retry       @ gateway/stream_consumer_transport.py:StreamTransportMixin._defer_draft_retry
 * PoP: w31_streamcons_1c03c4_p2_should_send_fresh_final @ gateway/stream_consumer_transport.py:StreamTransportMixin._should_send_fresh_final
 * PoP: w31_streamcons_1c03c4_p2_degrade_native_to_buffered_send @ gateway/stream_consumer_transport.py:StreamTransportMixin._degrade_native_to_buffered_send
 * PoP: w31_streamcons_1c03c4_p2_resolve_draft_streaming  @ gateway/stream_consumer_transport.py:StreamTransportMixin._resolve_draft_streaming
 * PoP: w31_streamcons_1c03c4_p2_resolve_native_streaming @ gateway/stream_consumer_transport.py:StreamTransportMixin._resolve_native_streaming
 * PoP: w31_streamcons_1c03c4_p2_track_preview_ids_from_result @ gateway/stream_consumer_transport.py:StreamTransportMixin._track_preview_ids_from_result
 * PoP: w31_streamcons_1c03c4_p2_hold_ambiguous_first_send @ gateway/stream_consumer_transport.py:StreamTransportMixin._hold_ambiguous_first_send
 *
 * The sockets are seams: every adapter call (send_stream_frame, send_draft,
 * edit_message, delete_message), the asyncio sleep, the monotonic clock and the
 * delivery ledger stay in the caller. What is ported here is the value-level
 * decision core of the transport -- which preview ids a fresh final replaces,
 * whether a draft snapshot is due, how a server cooldown moves the retry
 * horizon, which transport a chat resolves to, and what a possibly-landed first
 * send leaves behind.
 *
 * The auto-generated module-constant table (cn_1c03c4c0d9) shares this file; its
 * symbol is kept at the bottom so that gate stays green.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_cn_1c03c4c0d9.h"
#include "noc_w31_streamcons_1c03c4_p1.h"
#include "noc_w31_streamcons_1c03c4_p2.h"

#define W31_NO_EDIT "__no_edit__"

/* ------------------------------------------------------------------ helpers */

static char *st_strdup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* Python truthiness of a JSON value (missing == None == falsy). */
static int st_truthy(const json_t *v) {
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

/* Python's "is an int/float" view: bool is an int, None and str are not. */
static int st_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python str() of a scalar id: "123" for 123, "123.5" for 123.5, "True". */
static char *st_id_str(const json_t *v) {
    char buf[64];
    if (!v) return NULL;
    if (v->type == JSON_STRING) return st_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return st_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e15 && d < 1e15)
            snprintf(buf, sizeof buf, "%.0f", d);
        else
            snprintf(buf, sizeof buf, "%g", d);
        return st_strdup(buf);
    }
    return NULL;
}

/* `_message_id != "__no_edit__"` over the scalar shapes an id can hold. */
static int st_is_no_edit(const json_t *v) {
    return v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, W31_NO_EDIT) == 0;
}

typedef struct {
    char **v;
    size_t n, cap;
} st_strlist;

static void st_list_push(st_strlist *l, char *s) {
    if (!s) return;
    if (l->n == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 8;
        char **grown = (char **)realloc(l->v, cap * sizeof(char *));
        if (!grown) { free(s); return; }
        l->v = grown;
        l->cap = cap;
    }
    l->v[l->n++] = s;
}

static int st_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Sorted, de-duplicated (set semantics) JSON array; frees the list. */
static json_t *st_list_take(st_strlist *l) {
    json_t *out = json_array();
    size_t i;
    if (l->n > 1) qsort(l->v, l->n, sizeof(char *), st_cmp_str);
    for (i = 0; i < l->n; i++) {
        if (i > 0 && strcmp(l->v[i], l->v[i - 1]) == 0) { free(l->v[i]); continue; }
        json_append(out, json_string(l->v[i]));
        free(l->v[i]);
    }
    free(l->v);
    l->v = NULL;
    l->n = l->cap = 0;
    return out;
}

/* Push every element of `arr` (an id list) as str(); a missing arr is a no-op. */
static void st_list_push_array(st_strlist *l, const json_t *arr) {
    size_t i, n;
    if (!arr || arr->type != JSON_ARRAY) return;
    n = json_len(arr);
    for (i = 0; i < n; i++) st_list_push(l, st_id_str(json_get(arr, i)));
}

static const json_t *st_field(const json_t *in, const char *key) {
    return in && in->type == JSON_OBJECT ? json_obj_get(in, key) : NULL;
}

static double st_num(const json_t *in, const char *key, double def) {
    double d;
    if (st_number(st_field(in, key), &d)) return d;
    return def;
}

static int st_bool(const json_t *in, const char *key, int def) {
    const json_t *v = st_field(in, key);
    if (!v) return def;
    return st_truthy(v);
}

/* ------------------------------------------------------- gate p1: framing */

json_t *w31_streamcons_1c03c4_p1_constants(void) {
    json_t *o = json_object();
    json_set(o, "max_edit_backoff_secs", json_number(30.0)); /* _MAX_EDIT_BACKOFF_SECS */
    json_set(o, "min_new_msg_chars", json_number(4.0));      /* _MIN_NEW_MSG_CHARS */
    return o;
}

json_t *w31_streamcons_1c03c4_p1_draft_metadata(const json_t *in) {
    const json_t *md = st_field(in, "metadata");
    const json_t *reply = st_field(in, "reply_to");
    json_t *out;

    /* dict(self.metadata) if self.metadata else {} */
    if (md && md->type == JSON_OBJECT && json_object_size(md) > 0)
        out = json_copy(md);
    else
        out = json_object();
    if (st_truthy(reply) && !json_obj_get(out, "reply_to_message_id"))
        json_set(out, "reply_to_message_id", json_copy(reply));
    if (json_object_size(out) == 0) { /* `md or None` */
        json_free(out);
        return json_null();
    }
    return out;
}

json_t *w31_streamcons_1c03c4_p1_stale_preview_ids(const json_t *in) {
    const json_t *mid = st_field(in, "message_id");
    int segment_only = st_bool(in, "segment_only", 0);
    st_strlist ids = {0, 0, 0};

    st_list_push_array(&ids, st_field(in, segment_only ? "segment_preview_ids" : "preview_ids"));
    if (st_truthy(mid) && !st_is_no_edit(mid)) st_list_push(&ids, st_id_str(mid));
    return st_list_take(&ids);
}

int w31_streamcons_1c03c4_p1_has_real_preview(const json_t *in) {
    const json_t *mid = st_field(in, "message_id");
    return st_truthy(mid) && !st_is_no_edit(mid);
}

json_t *w31_streamcons_1c03c4_p1_adopt_message_id(const json_t *in) {
    const json_t *mid = st_field(in, "message_id");
    json_t *out = json_object();
    char *s;

    if (st_truthy(mid)) {                 /* if message_id: adopt + stamp the clock */
        s = st_id_str(mid);
        json_set(out, "message_id", s ? json_string(s) : json_null());
        free(s);
        json_set(out, "created", json_bool(1));
    } else {                              /* else the "__no_edit__" sentinel, no clock */
        json_set(out, "message_id", json_string(W31_NO_EDIT));
        json_set(out, "created", json_bool(0));
    }
    return out;
}

json_t *w31_streamcons_1c03c4_p1_close_native_state(const json_t *in) {
    json_t *out = json_object();
    json_set(out, "native_stream_opened", json_bool(0));
    json_set(out, "native_last_pushed_len", json_number(0));
    /* untouched by the method: proves the close is not a full reset */
    json_set(out, "already_sent", json_bool(st_bool(in, "already_sent", 0)));
    return out;
}

json_t *w31_streamcons_1c03c4_p1_enter_fallback_mode(const json_t *in) {
    const json_t *prefix = st_field(in, "prefix");
    json_t *out = json_object();
    json_set(out, "fallback_prefix", prefix ? json_copy(prefix) : json_null());
    json_set(out, "fallback_final_send", json_bool(1));
    json_set(out, "edit_supported", json_bool(0));
    json_set(out, "already_sent", json_bool(1));
    return out;
}

json_t *w31_streamcons_1c03c4_p1_track_preview_id(const json_t *in) {
    const json_t *mid = st_field(in, "message_id");
    st_strlist preview = {0, 0, 0}, segment = {0, 0, 0};
    json_t *out = json_object();

    st_list_push_array(&preview, st_field(in, "preview_ids"));
    st_list_push_array(&segment, st_field(in, "segment_preview_ids"));
    if (st_truthy(mid) && !st_is_no_edit(mid)) { /* a real id joins both sets */
        st_list_push(&preview, st_id_str(mid));
        st_list_push(&segment, st_id_str(mid));
    }
    json_set(out, "preview_ids", st_list_take(&preview));
    json_set(out, "segment_preview_ids", st_list_take(&segment));
    return out;
}

/* ------------------------------------- gate p2: cadence, backoff, transport */

int w31_streamcons_1c03c4_p2_should_push_draft(const json_t *in) {
    const json_t *accumulated = st_field(in, "accumulated");
    double now = st_num(in, "now", 0.0);
    double retry_until = st_num(in, "draft_retry_until", 0.0);
    double last_edit = st_num(in, "last_edit_time", 0.0);
    double interval = st_num(in, "current_edit_interval", 0.0);

    if (!st_truthy(accumulated)) return 0;
    if (!(now >= retry_until)) return 0;
    if (last_edit == 0.0) return 1;
    return now - last_edit >= interval;
}

json_t *w31_streamcons_1c03c4_p2_defer_draft_retry(const json_t *in) {
    const json_t *retry_after = st_field(in, "retry_after");
    double now = st_num(in, "now", 0.0);
    double until = st_num(in, "draft_retry_until", 0.0);
    double ra = 0.0;
    json_t *out = json_object();

    /* isinstance(retry_after, (int, float)) and retry_after > 0 */
    if (!st_number(retry_after, &ra) || ra <= 0.0) {
        json_set(out, "deferred", json_bool(0));
        json_set(out, "retry_until", json_number(until));
        return out;
    }
    if (now + ra > until) until = now + ra; /* max(until, now + retry_after) */
    json_set(out, "deferred", json_bool(1));
    json_set(out, "retry_until", json_number(until));
    return out;
}

int w31_streamcons_1c03c4_p2_should_send_fresh_final(const json_t *in) {
    const json_t *mid = st_field(in, "message_id");
    const json_t *created = st_field(in, "message_created_ts");
    double threshold = st_num(in, "fresh_final_after_seconds", 0.0);
    double now = st_num(in, "now", 0.0);
    double ts;

    if (!(threshold > 0.0)) return 0;
    if (!(st_truthy(mid) && !st_is_no_edit(mid))) return 0;
    if (!st_number(created, &ts)) return 0;
    return now - ts >= threshold;
}

json_t *w31_streamcons_1c03c4_p2_degrade_native_to_buffered_send(const json_t *in) {
    json_t *out = json_object();
    (void)in;
    json_set(out, "use_native_streaming", json_bool(0)); /* native mode off */
    json_set(out, "native_stream_opened", json_bool(0)); /* close_native_state() */
    json_set(out, "native_last_pushed_len", json_number(0));
    json_set(out, "buffer_only", json_bool(1));          /* cfg.buffer_only = True */
    return out;
}

/* ASCII lower of a transport name; NULL/None reads as "edit". */
static char *st_lower(const char *s) {
    char *p;
    size_t i, n;
    if (!s) s = "edit";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    for (i = 0; i < n; i++) {
        char c = s[i];
        p[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    p[n] = '\0';
    return p;
}

/* The adapter probe as a value: fixed true/false, a raise, or "truthy" against
 * the fixture's `probe_value` (to exercise the bool(...) coercion). */
static int st_probe(const json_t *in, const char *key) {
    const json_t *probe = st_field(in, key);
    const json_t *value = st_field(in, "probe_value");
    if (!probe || probe->type == JSON_NULL) return 1; /* absent probe -> supported */
    if (probe->type == JSON_BOOL) return probe->bool_val ? 1 : 0;
    if (probe->type == JSON_NUMBER) return probe->num_val != 0.0;
    if (probe->type == JSON_STRING && probe->str_val) {
        if (strcmp(probe->str_val, "raise") == 0) return 0;   /* probe raised */
        if (strcmp(probe->str_val, "truthy") == 0) return st_truthy(value);
        if (strcmp(probe->str_val, "false") == 0) return 0;
        return 1;
    }
    return 0;
}

int w31_streamcons_1c03c4_p2_resolve_draft_streaming(const json_t *in) {
    const char *transport = json_get_str(in, "transport", NULL);
    char *lower = st_lower(transport);
    int is_edit;

    if (!lower) return 0;
    is_edit = strcmp(lower, "edit") == 0 || strcmp(lower, "off") == 0;
    free(lower);
    if (is_edit) return 0;                            /* explicit edit/off never drafts */
    if (!st_bool(in, "adapter_is_base", 0)) return 0; /* MagicMock/double -> edit */
    /* supports_draft_streaming(chat_id=..., ...) falling back to the legacy
     * signature on TypeError: both signatures yield the same probe answer, so
     * only the outcome (and a raising probe) is visible. */
    return st_probe(in, "probe");
}

int w31_streamcons_1c03c4_p2_resolve_native_streaming(const json_t *in) {
    if (!st_bool(in, "adapter_is_base", 0)) return 0;
    if (!st_bool(in, "adapter_class_flag", 0)) return 0; /* SUPPORTS_NATIVE_STREAMING */
    if (!st_bool(in, "has_probe", 1)) return 0;          /* supports_native_streaming */
    return st_probe(in, "probe");
}

json_t *w31_streamcons_1c03c4_p2_track_preview_ids_from_result(const json_t *in) {
    const json_t *result = st_field(in, "result");
    const json_t *raw;
    st_strlist ids = {0, 0, 0};
    json_t *out = json_object();

    if (!result || result->type != JSON_OBJECT) result = NULL;
    raw = st_field(result, "raw_response");
    /* primary id, then continuation ids, then raw_response["message_ids"] */
    st_list_push(&ids, st_id_str(st_field(result, "message_id")));
    st_list_push_array(&ids, st_field(result, "continuation_message_ids"));
    if (raw && raw->type == JSON_OBJECT) st_list_push_array(&ids, st_field(raw, "message_ids"));
    json_set(out, "preview_ids", st_list_take(&ids));
    return out;
}

int w31_streamcons_1c03c4_p2_adapter_prefers_fresh_final(const json_t *in) {
    const json_t *mid = st_field(in, "message_id");
    const json_t *hook = st_field(in, "hook");

    /* False without a real preview / hook, or on any error. */
    if (!(st_truthy(mid) && !st_is_no_edit(mid))) return 0;
    if (!hook || hook->type == JSON_NULL) return 0;
    /* ``return result is True``: a truthy non-True answer (1, "yes", a mock
     * child) must not enable fresh-final. */
    if (hook->type == JSON_BOOL) return hook->bool_val ? 1 : 0;
    if (hook->type == JSON_STRING && hook->str_val)
        return strcmp(hook->str_val, "true") == 0 ||
               strcmp(hook->str_val, "meta_only") == 0 ||
               strcmp(hook->str_val, "text_only") == 0;
    return 0;
}

json_t *w31_streamcons_1c03c4_p2_hold_ambiguous_first_send(const json_t *in) {
    const json_t *text = st_field(in, "text");
    const json_t *raw_text = st_field(in, "raw_text");
    const json_t *cursor = st_field(in, "cursor");
    int finalize = st_bool(in, "finalize", 0);
    json_t *out = json_object();

    json_set(out, "ok", json_bool(1));
    json_set(out, "last_sent_text", text ? json_copy(text) : json_string(""));
    json_set(out, "delivery_ambiguous", json_bool(finalize ? 1 : 0));
    json_set(out, "preview_uncertain", json_bool(finalize ? 0 : 1));
    json_set(out, "edit_supported", json_bool(0));
    json_set(out, "already_sent", json_bool(1));
    json_set(out, "message_id", json_string(W31_NO_EDIT));
    if (finalize) {
        json_set(out, "fallback_prefix", json_null());
        json_set(out, "fallback_final_send", json_null());
    } else {
        /* raw = raw_text if raw_text is not None else text; strip one cursor */
        json_t *raw = (raw_text && raw_text->type != JSON_NULL)
                          ? json_copy(raw_text)
                          : (text ? json_copy(text) : json_string(""));
        if (st_truthy(cursor) && raw->type == JSON_STRING && raw->str_val) {
            const char *cs = cursor->str_val;
            size_t cl = strlen(cs), rl = strlen(raw->str_val);
            if (rl >= cl && strcmp(raw->str_val + rl - cl, cs) == 0)
                raw->str_val[rl - cl] = '\0';
        }
        json_set(out, "fallback_prefix", raw);
        json_set(out, "fallback_final_send", json_bool(1));
    }
    return out;
}

/* --------------------------------- legacy constant table (cn_1c03c4c0d9) --- */

json_t *cn_1c03c4c0d9_constants(void) {
    json_t *out = json_object();
    json_set(out, "_MAX_EDIT_BACKOFF_SECS", json_number(30));
    return out;
}
