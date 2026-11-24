/*
 * tui_gateway/hosted_room_router_call.c -- real C11 port of the pure inputs and
 * the claim gate of upstream Hermes tui_gateway/hosted_room_router_call.py.
 *
 * PoP: w20_hostedroom_8052fe_member_roster @ tui_gateway/hosted_room_router_call.py:_member_roster
 * PoP: w20_hostedroom_8052fe_recent_messages @ tui_gateway/hosted_room_router_call.py:_recent_messages
 * PoP: w20_hostedroom_8052fe_start @ tui_gateway/hosted_room_router_call.py:RouterCalls.start
 * PoP: w20_hostedroom_8052fe_handles @ tui_gateway/hosted_room_router_call.py:ask_router
 * PoP: w20_hostedroom_8052fe_claim_ttl @ tui_gateway/hosted_room_router_call.py:CLAIM_TTL_SECONDS
 *
 * `call_llm`, `spawn_context_thread`, `read_profile_meta` and the store's claim
 * rows stay upstream; RouterCalls.start's state machine is driven here with the
 * claim answer and the two failure paths handed in as data.
 */
#include "hermes_json.h"
#include "noc_w20_hostedroom_8052fe.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HRRC_ROUTER_RECENT_MESSAGES 5
#define HRRC_CLAIM_TTL_SECONDS 120.0
#define HRRC_DEFAULT_MEMBER "member"

static char *hrrc_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int hrrc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static int hrrc_is_int(const json_t *v) {
    if (!v || v->type != JSON_NUMBER) return 0;
    if (!isfinite(v->num_val)) return 0;
    return floor(v->num_val) == v->num_val;
}

/* Python str(v) for the scalar shapes a room dict carries. */
static char *hrrc_str(const json_t *v) {
    char buf[64];
    if (!v) return NULL;
    switch (v->type) {
    case JSON_NULL:   return NULL;
    case JSON_BOOL:   return hrrc_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return hrrc_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (hrrc_is_int(v)) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return hrrc_dup(buf);
    default:
        return NULL;
    }
}

/* `str(v or "")` -- "" for the falsy shapes, else str(v). */
static char *hrrc_str_or_empty(const json_t *v) {
    char *s;
    if (!hrrc_truthy(v)) return hrrc_dup("");
    s = hrrc_str(v);
    return s ? s : hrrc_dup("");
}

json_t *w20_hostedroom_8052fe_member_roster(const json_t *room, const json_t *homes) {
    json_t *out = json_array();
    json_t *members = json_obj_get(room, "members");
    size_t i;
    for (i = 0; members && members->type == JSON_ARRAY && i < members->c.count; i++) {
        json_t *m = members->c.items[i];
        json_t *profile, *target, *kind, *meta;
        char *handle, *name, *description;
        json_t *item;
        int local = 1;
        if (!m || m->type != JSON_OBJECT) continue;
        profile = json_obj_get(m, "profile");
        target = json_obj_get(m, "target");
        kind = (target && target->type == JSON_OBJECT) ? json_obj_get(target, "kind") : NULL;
        /* (member.get("target") or {}).get("kind", "local") == "local" */
        if (kind) {
            local = (kind->type == JSON_STRING && !strcmp(kind->str_val, "local")) ? 1 : 0;
        }
        meta = NULL;
        if (local) {
            char *key = hrrc_str_or_empty(profile);
            if (key && key[0] && homes && homes->type == JSON_OBJECT) {
                meta = json_obj_get(homes, key);
            }
            free(key);
        }
        handle = hrrc_str(json_obj_get(m, "handle"));
        name = hrrc_str_or_empty(json_obj_get(m, "display_name"));
        description = (meta && meta->type == JSON_STRING) ? hrrc_dup(meta->str_val) : hrrc_dup("");
        item = json_object();
        json_set(item, "handle", json_string(handle ? handle : ""));
        json_set(item, "name", json_string(name ? name : ""));
        json_set(item, "description", json_string(description ? description : ""));
        json_append(out, item);
        free(handle);
        free(name);
        free(description);
    }
    return out;
}

