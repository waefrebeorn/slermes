#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_hostedroom_beaf1a.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static long long num_field(json_t *in, const char *key, long long def) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_NUMBER) return def;
    return (long long)v->num_val;
}

static int bool_field(json_t *in, const char *key, int def) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_BOOL) return def;
    return v->bool_val ? 1 : 0;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "schema_ready")) {
            out = json_bool(w19_hostedroom_beaf1a_schema_ready(bool_field(in, "replicas", 0),
                                                               bool_field(in, "replica_events", 0)));
        } else if (!strcmp(func, "schema_sql")) {
            out = w19_hostedroom_beaf1a_schema_sql();
        } else if (!strcmp(func, "control_event")) {
            out = w19_hostedroom_beaf1a_control_event(json_get_str(in, "kind", ""),
                                                      num_field(in, "epoch", 0),
                                                      json_obj_get(in, "payload"));
        } else if (!strcmp(func, "event_bytes")) {
            out = w19_hostedroom_beaf1a_event_bytes(json_obj_get(in, "event"));
        } else if (!strcmp(func, "store_values")) {
            out = w19_hostedroom_beaf1a_store_values(json_get_str(in, "room_name", ""),
                                                     json_get_str(in, "members_json", ""),
                                                     json_get_str(in, "gateway_id", ""),
                                                     num_field(in, "epoch", 0),
                                                     num_field(in, "new_last", 0),
                                                     num_field(in, "latest_seq", 0));
        } else if (!strcmp(func, "row_state")) {
            out = w19_hostedroom_beaf1a_row_state(bool_field(in, "found", 0),
                                                  num_field(in, "room_count", 0),
                                                  num_field(in, "stored_epoch", 0),
                                                  num_field(in, "stored_last_seq", 0),
                                                  num_field(in, "stored_bytes", 0));
        } else if (!strcmp(func, "ingest_plan")) {
            out = w19_hostedroom_beaf1a_ingest_plan(in);
        } else if (!strcmp(func, "promote_plan")) {
            out = w19_hostedroom_beaf1a_promote_plan(in);
        } else if (!strcmp(func, "demote_plan")) {
            out = w19_hostedroom_beaf1a_demote_plan(in);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
