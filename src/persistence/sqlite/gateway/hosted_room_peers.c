
/*
 * hosted_room_peers.c - real C11 port of the pure helpers of upstream Hermes
 * persistence/sqlite/gateway/hosted_room_peers.py. The rest of the module is a
 * SQLite repository (transactions, schema steps, probes) and stays in Python.
 * PoP: w19_hostedroom_5e13e6_reservation_superseded @ persistence/sqlite/gateway/hosted_room_peers.py:_reservation_superseded
 * PoP: w19_hostedroom_5e13e6_reservation_values @ persistence/sqlite/gateway/hosted_room_peers.py:_reservation_values
 * PoP: w19_hostedroom_5e13e6_remote_run_identity @ persistence/sqlite/gateway/hosted_room_peers.py:_remote_run_identity
 */
#include "hermes_json.h"
#include "noc_w19_hostedroom_5e13e6.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* _REMOTE_RUN_IDENTITY_COLUMNS: the receipt key, in table order. */
static const char *const IDENTITY_COLUMNS[] = {
    "room_id", "home_install_id", "authority_gateway_id", "authority_epoch", "member_id",
    "target_install_id", "target_profile", "task_id", "execution_generation", NULL
};

/* Python int(v) over the JSON scalars these settings accept; 1 on success. */
static int py_int(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        if (!s) return 0;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        if (!*s) return 0;
        char *end = NULL;
        long long x = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
        if (*end != '\0') return 0;
        *out = x;
        return 1;
    }
    return 0;
}

/* Python str() of a JSON scalar into buf (or a static literal). */
static const char *py_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) return "None";
    switch (v->type) {
        case JSON_STRING: return v->str_val ? v->str_val : "";
        case JSON_BOOL:   return v->bool_val ? "True" : "False";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d) snprintf(buf, cap, "%lld", (long long)d);
            else snprintf(buf, cap, "%g", d);
            return buf;
        }
        default: return "<obj>";
    }
}

/* row_epoch = int(row["authority_epoch"]); row_epoch > epoch or (== and gw differs). */
int w19_hostedroom_5e13e6_reservation_superseded(json_t *row, const char *gateway_id, long long epoch) {
    long long row_epoch = 0;
    json_t *e = (row && row->type == JSON_OBJECT) ? json_obj_get(row, "authority_epoch") : NULL;
    if (!e || !py_int(e, &row_epoch)) row_epoch = 0;
    if (row_epoch > epoch) return 1;
    if (row_epoch != epoch) return 0;
    char buf[64];
    json_t *gw = (row && row->type == JSON_OBJECT) ? json_obj_get(row, "authority_gateway_id") : NULL;
    const char *row_gw = py_str(gw, buf, sizeof(buf));
    return strcmp(row_gw, gateway_id ? gateway_id : "") != 0;
}

json_t *w19_hostedroom_5e13e6_reservation_values(json_t *claims) {
    json_t *out = json_array();
    const char *names[] = { "room_id", "member_id", "target_profile", "authority_gateway_id" };
    for (size_t i = 0; i < 4; i++) {
        json_t *v = (claims && claims->type == JSON_OBJECT) ? json_obj_get(claims, names[i]) : NULL;
        json_append(out, v ? json_copy(v) : json_null());
    }
    json_t *epoch = (claims && claims->type == JSON_OBJECT) ? json_obj_get(claims, "authority_epoch") : NULL;
    long long e = 0;
    if (epoch && epoch->type != JSON_NULL && !py_int(epoch, &e)) e = 0;  /* int(x or 0) */
    json_append(out, json_number((double)e));
    return out;
}

json_t *w19_hostedroom_5e13e6_remote_run_identity(json_t *record) {
    json_t *out = json_array();
    for (size_t i = 0; IDENTITY_COLUMNS[i]; i++) {
        json_t *v = (record && record->type == JSON_OBJECT) ? json_obj_get(record, IDENTITY_COLUMNS[i]) : NULL;
        json_append(out, v ? json_copy(v) : json_null());
    }
    return out;
}
