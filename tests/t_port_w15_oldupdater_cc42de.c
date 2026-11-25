#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_oldupdater_cc42de.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        char *s = NULL;
        if (!strcmp(func, "capture_active_lazy_features")) {
            s = w15_oldupdater_cc42de_capture_active_lazy_features();
        }
        else if (!strcmp(func, "refresh_active_lazy_features")) {
            s = w15_oldupdater_cc42de_refresh_active_lazy_features();
        }
        else if (!strcmp(func, "refresh_active_memory_provider_dependencies")) {
            s = w15_oldupdater_cc42de_refresh_active_memory_provider_dependencies();
        }
        else if (!strcmp(func, "npm_lockfile_changed")) {
            s = w15_oldupdater_cc42de_npm_lockfile_changed();
        }
        else if (!strcmp(func, "update_node_dependencies")) {
            s = w15_oldupdater_cc42de_update_node_dependencies();
        }
        else if (!strcmp(func, "rebuild_desktop_after_update")) {
            s = w15_oldupdater_cc42de_rebuild_desktop_after_update();
        }
        else if (!strcmp(func, "write_update_incomplete_marker")) {
            s = w15_oldupdater_cc42de_write_update_incomplete_marker();
        }
        else if (!strcmp(func, "path_uid")) {
            s = w15_oldupdater_cc42de_path_uid();
        }
        else if (!strcmp(func, "leftover_pausable_gateway_pids")) {
            s = w15_oldupdater_cc42de_leftover_pausable_gateway_pids();
        }
        else if (!strcmp(func, "ledger_manual_serve_holders")) {
            s = w15_oldupdater_cc42de_ledger_manual_serve_holders();
        }
        else if (!strcmp(func, "relaunch_stopped_serves")) {
            s = w15_oldupdater_cc42de_relaunch_stopped_serves(json_get_bool(in, "pending", false) ? 1 : 0);
        }
        else if (!strcmp(func, "orphaned_desktop_backend_pids")) {
            s = w15_oldupdater_cc42de_orphaned_desktop_backend_pids();
        }
        else if (!strcmp(func, "ledger_reapable_backend_pids")) {
            s = w15_oldupdater_cc42de_ledger_reapable_backend_pids();
        }
        else if (!strcmp(func, "handoff_reapable_backend_pids")) {
            s = w15_oldupdater_cc42de_handoff_reapable_backend_pids();
        }
        else if (!strcmp(func, "stop_process_trees")) {
            s = w15_oldupdater_cc42de_stop_process_trees();
        }
        else { out = json_string("UNKNOWN_FUNC"); }
        if (s) { out = json_string(s); free(s); }
        else if (out == NULL) { out = json_null(); }
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
