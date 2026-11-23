/*
 * status_pid_records.c — C port of upstream Hermes gateway/status_pid_records.py.
 *
 * PoP: w13_statuspid_56d22b_previous @ gateway/status_pid_records.py:names_a_previous_gateway
 * PoP: w13_statuspid_56d22b_strict @ gateway/status_pid_records.py:strict_identity_records
 *
 * The gateway.status helpers (_read_pid_record, _pid_from_record,
 * _start_times_agree, _pid_exists, _get_process_start_time) are seams in the
 * oracle; the record-selection decision is ported exactly, including the
 * fail-closed cases (garbled start time, another LIVE instance).
 */
#include "noc_w13_statuspid_56d22b.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

/* Python truthiness of a JSON value. */
static int jtruthy(const struct json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

/* Stub seam: _pid_from_record -- decimal string / integral number -> pid, else -1. */
static int pid_from(const struct json_t *rec, long long *out) {
    if (!json_is_object(rec)) return 0;
    struct json_t *v = json_obj_get(rec, "pid");
    if (!v) return 0;
    if (v->type == JSON_NUMBER) {
        if (v->num_val != (double)(long long)v->num_val) return 0;
        *out = (long long)v->num_val;
        return 1;
    }
    if (v->type == JSON_STRING && v->str_val && v->str_val[0]) {
        char *end = NULL;
        long long p = strtoll(v->str_val, &end, 10);
        if (end == v->str_val || (end && *end)) return 0;
        *out = p;
        return 1;
    }
    return 0;
}

/* Stub seam: _start_times_agree -- scalar equality (Python ==). */
static int agree(const struct json_t *a, const struct json_t *b) {
    if (!a || !b) return 0;
    if (a->type == JSON_NULL || b->type == JSON_NULL) return 0;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return a->str_val && b->str_val && !strcmp(a->str_val, b->str_val);
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    return 0;
}

/* Stub seam: _pid_exists(pid) over the fixture's live_pids array. */
static int pid_exists(const struct json_t *pids, long long pid) {
    if (!json_is_array(pids)) return 0;
    for (size_t i = 0; i < json_len(pids); i++) {
        struct json_t *v = json_get(pids, i);
        if (v && v->type == JSON_NUMBER && v->num_val == (double)pid) return 1;
        if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) {
            char *end = NULL;
            long long p = strtoll(v->str_val, &end, 10);
            if (end != v->str_val && end && *end == '\0' && p == pid) return 1;
        }
    }
    return 0;
}

/* Stub seam: _get_process_start_time(pid) from the fixture object's decimal key. */
static struct json_t *live_start(const struct json_t *map, long long pid) {
    if (!json_is_object(map)) return NULL;
    for (size_t i = 0; i < json_object_size(map); i++) {
        const char *k = json_object_get_key_at(map, i);
        if (!k) continue;
        char *end = NULL;
        long long p = strtoll(k, &end, 10);
        if (end != k && *end == '\0' && p == pid) return json_object_get_at(map, i);
    }
    return NULL;
}

struct json_t *w13_statuspid_56d22b_previous(const struct json_t *ctx) {
    struct json_t *pid_record = json_is_object(ctx) ? json_obj_get(ctx, "pid_record") : NULL;
    struct json_t *lock_record = json_is_object(ctx) ? json_obj_get(ctx, "lock_record") : NULL;
    struct json_t *pids = json_is_object(ctx) ? json_obj_get(ctx, "live_pids") : NULL;
    struct json_t *starts = json_is_object(ctx) ? json_obj_get(ctx, "live_start_times") : NULL;

    long long pid = 0;
    if (!pid_from(pid_record, &pid) || pid <= 0) return json_bool(0);
    struct json_t *recorded = json_obj_get(pid_record, "start_time");
    struct json_t *held = json_obj_get(lock_record, "start_time");
    if (!recorded || recorded->type == JSON_NULL || !held || held->type == JSON_NULL) return json_bool(0);
    if (agree(held, recorded)) return json_bool(0);
    if (!pid_exists(pids, pid)) return json_bool(1);
    struct json_t *live = live_start(starts, pid);
    return json_bool(live != NULL && !agree(live, recorded));
}

struct json_t *w13_statuspid_56d22b_strict(const struct json_t *ctx) {
    struct json_t *lock = json_is_object(ctx) ? json_obj_get(ctx, "lock_record") : NULL;
    struct json_t *pid_rec = json_is_object(ctx) ? json_obj_get(ctx, "pid_record") : NULL;
    struct json_t *pe = json_is_object(ctx) ? json_obj_get(ctx, "pid_exists") : NULL;
    int exists = pe && pe->type == JSON_BOOL && pe->bool_val;

    struct json_t *out = json_array();
    if (!exists) {
        json_append(out, lock ? json_copy(lock) : json_null());
        return out;
    }
    /* names_a_previous_gateway is only reached when both records are truthy. */
    int previous = 0;
    if (jtruthy(pid_rec) && jtruthy(lock)) {
        struct json_t *sub = json_object();
        json_set(sub, "pid_record", json_copy(pid_rec));
        json_set(sub, "lock_record", json_copy(lock));
        json_set(sub, "live_pids", json_copy(json_is_object(ctx) ? json_obj_get(ctx, "live_pids") : NULL));
        json_set(sub, "live_start_times", json_copy(json_is_object(ctx) ? json_obj_get(ctx, "live_start_times") : NULL));
        struct json_t *r = w13_statuspid_56d22b_previous(sub);
        previous = r && r->type == JSON_BOOL && r->bool_val;
        json_free(r);
        json_free(sub);
    }
    if (previous) {
        json_append(out, lock ? json_copy(lock) : json_null());
    } else {
        json_append(out, pid_rec ? json_copy(pid_rec) : json_null());
        json_append(out, lock ? json_copy(lock) : json_null());
    }
    return out;
}

