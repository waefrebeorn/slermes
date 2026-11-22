#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/hosted_room_replicas.py: 8 missing functions ── */


/* PoP: replica_transaction @ gateway/hosted_room_replicas.py:replica_transaction */
void *replica_transaction(void) { return NULL; }

/* PoP: event_bytes @ gateway/hosted_room_replicas.py:event_bytes */
int event_bytes(void) { return 0; }

/* PoP: validate_page @ gateway/hosted_room_replicas.py:_validate_page */
/* Python: _validate_page(page) -> (events, authority) or raises ReplicaError /
 * ReplicaGapError. Helpers: identifier (strip, non-empty, <=128 chars,
 * ^[A-Za-z0-9][A-Za-z0-9._:-]*$) and bounded_int(low=1, rejects bool/non-int).
 * No callers in C, so the signature returns 1 on success / 0 with "Class: msg"
 * in err. The (events, authority) payload is the input page itself. */
static int vp_err(char *err, size_t cap, const char *cls, const char *msg) {
    snprintf(err, cap, "%s: %s", cls, msg);
    return 0;
}
static int vp_int(json_t *v, const char *msg, char *err, size_t cap) {
    if (!v || v->type != JSON_NUMBER) return vp_err(err, cap, "ReplicaError", msg);
    double d = v->num_val;
    if (d != (double)(long long)d || d < 1.0) return vp_err(err, cap, "ReplicaError", msg);
    return 1;
}
static int vp_identifier(json_t *v, char *err, size_t cap) {
    if (!v || v->type != JSON_STRING)
        return vp_err(err, cap, "ReplicaError", "page.authority.gateway_id must be a string");
    const char *s = v->str_val;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t n = strlen(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r')) n--;
    int ok = n && n <= 128 && isalnum((unsigned char)s[0]);
    for (size_t i = 1; ok && i < n; i++)
        if (!isalnum((unsigned char)s[i]) && s[i] != '.' && s[i] != '_' && s[i] != ':' && s[i] != '-') ok = 0;
    if (!ok) return vp_err(err, cap, "ReplicaError", "invalid page.authority.gateway_id");
    return 1;
}
static int vp_nonempty_str(json_t *v, const char *field, char *err, size_t cap) {
    if (!v || v->type != JSON_STRING || !*v->str_val) {
        char msg[128];
        snprintf(msg, sizeof(msg), "event.%s must be a non-empty string", field);
        return vp_err(err, cap, "ReplicaError", msg);
    }
    return 1;
}
int validate_page(const json_t *page, char *err, size_t errcap) {
    err[0] = '\0';
    if (!page || page->type != JSON_OBJECT) return vp_err(err, errcap, "ReplicaError", "page must be an object");
    json_t *events = json_obj_get(page, "events");
    json_t *authority = json_obj_get(page, "authority");
    if (!events || events->type != JSON_ARRAY) return vp_err(err, errcap, "ReplicaError", "page.events must be a list");
    if (!authority || authority->type != JSON_OBJECT)
        return vp_err(err, errcap, "ReplicaError", "page.authority is required for replication");
    if (!vp_identifier(json_obj_get(authority, "gateway_id"), err, errcap)) return 0;
    if (!vp_int(json_obj_get(authority, "epoch"), "page.authority.epoch must be a positive integer", err, errcap)) return 0;
    long long prev = -1;
    size_t n = json_array_size(events);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_array_get(events, i);
        if (!e || e->type != JSON_OBJECT) return vp_err(err, errcap, "ReplicaError", "page events must be objects");
        json_t *seqv = json_obj_get(e, "seq");
        if (!seqv || seqv->type != JSON_NUMBER || seqv->num_val != (double)(long long)seqv->num_val || seqv->num_val < 1.0)
            return vp_err(err, errcap, "ReplicaError", "event.seq must be a positive integer");
        long long seq = (long long)seqv->num_val;
        if (prev >= 0 && seq != prev + 1) return vp_err(err, errcap, "ReplicaGapError", "page events must be contiguous");
        prev = seq;
        if (!vp_nonempty_str(json_obj_get(e, "event_id"), "event_id", err, errcap)) return 0;
        if (!vp_nonempty_str(json_obj_get(e, "kind"), "kind", err, errcap)) return 0;
        json_t *actor = json_obj_get(e, "actor");
        if (!actor || actor->type != JSON_OBJECT) return vp_err(err, errcap, "ReplicaError", "event.actor must be an object");
        if (!json_obj_get(e, "payload")) return vp_err(err, errcap, "ReplicaError", "event.payload is required");
    }
    return 1;
}

/* PoP: ingest_page @ gateway/hosted_room_replicas.py:ingest_page */
char *ingest_page(void) { return strdup("{}"); }

/* PoP: replica_state @ gateway/hosted_room_replicas.py:replica_state */
char *replica_state(void) { return strdup("{}"); }

/* PoP: promote_replica @ gateway/hosted_room_replicas.py:promote_replica */
char *promote_replica(void) { return strdup("{}"); }

/* PoP: demote_room @ gateway/hosted_room_replicas.py:demote_room */
char *demote_room(void) { return strdup("{}"); }
