/*
 * run_notifications_restart.c - C home for gateway/run_notifications_restart.py.
 * Faithful port of the restart-notice marker decision tree: TTL expiry, the
 * boot-claim/replay comparison, keep-vs-consume bookkeeping, and the requester
 * notice delivery gate (suppression, transport availability, send refusal).
 * Collaborators (transport, config, send result) are injected as decision inputs.
 * PoP: w19_runnotific_a1bcca_notice_expired @ gateway/run_notifications_restart.py:_notice_expired
 * PoP: w19_runnotific_a1bcca_send_restart_notification @ gateway/run_notifications_restart.py:GatewayRestartNoticeMixin._send_restart_notification
 * PoP: w19_runnotific_a1bcca_deliver_restart_notification @ gateway/run_notifications_restart.py:GatewayRestartNoticeMixin._deliver_restart_notification
 * PoP: w19_runnotific_a1bcca_schedule_replay_wanted @ gateway/run_notifications_restart.py:GatewayRestartNoticeMixin._schedule_restart_notice_replay
 * PoP: w19_runnotific_a1bcca_notice_ttl_seconds @ gateway/run_notifications_restart.py:_RESTART_NOTICE_TTL_SECONDS
 * PoP: w19_runnotific_a1bcca_note_restart_notice_boot @ gateway/run_notifications_restart.py:GatewayRestartNoticeMixin._note_restart_notice_boot
 */
#include "noc_w19_runnotific_a1bcca.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RN_TTL_SECONDS (30 * 60)

static char *rn_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* Python truthiness of a JSON value. */
static int rn_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* str(value) for the scalar coordinates a restart marker carries. */
static char *rn_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return rn_dup("None");
    switch (v->type) {
        case JSON_BOOL: return rn_dup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return rn_dup(buf);
        }
        case JSON_STRING: return rn_dup(v->str_val ? v->str_val : "");
        default: return rn_dup("");
    }
}

json_t *w19_runnotific_a1bcca_notice_expired(json_t *age_seconds, int stat_ok) {
    double age = (age_seconds && age_seconds->type == JSON_NUMBER) ? age_seconds->num_val : 0.0;
    if (!stat_ok) return json_bool(1);   /* OSError -> True */
    return json_bool(age > (double)RN_TTL_SECONDS);
}

json_t *w19_runnotific_a1bcca_notice_ttl_seconds(void) {
    return json_number((double)RN_TTL_SECONDS);
}

/*
 * _send_restart_notification: marker read, claim comparison, TTL check, delivery,
 * and the finally-unlink bookkeeping. `deliver` is the injected _deliver call.
 */
json_t *w19_runnotific_a1bcca_send_restart_notification(json_t *spec) {
    json_t *out = json_object();
    int exists, replay, expired, keep = 1, raises = 0, delivered = 0;
    const char *raw, *claimed;
    json_t *deliver, *ret = NULL;
    if (!spec || spec->type != JSON_OBJECT) spec = json_object();
    exists = rn_truthy(json_obj_get(spec, "exists"));
    replay = rn_truthy(json_obj_get(spec, "replay"));
    expired = rn_truthy(json_obj_get(spec, "expired"));
    raw = json_get_str(spec, "raw", "");
    claimed = json_get_str(spec, "claimed", NULL);
    deliver = json_obj_get(spec, "deliver");
    /* read_text(encoding="utf-8-sig"): a leading BOM is not part of the value */
    if (raw && strlen(raw) >= 3 && (unsigned char)raw[0] == 0xEF
        && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF)
        raw += 3;
    json_set(out, "return_value", json_null());
    json_set(out, "exists_after", json_bool(0));
    json_set(out, "unlink", json_bool(0));
    json_set(out, "claimed", json_null());

    if (!exists) {
        /* no marker: return None, nothing read, nothing consumed */
        return out;
    }
    if (!replay) {
        json_set(out, "claimed", json_string(raw ? raw : ""));
    } else if (!claimed || strcmp(raw ? raw : "", claimed) != 0) {
        /* a newer marker: not ours to answer or consume */
        json_set(out, "exists_after", json_bool(1));
        return out;
    }
    if (expired) {
        keep = 0;
    } else {
        /* json.loads(raw): a marker that is not JSON makes the read raise, so it is consumed. */
        const char *payload_text = raw ? raw : "";
        json_t *payload;
        if (strlen(payload_text) >= 3 && (unsigned char)payload_text[0] == 0xEF
            && (unsigned char)payload_text[1] == 0xBB && (unsigned char)payload_text[2] == 0xBF)
            payload_text += 3;   /* utf-8-sig */
        payload = json_parse(payload_text, NULL);
        if (!payload) {
            keep = 0;
        } else {
            json_free(payload);
            if (deliver && deliver->type == JSON_OBJECT) {
                raises = rn_truthy(json_obj_get(deliver, "raises"));
                if (!raises) {
                    keep = rn_truthy(json_obj_get(deliver, "keep"));
                    ret = json_obj_get(deliver, "result");
                    delivered = 1;
                }
            }
            if (raises) keep = 0;
        }
    }
    if (delivered && ret && ret->type != JSON_NULL)
        json_set(out, "return_value", json_copy(ret));
    json_set(out, "exists_after", json_bool(keep ? 1 : 0));
    json_set(out, "unlink", json_bool(keep ? 0 : 1));
    return out;
}

