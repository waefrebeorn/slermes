/* JSON-lines harness for the w31_ops_0ed859_p2 gate
 * (ops payload / argv derivation).
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_ops_0ed859_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "pool_entry_summary"))
        return w31_ops_0ed859_p2_pool_entry_summary(in);
    if (!strcmp(func, "safe_backup_upload_name")) {
        char out[512];
        int nt = json_get_bool(in, "nt", true) ? 1 : 0;
        const json_t *fn = json_obj_get(in, "filename");
        w31_ops_0ed859_p2_safe_backup_upload_name(
            (fn && fn->type == JSON_STRING) ? fn->str_val : NULL, nt, out, sizeof(out));
        return json_string(out);
    }
    if (!strcmp(func, "hook_body_fields"))
        return w31_ops_0ed859_p2_hook_body_fields(in);
    if (!strcmp(func, "hook_event_verdict"))
        return w31_ops_0ed859_p2_hook_event_verdict(in);
    if (!strcmp(func, "spawn_import_argv"))
        return w31_ops_0ed859_p2_spawn_import_argv(in);
    if (!strcmp(func, "memory_reset_target"))
        return w31_ops_0ed859_p2_memory_reset_target(in);
    if (!strcmp(func, "pairing_approve_target"))
        return w31_ops_0ed859_p2_pairing_approve_target(in);
    if (!strcmp(func, "backup_download_verdict"))
        return w31_ops_0ed859_p2_backup_download_verdict(in);
    return json_string("UNKNOWN_FUNC");
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
        out = dispatch(in, func);
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
