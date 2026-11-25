/* tests/t_port_w33_maintenanc_3e1b75_p2.c
 * JSON-lines harness for persistence/sqlite/housekeeping/maintenance.py
 * filter assembly, guards, reports and statements (gate
 * w33_maintenanc_3e1b75_p2).  One request per stdin line; one response line
 * carrying "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_maintenanc_3e1b75_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "prune_filter_names")) {
            out = w33_maintenanc_3e1b75_p2_prune_filter_names();
        } else if (!strcmp(func, "prune_filter_where")) {
            out = w33_maintenanc_3e1b75_p2_prune_filter_where(in);
        } else if (!strcmp(func, "prune_where")) {
            out = w33_maintenanc_3e1b75_p2_prune_where(in);
        } else if (!strcmp(func, "count_open_sql")) {
            out = w33_maintenanc_3e1b75_p2_count_open_sql(in);
        } else if (!strcmp(func, "prune_open_guard")) {
            out = w33_maintenanc_3e1b75_p2_prune_open_guard(json_get_bool(in, "include_open", false));
        } else if (!strcmp(func, "reclaim_estimate")) {
            out = w33_maintenanc_3e1b75_p2_reclaim_estimate(in);
        } else if (!strcmp(func, "export_safe")) {
            out = w33_maintenanc_3e1b75_p2_export_safe(in);
        } else if (!strcmp(func, "tree_key")) {
            out = w33_maintenanc_3e1b75_p2_tree_key(json_get_str(in, "path", ""));
        } else if (!strcmp(func, "inside_tree")) {
            out = w33_maintenanc_3e1b75_p2_inside_tree(in);
        } else if (!strcmp(func, "tree_serve_facts")) {
            out = w33_maintenanc_3e1b75_p2_tree_serve_facts(in);
        } else if (!strcmp(func, "auto_archive_extra_set")) {
            out = w33_maintenanc_3e1b75_p2_auto_archive_extra_set();
        } else if (!strcmp(func, "heartbeat_register")) {
            out = w33_maintenanc_3e1b75_p2_heartbeat_register(in);
        } else if (!strcmp(func, "heartbeat_delete")) {
            out = w33_maintenanc_3e1b75_p2_heartbeat_delete(in);
        } else if (!strcmp(func, "prune_batches")) {
            out = w33_maintenanc_3e1b75_p2_prune_batches(in);
        } else if (!strcmp(func, "retained_tree_sql")) {
            out = w33_maintenanc_3e1b75_p2_retained_tree_sql(json_get_bool(in, "pending_only", false));
        } else if (!strcmp(func, "served_tree_sql")) {
            out = w33_maintenanc_3e1b75_p2_served_tree_sql();
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