/* int(v) for a seq value: 1 on success. */
static int hrrc_int_of(const json_t *v, long long *out) {
    char *end;
    long long n;
    if (!v) return 0;
    if (v->type == JSON_BOOL) return 0;
    if (v->type == JSON_NUMBER) {
        if (!isfinite(v->num_val)) return 0;
        *out = (long long)v->num_val;
        return 1;
    }
    if (v->type != JSON_STRING) return 0;
    n = strtoll(v->str_val, &end, 10);
    if (end == v->str_val || *end) return 0;
    *out = n;
    return 1;
}

static const char *hrrc_member_name(const json_t *members, const char *member_id) {
    size_t i;
    if (!member_id) return NULL;
    for (i = 0; members && members->type == JSON_ARRAY && i < members->c.count; i++) {
        json_t *m = members->c.items[i];
        char *key;
        if (!m || m->type != JSON_OBJECT) continue;
        key = hrrc_str(json_obj_get(m, "member_id"));
        if (key && !strcmp(key, member_id)) {
            const char *display = json_get_str(m, "display_name", NULL);
            const char *handle = json_get_str(m, "handle", NULL);
            free(key);
            if (display && display[0]) return display;
            return handle ? handle : "";
        }
        free(key);
    }
    return NULL;
}

json_t *w20_hostedroom_8052fe_recent_messages(const json_t *events, const json_t *members,
                                              const json_t *before_seq) {
    json_t *out = json_array();
    size_t *kept, nkept = 0;
    long long before = 0;
    size_t i, start;
    if (!hrrc_int_of(before_seq, &before)) before = 0;
    kept = (size_t *)calloc((events && events->type == JSON_ARRAY ? events->c.count : 0) + 1,
                            sizeof(size_t));
    if (!kept) return out;
    for (i = 0; events && events->type == JSON_ARRAY && i < events->c.count; i++) {
        json_t *e = events->c.items[i];
        const char *kind;
        long long seq;
        if (!e || e->type != JSON_OBJECT) continue;
        if (!hrrc_int_of(json_obj_get(e, "seq"), &seq)) continue;
        if (seq >= before) continue;
        kind = json_get_str(e, "kind", NULL);
        if (!kind) continue;
        if (strcmp(kind, "message.user") && strcmp(kind, "message.member")) continue;
        kept[nkept++] = i;                  /* indices: the events array owns the nodes */
    }
    start = (nkept > HRRC_ROUTER_RECENT_MESSAGES)
                ? nkept - HRRC_ROUTER_RECENT_MESSAGES : 0;
    for (i = start; i < nkept; i++) {
        json_t *e = events->c.items[kept[i]];
        const char *kind = json_get_str(e, "kind", "");
        json_t *payload = json_obj_get(e, "payload");
        json_t *item = json_object();
        char *author, *text;
        if (!strcmp(kind, "message.user")) {
            author = hrrc_dup("user");
        } else {
            char *mid = hrrc_str((payload && payload->type == JSON_OBJECT)
                                     ? json_obj_get(payload, "member_id") : NULL);
            const char *named = hrrc_member_name(members, mid);
            author = hrrc_dup(named ? named : HRRC_DEFAULT_MEMBER);
            free(mid);
        }
        text = hrrc_str_or_empty((payload && payload->type == JSON_OBJECT)
                                     ? json_obj_get(payload, "text") : NULL);
        json_set(item, "author", json_string(author ? author : ""));
        json_set(item, "text", json_string(text ? text : ""));
        json_append(out, item);
        free(author);
        free(text);
    }
    free(kept);
    return out;
}

/* str.casefold() for ASCII; anything else is left byte-identical. */
static char *hrrc_casefold(const char *s) {
    size_t n = strlen(s ? s : ""), i;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    out[n] = '\0';
    return out;
}

