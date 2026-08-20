/* t_port_plugins_toolsets.c — oracle harness for the hermes_cli/plugins.py +
 * hermes_cli/tools_config.py pure-logic ports.
 *
 * Reads a fixture file: one directive per line, `OP<TAB>arg[<TAB>arg...]`.
 * Emits one JSON record per directive. tests/sta_oracle_plugins_toolsets.py
 * replays the SAME directives against the live Python modules and the runner
 * diffs the two streams.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plugins_core.h"
#include "tools_config_toolsets.h"
#include "hermes_json.h"

static void emit_str(const char *op, const char *arg, const char *value) {
    json_t *o = json_object();
    json_set(o, "op", json_string(op));
    json_set(o, "arg", json_string(arg ? arg : ""));
    json_set(o, "value", value ? json_string(value) : json_null());
    char *t = json_serialize(o);
    printf("%s\n", t ? t : "{}");
    free(t);
    json_free(o);
}

static void emit_bool(const char *op, const char *arg, bool value) {
    json_t *o = json_object();
    json_set(o, "op", json_string(op));
    json_set(o, "arg", json_string(arg ? arg : ""));
    json_set(o, "value", json_bool(value));
    char *t = json_serialize(o);
    printf("%s\n", t ? t : "{}");
    free(t);
    json_free(o);
}

static void emit_list(const char *op, const char *arg, char **items, size_t n) {
    json_t *o = json_object();
    json_set(o, "op", json_string(op));
    json_set(o, "arg", json_string(arg ? arg : ""));
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++)
        json_append(arr, json_string(items[i] ? items[i] : ""));
    json_set(o, "value", arr);
    char *t = json_serialize(o);
    printf("%s\n", t ? t : "{}");
    free(t);
    json_free(o);
}

static void emit_clist(const char *op, const char *arg, const char **items,
                       size_t n) {
    json_t *o = json_object();
    json_set(o, "op", json_string(op));
    json_set(o, "arg", json_string(arg ? arg : ""));
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++)
        json_append(arr, json_string(items[i] ? items[i] : ""));
    json_set(o, "value", arr);
    char *t = json_serialize(o);
    printf("%s\n", t ? t : "{}");
    free(t);
    json_free(o);
}

/* Split a tab-separated line into fields (in place). */
static size_t split_tabs(char *line, char **out, size_t max) {
    size_t n = 0;
    char *p = line;
    while (n < max) {
        out[n++] = p;
        char *tab = strchr(p, '\t');
        if (!tab) break;
        *tab = '\0';
        p = tab + 1;
    }
    return n;
}

