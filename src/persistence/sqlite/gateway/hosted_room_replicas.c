/*
 * hosted_room_replicas.c - real C11 port of upstream Hermes
 *   persistence/sqlite/gateway/hosted_room_replicas.py decision logic.
 * The replica verbs' bodies run against a sqlite3 connection in Python; here each body's
 * decisions (epoch regression, sequence gap, byte budget, row values, control-event
 * codec, takeover/demotion preconditions) are pure C over the state a caller injects.
 * PoP: w19_hostedroom_beaf1a_schema_ready @ persistence/sqlite/gateway/hosted_room_replicas.py:_replica_schema_ready
 * PoP: w19_hostedroom_beaf1a_control_event @ persistence/sqlite/gateway/hosted_room_replicas.py:_append_control_event
 * PoP: w19_hostedroom_beaf1a_event_bytes @ session_rules/hosted_rooms.py:_event_bytes
 * PoP: w19_hostedroom_beaf1a_store_values @ persistence/sqlite/gateway/hosted_room_replicas.py:_store_replica
 * PoP: w19_hostedroom_beaf1a_row_state @ persistence/sqlite/gateway/hosted_room_replicas.py:_replica_row_state
 * PoP: w19_hostedroom_beaf1a_ingest_plan @ persistence/sqlite/gateway/hosted_room_replicas.py:SQLiteHostedRoomReplicaRepository.ingest_page
 * PoP: w19_hostedroom_beaf1a_promote_plan @ persistence/sqlite/gateway/hosted_room_replicas.py:SQLiteHostedRoomReplicaRepository.promote_replica
 * PoP: w19_hostedroom_beaf1a_demote_plan @ persistence/sqlite/gateway/hosted_room_replicas.py:SQLiteHostedRoomReplicaRepository.demote_room
 */
#include "noc_w19_hostedroom_beaf1a.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_REPLICA_ROOMS 256LL
#define MAX_REPLICA_EVENT_BYTES (256LL * 1024LL * 1024LL)
#define MAX_ACTOR_JSON_BYTES 4096LL
#define MAX_EVENT_JSON_BYTES (256LL * 1024LL)

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ---- growable string buffer --------------------------------------------- */
typedef struct { char *p; size_t len, cap; int oom; } sbuf;

static void sb_init(sbuf *b) { b->p = NULL; b->len = 0; b->cap = 0; b->oom = 0; }

static void sb_add(sbuf *b, const char *s, size_t n) {
    if (b->oom) return;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 256;
        while (cap < b->len + n + 1) cap *= 2;
        char *np = (char *)realloc(b->p, cap);
        if (!np) { b->oom = 1; return; }
        b->p = np;
        b->cap = cap;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void sb_put(sbuf *b, const char *s) { sb_add(b, s, strlen(s)); }

/* ---- Python scalar coercions -------------------------------------------- */
static void py_float_repr(double v, char *buf, size_t cap) {
    if (v != v) { snprintf(buf, cap, "NaN"); return; }
    if (v == (double)1 / 0.0) { snprintf(buf, cap, "Infinity"); return; }
    if (v == (double)-1 / 0.0) { snprintf(buf, cap, "-Infinity"); return; }
    if (v == (double)(long long)v && v < 1e16 && v > -1e16) {
        snprintf(buf, cap, "%lld", (long long)v);
        return;
    }
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, cap, "%.*g", prec, v);
        if (strtod(buf, NULL) == v) return;
    }
}

static void py_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER: py_float_repr(v->num_val, buf, cap); return;
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
        default: snprintf(buf, cap, "None"); return;
    }
}

/* ---- json.dumps(..., ensure_ascii=False, sort_keys=?, separators=(",", ":")) */
static void dump_string(sbuf *b, const char *s) {
    sb_put(b, "\"");
    for (const unsigned char *c = (const unsigned char *)(s ? s : ""); *c; c++) {
        switch (*c) {
            case '"': sb_put(b, "\\\""); break;
            case '\\': sb_put(b, "\\\\"); break;
            case '\n': sb_put(b, "\\n"); break;
            case '\r': sb_put(b, "\\r"); break;
            case '\t': sb_put(b, "\\t"); break;
            case '\b': sb_put(b, "\\b"); break;
            case '\f': sb_put(b, "\\f"); break;
            default:
                if (*c < 0x20) {
                    char tmp[8];
                    snprintf(tmp, sizeof(tmp), "\\u%04x", *c);
                    sb_put(b, tmp);
                } else {
                    char one[2] = {(char)*c, 0};
                    sb_put(b, one);
                }
        }
    }
    sb_put(b, "\"");
}

