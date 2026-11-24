/*
 * hosted_room_peer_transport.c - C home for tui_gateway/hosted_room_peer_transport.py.
 * Peer-backed room member transport: stable route identity, the fenced member
 * dispatch payload (with its prompt digest), the failover link ordering, and the
 * admission guards. The transport/send seams are injected as decision inputs.
 * PoP: w19_hostedroom_e90a2b_route_identity @ tui_gateway/hosted_room_peer_transport.py:route_identity
 * PoP: w19_hostedroom_e90a2b_build_member_dispatch @ tui_gateway/hosted_room_peer_transport.py:build_member_dispatch
 * PoP: w19_hostedroom_e90a2b_failover_order @ tui_gateway/hosted_room_peer_transport.py:FailoverHostedRoomPeerClient._call
 * PoP: w19_hostedroom_e90a2b_failover_call @ tui_gateway/hosted_room_peer_transport.py:FailoverHostedRoomPeerClient._call
 * PoP: w19_hostedroom_e90a2b_validate_coordinates @ tui_gateway/hosted_room_peer_transport.py:PeerHostedRoomTransport._validate_coordinates
 * PoP: w19_hostedroom_e90a2b_submit_guard @ tui_gateway/hosted_room_peer_transport.py:PeerHostedRoomTransport.submit
 * PoP: w19_hostedroom_e90a2b_interrupt_decision @ tui_gateway/hosted_room_peer_transport.py:PeerHostedRoomTransport.interrupt
 * PoP: w19_hostedroom_e90a2b_validate_source_event_seq @ tui_gateway/hosted_room_peer_transport.py:PeerHostedRoomTransport.__init__
 * PoP: w19_hostedroom_e90a2b_bind_scope_args @ tui_gateway/hosted_room_peer_transport.py:PeerHostedRoomTransport.__init__
 */
#include "noc_w19_hostedroom_e90a2b.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HR_PROTOCOL_VERSION 2
#define HR_ROOM_SESSION_SOURCE "bot_room"

static char *hr_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static int hr_truthy(const json_t *v) {
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

static char *hr_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return hr_dup("None");
    switch (v->type) {
        case JSON_BOOL: return hr_dup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: snprintf(buf, sizeof(buf), "%.17g", v->num_val); return hr_dup(buf);
        case JSON_STRING: return hr_dup(v->str_val ? v->str_val : "");
        default: return hr_dup("");
    }
}

/* ---------------- SHA-256 ---------------- */

typedef struct {
    uint32_t h[8];
    uint64_t bits;
    unsigned char buf[64];
    size_t used;
} hr_sha256;

static const uint32_t HR_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u
};

