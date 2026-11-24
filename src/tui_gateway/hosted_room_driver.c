/*
 * hosted_room_driver.c - C11 port of upstream Hermes tui_gateway/hosted_room_driver.py.
 *
 * Three differential gates share this one port file, partitioned by the pure
 * layer each one owns:
 *
 *   w28_hostedroom_3fa737_p1 - value shaping: room session title, member id,
 *        session-id coercion, UTF-8 bounded text, terminal receipts, history
 *        receipt search, session kwargs, task fences.
 *   w28_hostedroom_3fa737_p2 - decisions/predicates: info activity, exact
 *        admission, peer Stop acknowledgement, interrupt ack, Discard guards,
 *        the Stop outcome router, deadline predicates, retry routing.
 *   w28_hostedroom_3fa737_p3 - arithmetic and scheduling: unavailable-route
 *        backoff, lease renewal/cache decisions, ambiguous expiry, deadline
 *        settlement payload, room-cycle rotation, approval payload, config
 *        validation.
 *
 * PoP: w28_hostedroom_3fa737_p1_room_session_title @ tui_gateway/hosted_room_driver.py:room_session_title
 * PoP: w28_hostedroom_3fa737_p1_member_id @ tui_gateway/hosted_room_driver.py:_member_id
 * PoP: w28_hostedroom_3fa737_p1_session_id @ tui_gateway/hosted_room_driver.py:_session_id
 * PoP: w28_hostedroom_3fa737_p1_truncate_utf8 @ tui_gateway/hosted_room_driver.py:_truncate_utf8
 * PoP: w28_hostedroom_3fa737_p1_bounded_result @ tui_gateway/hosted_room_driver.py:_bounded_terminal_result
 * PoP: w28_hostedroom_3fa737_p1_receipt_from @ tui_gateway/hosted_room_driver.py:_receipt_from
 * PoP: w28_hostedroom_3fa737_p1_discarded_receipt @ tui_gateway/hosted_room_driver.py:discarded_receipt
 * PoP: w28_hostedroom_3fa737_p1_find_terminal_receipt @ tui_gateway/hosted_room_driver.py:_find_terminal_receipt
 * PoP: w28_hostedroom_3fa737_p1_session_kw @ tui_gateway/hosted_room_driver.py:_session_kw
 * PoP: w28_hostedroom_3fa737_p1_fences @ tui_gateway/hosted_room_driver.py:_fences
 * PoP: w28_hostedroom_3fa737_p2_info_active @ tui_gateway/hosted_room_driver.py:_info_active
 * PoP: w28_hostedroom_3fa737_p2_info_is_active_for @ tui_gateway/hosted_room_driver.py:_info_is_active_for
 * PoP: w28_hostedroom_3fa737_p2_peer_stop_acknowledged @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._peer_stop_acknowledged
 * PoP: w28_hostedroom_3fa737_p2_interrupt_ack @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._stop_outcome
 * PoP: w28_hostedroom_3fa737_p2_discard_guard @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._discard_unknown_stop
 * PoP: w28_hostedroom_3fa737_p2_discard_outcome @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._discard_unknown_stop
 * PoP: w28_hostedroom_3fa737_p2_stop_outcome @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._stop_outcome
 * PoP: w28_hostedroom_3fa737_p2_past_execution_deadline @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._past_execution_deadline
 * PoP: w28_hostedroom_3fa737_p2_defer_unconfirmed_guard @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._defer_unconfirmed_stop
 * PoP: w28_hostedroom_3fa737_p2_deadline_id @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._expire_attempt_deadline
 * PoP: w28_hostedroom_3fa737_p2_reconcile_deadline @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._reconcile_indeterminate
 * PoP: w28_hostedroom_3fa737_p2_retry_route @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime.retry_indeterminate
 * PoP: w28_hostedroom_3fa737_p3_retry_delay @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._defer_unavailable_route
 * PoP: w28_hostedroom_3fa737_p3_renew_decision @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._renew_lease_if_needed
 * PoP: w28_hostedroom_3fa737_p3_cache_lease_keep @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._cache_lease
 * PoP: w28_hostedroom_3fa737_p3_ambiguous_expiry @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._mark_ambiguous
 * PoP: w28_hostedroom_3fa737_p3_deadline_settlement @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._complete_acknowledged_stop
 * PoP: w28_hostedroom_3fa737_p3_cycle_plan @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._run_cycle
 * PoP: w28_hostedroom_3fa737_p3_approval_action @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime._report_pending_action
 * PoP: w28_hostedroom_3fa737_p3_validate_config @ tui_gateway/hosted_room_driver.py:HostedRoomRuntime.__init__
 *
 * Every seam stays in the caller: the store transitions in gateway.hosted_room_driver
 * (start/settle/requeue/defer/cancel/recover), the session transport (submit, info,
 * history, interrupt, discard, resume), the lease keeper thread, the supervisor's
 * threading.Event/Thread machinery and the logger.  Their observations enter here as
 * fixed fixture values; what this file owns is the arithmetic and the decisions those
 * values make.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w28_hostedroom_3fa737_p1.h"
#include "noc_w28_hostedroom_3fa737_p2.h"
#include "noc_w28_hostedroom_3fa737_p3.h"

#define HD_ROOM_SESSION_SOURCE "bot_room"
#define HD_MAX_TERMINAL_TEXT_BYTES (64 * 1024)
#define HD_ERROR_MAX_BYTES 4096

static const char HD_TRUNCATION_NOTICE[] =
    "\n\n[Reply truncated. Ask the Bot to share the full result as a file.]";

static const char *const HD_STOP_ACK_STATUSES[] = {"cancelled", "interrupted"};
static const char *const HD_SETTLED_FAILED[] = {"settled", "failed"};

/* ------------------------------------------------------------------ helpers */

