/*
 * t_port_web_server_sessions_admin.c — oracle harness for the sessions
 * admin cluster. Ops:
 *   {"op":"count_empty","db":"..."}
 *   {"op":"delete_empty","db":"..."}   (also reports post-state)
 *   {"op":"message_count","db":"...","sid":...}
 *   {"op":"session_count","db":"...","source":..,"include_archived":..,
 *    "archived_only":..,"exclude_children":..,"min_messages":N}
 *   {"op":"stats","db":"..."}
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "web_server_sessions_admin.h"

static const char *gs(json_t *o, const char *k) {
    json_t *v = o ? json_object_get(o, k) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    json_t *fx = json_parse(buf, NULL);
    if (!fx) return 2;
    const char *op = json_get_str(fx, "op", "");
    const char *db = json_get_str(fx, "db", "");

    if (strcmp(op, "count_empty") == 0) {
        printf("{\"count\":%d}\n", ws_sessions_count_empty(db));
    } else if (strcmp(op, "delete_empty") == 0) {
        int deleted = ws_sessions_delete_empty(db);
        ws_session_count_opts_t all = {.include_archived = true};
        printf("{\"deleted\":%d,\"remaining\":%d,\"empty_left\":%d}\n", deleted,
               ws_sessions_session_count(db, &all),
               ws_sessions_count_empty(db));
    } else if (strcmp(op, "message_count") == 0) {
        printf("{\"count\":%d}\n",
               ws_sessions_message_count(db, gs(fx, "sid")));
    } else if (strcmp(op, "session_count") == 0) {
        ws_session_count_opts_t o = {
            .source = gs(fx, "source"),
            .include_archived = json_get_bool(fx, "include_archived", false),
            .archived_only = json_get_bool(fx, "archived_only", false),
            .exclude_children = json_get_bool(fx, "exclude_children", false),
            .min_message_count = (int)json_get_num(fx, "min_messages", 0),
        };
        printf("{\"count\":%d}\n", ws_sessions_session_count(db, &o));
    } else if (strcmp(op, "stats") == 0) {
        json_t *r = ws_sessions_stats(db);
        char *d = json_dumps(r, 0);
        printf("%s\n", d);
        free(d);
        json_free(r);
    } else {
        return 2;
    }
    json_free(fx);
    return 0;
}
