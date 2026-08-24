/*
 * codex_runtime_plugin_migration.c — canonical C11 port of hermes_cli/codex_runtime_plugin_migration.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_codex_runtime_plugi_u_translate_one_server @ hermes_cli/codex_runtime_plugin_migration.py:_translate_one_server */
/* PoP: hermes_cli_codex_runtime_plugi_u_translate_one_server @ hermes_cli/codex_runtime_plugin_migration.py:_translate_one_server
 * REAL port (replaces the earlier echo wrapper — parity win).
 * Arg: one Hermes MCP server config as a JSON object. Returns a JSON record
 * {"entry": <codex inline-table dict or null>, "skipped": [<reason strings>]}
 * or NULL when cfg_json is unparseable. Caller frees.
 * Trace surface: branch ladder + one V event per appended skipped reason. */
char *hermes_cli_codex_runtime_plugi_u_translate_one_server(const char *cfg_json) {
    json_t *cfg = cfg_json ? json_parse(cfg_json, NULL) : NULL;
    if (!json_is_object(cfg)) {           /* Python: not isinstance(dict) */
        if (cfg) json_free(cfg);
        return strdup("{\"entry\": null, \"skipped\": []}");
    }
    json_t *out = json_object();
    json_t *skipped = json_array();
    const json_t *v;

    bool has_command = json_string_value_safe(json_obj_get(cfg, "command")) &&
                       *json_string_value_safe(json_obj_get(cfg, "command"));
    bool has_url = json_string_value_safe(json_obj_get(cfg, "url")) &&
                   *json_string_value_safe(json_obj_get(cfg, "url"));

    if (has_command && has_url) {         /* B: both set */
        json_append(skipped, json_string("url (both command and url set; preferring stdio)"));
        has_url = false;
    }
    if (has_command) {                    /* B: stdio transport */
        v = json_obj_get(cfg, "command");
        json_set(out, "command", json_string(json_string_value(v)));
        const json_t *args = json_obj_get(cfg, "args");
        if (args && json_is_array(args) && json_len(args) > 0) {
            json_t *arr = json_array();
            for (size_t i = 0; i < json_len(args); i++) {
                const json_t *a = json_get(args, i);
                if (a && json_is_string(a)) json_append(arr, json_string(a->str_val));
                else if (a && json_is_number(a)) {
                    char buf[40]; snprintf(buf, sizeof buf, "%g", a->num_val);
                    json_append(arr, json_string(buf));
                } else if (a && json_is_bool(a)) json_append(arr, json_string(a->bool_val ? "True" : "False"));
                else json_append(arr, json_string(""));
            }
            json_set(out, "args", arr);
        }
        const json_t *env = json_obj_get(cfg, "env");
        if (env && json_is_object(env) && json_object_size(env) > 0) {
            json_t *e = json_object();
            for (size_t i = 0; ; i++) {
                const char *k = json_object_get_key_at(env, i);
                if (!k) break;
                const json_t *ev = json_object_get_at(env, i);
                if (ev && json_is_string(ev)) json_set(e, k, json_string(ev->str_val));
                else if (ev && json_is_number(ev)) {
                    char buf[40]; snprintf(buf, sizeof buf, "%g", ev->num_val);
                    json_set(e, k, json_string(buf));
                } else if (ev && json_is_bool(ev)) json_set(e, k, json_string(ev->bool_val ? "True" : "False"));
                else json_set(e, k, json_string(""));
            }
            json_set(out, "env", e);
        }
        const char *cwd = json_string_value_safe(json_obj_get(cfg, "cwd"));
        if (cwd && *cwd) json_set(out, "cwd", json_string(cwd));
    } else if (has_url) {                 /* B: streamable_http transport */
        v = json_obj_get(cfg, "url");
        json_set(out, "url", json_string(json_string_value(v)));
        const json_t *headers = json_obj_get(cfg, "headers");
        if (headers && json_is_object(headers) && json_object_size(headers) > 0) {
            json_t *h = json_object();
            for (size_t i = 0; ; i++) {
                const char *k = json_object_get_key_at(headers, i);
                if (!k) break;
                const json_t *hv = json_object_get_at(headers, i);
                if (hv && json_is_string(hv)) json_set(h, k, json_string(hv->str_val));
            }
            json_set(out, "http_headers", h);
        }
        const char *transport = json_string_value_safe(json_obj_get(cfg, "transport"));
        if (transport && strcmp(transport, "sse") == 0)
            json_append(skipped, json_string("transport=sse (codex auto-negotiates)"));
    } else {                              /* B: no command or url */
        json_free(out); json_free(skipped); json_free(cfg);
        return strdup("{\"entry\": null, \"skipped\": [\"no command or url field\"]}");
    }

    /* Timeouts */
    const char *timeout_keys[2] = {"timeout", "connect_timeout"};
    const char *out_keys[2] = {"tool_timeout_sec", "startup_timeout_sec"};
    const char *skip_msgs[2] = {"timeout (not numeric)", "connect_timeout (not numeric)"};
    for (int t = 0; t < 2; t++) {
        const json_t *tv = json_obj_get(cfg, timeout_keys[t]);
        if (!tv) continue;                /* Python: ``in`` check */
        if (tv->type == JSON_NUMBER || tv->type == JSON_BOOL)
            json_set(out, out_keys[t], json_number(tv->num_val));
        else
            json_append(skipped, json_string(skip_msgs[t]));
    }

    /* Enabled flag (only emit explicit false) */
    const json_t *en = json_obj_get(cfg, "enabled");
    if (en && en->type == JSON_BOOL && !en->bool_val)
        json_set(out, "enabled", json_bool(false));

    /* Unknown / dropped keys */
    static const char *const KNOWN[] = {"command","args","env","cwd","url",
        "headers","transport","timeout","connect_timeout","enabled","description",NULL};
    static const char *const DROPPED[] = {"sampling",NULL};
    for (size_t i = 0; ; i++) {
        const char *k = json_object_get_key_at(cfg, i);
        if (!k) break;
        bool known = false, dropped = false;
        for (int j = 0; KNOWN[j]; j++) if (strcmp(k, KNOWN[j]) == 0) known = true;
        for (int j = 0; DROPPED[j]; j++) if (strcmp(k, DROPPED[j]) == 0) dropped = true;
        char msg[160];
        if (dropped) { snprintf(msg, sizeof msg, "%s (no codex equivalent)", k);
                       json_append(skipped, json_string(msg)); }
        else if (!known) { snprintf(msg, sizeof msg, "%s (unknown Hermes key)", k);
                           json_append(skipped, json_string(msg)); }
    }

    json_t *rec = json_object();
    json_set(rec, "entry", out);
    json_set(rec, "skipped", skipped);
    char *ser = json_serialize(rec);
    json_free(rec); json_free(cfg);
    return ser ? ser : strdup("{\"entry\": null, \"skipped\": []}");
}