static char *hd_dup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *hd_concat(const char *a, const char *b) {
    size_t na = a ? strlen(a) : 0, nb = b ? strlen(b) : 0;
    char *out = (char *)malloc(na + nb + 1);
    if (!out) return NULL;
    if (na) memcpy(out, a, na);
    if (nb) memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}

static char *hd_strip(const char *s) {
    size_t a = 0, b = strlen(s);
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' || s[b - 1] == '\r')) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static json_t *hd_obj_get(const json_t *o, const char *key) {
    if (!o || o->type != JSON_OBJECT) return NULL;
    return json_obj_get(o, key);
}

static int hd_contains_key(const json_t *o, const char *key) { return hd_obj_get(o, key) != NULL; }

/* Python truthiness of a JSON value (None/False/0/""/[]/{} are falsy). */
static int hd_truthy(const json_t *v) {
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

static int hd_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }

/* An integral JSON number: what json.loads produced as a Python int. */
static int hd_is_py_int(const json_t *v) {
    if (!v || v->type != JSON_NUMBER) return 0;
    return (double)(long long)v->num_val == v->num_val;
}

static long long hd_to_int(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING && v->str_val) return strtoll(v->str_val, NULL, 10);
    return 0;
}

static double hd_to_float(const json_t *v) {
    if (!v) return 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING && v->str_val) return strtod(v->str_val, NULL);
    return 0.0;
}

/* Python str() of a JSON value, for the few places the source coerces with str(). */
static char *hd_py_str(const json_t *v) {
    char buf[80];
    if (!v || v->type == JSON_NULL) return hd_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return hd_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return hd_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if ((double)(long long)v->num_val == v->num_val)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%.15g", v->num_val);
        return hd_dup(buf);
    default: return hd_dup("");
    }
}

/* str(value or "") -- the coercion _truncate_utf8 and _member_id perform. */
static char *hd_str_or_empty(const json_t *v) {
    if (!hd_truthy(v)) return hd_dup("");
    return hd_py_str(v);
}

/* Python ==: null matches only null, numbers compare by value, bool == 0/1 counts. */
static int hd_py_eq(const json_t *a, const json_t *b) {
    int an = hd_is_none(a), bn = hd_is_none(b);
    if (an || bn) return an && bn;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER)
        return ((b->num_val != 0.0) ? 1 : 0) == (a->bool_val ? 1 : 0);
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL)
        return ((a->num_val != 0.0) ? 1 : 0) == (b->bool_val ? 1 : 0);
    if (a->type != b->type) return 0;
    switch (a->type) {
    case JSON_BOOL:   return a->bool_val == b->bool_val;
    case JSON_NUMBER: return a->num_val == b->num_val;
    case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    default:          return 0; /* containers are compared by identity upstream too */
    }
}

static int hd_is_str(const json_t *v, const char *want) {
    return v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, want) == 0;
}

static int hd_name_in(const char *s, const char *const *names, size_t n) {
    if (!s) return 0;
    for (size_t i = 0; i < n; i++)
        if (strcmp(s, names[i]) == 0) return 1;
    return 0;
}

static int hd_in_names(const json_t *v, const char *const *names, size_t n) {
    return v && v->type == JSON_STRING && hd_name_in(v->str_val, names, n);
}

/* str(x or "").startswith("deadline:") */
static int hd_is_deadline_prefix(const json_t *v) {
    char *s = hd_str_or_empty(v);
    int ok = strncmp(s, "deadline:", 9) == 0;
    free(s);
    return ok;
}

static json_t *hd_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }

static char *hd_reply_id(const char *task_id, long long generation) {
    size_t n = strlen(task_id ? task_id : "") + 48;
    char *buf = (char *)malloc(n);
    if (!buf) return NULL;
    snprintf(buf, n, "reply:%s:%lld", task_id ? task_id : "", generation);
    return buf;
}

static const json_t *hd_identity(const json_t *task) { return hd_obj_get(task, "identity"); }

static const char *hd_task_id(const json_t *task) {
    const json_t *id = hd_obj_get(hd_identity(task), "task_id");
    return (id && id->type == JSON_STRING && id->str_val) ? id->str_val : "";
}

static long long hd_task_generation(const json_t *task) {
    return hd_to_int(hd_obj_get(task, "execution_generation"));
}

/* ================================================================== gate p1 */

/* Longest prefix of s whose bytes end on a complete UTF-8 sequence. */
static size_t hd_utf8_complete(const char *s, size_t n) {
    size_t i = 0, last = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        size_t len;
        if (c < 0x80) len = 1;
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        else return 0;
        if (i + len > n) break;
        i += len;
        last = i;
    }
    return last;
}

/* _truncate_utf8: cut to max_bytes including the notice, never splitting a code point. */
static void hd_truncate(const char *text, long long max_bytes, char **out_text, int *out_truncated) {
    size_t n = strlen(text);
    size_t notice = strlen(HD_TRUNCATION_NOTICE);
    long long avail;
    size_t limit = 0, keep;
    char *out;
    *out_truncated = 0;
    if ((long long)n <= max_bytes) {
        *out_text = hd_dup(text);
        return;
    }
    *out_truncated = 1;
    avail = max_bytes - (long long)notice;
    if (avail > 0) {
        limit = (size_t)avail;
        if (limit > n) limit = n;
    }
    keep = hd_utf8_complete(text, limit);
    if (keep == 0) {
        *out_text = hd_strip(HD_TRUNCATION_NOTICE);
        return;
    }
    out = (char *)malloc(keep + notice + 1);
    if (!out) { *out_text = hd_dup(""); return; }
    memcpy(out, text, keep);
    memcpy(out + keep, HD_TRUNCATION_NOTICE, notice + 1);
    *out_text = out;
}