json_t *w20_hostedroom_8052fe_handles(const json_t *members) {
    /* The dict comprehension dedupes by folded handle: the first position wins,
     * the last value sticks. */
    json_t *out = json_array();
    char **keys = NULL;
    size_t nkeys = 0, i, j;
    for (i = 0; members && members->type == JSON_ARRAY && i < members->c.count; i++) {
        json_t *m = members->c.items[i];
        char *handle, *folded, *id;
        if (!m || m->type != JSON_OBJECT) continue;
        handle = hrrc_str(json_obj_get(m, "handle"));
        if (!handle) continue;
        folded = hrrc_casefold(handle);
        id = hrrc_str(json_obj_get(m, "member_id"));
        for (j = 0; folded && j < nkeys; j++) {
            if (!strcmp(keys[j], folded)) break;
        }
        if (folded && j < nkeys) {
            json_t *pair = out->c.items[j];
            json_free(pair->c.items[1]);
            pair->c.items[1] = json_string(id ? id : "");
        } else {
            json_t *pair = json_array();
            char **grown = (char **)realloc(keys, (nkeys + 1) * sizeof(char *));
            if (grown) keys = grown;
            json_append(pair, json_string(folded ? folded : ""));
            json_append(pair, json_string(id ? id : ""));
            json_append(out, pair);
            keys[nkeys++] = folded ? folded : hrrc_dup("");
            folded = NULL;                       /* owned by keys[] */
        }
        free(handle);
        free(folded);
        free(id);
    }
    for (i = 0; i < nkeys; i++) free(keys[i]);
    free(keys);
    return out;
}

json_t *w20_hostedroom_8052fe_claim_ttl(void) {
    return json_number(HRRC_CLAIM_TTL_SECONDS);
}

/* ---------- RouterCalls.start ---------- */

static void hrrc_log(json_t *logs, const char *room_id, const char *what) {
    char buf[512];
    snprintf(buf, sizeof(buf), "hosted room %s: %s", room_id ? room_id : "", what);
    json_append(logs, json_string(buf));
}

/* {"__raise__": {...}} -> 1 */
static int hrrc_raises(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

json_t *w20_hostedroom_8052fe_start(const char *room_id, const char *discussion_event_id,
                                    int already_inflight, const json_t *claim,
                                    const json_t *call_result, int record_raises,
                                    int release_raises, const json_t *now) {
    json_t *out = json_object();
    json_t *logs = json_array();
    double now_v = 0.0;
    int started = 0, raised = 0, claim_called = 0, record_called = 0;
    int record_ok = 1, release_called = 0, release_ok = 1, inflight_after = 0;

    if (now && now->type == JSON_NUMBER) now_v = now->num_val;

    if (already_inflight) {
        /* `if key in self._inflight: return False` -- no claim is taken. */
        inflight_after = 1;
    } else {
        claim_called = 1;
        if (hrrc_raises(claim)) {
            /* The claim raised: the key is discarded and the error propagates. */
            raised = 1;
        } else if (!hrrc_truthy(claim)) {
            inflight_after = 0;                     /* discarded */
        } else {
            started = 1;
            record_called = 1;
            record_ok = !record_raises;
            release_called = 1;
            release_ok = !release_raises;
            if (!record_ok) hrrc_log(logs, room_id, "recording the router answer failed");
            if (!release_ok) hrrc_log(logs, room_id, "releasing the router claim failed");
            inflight_after = 0;                     /* discarded in the finally */
        }
    }

    json_set(out, "started", json_bool(started));
    json_set(out, "raised", json_bool(raised));
    json_set(out, "claim_called", json_bool(claim_called));
    json_set(out, "expires_at", json_number(claim_called ? now_v + HRRC_CLAIM_TTL_SECONDS : 0.0));
    json_set(out, "now", json_number(now_v));
    json_set(out, "record_called", json_bool(record_called));
    json_set(out, "recorded", (record_called && record_ok)
                                   ? (call_result ? json_copy(call_result) : json_null())
                                   : json_null());
    json_set(out, "record_ok", json_bool(record_ok));
    json_set(out, "release_called", json_bool(release_called));
    json_set(out, "release_ok", json_bool(release_ok));
    json_set(out, "inflight_after", json_bool(inflight_after));
    json_set(out, "logs", logs);
    (void)discussion_event_id;
    return out;
}
