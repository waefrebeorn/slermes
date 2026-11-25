/* JSON-lines harness for the w28_files_ee43ed_p1 gate
 * (path scoping & sanitisation).
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_files_ee43ed_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "is_sensitive_filename")) {
        return json_bool(w28_files_ee43ed_p1_is_sensitive_filename(json_get_str(in, "name", "")));
    }
    if (!strcmp(func, "is_sensitive_path")) {
        return json_bool(w28_files_ee43ed_p1_is_sensitive_path(json_get_str(in, "path", "")));
    }
    if (!strcmp(func, "deny_path_under")) {
        return json_bool(w28_files_ee43ed_p1_deny_path_under(
            json_get_str(in, "resolved", ""), json_get_str(in, "base", ""),
            json_get_bool(in, "nt", true) ? 1 : 0));
    }
    if (!strcmp(func, "is_local_sensitive_path")) {
        return json_bool(w28_files_ee43ed_p1_is_local_sensitive_path(in));
    }
    if (!strcmp(func, "sanitize_filename")) {
        char out[512];
        w28_files_ee43ed_p1_sanitize_filename(json_get_str(in, "name", ""),
                                             json_get_bool(in, "nt", true) ? 1 : 0, out, sizeof(out));
        return json_string(out);
    }
    if (!strcmp(func, "filename_stem")) {
        char out[512];
        w28_files_ee43ed_p1_filename_stem(json_get_str(in, "name", ""),
                                         json_get_bool(in, "nt", true) ? 1 : 0, out, sizeof(out));
        return json_string(out);
    }
    if (!strcmp(func, "path_text")) return w28_files_ee43ed_p1_path_text(in);
    if (!strcmp(func, "path_is_under")) {
        return json_bool(w28_files_ee43ed_p1_path_is_under(
            json_get_str(in, "root", ""), json_get_str(in, "target", ""),
            json_get_bool(in, "nt", true) ? 1 : 0));
    }
    if (!strcmp(func, "file_url_path")) return w28_files_ee43ed_p1_file_url_path(in);
    if (!strcmp(func, "agent_container_translatable")) {
        return json_bool(w28_files_ee43ed_p1_agent_container_translatable(json_get_str(in, "raw", "")));
    }
    if (!strcmp(func, "unmapped_container_path")) return json_bool(w28_files_ee43ed_p1_unmapped_container_path(in));
    if (!strcmp(func, "managed_path_verdict")) return w28_files_ee43ed_p1_managed_path_verdict(in);
    if (!strcmp(func, "if_none_match")) return w28_files_ee43ed_p1_if_none_match(json_get_str(in, "header", ""));
    if (!strcmp(func, "if_none_match_hit")) {
        return json_bool(w28_files_ee43ed_p1_if_none_match_hit(
            json_get_str(in, "header", ""), json_get_str(in, "etag", "")));
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