json_t *w28_hostedroom_3fa737_p1_room_session_title(const char *room_id) {
    char *s = hd_concat("Group: ", room_id ? room_id : "");
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

json_t *w28_hostedroom_3fa737_p1_member_id(const json_t *task) {
    const json_t *payload = hd_obj_get(task, "payload");
    const json_t *v = NULL;
    char *s;
    json_t *out;
    if (payload && payload->type != JSON_OBJECT) payload = NULL;
    if (payload) {
        v = hd_obj_get(payload, "target_member_id");
        if (!hd_truthy(v)) v = hd_obj_get(payload, "target_profile");
    }
    s = hd_str_or_empty(v);
    out = json_string(s ? s : "");
    free(s);
    return out;
}

/* _session_id: {"ok": bool, "value": str|null, "error": str|null}. */
json_t *w28_hostedroom_3fa737_p1_session_id(const json_t *session) {
    const json_t *v = NULL;
    json_t *out = json_object();
    if (session && session->type == JSON_OBJECT)
        v = hd_contains_key(session, "session_id") ? hd_obj_get(session, "session_id")
                                                   : hd_obj_get(session, "id");
    if (v && v->type == JSON_STRING && v->str_val && v->str_val[0] != '\0') {
        json_set(out, "ok", json_bool(1));
        json_set(out, "value", json_string(v->str_val));
        json_set(out, "error", json_null());
    } else {
        json_set(out, "ok", json_bool(0));
        json_set(out, "value", json_null());
        json_set(out, "error", json_string("session adapter returned no session_id"));
    }
    return out;
}

json_t *w28_hostedroom_3fa737_p1_truncate_utf8(const char *text, long long max_bytes) {
    char *cut = NULL;
    int truncated = 0;
    json_t *out = json_object();
    hd_truncate(text ? text : "", max_bytes, &cut, &truncated);
    json_set(out, "text", json_string(cut ? cut : ""));
    json_set(out, "truncated", json_bool(truncated));
    free(cut);
    return out;
}

json_t *w28_hostedroom_3fa737_p1_bounded_result(const json_t *receipt) {
    char *text_in = hd_str_or_empty(hd_obj_get(receipt, "text"));
    char *error_in = hd_str_or_empty(hd_obj_get(receipt, "error"));
    char *text = NULL, *error = NULL;
    int text_truncated = 0, error_truncated = 0;
    json_t *out = json_object();
    hd_truncate(text_in, HD_MAX_TERMINAL_TEXT_BYTES, &text, &text_truncated);
    hd_truncate(error_in, HD_ERROR_MAX_BYTES, &error, &error_truncated);
    json_set(out, "message_id", hd_or_null(hd_obj_get(receipt, "message_id")));
    json_set(out, "text", json_string(text ? text : ""));
    if (error && error[0]) json_set(out, "error", json_string(error));
    if (text_truncated || error_truncated) json_set(out, "truncated", json_bool(1));
    free(text_in);
    free(error_in);
    free(text);
    free(error);
    return out;
}

json_t *w28_hostedroom_3fa737_p1_receipt_from(const json_t *receipt, const char *task_id,
                                              long long generation) {
    json_t *out = json_object();
    const json_t *settlement = hd_obj_get(receipt, "settlement_id");
    json_set(out, "status",
             json_string(hd_is_str(hd_obj_get(receipt, "status"), "settled") ? "settled" : "failed"));
    if (hd_truthy(settlement)) {
        json_set(out, "settlement_id", json_copy(settlement));
    } else {
        char *fallback = hd_reply_id(task_id, generation);
        json_set(out, "settlement_id", json_string(fallback ? fallback : ""));
        free(fallback);
    }
    json_set(out, "result", w28_hostedroom_3fa737_p1_bounded_result(receipt));
    return out;
}

json_t *w28_hostedroom_3fa737_p1_discarded_receipt(const json_t *result, const json_t *task) {
    if (!hd_in_names(hd_obj_get(result, "status"), HD_SETTLED_FAILED, 2)) return NULL;
    return w28_hostedroom_3fa737_p1_receipt_from(result, hd_task_id(task), hd_task_generation(task));
}

json_t *w28_hostedroom_3fa737_p1_find_terminal_receipt(const json_t *history, const char *task_id,
                                                       long long generation) {
    size_t n = json_len(history);
    for (size_t i = n; i-- > 0;) {
        const json_t *message = json_get(history, i);
        const json_t *mid, *gen, *status, *receipt_id, *content;
        int has_id, has_gen;
        json_t *material, *result, *out;
        char *fallback;
        if (!message || message->type != JSON_OBJECT) continue;
        mid = hd_obj_get(message, "task_id");
        gen = hd_obj_get(message, "execution_generation");
        has_id = mid && mid->type == JSON_STRING && mid->str_val &&
                 strcmp(mid->str_val, task_id ? task_id : "") == 0;
        has_gen = gen && gen->type == JSON_NUMBER && gen->num_val == (double)generation;
        if (!has_id || !has_gen) continue;
        if (!hd_is_str(hd_obj_get(message, "role"), "assistant")) continue;
        status = hd_obj_get(message, "status");
        if (!hd_in_names(status, HD_SETTLED_FAILED, 2)) continue;
        receipt_id = hd_obj_get(message, "message_id");
        content = hd_obj_get(message, "content");
        material = json_object();
        if (receipt_id && receipt_id->type == JSON_STRING && receipt_id->str_val &&
            receipt_id->str_val[0] != '\0') {
            fallback = hd_dup(receipt_id->str_val);
        } else {
            fallback = hd_reply_id(task_id, generation);
        }
        json_set(material, "message_id", json_string(fallback ? fallback : ""));
        if (content) json_set(material, "text", json_copy(content));
        else json_set(material, "text", json_string(""));
        result = w28_hostedroom_3fa737_p1_bounded_result(material);
        json_free(material);
        out = json_object();
        json_set(out, "status", json_copy(status));
        json_set(out, "settlement_id", json_string(fallback ? fallback : ""));
        json_set(out, "result", result);
        free(fallback);
        return out;
    }
    return NULL;
}

json_t *w28_hostedroom_3fa737_p1_session_kw(const char *profile, const char *session_id) {
    json_t *out = json_object();
    json_set(out, "profile", json_string(profile ? profile : ""));
    json_set(out, "session_id", json_string(session_id ? session_id : ""));
    json_set(out, "source", json_string(HD_ROOM_SESSION_SOURCE));
    return out;
}

json_t *w28_hostedroom_3fa737_p1_fences(const json_t *task) {
    json_t *out = json_object();
    json_set(out, "expected_execution_generation",
             json_number((double)hd_to_int(hd_obj_get(task, "execution_generation"))));
    json_set(out, "expected_cancel_generation",
             json_number((double)hd_to_int(hd_obj_get(task, "cancel_generation"))));
    return out;
}

/* ================================================================== gate p2 */

json_t *w28_hostedroom_3fa737_p2_info_active(const json_t *info) {
    const json_t *v;
    if (!info || info->type != JSON_OBJECT) return json_bool(0);
    v = hd_contains_key(info, "active") ? hd_obj_get(info, "active") : hd_obj_get(info, "running");
    return json_bool(hd_truthy(v));
}

json_t *w28_hostedroom_3fa737_p2_info_is_active_for(const json_t *info, const char *task_id,
                                                    int require_exact) {
    const json_t *v;
    int accepted;
    if (!hd_truthy(w28_hostedroom_3fa737_p2_info_active(info))) return json_bool(0);
    v = hd_obj_get(info, "task_id");
    if (hd_is_none(v)) {
        accepted = require_exact ? 0 : 1; /* None is accepted only when not requiring exact */
    } else {
        accepted = v->type == JSON_STRING && v->str_val &&
                   strcmp(v->str_val, task_id ? task_id : "") == 0;
    }
    return json_bool(accepted);
}

json_t *w28_hostedroom_3fa737_p2_peer_stop_acknowledged(const json_t *info, const json_t *task,
                                                       int has_session) {
    char *status = hd_str_or_empty(hd_obj_get(info, "status"));
    char *info_task = hd_str_or_empty(hd_obj_get(info, "task_id"));
    int ack = 0;
    if (has_session && !hd_truthy(w28_hostedroom_3fa737_p2_info_active(info)) &&
        hd_name_in(status, HD_STOP_ACK_STATUSES, 2) &&
        strcmp(info_task, hd_task_id(task)) == 0 &&
        hd_to_int(hd_obj_get(info, "execution_generation")) == hd_task_generation(task))
        ack = 1;
    free(status);
    free(info_task);
    return json_bool(ack);
}

json_t *w28_hostedroom_3fa737_p2_interrupt_ack(const json_t *result) {
    const json_t *interrupted;
    char *status;
    int ack;
    if (!result || result->type != JSON_OBJECT) return json_bool(0);
    interrupted = hd_obj_get(result, "interrupted");
    if (interrupted && interrupted->type == JSON_BOOL && interrupted->bool_val) return json_bool(1);
    status = hd_str_or_empty(hd_obj_get(result, "status"));
    ack = hd_name_in(status, HD_STOP_ACK_STATUSES, 2);
    free(status);
    return json_bool(ack);
}

json_t *w28_hostedroom_3fa737_p2_discard_guard(const json_t *info, const json_t *task) {
    const json_t *mid, *gen;
    if (hd_truthy(w28_hostedroom_3fa737_p2_info_active(info))) return json_bool(0);
    mid = hd_obj_get(info, "task_id");
    if (!(mid && mid->type == JSON_STRING && mid->str_val &&
          strcmp(mid->str_val, hd_task_id(task)) == 0))
        return json_bool(0);
    gen = hd_obj_get(info, "execution_generation");
    if (!hd_is_py_int(gen)) return json_bool(0);
    return json_bool(hd_to_int(gen) == hd_task_generation(task));
}

json_t *w28_hostedroom_3fa737_p2_discard_outcome(const json_t *result, const json_t *task) {
    json_t *out = json_object();
    const json_t *discarded = hd_obj_get(result, "discarded");
    const json_t *mid, *gen;
    int usable = 0;
    json_t *receipt = NULL;
    if (hd_truthy(result) && result->type == JSON_OBJECT && discarded &&
        discarded->type == JSON_BOOL && discarded->bool_val) {
        mid = hd_obj_get(result, "task_id");
        gen = hd_obj_get(result, "execution_generation");
        if (mid && mid->type == JSON_STRING && mid->str_val &&
            strcmp(mid->str_val, hd_task_id(task)) == 0 && hd_is_py_int(gen) &&
            hd_to_int(gen) == hd_task_generation(task))
            usable = 1;
    }
    json_set(out, "usable", json_bool(usable));
    if (!usable) {
        json_set(out, "outcome", json_string("none"));
    } else {
        receipt = w28_hostedroom_3fa737_p1_discarded_receipt(result, task);
        if (receipt) json_set(out, "outcome", json_string("receipt"));
        else if (hd_is_str(hd_obj_get(result, "status"), "cancelled"))
            json_set(out, "outcome", json_string("cancelled"));
        else json_set(out, "outcome", json_string("none"));
    }
    json_set(out, "receipt", receipt ? receipt : json_null());
    return out;
}

json_t *w28_hostedroom_3fa737_p2_stop_outcome(const json_t *spec) {
    const json_t *task = hd_obj_get(spec, "task");
    const json_t *info = hd_obj_get(spec, "info");
    int local = hd_truthy(hd_obj_get(spec, "local"));
    int has_session = hd_truthy(hd_obj_get(spec, "has_session"));
    int has_discard = hd_truthy(hd_obj_get(spec, "has_discard"));
    const char *outcome = "nack";
    json_t *receipt = NULL;
    json_t *out;
    if (!has_session) {
        outcome = local ? "ack" : "nack";
    } else if (hd_is_str(hd_obj_get(info, "status"), "unknown") &&
               hd_is_str(hd_obj_get(task, "status"), "stopping") && has_discard) {
        if (!hd_truthy(w28_hostedroom_3fa737_p2_discard_guard(info, task))) {
            outcome = "nack";
        } else {
            json_t *decision =
                w28_hostedroom_3fa737_p2_discard_outcome(hd_obj_get(spec, "discard_result"), task);
            const char *kind = json_get_str(decision, "outcome", "none");
            if (kind && strcmp(kind, "receipt") == 0) {
                outcome = "receipt";
                receipt = hd_or_null(hd_obj_get(decision, "receipt"));
            } else if (kind && strcmp(kind, "cancelled") == 0) {
                outcome = "ack";
            } else {
                outcome = "nack";
            }
            json_free(decision);
        }
    } else if (!hd_truthy(w28_hostedroom_3fa737_p2_info_active(info))) {
        outcome = "ack";
    } else if (!hd_truthy(w28_hostedroom_3fa737_p2_info_is_active_for(info, hd_task_id(task), 1))) {
        outcome = "nack"; /* stale admission: interrupted under its own id, Stop stays pending */
    } else {
        outcome = hd_truthy(w28_hostedroom_3fa737_p2_interrupt_ack(hd_obj_get(spec, "interrupt_result")))
                      ? "ack"
                      : "nack";
    }
    out = json_object();
    json_set(out, "outcome", json_string(outcome));
    json_set(out, "receipt", receipt ? receipt : json_null());
    return out;
}

json_t *w28_hostedroom_3fa737_p2_past_execution_deadline(const json_t *task, double now,
                                                         double timeout) {
    const json_t *started = hd_obj_get(task, "started_at");
    if (hd_is_none(started)) return json_bool(0);
    return json_bool(now >= hd_to_float(started) + timeout);
}

json_t *w28_hostedroom_3fa737_p2_defer_unconfirmed_guard(const json_t *task, double now,
                                                         double defer_seconds,
                                                         int served_elsewhere) {
    double since;
    if (!served_elsewhere) return json_bool(0);
    since = hd_to_float(hd_obj_get(task, "updated_at"));
    return json_bool(now >= since + defer_seconds);
}

json_t *w28_hostedroom_3fa737_p2_deadline_id(const json_t *task) {
    size_t n = 40;
    char *buf = (char *)malloc(n);
    json_t *out;
    if (buf) snprintf(buf, n, "deadline:%lld", hd_task_generation(task));
    out = json_string(buf ? buf : "deadline:0");
    free(buf);
    return out;
}

json_t *w28_hostedroom_3fa737_p2_reconcile_deadline(const json_t *task, double now,
                                                    double defer_seconds) {
    const json_t *v = hd_obj_get(task, "indeterminate_at");
    double base;
    if (!hd_truthy(v)) v = hd_obj_get(task, "updated_at");
    if (!hd_truthy(v)) v = hd_obj_get(task, "created_at");
    base = hd_truthy(v) ? hd_to_float(v) : now;
    return json_number(defer_seconds + base);
}

json_t *w28_hostedroom_3fa737_p2_retry_route(const json_t *task,
                                             const json_t *requested_generation,
                                             int has_binding) {
    const json_t *status = hd_obj_get(task, "status");
    if (!hd_is_none(requested_generation) &&
        hd_to_int(requested_generation) != hd_task_generation(task))
        return json_string("stale");
    if (!hd_is_str(status, "indeterminate") && !hd_is_str(status, "deferred"))
        return json_string("invalid");
    if (!has_binding) return json_string("unavailable");
    return json_string(hd_is_str(status, "deferred") ? "deferred" : "indeterminate");
}

/* ================================================================== gate p3 */

json_t *w28_hostedroom_3fa737_p3_retry_delay(const json_t *previous, double lo, double hi,
                                             double now) {
    double delay;
    json_t *out = json_object();
    if (!previous || previous->type != JSON_OBJECT) {
        delay = lo;
    } else {
        double prior = hd_to_float(hd_obj_get(previous, "delay")) * 2.0;
        delay = prior < lo ? lo : (prior > hi ? hi : prior);
    }
    json_set(out, "delay", json_number(delay));
    json_set(out, "next_attempt_at", json_number(now + delay));
    return out;
}

json_t *w28_hostedroom_3fa737_p3_renew_decision(const json_t *lease, const json_t *known,
                                                double now, double ttl, int force) {
    json_t *out = json_object();
    double expires = hd_to_float(hd_obj_get(lease, "expires_at"));
    long long generation = hd_to_int(hd_obj_get(lease, "lease_generation"));
    if (known && known->type == JSON_OBJECT &&
        hd_to_int(hd_obj_get(known, "lease_generation")) == generation &&
        hd_to_float(hd_obj_get(known, "expires_at")) > expires) {
        expires = hd_to_float(hd_obj_get(known, "expires_at"));
    }
    json_set(out, "renewed", json_bool(force || !(now < expires - (ttl / 2.0))));
    return out;
}

json_t *w28_hostedroom_3fa737_p3_cache_lease_keep(const json_t *known, const json_t *incoming) {
    json_t *out = json_object();
    int keep = 0;
    if (known && known->type == JSON_OBJECT) {
        int same_generation =
            hd_to_int(hd_obj_get(known, "lease_generation")) ==
            hd_to_int(hd_obj_get(incoming, "lease_generation"));
        int same_process = hd_py_eq(hd_obj_get(known, "process_generation"),
                                    hd_obj_get(incoming, "process_generation"));
        double known_expires = hd_to_float(hd_obj_get(known, "expires_at"));
        double incoming_expires = hd_to_float(hd_obj_get(incoming, "expires_at"));
        keep = same_generation && same_process && known_expires >= incoming_expires;
    }
    json_set(out, "keep_known", json_bool(keep));
    json_set(out, "expires_at",
             json_number(keep ? hd_to_float(hd_obj_get(known, "expires_at"))
                              : hd_to_float(hd_obj_get(incoming, "expires_at"))));
    return out;
}

json_t *w28_hostedroom_3fa737_p3_ambiguous_expiry(double attempt_expires_at, const json_t *known,
                                                  int attempt_generation) {
    double expires = attempt_expires_at;
    if (known && known->type == JSON_OBJECT &&
        hd_to_int(hd_obj_get(known, "lease_generation")) == attempt_generation) {
        double known_expires = hd_to_float(hd_obj_get(known, "expires_at"));
        if (known_expires > expires) expires = known_expires;
    }
    return json_number(expires);
}

json_t *w28_hostedroom_3fa737_p3_deadline_settlement(const json_t *task, double timeout) {
    json_t *out = json_object();
    if (!hd_is_deadline_prefix(hd_obj_get(task, "cancel_id"))) {
        json_set(out, "branch", json_string("cancel"));
        json_set(out, "cancel_id", hd_or_null(hd_obj_get(task, "cancel_id")));
        json_set(out, "expected_cancel_generation",
                 json_number((double)hd_to_int(hd_obj_get(task, "cancel_generation"))));
    } else {
        size_t n = 40;
        char *buf = (char *)malloc(n);
        json_t *result = json_object();
        if (buf) snprintf(buf, n, "deadline:%lld", hd_task_generation(task));
        json_set(out, "branch", json_string("deadline"));
        json_set(out, "settlement_id", json_string(buf ? buf : "deadline:0"));
        json_set(out, "status", json_string("failed"));
        json_set(result, "error",
                 json_string("This Group Chat turn exceeded its configured time limit and was stopped."));
        json_set(result, "reason_code", json_string("turn_deadline_exceeded"));
        json_set(result, "timeout_seconds", json_number(timeout));
        json_set(out, "result", result);
        free(buf);
    }
    return out;
}

/* The supervisor's rotating window: one cursor step per cycle, then a bounded fan-out
 * over the rooms that are not already running.  "chosen" is reported sorted (thread
 * creation order is not observable); "next_cursor" is where the cursor lands. */
json_t *w28_hostedroom_3fa737_p3_cycle_plan(long long cursor, const json_t *rooms,
                                            const json_t *active, long long max_concurrent,
                                            int stop) {
    json_t *out = json_object();
    json_t *chosen = json_array();
    size_t n = json_len(rooms);
    long long available = max_concurrent - (long long)json_len(active);
    long long start, next;
    const char **picked;
    size_t picked_count = 0;
    if (n == 0 || available <= 0) {
        json_set(out, "next_cursor", json_number((double)cursor));
        json_set(out, "chosen", chosen);
        return out;
    }
    start = cursor % (long long)n;
    if (start < 0) start += (long long)n;
    next = (start + 1) % (long long)n;
    picked = (const char **)calloc(n, sizeof *picked);
    for (size_t i = 0; i < n && picked; i++) {
        const json_t *room = json_get(rooms, (size_t)((start + (long long)i) % (long long)n));
        int is_active = 0;
        if (stop || available <= 0) break;
        for (size_t j = 0; j < json_len(active); j++) {
            if (hd_py_eq(json_get(active, j), room)) { is_active = 1; break; }
        }
        if (is_active) continue;
        if (room && room->type == JSON_STRING && room->str_val) picked[picked_count++] = room->str_val;
        available -= 1;
    }
    for (size_t i = 0; picked && i + 1 < picked_count; i++) { /* insertion sort */
        for (size_t j = i + 1; j < picked_count; j++) {
            if (strcmp(picked[j], picked[i]) < 0) {
                const char *tmp = picked[i];
                picked[i] = picked[j];
                picked[j] = tmp;
            }
        }
    }
    for (size_t i = 0; i < picked_count; i++) json_append(chosen, json_string(picked[i]));
    free(picked);
    json_set(out, "next_cursor", json_number((double)next));
    json_set(out, "chosen", chosen);
    return out;
}

static int hd_choice_allowed(const json_t *v) {
    return hd_is_str(v, "once") || hd_is_str(v, "deny");
}

json_t *w28_hostedroom_3fa737_p3_approval_action(const json_t *spec) {
    const json_t *info = hd_obj_get(spec, "info");
    const json_t *task = hd_obj_get(spec, "task");
    const json_t *session = hd_obj_get(spec, "session_id");
    const json_t *approval = hd_obj_get(info, "pending_approval");
    const json_t *room = hd_obj_get(hd_identity(task), "room_id");
    json_t *out = json_object();
    json_t *action = NULL;
    int called = hd_truthy(hd_obj_get(spec, "has_pending"));
    if (!hd_truthy(approval)) approval = hd_obj_get(info, "approval");
    if (called && approval && approval->type == JSON_OBJECT) {
        json_t *safe = json_copy(approval);
        json_t *choices = json_array();
        const json_t *raw = hd_obj_get(approval, "choices");
        if (raw && raw->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(raw); i++) {
                const json_t *c = json_get(raw, i);
                if (hd_choice_allowed(c)) json_append(choices, json_copy(c));
            }
        }
        if (json_len(choices) == 0) {
            json_append(choices, json_string("once"));
            json_append(choices, json_string("deny"));
        }
        json_set(safe, "choices", choices);
        action = json_object();
        json_set(action, "kind", json_string("approval"));
        json_set(action, "task_id", json_string(hd_task_id(task)));
        json_set(action, "execution_generation", json_number((double)hd_task_generation(task)));
        json_set(action, "run_id", hd_or_null(hd_obj_get(info, "run_id")));
        json_set(action, "session_id", hd_or_null(session));
        json_set(action, "request_id", hd_or_null(hd_obj_get(safe, "request_id")));
        json_set(action, "approval", safe);
    }
    json_set(out, "called", json_bool(called));
    json_set(out, "room_id", room ? json_copy(room) : json_null());
    json_set(out, "member_id", w28_hostedroom_3fa737_p1_member_id(task));
    json_set(out, "action", action ? action : json_null());
    return out;
}