/*
 * _deliver_restart_notification: one send of the requester notice.
 * data = marker payload; ctx = injected collaborators (config gate, transport,
 * metadata seam, send result) - identical seams on both sides.
 */
json_t *w19_runnotific_a1bcca_deliver_restart_notification(json_t *data, json_t *ctx) {
    json_t *out = json_object();
    json_t *platform_node, *chat_node, *thread_node;
    json_t *cfg, *transport_present, *meta_node, *relay, *failed, *reconn;
    char *platform_str = NULL, *chat_str = NULL, *thread_str = NULL;
    json_t *target = NULL;
    int sent = 0;

    json_set(out, "target", json_null());
    json_set(out, "keep", json_bool(0));
    json_set(out, "metadata", json_null());
    json_set(out, "sent", json_bool(0));
    if (!data || data->type != JSON_OBJECT) data = json_object();
    if (!ctx || ctx->type != JSON_OBJECT) ctx = json_object();

    platform_node = json_obj_get(data, "platform");
    chat_node = json_obj_get(data, "chat_id");
    thread_node = json_obj_get(data, "thread_id");
    if (!rn_truthy(platform_node) || !rn_truthy(chat_node)) {
        /* (None, False): an unusable marker is consumed */
        return out;
    }
    if (!rn_truthy(json_obj_get(ctx, "platform_known"))) {
        /* Platform(platform_str) raises: the outer except consumes the marker */
        json_set(out, "keep", json_bool(0));
        return out;
    }
    platform_str = rn_str(platform_node);
    chat_str = rn_str(chat_node);
    if (thread_node && rn_truthy(thread_node)) thread_str = rn_str(thread_node);

    cfg = json_obj_get(ctx, "platform_cfg");
    if (cfg && cfg->type == JSON_OBJECT
        && !rn_truthy(json_obj_get(cfg, "gateway_restart_notification"))) {
        /* suppressed by config: consumed, keep False */
        free(platform_str);
        free(chat_str);
        free(thread_str);
        return out;
    }
    transport_present = json_obj_get(ctx, "transport_present");
    if (!rn_truthy(transport_present)) {
        json_set(out, "keep", json_bool(1));   /* kept for the reconnect replay */
        free(platform_str);
        free(chat_str);
        free(thread_str);
        return out;
    }

    meta_node = json_obj_get(ctx, "metadata");
    if (meta_node && meta_node->type != JSON_NULL)
        json_set(out, "metadata", json_copy(meta_node));
    relay = json_obj_get(data, "delivered_via_upstream_relay");
    if (relay && relay->type == JSON_BOOL && relay->bool_val) {
        json_t *meta = json_obj_get(out, "metadata");
        json_t *box = (meta && meta->type == JSON_OBJECT) ? meta : json_object();
        const char *fields[2] = { "user_id", "scope_id" };
        size_t i;
        for (i = 0; i < 2; i++) {
            json_t *v = json_obj_get(data, fields[i]);
            if (rn_truthy(v)) {
                char *s = rn_str(v);
                if (s) {
                    json_set(box, fields[i], json_string(s));
                    free(s);
                }
            }
        }
        if (box != meta) json_set(out, "metadata", box);
    }

    /* transport.send(...) result: adapter errors come back as SendResult(success=False) */
    sent = 1;   /* the send was attempted; failure is reported by the result */
    failed = json_obj_get(ctx, "send_failed");
    if (rn_truthy(failed)) {
        reconn = json_obj_get(ctx, "reconnect_only");
        json_set(out, "keep", json_bool(rn_truthy(reconn) ? 1 : 0));
        json_set(out, "sent", json_bool(1));
        free(platform_str);
        free(chat_str);
        free(thread_str);
        return out;
    }
    sent = 1;
    target = json_array();
    json_append(target, json_string(platform_str ? platform_str : ""));
    json_append(target, json_string(chat_str ? chat_str : ""));
    json_append(target, thread_str ? json_string(thread_str) : json_null());
    json_set(out, "target", target);
    json_set(out, "keep", json_bool(0));
    json_set(out, "sent", json_bool(sent));
    free(platform_str);
    free(chat_str);
    free(thread_str);
    return out;
}

/*
 * _schedule_restart_notice_replay gate: before the boot pass, while going down, or
 * with no owed marker, the replay is not scheduled.
 */
json_t *w19_runnotific_a1bcca_schedule_replay_wanted(json_t *spec) {
    int context_present, restart_requested, draining, requester, planned;
    if (!spec || spec->type != JSON_OBJECT) spec = json_object();
    context_present = rn_truthy(json_obj_get(spec, "context_present"));
    restart_requested = rn_truthy(json_obj_get(spec, "restart_requested"));
    draining = rn_truthy(json_obj_get(spec, "draining"));
    requester = rn_truthy(json_obj_get(spec, "requester_pending"));
    planned = rn_truthy(json_obj_get(spec, "planned_pending"));
    if (!context_present || restart_requested || draining) return json_bool(0);
    return json_bool((requester || planned) ? 1 : 0);
}

/* _note_restart_notice_boot: capture the context once, on the first boot notice. */
json_t *w19_runnotific_a1bcca_note_restart_notice_boot(json_t *context_present) {
    if (rn_truthy(context_present)) return json_bool(0);  /* already captured */
    return json_bool(1);
}