static int dump_value(sbuf *b, const json_t *v, int sort_keys);

static int key_order(const json_t *obj, size_t a, size_t c) {
    return strcmp(json_object_get_key_at(obj, a), json_object_get_key_at(obj, c));
}

static int dump_object(sbuf *b, const json_t *v, int sort_keys) {
    size_t n = json_object_size(v);
    size_t *idx = n ? (size_t *)malloc(n * sizeof(size_t)) : NULL;
    if (n && !idx) return 0;
    for (size_t i = 0; i < n; i++) idx[i] = i;
    if (sort_keys) {
        for (size_t i = 0; i < n; i++) {
            for (size_t j = i + 1; j < n; j++) {
                if (key_order(v, idx[i], idx[j]) > 0) {
                    size_t t = idx[i];
                    idx[i] = idx[j];
                    idx[j] = t;
                }
            }
        }
    }
    sb_put(b, "{");
    for (size_t i = 0; i < n; i++) {
        if (i) sb_put(b, ",");
        dump_string(b, json_object_get_key_at(v, idx[i]));
        sb_put(b, ":");
        if (!dump_value(b, json_object_get_at(v, idx[i]), sort_keys)) { free(idx); return 0; }
    }
    sb_put(b, "}");
    free(idx);
    return 1;
}

static int dump_value(sbuf *b, const json_t *v, int sort_keys) {
    if (!v) { sb_put(b, "null"); return 1; }
    switch (v->type) {
        case JSON_NULL: sb_put(b, "null"); return 1;
        case JSON_BOOL: sb_put(b, v->bool_val ? "true" : "false"); return 1;
        case JSON_NUMBER: {
            char buf[64];
            py_float_repr(v->num_val, buf, sizeof(buf));
            sb_put(b, buf);
            return 1;
        }
        case JSON_STRING: dump_string(b, v->str_val); return 1;
        case JSON_ARRAY:
            sb_put(b, "[");
            for (size_t i = 0; i < json_len(v); i++) {
                if (i) sb_put(b, ",");
                if (!dump_value(b, json_get(v, i), sort_keys)) return 0;
            }
            sb_put(b, "]");
            return 1;
        case JSON_OBJECT: return dump_object(b, v, sort_keys);
    }
    return 0;
}

static char *py_json_dump(const json_t *v, int sort_keys) {
    sbuf b;
    sb_init(&b);
    if (!dump_value(&b, v, sort_keys)) { free(b.p); return NULL; }
    if (!b.p) return dupstr("");
    return b.p;
}

static long long bytes_of(const char *s) { return s ? (long long)strlen(s) : 0; }

