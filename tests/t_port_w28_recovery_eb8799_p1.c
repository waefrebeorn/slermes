/* JSON-lines harness for the w28_recovery_eb8799_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_recovery_eb8799_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "sidecar_path")) {
            out = w28_rec_p1_sidecar_path(json_get_str(in, "path", ""),
                                         json_get_str(in, "suffix", ""));
        } else if (!strcmp(func, "format_bytes")) {
            out = w28_rec_p1_format_bytes(json_get_num(in, "value", 0.0));
        } else if (!strcmp(func, "disk_space_preflight")) {
            out = w28_rec_p1_disk_space_preflight(in);
        } else if (!strcmp(func, "quoted_columns")) {
            out = w28_rec_p1_quoted_columns(json_obj_get(in, "columns"));
        } else if (!strcmp(func, "compatible_columns")) {
            out = w28_rec_p1_compatible_columns(json_get_str(in, "table", ""),
                                                json_obj_get(in, "source"),
                                                json_obj_get(in, "destination"));
        } else if (!strcmp(func, "table_inventory")) {
            out = w28_rec_p1_table_inventory(in);
        } else if (!strcmp(func, "inspect")) {
            out = w28_rec_p1_inspect_connection(in);
        } else if (!strcmp(func, "append_skipped_range")) {
            out = w28_rec_p1_append_skipped_range(json_obj_get(in, "ranges"),
                                                  json_get_num(in, "low", 0.0),
                                                  json_get_num(in, "high", 0.0),
                                                  json_get_str(in, "error", ""));
        } else if (!strcmp(func, "state_meta_precheck")) {
            out = w28_rec_p1_state_meta_precheck(in);
        } else if (!strcmp(func, "reconcile")) {
            out = w28_rec_p1_reconcile(json_get_str(in, "table", ""),
                                       json_get_str(in, "where", ""),
                                       json_get_str(in, "mutation", ""),
                                       json_get_num(in, "count", 0.0));
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