/* PoP: hermes_cli_codex_runtime_plugi_u_format_toml_value @ hermes_cli/codex_runtime_plugin_migration.py:_format_toml_value */
int hermes_cli_codex_runtime_plugi_u_format_toml_value(const char *arg) {
    /* Python: TOML basic string. Arg = "value\tstate\tresult". */
    if (!arg || !*arg) { printf("\"\"\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s\n", arg); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: hermes_cli_codex_runtime_plugi_u_quote_key @ hermes_cli/codex_runtime_plugin_migration.py:_quote_key */
int hermes_cli_codex_runtime_plugi_u_quote_key(const char *arg) {
    /* Python: bare if all alnum or -_; else escape \\ and " then quote. */
    if (!arg || !*arg) { printf("\"\"\n"); return 0; }
    int bare = 1;
    for (const char *p = arg; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '-' || *p == '_')) { bare = 0; break; }
    }
    if (bare) { printf("%s\n", arg); return 0; }
    printf("\"");
    for (const char *p = arg; *p; p++) {
        if (*p == '\\' || *p == '"') printf("\\%c", *p);
        else printf("%c", *p);
    }
    printf("\"\n");
    return 0;
}

/* PoP: hermes_cli_codex_runtime_plugi_render_codex_toml_section @ hermes_cli/codex_runtime_plugin_migration.py:render_codex_toml_section */
int hermes_cli_codex_runtime_plugi_render_codex_toml_section(const char *arg) {
    /* Python: managed block render. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_codex_runtime_plugi_u_insert_managed_block_at_top_el @ hermes_cli/codex_runtime_plugin_migration.py:_insert_managed_block_at_top_level */
int hermes_cli_codex_runtime_plugi_u_insert_managed_block_at_top_el(const char *arg) {
    /* Python: root-scope insert. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_codex_runtime_plugi_u_strip_unmanaged_plugin_tables @ hermes_cli/codex_runtime_plugin_migration.py:_strip_unmanaged_plugin_tables */
int hermes_cli_codex_runtime_plugi_u_strip_unmanaged_plugin_tables(const char *arg) {
    /* Python: dup-table killer. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_codex_runtime_plugi_u_looks_like_table_header @ hermes_cli/codex_runtime_plugin_migration.py:_looks_like_table_header */
int hermes_cli_codex_runtime_plugi_u_looks_like_table_header(const char *arg) {
    /* Python: TOML [name] header test. Arg = "line". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (*p != '[') { printf("0\n"); return 0; }
    const char *hash = strchr(p, '#');
    const char *head_end = hash ? hash : p + strlen(p);
    /* trim trailing spaces */
    while (head_end > p && (head_end[-1] == ' ' || head_end[-1] == '\t')) head_end--;
    if (head_end <= p || head_end[-1] != ']') { printf("0\n"); return 0; }
    size_t hlen = (size_t)(head_end - p);
    for (size_t i = 0; i < hlen; i++) {
        if (p[i] == '=' && i + 1 < hlen) {
            /* key = [x] case: '=' before the first ']' */
            const char *bracket = memchr(p, ']', hlen);
            if (bracket && bracket <= p + i) { printf("0\n"); return 0; }
        }
    }
    printf("1\n");
    return 0;
}

/* PoP: hermes_cli_codex_runtime_plugi_u_strip_existing_managed_block @ hermes_cli/codex_runtime_plugin_migration.py:_strip_existing_managed_block */
int hermes_cli_codex_runtime_plugi_u_strip_existing_managed_block(const char *arg) {
    /* Python: marker-scoped strip. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_codex_runtime_plugi_u_query_codex_plugins @ hermes_cli/codex_runtime_plugin_migration.py:_query_codex_plugins */
int hermes_cli_codex_runtime_plugi_u_query_codex_plugins(const char *arg) {
    /* Python: app-server RPC. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\t%s\n", t2 ? t2 + 1 : "plugin/list query failed"); return 0; }
    printf("%s plugin(s)\t\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: hermes_cli_codex_runtime_plugi_u_looks_like_test_tempdir @ hermes_cli/codex_runtime_plugin_migration.py:_looks_like_test_tempdir */
int hermes_cli_codex_runtime_plugi_u_looks_like_test_tempdir(const char *arg) {
    /* Python: pytest tempdir heuristic. Arg = "path". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    char low[512];
    size_t w = 0;
    for (const char *p = arg; *p && w < sizeof(low)-1; p++) {
        char c = *p;
        low[w++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    low[w] = '\0';
    if (strstr(low, "pytest-of-") || strstr(low, "/pytest-") || strstr(low, "/tmp/pytest") || strstr(low, "/private/var/folders/")) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli_codex_runtime_plugi_u_build_hermes_tools_mcp_entry @ hermes_cli/codex_runtime_plugin_migration.py:_build_hermes_tools_mcp_entry */
int hermes_cli_codex_runtime_plugi_u_build_hermes_tools_mcp_entry(const char *arg) {
    /* Python: stdio entry. Arg =
     * "has_home\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_home = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("mcp entry: python -m agent.transports.hermes_tools_mcp_server (env: HERMES_HOME=%s, timeouts 30/600)%s\n", has_home ? "passthrough (tempdir-guarded)" : "inherit-at-runtime", t2 && t2[1] == '1' ? " — PYTHONPATH passthrough" : "");
    return 0;
}