/* Semicolon-separated dependency list: "key:name:dep1,dep2;key2:name2:" */
static void run_load_order(const char *spec) {
    char *copy = strdup(spec ? spec : "");
    plugins_dep_node_t nodes[16];
    char *reqbufs[16][8];
    size_t n_nodes = 0;
    memset(nodes, 0, sizeof nodes);

    char *saveptr = NULL;
    for (char *tok = strtok_r(copy, ";", &saveptr);
         tok && n_nodes < 16; tok = strtok_r(NULL, ";", &saveptr)) {
        char *key = tok;
        char *name = strchr(key, ':');
        if (!name) continue;
        *name++ = '\0';
        char *reqs = strchr(name, ':');
        if (reqs) *reqs++ = '\0';
        nodes[n_nodes].key = key;
        nodes[n_nodes].name = name;
        size_t rn = 0;
        if (reqs && *reqs) {
            char *rp = reqs;
            while (rp && *rp && rn < 7) {
                char *comma = strchr(rp, ',');
                if (comma) *comma = '\0';
                reqbufs[n_nodes][rn++] = rp;
                rp = comma ? comma + 1 : NULL;
            }
        }
        reqbufs[n_nodes][rn] = NULL;
        nodes[n_nodes].requires = (const char *const *)reqbufs[n_nodes];
        n_nodes++;
    }
    size_t out_n = 0;
    const char **order = plugins_resolve_plugin_load_order(nodes, n_nodes,
                                                           &out_n);
    emit_clist("load_order", spec, order, out_n);
    free((void *)order);
    free(copy);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("fopen"); return 2; }

    char line[8192];
    while (fgets(line, sizeof line, f)) {
        size_t len = strlen(line);
        while (len && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (!len || line[0] == '#') continue;

        char *fields[8];
        size_t nf = split_tabs(line, fields, 8);
        const char *op = fields[0];
        const char *a1 = nf > 1 ? fields[1] : "";
        const char *a2 = nf > 2 ? fields[2] : "";

        if (strcmp(op, "valid_section_id") == 0) {
            emit_bool(op, a1, plugins_is_valid_system_prompt_section_id(a1));

        } else if (strcmp(op, "format_section") == 0) {
            char *s = plugins_format_system_prompt_section(a1, a2);
            emit_str(op, a1, s);
            free(s);

        } else if (strcmp(op, "format_sections") == 0) {
            /* a1 = "id1=body1|id2=body2" */
            char *copy = strdup(a1);
            plugins_section_t secs[8];
            size_t n = 0;
            char *sp = NULL;
            for (char *tok = strtok_r(copy, "|", &sp); tok && n < 8;
                 tok = strtok_r(NULL, "|", &sp)) {
                char *eq = strchr(tok, '=');
                if (!eq) continue;
                *eq = '\0';
                secs[n].id = tok;
                secs[n].content = eq + 1;
                n++;
            }
            char *s = plugins_format_system_prompt_sections(secs, n);
            emit_str(op, a1, s);
            free(s);
            free(copy);

        } else if (strcmp(op, "detect_kind") == 0) {
            emit_str(op, a1, plugins_detect_kind_from_source(a1));

        } else if (strcmp(op, "skill_namespace") == 0) {
            char *s = plugins_portable_skill_namespace(a1);
            emit_str(op, a1, s);
            free(s);

        } else if (strcmp(op, "load_order") == 0) {
            run_load_order(a1);

        } else if (strcmp(op, "web_caps") == 0) {
            size_t n = 0;
            char **caps = tools_config_web_provider_capabilities(a1, &n);
            emit_list(op, a1, caps, n);
            tools_config_free_list(caps, n);

        } else if (strcmp(op, "checklist_keys") == 0) {
            size_t n = 0;
            char **keys = tools_config_checklist_toolset_keys(a1, &n);
            emit_list(op, a1, keys, n);
            tools_config_free_list(keys, n);

        } else if (strcmp(op, "known_platforms") == 0) {
            size_t n = 0;
            char **p = tools_config_known_tool_platforms(&n);
            emit_list(op, a1, p, n);
            tools_config_free_list(p, n);

        } else if (strcmp(op, "effective_toolsets") == 0) {
            size_t n = 0;
            tools_config_toolset_row_t *rows =
                tools_config_get_effective_configurable_toolsets(&n);
            json_t *o = json_object();
            json_set(o, "op", json_string(op));
            json_set(o, "arg", json_string(a1));
            json_t *arr = json_array();
            for (size_t i = 0; i < n; i++) {
                json_t *row = json_object();
                json_set(row, "key", json_string(rows[i].key ?: ""));
                json_set(row, "label", json_string(rows[i].label ?: ""));
                json_set(row, "description",
                         json_string(rows[i].description ?: ""));
                json_append(arr, row);
            }
            json_set(o, "value", arr);
            char *t = json_serialize(o);
            printf("%s\n", t ? t : "{}");
            free(t);
            json_free(o);
            tools_config_free_toolset_rows(rows, n);

        } else if (strcmp(op, "apply_toolset_change") == 0) {
            /* a1 = config JSON, a2 = "platform:action:ts1,ts2" */
            json_t *cfg = json_parse(a1, NULL);
            char *spec = strdup(a2);
            char *platform = spec;
            char *action = strchr(platform, ':');
            char *names = NULL;
            if (action) {
                *action++ = '\0';
                names = strchr(action, ':');
                if (names) *names++ = '\0';
            }
            const char *list[8];
            size_t ln = 0;
            if (names && *names) {
                char *np = names;
                while (np && *np && ln < 8) {
                    char *comma = strchr(np, ',');
                    if (comma) *comma = '\0';
                    list[ln++] = np;
                    np = comma ? comma + 1 : NULL;
                }
            }
            if (cfg) {
                tools_config_apply_toolset_change(cfg, platform, list, ln,
                                                  action ? action : "enable");
                char *t = json_serialize(cfg);
                emit_str(op, a2, t);
                free(t);
                json_free(cfg);
            } else {
                emit_str(op, a2, NULL);
            }
            free(spec);

        } else if (strcmp(op, "reconfigure_enabled") == 0) {
            /* a1 = toolset key, a2 = config JSON */
            json_t *cfg = json_parse(a2, NULL);
            emit_bool(op, a1,
                      tools_config_toolset_enabled_for_reconfigure(a1, cfg));
            if (cfg) json_free(cfg);

        } else if (strcmp(op, "post_setup_keys") == 0) {
            size_t n = 0;
            char **keys = tools_config_valid_post_setup_keys(&n);
            emit_list(op, a1, keys, n);
            tools_config_free_list(keys, n);

        } else if (strcmp(op, "restorable_dep") == 0) {
            const char *module = NULL;
            const char *const *args = NULL;
            bool ok = tools_config_restorable_python_tool_dependency(
                a1, &module, &args);
            json_t *o = json_object();
            json_set(o, "op", json_string(op));
            json_set(o, "arg", json_string(a1));
            if (!ok) {
                json_set(o, "value", json_null());
            } else {
                json_t *v = json_object();
                json_set(v, "module", json_string(module ?: ""));
                json_t *arr = json_array();
                for (const char *const *p = args; p && *p; p++)
                    json_append(arr, json_string(*p));
                json_set(v, "install_args", arr);
                json_set(o, "value", v);
            }
            char *t = json_serialize(o);
            printf("%s\n", t ? t : "{}");
            free(t);
            json_free(o);

        } else if (strcmp(op, "detect_active_provider") == 0) {
            json_t *providers = json_parse(a1, NULL);
            size_t idx = tools_config_detect_active_provider_index(providers,
                                                                   NULL);
            json_t *o = json_object();
            json_set(o, "op", json_string(op));
            json_set(o, "arg", json_string(a1));
            json_set(o, "value", json_number((double)idx));
            char *t = json_serialize(o);
            printf("%s\n", t ? t : "{}");
            free(t);
            json_free(o);
            if (providers) json_free(providers);

        } else if (strcmp(op, "scan_directory") == 0) {
            /* a1 = directory (relative to ORACLE_FIXDIR), a2 = source label */
            const char *fixdir = getenv("ORACLE_FIXDIR");
            char path[4096];
            snprintf(path, sizeof path, "%s/%s", fixdir ? fixdir : ".", a1);
            plugins_scan_result_t *r = plugins_scan_directory(path, a2, NULL);
            json_t *o = json_object();
            json_set(o, "op", json_string(op));
            json_set(o, "arg", json_string(a1));
            json_t *arr = json_array();
            for (size_t i = 0; i < plugins_scan_count(r); i++) {
                json_t *m = json_object();
                json_set(m, "key", json_string(plugins_scan_key(r, i) ?: ""));
                json_set(m, "name", json_string(plugins_scan_name(r, i) ?: ""));
                json_set(m, "source",
                         json_string(plugins_scan_source(r, i) ?: ""));
                json_set(m, "kind", json_string(plugins_scan_kind(r, i) ?: ""));
                json_set(m, "portable",
                         json_bool(plugins_scan_portable(r, i)));
                json_append(arr, m);
            }
            json_set(o, "value", arr);
            char *t = json_serialize(o);
            printf("%s\n", t ? t : "{}");
            free(t);
            json_free(o);
            plugins_scan_free(r);

        } else if (strcmp(op, "skill_dirs") == 0) {
            const char *fixdir = getenv("ORACLE_FIXDIR");
            char path[4096];
            snprintf(path, sizeof path, "%s/%s", fixdir ? fixdir : ".", a1);
            size_t n = 0;
            char **dirs = plugins_list_skill_dirs(path, &n);
            emit_list(op, a1, dirs, n);
            plugins_free_list(dirs, n);

        } else if (strcmp(op, "cache_roundtrip") == 0) {
            /* a1 = "k1,k2", a2 = "m1,m2" -> persist then read back */
            char *c1 = strdup(a1), *c2 = strdup(a2);
            const char *keys[8], *mcp[8];
            size_t kn = 0, mn = 0;
            for (char *p = c1; p && *p && kn < 8;) {
                char *comma = strchr(p, ',');
                if (comma) *comma = '\0';
                if (*p) keys[kn++] = p;
                p = comma ? comma + 1 : NULL;
            }
            for (char *p = c2; p && *p && mn < 8;) {
                char *comma = strchr(p, ',');
                if (comma) *comma = '\0';
                if (*p) mcp[mn++] = p;
                p = comma ? comma + 1 : NULL;
            }
            tools_config_free_list(NULL, 0);
            plugins_persist_toolset_keys(keys, kn, mcp, mn);
            char *blob = plugins_read_keys_cache();
            /* Re-serialize so key order is canonical on both sides. */
            json_t *parsed = blob ? json_parse(blob, NULL) : NULL;
            char *canon = parsed ? json_serialize(parsed) : NULL;
            emit_str(op, a1, canon ? canon : blob);
            free(canon);
            if (parsed) json_free(parsed);
            free(blob);
            free(c1);
            free(c2);

        } else {
            emit_str("UNKNOWN_OP", op, NULL);
        }
    }
    fclose(f);
    return 0;
}
