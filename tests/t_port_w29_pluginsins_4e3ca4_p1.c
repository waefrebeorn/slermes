/* JSON-lines harness for the w29_pluginsins_4e3ca4_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_pluginsins_4e3ca4_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *opt_str(const char *v) { return v ? json_string(v) : json_null(); }

static json_t *dispatch(const char *func, json_t *in) {
    noc_finding_t f;
    const char *line = json_get_str(in, "line", "");
    const char *rel = json_get_str(in, "rel_path", "");
    f.pattern_id = json_get_str(in, "pid", "");
    f.severity = json_get_str(in, "sev", "");
    f.category = json_get_str(in, "cat", "");

    if (!strcmp(func, "is_doc_prose")) return json_bool(noc_pis_p1_is_doc_prose(rel));
    if (!strcmp(func, "is_ci_workflow")) return json_bool(noc_pis_p1_is_ci_workflow(rel));
    if (!strcmp(func, "is_agent_facing")) return json_bool(noc_pis_p1_is_agent_facing(&f));
    if (!strcmp(func, "prose_cap")) return opt_str(noc_pis_p1_prose_cap(&f));
    if (!strcmp(func, "is_trusted_installer_pipe"))
        return json_bool(noc_pis_p1_is_trusted_installer_pipe(&f, line));
    if (!strcmp(func, "is_locale_catalog")) return json_bool(noc_pis_p1_is_locale_catalog(rel));
    if (!strcmp(func, "catalog_cap")) return opt_str(noc_pis_p1_catalog_cap(&f));
    if (!strcmp(func, "is_self_uninstall_doc"))
        return json_bool(noc_pis_p1_is_self_uninstall_doc(&f, line));
    if (!strcmp(func, "is_test_tree")) return json_bool(noc_pis_p1_is_test_tree(rel));
    if (!strcmp(func, "is_disclosed_config_note"))
        return json_bool(noc_pis_p1_is_disclosed_config_note(&f, rel));
    if (!strcmp(func, "is_json_prose_value"))
        return json_bool(noc_pis_p1_is_json_prose_value(&f, rel, line));
    if (!strcmp(func, "is_repo_root_traversal"))
        return json_bool(noc_pis_p1_is_repo_root_traversal(&f, line));
    if (!strcmp(func, "is_pip_install_in_prose_literal"))
        return json_bool(noc_pis_p1_is_pip_install_in_prose_literal(&f, line));
    if (!strcmp(func, "is_hex_in_char_class"))
        return json_bool(noc_pis_p1_is_hex_in_char_class(&f, line));
    if (!strcmp(func, "logical_line"))
        return noc_pis_p1_logical_line(json_obj_get(in, "lines"), (int)json_get_num(in, "index", 0));
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
        if (!*func) { json_free(in); continue; }
        out = dispatch(func, in);
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