#define HR_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void hr_sha256_block(hr_sha256 *s, const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h;
    size_t i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16)
             | ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = HR_ROTR(w[i - 15], 7) ^ HR_ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = HR_ROTR(w[i - 2], 17) ^ HR_ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = HR_ROTR(e, 6) ^ HR_ROTR(e, 11) ^ HR_ROTR(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + HR_K[i] + w[i];
        uint32_t S0 = HR_ROTR(a, 2) ^ HR_ROTR(a, 13) ^ HR_ROTR(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
    s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void hr_sha256_init(hr_sha256 *s) {
    s->h[0] = 0x6a09e667u; s->h[1] = 0xbb67ae85u; s->h[2] = 0x3c6ef372u; s->h[3] = 0xa54ff53au;
    s->h[4] = 0x510e527fu; s->h[5] = 0x9b05688cu; s->h[6] = 0x1f83d9abu; s->h[7] = 0x5be0cd19u;
    s->bits = 0;
    s->used = 0;
}

static void hr_sha256_update(hr_sha256 *s, const unsigned char *p, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        s->buf[s->used++] = p[i];
        if (s->used == 64) {
            hr_sha256_block(s, s->buf);
            s->bits += 512;
            s->used = 0;
        }
    }
}

static char *hr_sha256_hex(const unsigned char *data, size_t n, int truncate32) {
    hr_sha256 s;
    unsigned char pad[72];
    size_t padlen;
    char *out;
    int i;
    hr_sha256_init(&s);
    hr_sha256_update(&s, data, n);
    s.bits += (uint64_t)s.used * 8u;
    padlen = (s.used < 56) ? (56 - s.used) : (120 - s.used);
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    hr_sha256_update(&s, pad, padlen);
    {
        unsigned char lenbuf[8];
        for (i = 0; i < 8; i++) lenbuf[i] = (unsigned char)(s.bits >> (56 - 8 * i));
        hr_sha256_update(&s, lenbuf, 8);
    }
    out = (char *)malloc(65);
    if (!out) return hr_dup("");
    for (i = 0; i < 8; i++) snprintf(out + i * 8, 9, "%08x", s.h[i]);
    out[64] = '\0';
    if (truncate32) out[32] = '\0';
    return out;
}

/* ---------------- route_identity ---------------- */

json_t *w19_hostedroom_e90a2b_route_identity(json_t *stored_rows, const char *home_install_id,
                                             const char *room_id, const char *member_id) {
    json_t *out = json_array();
    const char *room = room_id ? room_id : "";
    const char *member = member_id ? member_id : "";
    size_t i;
    if (stored_rows && stored_rows->type == JSON_ARRAY) {
        for (i = 0; i < json_len(stored_rows); i++) {
            json_t *row = json_get(stored_rows, i);
            const char *r_room, *r_member, *scope, *trace;
            if (!row || row->type != JSON_OBJECT) continue;
            r_room = json_get_str(row, "room_id", NULL);
            r_member = json_get_str(row, "member_id", NULL);
            if (!r_room || !r_member) continue;
            if (strcmp(r_room, room) != 0 || strcmp(r_member, member) != 0) continue;
            scope = json_get_str(row, "cancellation_scope_id", NULL);
            trace = json_get_str(row, "trace_id", NULL);
            if (hr_truthy(json_obj_get(row, "cancellation_scope_id"))
                && hr_truthy(json_obj_get(row, "trace_id"))) {
                char *s1 = hr_str(json_obj_get(row, "cancellation_scope_id"));
                char *s2 = hr_str(json_obj_get(row, "trace_id"));
                json_append(out, json_string(s1 ? s1 : ""));
                json_append(out, json_string(s2 ? s2 : ""));
                free(s1);
                free(s2);
                return out;
            }
            (void)scope;
            (void)trace;
            break;   /* the first matching row is the stored one */
        }
    }
    {
        const char *home = home_install_id ? home_install_id : "";
        size_t hn = strlen(home), rn = strlen(room), mn = strlen(member);
        unsigned char *seed = (unsigned char *)malloc(hn + rn + mn + 3);
        char *digest;
        char buf[512];
        size_t at = 0;
        if (!seed) { json_append(out, json_string("")); json_append(out, json_string("")); return out; }
        memcpy(seed + at, home, hn); at += hn; seed[at++] = 0;   /* "\0".join(...) */
        memcpy(seed + at, room, rn); at += rn; seed[at++] = 0;
        memcpy(seed + at, member, mn); at += mn;
        digest = hr_sha256_hex(seed, at, 1);
        snprintf(buf, sizeof(buf), "cancel-%s", room);
        json_append(out, json_string(buf));
        snprintf(buf, sizeof(buf), "trace-%s", digest ? digest : "");
        json_append(out, json_string(buf));
        free(digest);
        free(seed);
    }
    return out;
}

/* ---------------- build_member_dispatch ---------------- */

json_t *w19_hostedroom_e90a2b_build_member_dispatch(json_t *binding, json_t *route, json_t *args) {
    json_t *out = json_object();
    const char *room_id, *home, *member, *target_install, *profile, *task_id, *prompt;
    const char *capability, *policy, *trace, *gateway_id;
    double epoch = 0, generation = 0, seq = 0;
    char *digest;
    if (!binding || binding->type != JSON_OBJECT) binding = json_object();
    if (!route || route->type != JSON_OBJECT) route = json_object();
    if (!args || args->type != JSON_OBJECT) args = json_object();
    room_id = json_get_str(args, "room_id", "");
    home = json_get_str(route, "home_install_id", "");
    member = json_get_str(route, "member_id", "");
    target_install = json_get_str(route, "target_install_id", "");
    capability = json_get_str(route, "capability_digest", "");
    policy = json_get_str(route, "execution_policy_digest", "");
    trace = json_get_str(args, "trace_id", "");
    gateway_id = json_get_str(binding, "gateway_id", "");
    epoch = json_get_num(binding, "authority_epoch", 0);
    profile = json_get_str(args, "target_profile", "");
    task_id = json_get_str(args, "task_id", "");
    prompt = json_get_str(args, "prompt", "");
    generation = json_get_num(args, "execution_generation", 0);
    seq = json_get_num(args, "source_event_seq", 0);

    digest = hr_sha256_hex((const unsigned char *)(prompt ? prompt : ""), strlen(prompt ? prompt : ""), 0);
    json_set(out, "protocol_version", json_number(HR_PROTOCOL_VERSION));
    json_set(out, "room_id", json_string(room_id ? room_id : ""));
    json_set(out, "home_install_id", json_string(home ? home : ""));
    json_set(out, "authority_gateway_id", json_string(gateway_id ? gateway_id : ""));
    json_set(out, "authority_epoch", json_number(epoch));
    json_set(out, "member_id", json_string(member ? member : ""));
    json_set(out, "target_install_id", json_string(target_install ? target_install : ""));
    json_set(out, "target_profile", json_string(profile ? profile : ""));
    json_set(out, "task_id", json_string(task_id ? task_id : ""));
    json_set(out, "execution_generation", json_number(generation));
    json_set(out, "source_event_seq", json_number(seq));
    json_set(out, "cancellation_scope_id", json_string(json_get_str(route, "cancellation_scope_id", "")));
    json_set(out, "prompt", json_string(prompt ? prompt : ""));
    json_set(out, "prompt_digest", json_string(digest ? digest : ""));
    json_set(out, "capability_digest", json_string(capability ? capability : ""));
    json_set(out, "execution_policy_digest", json_string(policy ? policy : ""));
    json_set(out, "trace_id", json_string(trace ? trace : ""));
    free(digest);
    return out;
}

/* ---------------- failover ---------------- */

/* _call's candidate order: active first, the primary re-probed after a cooldown. */
static json_t *hr_order(size_t active, double now, double last_probe, double interval, size_t n) {
    json_t *order = json_array();
    size_t i;
    if (active != 0 && (now - last_probe) >= interval) {
        json_append(order, json_number(0));
        json_append(order, json_number((double)active));
    } else {
        json_append(order, json_number((double)active));
    }
    for (i = 0; i < n; i++) {
        int present = 0;
        size_t k;
        for (k = 0; k < json_len(order); k++)
            if ((size_t)json_number_value(json_get(order, k)) == i) { present = 1; break; }
        if (!present) json_append(order, json_number((double)i));
    }
    return order;
}

json_t *w19_hostedroom_e90a2b_failover_order(json_t *spec) {
    size_t active = 0, n = 1;
    double now, last_probe, interval;
    if (!spec || spec->type != JSON_OBJECT) spec = json_object();
    {
        json_t *a = json_obj_get(spec, "active");
        json_t *nn = json_obj_get(spec, "candidates");
        if (a && a->type == JSON_NUMBER) active = (size_t)a->num_val;
        if (nn && nn->type == JSON_NUMBER) n = (size_t)nn->num_val;
    }
    now = json_get_num(spec, "now", 0);
    last_probe = json_get_num(spec, "last_probe", 0);
    interval = json_get_num(spec, "interval", 60);
    return hr_order(active, now, last_probe, interval, n);
}

json_t *w19_hostedroom_e90a2b_failover_call(json_t *spec) {
    json_t *out = json_object();
    json_t *order, *outcomes, *attempts;
    size_t active = 0, n = 1, i;
    double now, last_probe, interval;
    long result_index = -1;
    const char *raised = NULL;
    int ambiguous = 0, non_retryable = 0, retryable_seen = 0;
    if (!spec || spec->type != JSON_OBJECT) spec = json_object();
    {
        json_t *a = json_obj_get(spec, "active");
        json_t *nn = json_obj_get(spec, "candidates");
        if (a && a->type == JSON_NUMBER) active = (size_t)a->num_val;
        if (nn && nn->type == JSON_NUMBER) n = (size_t)nn->num_val;
    }
    now = json_get_num(spec, "now", 0);
    last_probe = json_get_num(spec, "last_probe", 0);
    interval = json_get_num(spec, "interval", 60);
    outcomes = json_obj_get(spec, "outcomes");
    if ((!json_obj_get(spec, "candidates")) && outcomes && outcomes->type == JSON_ARRAY)
        n = json_len(outcomes);
    order = hr_order(active, now, last_probe, interval, n);
    attempts = json_array();
    for (i = 0; i < json_len(order); i++) {
        size_t idx = (size_t)json_number_value(json_get(order, i));
        json_t *oc = (outcomes && (size_t)idx < json_len(outcomes)) ? json_get(outcomes, idx) : NULL;
        int raises = hr_truthy(json_obj_get((json_t *)oc, "raises"));
        json_append(attempts, json_number((double)idx));
        if (!raises) {
            result_index = (long)idx;
            break;
        }
        if (hr_truthy(json_obj_get((json_t *)oc, "ambiguous"))) { ambiguous = 1; raised = "ambiguous"; break; }
        if (!hr_truthy(json_obj_get((json_t *)oc, "retryable"))) {
            non_retryable = 1; raised = "non_retryable"; break;
        }
        retryable_seen = 1;
    }
    if (result_index < 0 && !ambiguous && !non_retryable) {
        if (retryable_seen) {
            /* every candidate refused retryably: raise the last error */
            size_t last = (size_t)json_number_value(json_get(order, json_len(order) - 1));
            json_t *oc = (outcomes && last < json_len(outcomes)) ? json_get(outcomes, last) : NULL;
            raised = hr_truthy(json_obj_get((json_t *)oc, "ambiguous")) ? "ambiguous" : "non_retryable";
            if (hr_truthy(json_obj_get((json_t *)oc, "retryable")))
                raised = "link failure";
        } else {
            raised = "no_candidate";
        }
    }
    if (result_index >= 0) active = (size_t)result_index;
    json_set(out, "order", attempts);
    json_set(out, "active", json_number((double)active));
    json_set(out, "result_index", result_index >= 0 ? json_number((double)result_index) : json_null());
    json_set(out, "raised", raised ? json_string(raised) : json_null());
    return out;
}

/* ---------------- admission guards ---------------- */

json_t *w19_hostedroom_e90a2b_validate_coordinates(const char *profile, const char *source,
                                                   json_t *title, const char *route_profile,
                                                   const char *room_id) {
    if (!source || strcmp(source, HR_ROOM_SESSION_SOURCE) != 0)
        return json_string("peer room transport requires source=bot_room");
    if (!profile || !route_profile || strcmp(profile, route_profile) != 0)
        return json_string("peer room transport profile does not match its grant");
    if (title && title->type != JSON_NULL && title->type == JSON_STRING) {
        char buf[512];
        snprintf(buf, sizeof(buf), "Group: %s", room_id ? room_id : "");
        if (strcmp(title->str_val ? title->str_val : "", buf) != 0)
            return json_string("peer room transport title does not match room identity");
    }
    return json_null();
}

json_t *w19_hostedroom_e90a2b_submit_guard(const char *profile, const char *source,
                                           const char *route_profile, const char *session_id,
                                           json_t *member_id, const char *route_member_id,
                                           json_t *self_session_id, int attachments_present) {
    json_t *title = json_null();
    json_t *guard;
    const char *self_sid = (self_session_id && self_session_id->type == JSON_STRING)
                         ? self_session_id->str_val : NULL;
    if (attachments_present)
        return json_string("attachments_unsupported_on_route: peer route cannot carry files");
    if (member_id && member_id->type != JSON_NULL && member_id->type == JSON_STRING) {
        if (strcmp(member_id->str_val ? member_id->str_val : "", route_member_id ? route_member_id : "") != 0)
            return json_string("peer room transport member does not match its grant");
    }
    guard = w19_hostedroom_e90a2b_validate_coordinates(profile, source, title, route_profile, "");
    json_free(title);
    if (guard->type == JSON_STRING) return guard;
    json_free(guard);
    if (self_sid != NULL && strcmp(self_sid, session_id ? session_id : "") != 0)
        return json_string("peer room session changed during admission");
    return json_null();
}

json_t *w19_hostedroom_e90a2b_interrupt_decision(json_t *dispatch_task_id,
                                                 const char *expected_task_id,
                                                 json_t *task_id, json_t *execution_generation,
                                                 int has_stop_receipt) {
    const char *expected = expected_task_id ? expected_task_id : "";
    if (dispatch_task_id && dispatch_task_id->type != JSON_NULL) {
        const char *dt = (dispatch_task_id->type == JSON_STRING) ? dispatch_task_id->str_val : NULL;
        if (!dt || strcmp(dt, expected) != 0) return json_string("none");
        return json_string("stop");
    }
    if (task_id && task_id->type == JSON_STRING && task_id->str_val
        && strcmp(task_id->str_val, expected) != 0)
        return json_string("none");
    if (!hr_truthy(execution_generation)) return json_string("none");
    if (!has_stop_receipt) return json_string("none");
    return json_string("stop_receipt");
}

json_t *w19_hostedroom_e90a2b_validate_source_event_seq(json_t *source_event_seq) {
    if (source_event_seq
        && ((source_event_seq->type == JSON_BOOL)
            || (source_event_seq->type == JSON_NUMBER && source_event_seq->num_val < 1)))
        return json_string("peer room source_event_seq must be positive");
    return json_null();
}

json_t *w19_hostedroom_e90a2b_bind_scope_args(json_t *binding, json_t *route) {
    json_t *out = json_object();
    if (!binding || binding->type != JSON_OBJECT) binding = json_object();
    if (!route || route->type != JSON_OBJECT) route = json_object();
    json_set(out, "room_id", json_string(json_get_str(binding, "room_id", "")));
    json_set(out, "home_install_id", json_string(json_get_str(route, "home_install_id", "")));
    json_set(out, "authority_gateway_id", json_string(json_get_str(binding, "gateway_id", "")));
    json_set(out, "authority_epoch", json_number(json_get_num(binding, "authority_epoch", 0)));
    json_set(out, "member_id", json_string(json_get_str(route, "member_id", "")));
    json_set(out, "target_install_id", json_string(json_get_str(route, "target_install_id", "")));
    json_set(out, "target_profile", json_string(json_get_str(route, "target_profile", "")));
    return out;
}