static long long obj_int(json_t *o, const char *k, long long def) {
    json_t *v = (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
    if (!v) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return def;
}

static int obj_flag(json_t *o, const char *k, int def) {
    json_t *v = (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return def;
}

static double obj_num(json_t *o, const char *k, double def) {
    json_t *v = (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
    if (!v || v->type != JSON_NUMBER) return def;
    return v->num_val;
}

static json_t *err_obj(const char *cls, const char *msg) {
    char buf[512];
    snprintf(buf, sizeof(buf), "%s: %s", cls, msg);
    json_t *out = json_object();
    json_set(out, "error", json_string(buf));
    json_set(out, "result", json_null());
    json_set(out, "control_row", json_null());
    json_set(out, "room_row", json_null());
    json_set(out, "update_params", json_null());
    json_set(out, "rows", json_array());
    return out;
}

int w19_hostedroom_beaf1a_schema_ready(int replicas, int replica_events) {
    return (replicas && replica_events) ? 1 : 0;
}

json_t *w19_hostedroom_beaf1a_schema_sql(void) {
    static const char *const REPLICAS =
        "CREATE TABLE IF NOT EXISTS hosted_room_replicas (\n"
        "            room_id TEXT PRIMARY KEY, name TEXT NOT NULL, members_json TEXT NOT NULL,\n"
        "            authority_gateway_id TEXT NOT NULL,\n"
        "            authority_epoch INTEGER NOT NULL CHECK (authority_epoch >= 1),\n"
        "            last_seq INTEGER NOT NULL DEFAULT 0 CHECK (last_seq >= 0),\n"
        "            latest_seq INTEGER NOT NULL DEFAULT 0, event_bytes INTEGER NOT NULL DEFAULT 0,\n"
        "            created_at REAL NOT NULL, updated_at REAL NOT NULL\n"
        "        )";
    static const char *const EVENTS =
        "CREATE TABLE IF NOT EXISTS hosted_room_replica_events (\n"
        "            room_id TEXT NOT NULL, seq INTEGER NOT NULL CHECK (seq >= 1), event_id TEXT NOT NULL,\n"
        "            kind TEXT NOT NULL, actor_json TEXT NOT NULL, authority_epoch INTEGER,\n"
        "            payload_json TEXT NOT NULL, created_at REAL NOT NULL,\n"
        "            PRIMARY KEY (room_id, seq)\n"
        "        )";
    json_t *out = json_object();
    json_set(out, "replicas", json_string(REPLICAS));
    json_set(out, "events", json_string(EVENTS));
    return out;
}

json_t *w19_hostedroom_beaf1a_control_event(const char *kind, long long epoch, json_t *payload) {
    const char *k = kind ? kind : "";
    json_t *actor = json_object();
    json_set(actor, "kind", json_string("system"));
    json_set(actor, "id", json_string("authority-control"));
    char *actor_json = py_json_dump(actor, 1);
    char *payload_json = py_json_dump(payload, 1);
    json_free(actor);
    char event_id[256];
    snprintf(event_id, sizeof(event_id), "system:authority-%s:%lld", k, epoch);
    char kind_text[128];
    snprintf(kind_text, sizeof(kind_text), "authority.%s", k);
    json_t *out = json_object();
    json_set(out, "event_id", json_string(event_id));
    json_set(out, "kind", json_string(kind_text));
    json_set(out, "actor_json", json_string(actor_json ? actor_json : ""));
    json_set(out, "payload_json", json_string(payload_json ? payload_json : ""));
    free(actor_json);
    free(payload_json);
    return out;
}

json_t *w19_hostedroom_beaf1a_event_bytes(json_t *event) {
    char idbuf[512], kindbuf[256];
    if (!event || event->type != JSON_OBJECT) return json_number(0);
    py_str(json_obj_get(event, "event_id"), idbuf, sizeof(idbuf));
    py_str(json_obj_get(event, "kind"), kindbuf, sizeof(kindbuf));
    char *actor_json = py_json_dump(json_obj_get(event, "actor"), 0);
    char *payload_json = py_json_dump(json_obj_get(event, "payload"), 0);
    long long total = bytes_of(idbuf) + bytes_of(kindbuf) + bytes_of(actor_json) + bytes_of(payload_json);
    free(actor_json);
    free(payload_json);
    return json_number((double)total);
}

json_t *w19_hostedroom_beaf1a_store_values(const char *room_name, const char *members_json,
                                           const char *gateway_id, long long epoch,
                                           long long new_last, long long latest_seq) {
    json_t *out = json_object();
    json_set(out, "name", json_string(room_name ? room_name : ""));
    json_set(out, "members_json", json_string(members_json ? members_json : ""));
    json_set(out, "authority_gateway_id", json_string(gateway_id ? gateway_id : ""));
    json_set(out, "authority_epoch", json_number((double)epoch));
    json_set(out, "last_seq", json_number((double)new_last));
    json_set(out, "latest_seq", json_number((double)(latest_seq > new_last ? latest_seq : new_last)));
    return out;
}

json_t *w19_hostedroom_beaf1a_row_state(int found, long long room_count, long long stored_epoch,
                                        long long last_seq, long long event_bytes) {
    json_t *out = json_object();
    if (!found && room_count >= MAX_REPLICA_ROOMS) {
        json_set(out, "error", json_string("ReplicaError: replica room capacity exhausted"));
        json_set(out, "found", json_bool(0));
        json_set(out, "stored_epoch", json_number(0));
        json_set(out, "last_seq", json_number(0));
        json_set(out, "event_bytes", json_number(0));
        return out;
    }
    json_set(out, "error", json_null());
    json_set(out, "found", json_bool(found ? 1 : 0));
    json_set(out, "stored_epoch", json_number((double)(found ? stored_epoch : 0)));
    json_set(out, "last_seq", json_number((double)(found ? last_seq : 0)));
    json_set(out, "event_bytes", json_number((double)(found ? event_bytes : 0)));
    return out;
}

static long long seq_of(json_t *e) { return obj_int(e, "seq", 0); }

static double created_at_of(json_t *e, double now) {
    json_t *v = (e && e->type == JSON_OBJECT) ? json_obj_get(e, "created_at") : NULL;
    if (!v || v->type == JSON_NULL) return now;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : now;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? v->num_val : now;
    if (v->type == JSON_STRING) {
        if (!v->str_val || !v->str_val[0]) return now;
        return strtod(v->str_val, NULL);
    }
    return now;
}

json_t *w19_hostedroom_beaf1a_ingest_plan(json_t *args) {
    int found = obj_flag(args, "found", 0);
    long long room_count = obj_int(args, "room_count", 0);
    long long stored_epoch = obj_int(args, "stored_epoch", 0);
    long long last_seq = obj_int(args, "stored_last_seq", 0);
    long long stored_bytes = obj_int(args, "stored_bytes", 0);
    long long page_epoch = obj_int(args, "page_epoch", 1);
    const char *room_id = json_get_str(args, "room_id", "");
    const char *gateway_id = json_get_str(args, "gateway_id", "");
    double now = obj_num(args, "now", 0.0);
    json_t *events = (args && args->type == JSON_OBJECT) ? json_obj_get(args, "events") : NULL;
    json_t *pls = (args && args->type == JSON_OBJECT) ? json_obj_get(args, "page_latest_seq") : NULL;
    int has_pls = pls && pls->type == JSON_NUMBER;

    json_t *out = err_obj("", "");
    json_set(out, "error", json_null());

    if (!found) {
        if (room_count >= MAX_REPLICA_ROOMS) {
            json_free(out);
            return err_obj("ReplicaError", "replica room capacity exhausted");
        }
        stored_epoch = 0;
        last_seq = 0;
        stored_bytes = 0;
    }
    if (page_epoch < stored_epoch) {
        json_free(out);
        return err_obj("ReplicaEpochRegressionError",
                       "page authority epoch is older than the stored replica epoch");
    }

    json_t *new_events = json_array();
    if (events && events->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(events); i++) {
            json_t *e = json_get(events, i);
            if (e && seq_of(e) > last_seq) json_append(new_events, json_copy(e));
        }
    }
    if (json_len(new_events) > 0 && seq_of(json_get(new_events, 0)) != last_seq + 1) {
        json_free(new_events);
        json_free(out);
        return err_obj("ReplicaGapError", "page skips sequences the replica has not stored");
    }

    long long added = 0;
    json_t *rows = json_array();
    for (size_t i = 0; i < json_len(new_events); i++) {
        json_t *e = json_get(new_events, i);
        json_t *size_j = w19_hostedroom_beaf1a_event_bytes(e);
        long long size = (long long)size_j->num_val;
        json_free(size_j);
        if (stored_bytes + added + size > MAX_REPLICA_EVENT_BYTES) {
            json_free(rows);
            json_free(new_events);
            json_free(out);
            return err_obj("ReplicaError", "replica event storage exhausted");
        }
        char *actor_json = py_json_dump(json_obj_get(e, "actor"), 1);
        if (bytes_of(actor_json) > MAX_ACTOR_JSON_BYTES) {
            free(actor_json);
            json_free(rows);
            json_free(new_events);
            json_free(out);
            return err_obj("ReplicaError", "actor is too large");
        }
        char *payload_json = py_json_dump(json_obj_get(e, "payload"), 1);
        if (bytes_of(payload_json) > MAX_EVENT_JSON_BYTES) {
            free(actor_json);
            free(payload_json);
            json_free(rows);
            json_free(new_events);
            json_free(out);
            return err_obj("ReplicaError", "payload is too large");
        }
        json_t *row = json_array();
        json_append(row, json_string(room_id));
        json_append(row, json_number((double)seq_of(e)));
        json_t *eid = json_obj_get(e, "event_id");
        json_append(row, eid && eid->type == JSON_STRING ? json_copy(eid) : json_null());
        json_t *ekind = json_obj_get(e, "kind");
        json_append(row, ekind && ekind->type == JSON_STRING ? json_copy(ekind) : json_null());
        json_append(row, json_string(actor_json ? actor_json : "{}"));
        json_t *epoch = json_obj_get(e, "authority_epoch");
        json_append(row, epoch ? json_copy(epoch) : json_null());
        json_append(row, json_string(payload_json ? payload_json : "null"));
        json_append(row, json_number(created_at_of(e, now)));
        json_append(rows, row);
        free(actor_json);
        free(payload_json);
        added += size;
    }

    long long new_last = json_len(new_events) > 0
        ? seq_of(json_get(new_events, json_len(new_events) - 1)) : last_seq;
    long long latest_seq = has_pls ? (long long)pls->num_val : new_last;

    json_t *authority = json_object();
    json_set(authority, "gateway_id", json_string(gateway_id));
    json_set(authority, "epoch", json_number((double)page_epoch));

    json_t *result = json_object();
    json_set(result, "room_id", json_string(room_id));
    json_set(result, "stored_seq", json_number((double)new_last));
    json_set(result, "ingested", json_number((double)json_len(new_events)));
    json_set(result, "authority", authority);
    json_set(result, "caught_up", json_bool(new_last >= (latest_seq > new_last ? latest_seq : new_last)));

    json_set(out, "result", result);
    json_set(out, "rows", rows);
    json_free(new_events);
    return out;
}

json_t *w19_hostedroom_beaf1a_promote_plan(json_t *args) {
    int found = obj_flag(args, "found", 0);
    const char *room_id = json_get_str(args, "room_id", "");
    const char *name = json_get_str(args, "name", "");
    const char *members_json = json_get_str(args, "members_json", "{}");
    const char *stored_gateway = json_get_str(args, "authority_gateway_id", "");
    long long previous_epoch = obj_int(args, "authority_epoch", 0);
    long long last_seq = obj_int(args, "last_seq", 0);
    long long event_bytes = obj_int(args, "event_bytes", 0);
    const char *local_gateway = json_get_str(args, "local_gateway_id", "");
    const char *reason = json_get_str(args, "reason", "authority-unreachable");
    double now = obj_num(args, "now", 0.0);

    if (!found) return err_obj("ReplicaError", "replica not found");
    if (!strcmp(stored_gateway, local_gateway))
        return err_obj("ReplicaError", "this gateway already holds the room authority");
    if (obj_flag(args, "room_exists", 0))
        return err_obj("RoomConflictError", "room_id already exists in the local authoritative store");
    if (obj_flag(args, "retired_exists", 0))
        return err_obj("RoomConflictError", "room_id belongs to a disbanded room");

    long long target_epoch = previous_epoch + 1;
    long long claim_seq = last_seq + 1;

    json_t *payload = json_object();
    json_set(payload, "previous_gateway_id", json_string(stored_gateway));
    json_set(payload, "authority_gateway_id", json_string(local_gateway));
    json_set(payload, "authority_epoch", json_number((double)target_epoch));
    json_set(payload, "promoted_from_replica", json_bool(1));
    json_set(payload, "reason", json_string(reason));
    json_t *claim = w19_hostedroom_beaf1a_control_event("claimed", target_epoch, payload);
    json_free(payload);

    const char *claim_event_id = json_get_str(claim, "event_id", "");
    const char *claim_kind = json_get_str(claim, "kind", "");
    const char *claim_actor = json_get_str(claim, "actor_json", "");
    const char *claim_payload = json_get_str(claim, "payload_json", "");
    long long claim_bytes = bytes_of(claim_event_id) + bytes_of(claim_kind)
        + bytes_of(claim_actor) + bytes_of(claim_payload);

    json_t *control_row = json_array();
    json_append(control_row, json_string(room_id));
    json_append(control_row, json_number((double)claim_seq));
    json_append(control_row, json_string(claim_event_id));
    json_append(control_row, json_string(claim_kind));
    json_append(control_row, json_string(claim_actor));
    json_append(control_row, json_number((double)target_epoch));
    json_append(control_row, json_string(claim_payload));
    json_append(control_row, json_number(now));

    json_t *room_row = json_array();
    json_append(room_row, json_string(room_id));
    json_append(room_row, json_string(name));
    json_append(room_row, json_string(members_json));
    json_append(room_row, json_string(local_gateway));
    json_append(room_row, json_number((double)target_epoch));
    json_append(room_row, json_number((double)(claim_seq + 1)));
    json_append(room_row, json_number((double)(event_bytes + claim_bytes)));
    json_append(room_row, json_number(now));
    json_append(room_row, json_number(now));

    json_t *result = json_object();
    json_set(result, "room_id", json_string(room_id));
    json_set(result, "authority_gateway_id", json_string(local_gateway));
    json_set(result, "authority_epoch", json_number((double)target_epoch));
    json_set(result, "previous_gateway_id", json_string(stored_gateway));
    json_set(result, "previous_epoch", json_number((double)previous_epoch));
    json_set(result, "claim_seq", json_number((double)claim_seq));
    json_set(result, "latest_seq", json_number((double)claim_seq));

    json_t *out = err_obj("", "");
    json_set(out, "error", json_null());
    json_set(out, "result", result);
    json_set(out, "control_row", control_row);
    json_set(out, "room_row", room_row);
    json_free(claim);
    return out;
}

json_t *w19_hostedroom_beaf1a_demote_plan(json_t *args) {
    int found = obj_flag(args, "found", 0);
    const char *room_id = json_get_str(args, "room_id", "");
    const char *current_gateway = json_get_str(args, "current_gateway", "");
    long long current_epoch = obj_int(args, "current_epoch", 0);
    long long next_seq = obj_int(args, "next_seq", 1);
    const char *observed_gateway = json_get_str(args, "observed_gateway", "");
    long long observed_epoch = obj_int(args, "observed_epoch", 0);
    const char *local_gateway = json_get_str(args, "local_gateway_id", "");
    double now = obj_num(args, "now", 0.0);

    if (!found) return err_obj("ReplicaError", "room not found in the local authoritative store");
    if (!strcmp(current_gateway, observed_gateway) && current_epoch == observed_epoch) {
        json_t *result = json_object();
        json_set(result, "room_id", json_string(room_id));
        json_set(result, "authority_gateway_id", json_string(current_gateway));
        json_set(result, "authority_epoch", json_number((double)current_epoch));
        json_set(result, "idempotent", json_bool(1));
        json_t *out = err_obj("", "");
        json_set(out, "error", json_null());
        json_set(out, "result", result);
        return out;
    }
    if (observed_epoch <= current_epoch)
        return err_obj("ReplicaEpochRegressionError",
                       "observed epoch does not supersede the stored authority");
    if (strcmp(current_gateway, local_gateway))
        return err_obj("ReplicaError", "room is not locally authoritative; nothing to demote");

    json_t *payload = json_object();
    json_set(payload, "previous_gateway_id", json_string(current_gateway));
    json_set(payload, "authority_gateway_id", json_string(observed_gateway));
    json_set(payload, "authority_epoch", json_number((double)observed_epoch));
    json_t *lost = w19_hostedroom_beaf1a_control_event("lost", observed_epoch, payload);
    json_free(payload);

    json_t *control_row = json_array();
    json_append(control_row, json_string(room_id));
    json_append(control_row, json_number((double)next_seq));
    json_append(control_row, json_copy(json_obj_get(lost, "event_id")));
    json_append(control_row, json_copy(json_obj_get(lost, "kind")));
    json_append(control_row, json_copy(json_obj_get(lost, "actor_json")));
    json_append(control_row, json_number((double)observed_epoch));
    json_append(control_row, json_copy(json_obj_get(lost, "payload_json")));
    json_append(control_row, json_number(now));

    json_t *update_params = json_array();
    json_append(update_params, json_string(observed_gateway));
    json_append(update_params, json_number((double)observed_epoch));
    json_append(update_params, json_number(now));
    json_append(update_params, json_string(room_id));

    json_t *result = json_object();
    json_set(result, "room_id", json_string(room_id));
    json_set(result, "authority_gateway_id", json_string(observed_gateway));
    json_set(result, "authority_epoch", json_number((double)observed_epoch));
    json_set(result, "idempotent", json_bool(0));

    json_t *out = err_obj("", "");
    json_set(out, "error", json_null());
    json_set(out, "result", result);
    json_set(out, "control_row", control_row);
    json_set(out, "update_params", update_params);
    json_free(lost);
    return out;
}
