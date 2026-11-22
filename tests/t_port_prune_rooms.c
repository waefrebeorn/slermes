/* t_port_prune_rooms.c — harness for hosted_rooms_gaps3.c
 * prune_disbanded_rooms_locked. Reads a seed + params, builds an in-memory
 * sqlite DB, runs the port, emits {"out":<count>,"db":<canonical dump>}.
 * Gate: run_one_oracle.sh prune_rooms oracle_prune_rooms.py ""
 *       "src/gateway/hosted_rooms_gaps3.o lib/libjson/json.o lib/libdb/sqlite3.o"
 */
#include "hermes_json.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int prune_disbanded_rooms_locked(void *conn, double now, int has_now,
                                 long long max_bytes, int has_max);

static const char *const DEPS[] = {
    "hosted_room_policy_transcript_state", "hosted_room_policy_transcript",
    "hosted_room_policy_publications", "hosted_room_policy_watermarks",
    "hosted_room_policy_events", "hosted_room_policy_threads",
    "hosted_room_policy_cursors", "hosted_room_driver_tasks",
    "hosted_room_driver_leases", "hosted_room_remote_runs", "hosted_room_links",
    "hosted_room_peer_reservations", "hosted_room_events"};
static void exec(sqlite3 *db, const char *sql) {
    char *err = NULL;
    sqlite3_exec(db, sql, NULL, NULL, &err);
    sqlite3_free(err);
}
static const char *js(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static double pj_num(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static void emit_json(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    if (v) { char *s = json_dumps(v, 0); if (s) { fputs(s, stdout); free(s); } }
    else fputs("null", stdout);
}
static int haskey(json_t *o, const char *k) {
    return json_obj_get(o, k) != NULL;
}
static json_t *dump_table(sqlite3 *db, const char *table, const char *col2) {
    json_t *rows = json_array();
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT room_id, %s FROM %s ORDER BY room_id, %s", col2, table, col2);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            json_t *r = json_array();
            const unsigned char *a = sqlite3_column_text(st, 0);
            const unsigned char *b = sqlite3_column_text(st, 1);
            json_array_append(r, json_string(a ? (const char *)a : ""));
            json_array_append(r, json_string(b ? (const char *)b : "None"));
            json_array_append(rows, r);
        }
    }
    sqlite3_finalize(st);
    return rows;
}
int main(void) {
    char line[131072];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        sqlite3 *db = NULL;
        sqlite3_open(":memory:", &db);
        exec(db, "CREATE TABLE hosted_rooms(room_id TEXT PRIMARY KEY, disbanded_at REAL, event_bytes INTEGER)");
        exec(db, "CREATE TABLE hosted_room_retired_ids(room_id TEXT, retired_at REAL)");
        for (size_t i = 0; i < sizeof(DEPS)/sizeof(DEPS[0]); i++) {
            char sql[200];
            snprintf(sql, sizeof(sql), "CREATE TABLE %s(room_id TEXT, v TEXT)", DEPS[i]);
            exec(db, sql);
        }
        json_t *rooms = json_obj_get(in, "rooms");
        if (rooms && rooms->type == JSON_ARRAY) {
            size_t nr = json_array_size(rooms);
            for (size_t i = 0; i < nr; i++) {
                json_t *r = json_array_get(rooms, i);
                if (!r || r->type != JSON_OBJECT) continue;
                json_t *dv = json_obj_get(r, "disbanded_at");
                char sql[512];
                if (!dv || dv->type == JSON_NULL)
                    snprintf(sql, sizeof(sql),
                        "INSERT OR REPLACE INTO hosted_rooms(room_id,disbanded_at,event_bytes) VALUES('%s',NULL,%lld)",
                        js(r, "room_id"), (long long)pj_num(r, "event_bytes"));
                else
                    snprintf(sql, sizeof(sql),
                        "INSERT OR REPLACE INTO hosted_rooms(room_id,disbanded_at,event_bytes) VALUES('%s',%.17g,%lld)",
                        js(r, "room_id"), pj_num(r, "disbanded_at"), (long long)pj_num(r, "event_bytes"));
                exec(db, sql);
            }
        }
        /* one dependent row per room, to observe the deletes */
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db, "SELECT room_id FROM hosted_rooms", -1, &st, NULL) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                const char *rid = (const char *)sqlite3_column_text(st, 0);
                char sql[300];
                snprintf(sql, sizeof(sql), "INSERT INTO hosted_room_events VALUES('%s','ev')", rid);
                exec(db, sql);
                snprintf(sql, sizeof(sql), "INSERT INTO hosted_room_links VALUES('%s','lk')", rid);
                exec(db, sql);
            }
        }
        sqlite3_finalize(st);

        int hn = haskey(in, "now"), hm = haskey(in, "max_bytes");
        int out = prune_disbanded_rooms_locked(db, pj_num(in, "now"), hn,
                                               (long long)pj_num(in, "max_bytes"), hm);
        char *s;
        printf("{\"rooms\":");
        json_t *rooms_in = json_obj_get(in, "rooms");
        s = rooms_in ? json_dumps(rooms_in, 0) : NULL;
        if (s) { fputs(s, stdout); free(s); } else fputs("[]", stdout);
        printf(",\"now\":"); emit_json(in, "now");
        printf(",\"max_bytes\":"); emit_json(in, "max_bytes");
        printf(",\"out\":%d,\"db\":{", out);
        printf("\"hosted_rooms\":");
        json_t *d = dump_table(db, "hosted_rooms", "disbanded_at");
        s = json_dumps(d, 0); if (s) { fputs(s, stdout); free(s); } else fputs("[]", stdout);
        json_free(d);
        printf(",\"retired\":");
        d = dump_table(db, "hosted_room_retired_ids", "retired_at");
        s = json_dumps(d, 0); if (s) { fputs(s, stdout); free(s); } else fputs("[]", stdout);
        json_free(d);
        printf(",\"events\":");
        d = dump_table(db, "hosted_room_events", "v");
        s = json_dumps(d, 0); if (s) { fputs(s, stdout); free(s); } else fputs("[]", stdout);
        json_free(d);
        printf(",\"links\":");
        d = dump_table(db, "hosted_room_links", "v");
        s = json_dumps(d, 0); if (s) { fputs(s, stdout); free(s); } else fputs("[]", stdout);
        json_free(d);
        printf("}}\n");
        sqlite3_close(db);
        json_free(in);
    }
    return 0;
}
