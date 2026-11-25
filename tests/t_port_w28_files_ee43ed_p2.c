/* JSON-lines harness for the w28_files_ee43ed_p2 gate
 * (listing / entry shaping / size & mime). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_files_ee43ed_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "fs_mime_type")) {
        char out[128];
        w28_files_ee43ed_p2_fs_mime_type(json_get_str(in, "path", ""), out, sizeof(out));
        return json_string(out);
    }
    if (!strcmp(func, "preview_governing")) return w28_files_ee43ed_p2_preview_governing(in);
    if (!strcmp(func, "looks_binary")) return json_bool(w28_files_ee43ed_p2_looks_binary(json_obj_get(in, "data")));
    if (!strcmp(func, "managed_response_meta")) return w28_files_ee43ed_p2_managed_response_meta(in);
    if (!strcmp(func, "managed_files_policy")) return w28_files_ee43ed_p2_managed_files_policy(in);
    if (!strcmp(func, "managed_file_size")) return w28_files_ee43ed_p2_managed_file_size(in);
    if (!strcmp(func, "managed_file_entry")) return w28_files_ee43ed_p2_managed_file_entry(in);
    if (!strcmp(func, "fs_list_sort")) return w28_files_ee43ed_p2_fs_list_sort(json_obj_get(in, "entries"));
    if (!strcmp(func, "managed_list_sort")) return w28_files_ee43ed_p2_managed_list_sort(json_obj_get(in, "entries"));
    if (!strcmp(func, "list_has_parent")) {
        return json_bool(w28_files_ee43ed_p2_list_has_parent(
            json_get_str(in, "target", ""), json_get_str(in, "locked_root", ""),
            json_get_bool(in, "nt", true) ? 1 : 0));
    }
    if (!strcmp(func, "is_readdir_hidden")) {
        return json_bool(w28_files_ee43ed_p2_is_readdir_hidden(json_get_str(in, "name", "")));
    }
    if (!strcmp(func, "backend_error")) return w28_files_ee43ed_p2_backend_error(json_get_str(in, "code", ""));
    if (!strcmp(func, "media_extension_verdict")) return w28_files_ee43ed_p2_media_extension_verdict(in);
    if (!strcmp(func, "sandbox_not_found_detail")) {
        return json_string(w28_files_ee43ed_p2_sandbox_not_found_detail(json_get_bool(in, "unmapped", false)));
    }
    return json_string("UNKNOWN_FUNC");
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
        out = dispatch(in, func);
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        fputs(",", stdout);
        emit_key("case", json_copy(json_obj_get(in, "case")));
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func") || !strcmp(k, "case")) continue;
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
