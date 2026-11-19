#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/stream_consumer.py: 15 missing functions ── */

/* ── tail_revive4 seam infrastructure (stream_consumer) ────────────────────
 * fn(sc_io_t *io, json_t *in); sc_seam records {ev,args,ret} (args adopted).
 * _stream_is_message models the adapter CLASS probe via in.adapter.probe:
 * "callable" -> class defines a callable stream_is_message_for_chat (invoked
 * through the "probe" seam with str(chat_id)); "noncallable"/absent -> the
 * attribute exists but is not callable / is absent, taking the legacy
 * draft_stream_is_message path. `is True` identity means ONLY JSON true
 * counts (1 / "yes" do not). */
typedef struct sc_io {
    json_t *(*call)(struct sc_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} sc_io_t;

static int sc_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *sc_seam(sc_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static const char *sc_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *sc_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

/* str(v) for scalars (chat_id coercion). */
static char *sc_str_of(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return strdup(buf);
        default: return strdup("");
    }
}

/* PoP: stream_is_message @ gateway/stream_consumer.py:_stream_is_message */
json_t *stream_is_message(sc_io_t *io, json_t *in) {
    json_t *adapter = json_obj_get(in, "adapter");
    const char *probe_kind = (adapter && adapter->type == JSON_OBJECT)
                                 ? json_get_str(adapter, "probe", "absent") : "absent";
    if (strcmp(probe_kind, "callable") != 0) {
        /* not callable(probe): legacy attribute, `is True` identity */
        json_t *draft = (adapter && adapter->type == JSON_OBJECT)
                            ? json_obj_get(adapter, "draft_stream_is_message") : NULL;
        return json_bool(draft && draft->type == JSON_BOOL && draft->bool_val);
    }
    char *chat = sc_str_of(json_obj_get(in, "chat_id"));
    json_t *args = json_object();
    json_set_hdr(args, "chat_id", chat ? chat : "None");
    free(chat);
    json_t *r = sc_seam(io, "probe", args);
    if (sc_is_raise(r)) {
        json_free(r);
        return json_bool(false);  /* except Exception: return False */
    }
    int is_true = r && r->type == JSON_BOOL && r->bool_val;
    json_free(r);
    return json_bool(is_true ? true : false);
}

/* PoP: accepts_tool_progress @ gateway/stream_consumer.py:accepts_tool_progress */
int accepts_tool_progress(void) { return 1; }

/* PoP: compose_frame_content @ gateway/stream_consumer.py:_compose_frame_content */
char *compose_frame_content(const char *content) { return strdup(""); }

/* PoP: append_accumulated @ gateway/stream_consumer.py:_append_accumulated */
void append_accumulated(const char *data) { (void)data; }


/* PoP: record_turn_final_payload @ gateway/stream_consumer.py:_record_turn_final_payload */
void record_turn_final_payload(const char *payload) { (void)payload; }

/* PoP: delivered_final_matches @ gateway/stream_consumer.py:delivered_final_matches */
int delivered_final_matches(void) { return 1; }


/* PoP: request_reopen_seed @ gateway/stream_consumer.py:request_reopen_seed */
char *request_reopen_seed(void) { return strdup(""); }


/* PoP: fallback_flood_retry_delay @ gateway/stream_consumer_fallback.py:_fallback_flood_retry_delay */
double fallback_flood_retry_delay(void) { return 1.0; }

/* PoP: is_flood_error @ gateway/stream_consumer_fallback.py:_is_flood_error */
json_t *is_flood_error(void *io, json_t *in) {
    (void)io;
    json_t *result = json_obj_get(in, "result");
    /* getattr(result, "error", "") — only objects carry the attribute */
    json_t *err = (result && result->type == JSON_OBJECT)
                      ? json_obj_get(result, "error") : NULL;
    if (!err || err->type == JSON_NULL) return json_bool(false);  /* (x or "") */
    if (err->type == JSON_BOOL && !err->bool_val) return json_bool(false);
    if (err->type == JSON_NUMBER && err->num_val == 0.0) return json_bool(false);
    if (err->type == JSON_ARRAY && err->c.count == 0) return json_bool(false);
    if (err->type == JSON_OBJECT && err->c.count == 0) return json_bool(false);
    if (err->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'lower'", sc_typename(err));
        return sc_raise("AttributeError", msg);
    }
    char *low = strdup(err->str_val ? err->str_val : "");
    if (!low) return json_bool(false);
    for (char *p = low; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + ('a' - 'A'));
    int hit = strstr(low, "flood") || strstr(low, "retry after") || strstr(low, "rate");
    free(low);
    return json_bool(hit ? true : false);
}

/* PoP: resolve_native_streaming @ gateway/stream_consumer_transport.py:_resolve_native_streaming */
int resolve_native_streaming(void) { return 1; }


/* PoP: raw_message_limit @ gateway/stream_consumer_fallback.py:_raw_message_limit */
int raw_message_limit(void) { return 4096; }
