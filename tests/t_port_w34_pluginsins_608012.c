/* JSON-lines harness for the w34_pluginsins_608012 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_pluginsins_608012.h"

static const char *S(json_t *o, const char *k) {
    const char *v = o ? json_get_str(o, k, "") : "";
    return v ? v : "";
}

static json_t *arr_of(json_t *o, const char *k) {
    json_t *v = o ? json_obj_get(o, k) : NULL;
    return v && v->type == JSON_ARRAY ? v : NULL;
}

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "path_class")) {
            out = w34_pluginsins_608012_path_class(S(in, "rel"));
        } else if (!strcmp(func, "should_allow")) {
            json_t *fv = json_obj_get(in, "force");
            int force = fv && fv->type == JSON_BOOL ? (fv->bool_val ? 1 : 0)
                                                    : (int)json_get_num(in, "force", 0);
            out = w34_pluginsins_608012_should_allow(S(in, "verdict"), arr_of(in, "findings"), force);
        } else if (!strcmp(func, "dangerous_summary")) {
            out = w34_pluginsins_608012_dangerous_summary(arr_of(in, "findings"));
        } else if (!strcmp(func, "scan_summary")) {
            out = w34_pluginsins_608012_scan_summary(S(in, "name"), S(in, "verdict"),
                                                     arr_of(in, "findings"));
        } else if (!strcmp(func, "provenance")) {
            out = w34_pluginsins_608012_provenance(S(in, "verdict"), S(in, "source"));
        } else if (!strcmp(func, "severity_at_most")) {
            out = w34_pluginsins_608012_severity_at_most(S(in, "sev"), S(in, "cap"));
        } else if (!strcmp(func, "comment_severity")) {
            out = w34_pluginsins_608012_comment_severity(S(in, "pattern_id"), S(in, "category"),
                                                         S(in, "sev"));
        } else if (!strcmp(func, "docstring_cap")) {
            out = w34_pluginsins_608012_docstring_cap(S(in, "pattern_id"), S(in, "category"),
                                                      S(in, "sev"));
        } else if (!strcmp(func, "prose_cap")) {
            out = w34_pluginsins_608012_prose_cap(S(in, "pattern_id"), S(in, "category"), S(in, "sev"));
        } else if (!strcmp(func, "catalog_cap")) {
            out = w34_pluginsins_608012_catalog_cap(S(in, "pattern_id"), S(in, "category"),
                                                    S(in, "sev"));
        } else if (!strcmp(func, "is_agent_facing")) {
            out = json_bool(w34_pluginsins_608012_is_agent_facing(S(in, "pattern_id"),
                                                                  S(in, "category")));
        } else if (!strcmp(func, "is_defensive_documentation")) {
            out = json_bool(w34_pluginsins_608012_is_defensive_documentation(
                S(in, "match"), S(in, "rel"), (int)json_get_num(in, "line", 0),
                arr_of(in, "docstring_lines")));
        } else if (!strcmp(func, "filter_findings")) {
            out = w34_pluginsins_608012_filter_findings(arr_of(in, "findings"), S(in, "rel"),
                                                        arr_of(in, "lines"),
                                                        arr_of(in, "docstring_lines"),
                                                        arr_of(in, "main_guard_lines"));
        } else if (!strcmp(func, "logical_line")) {
            out = w34_pluginsins_608012_logical_line(arr_of(in, "lines"),
                                                     (int)json_get_num(in, "index", 0));
        } else if (!strcmp(func, "limits")) {
            out = w34_pluginsins_608012_limits();
        } else if (!strcmp(func, "size_finding")) {
            out = w34_pluginsins_608012_size_finding(S(in, "rel"), json_get_num(in, "size", 0));
        } else if (!strcmp(func, "binary_finding")) {
            out = w34_pluginsins_608012_binary_finding(S(in, "rel"));
        } else if (!strcmp(func, "bytecode_finding")) {
            out = w34_pluginsins_608012_bytecode_finding(S(in, "rel"));
        } else if (!strcmp(func, "manifest_findings")) {
            out = w34_pluginsins_608012_manifest_findings(json_get_num(in, "file_count", 0),
                                                          json_get_num(in, "total_size", 0));
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