static const char *const HD_POSITIVE_PARAMS[] = {
    "lease_ttl_seconds", "poll_interval_seconds", "active_poll_interval_seconds",
    "turn_timeout_seconds", "indeterminate_defer_seconds",
};

json_t *w28_hostedroom_3fa737_p3_validate_config(const json_t *cfg) {
    json_t *out = json_object();
    const char *error = NULL;
    char *message = NULL;
    for (size_t i = 0; i < sizeof HD_POSITIVE_PARAMS / sizeof HD_POSITIVE_PARAMS[0]; i++) {
        double value = hd_to_float(hd_obj_get(cfg, HD_POSITIVE_PARAMS[i]));
        if (value <= 0.0) {
            message = hd_concat(HD_POSITIVE_PARAMS[i], " must be positive");
            error = message;
            break;
        }
    }
    if (!error) {
        const json_t *concurrent = hd_obj_get(cfg, "max_concurrent_rooms");
        if (!hd_is_py_int(concurrent) || hd_to_int(concurrent) < 1)
            error = "max_concurrent_rooms must be a positive integer";
    }
    if (!error) {
        double lo = hd_to_float(hd_obj_get(cfg, "unavailable_retry_min_seconds"));
        double hi = hd_to_float(hd_obj_get(cfg, "unavailable_retry_max_seconds"));
        if (!(0.0 < lo && lo <= hi)) error = "unavailable retry bounds are invalid";
    }
    if (!error) {
        int has_rpc = hd_truthy(hd_obj_get(cfg, "has_rpc"));
        int has_resolver = hd_truthy(hd_obj_get(cfg, "has_transport_resolver"));
        if (!has_rpc && !has_resolver) error = "rpc or transport_resolver is required";
    }
    json_set(out, "ok", json_bool(error == NULL));
    json_set(out, "error", error ? json_string(error) : json_null());
    free(message);
    return out;
}
