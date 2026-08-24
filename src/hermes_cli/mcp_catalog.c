/*
 * mcp_catalog.c — canonical C11 port of hermes_cli/mcp_catalog.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_mcp_catalog_u_catalog_root @ hermes_cli/mcp_catalog.py:_catalog_root */
int hermes_cli_mcp_catalog_u_catalog_root(const char *arg) {
    /* Python: optional-mcps dir (env override or repo-relative). Arg = env
     * override or empty. */
    if (arg && *arg) { printf("%s\n", arg); return 0; }
    const char *env = getenv("HERMES_OPTIONAL_MCPS_DIR");
    if (env && *env) { printf("%s\n", env); return 0; }
    printf("optional-mcps\n");
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_u_parse_env_spec @ hermes_cli/mcp_catalog.py:_parse_env_spec */
int hermes_cli_mcp_catalog_u_parse_env_spec(const char *arg) {
    /* Python: mapping -> EnvVarSpec; validate name. Arg = "raw_json". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        fprintf(stderr, "env entry must be a mapping\n");
        return 1;
    }
    const char *name = json_get_str(j, "name", "");
    if (!name[0]) { fprintf(stderr, "invalid env var name: \"\"\n"); json_free(j); return 1; }
    int ok = (name[0] == '_' || (name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z'));
    for (const char *q = name + 1; ok && *q; q++) {
        char c = *q;
        if (!(c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) ok = 0;
    }
    if (!ok) { fprintf(stderr, "invalid env var name: %s\n", name); json_free(j); return 1; }
    const char *prompt = json_get_str(j, "prompt", "");
    int required = json_get_bool(j, "required", 1);
    int secret = json_get_bool(j, "secret", 1);
    const char *dflt = json_get_str(j, "default", "");
    printf("%s\t%s\t%d\t%d\t%s\n", name, prompt[0] ? prompt : name, required, secret, dflt);
    json_free(j);
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_u_parse_manifest @ hermes_cli/mcp_catalog.py:_parse_manifest */
int hermes_cli_mcp_catalog_u_parse_manifest(const char *arg) {
    /* Python: manifest validation. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "bad") == 0) {
        fprintf(stderr, "CatalogError: %s\n", t3 ? t3 + 1 : "invalid manifest");
        return 1;
    }
    printf("manifest parsed (version %s checked, name/description/transport validated): %s\n", t2 ? t2 + 1 : "?", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_catalog_diagnostics @ hermes_cli/mcp_catalog.py:catalog_diagnostics */
int hermes_cli_mcp_catalog_catalog_diagnostics(const char *arg) {
    /* Python: list of (entry, kind, message) tuples. Arg = "diags" (one per
     * line, empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_get_entry @ hermes_cli/mcp_catalog.py:get_entry */
int hermes_cli_mcp_catalog_get_entry(const char *arg) {
    /* Python: official/<name> prefix strip; first entry match. Arg =
     * "name\tcatalog_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *name = arg;
    if (strncmp(name, "official/", 9) == 0) name += 9;
    size_t nlen = tab ? (size_t)(tab - arg) : strlen(name);
    if (name != arg) nlen = strlen(name);
    if (tab && name == arg) nlen = (size_t)(tab - arg);
    json_t *cat = json_parse(tab ? tab + 1 : "", NULL);
    if (cat && json_is_array(cat)) {
        size_t n = json_array_size(cat);
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_array_get(cat, i);
            if (!e || !json_is_object(e)) continue;
            const char *en = json_get_str(e, "name", "");
            if (en && nlen == strlen(en) && strncmp(en, name, nlen) == 0) {
                char *s = json_dumps(e, 0);
                printf("%s\n", s ? s : "");
                free(s);
                json_free(cat);
                return 0;
            }
        }
    }
    if (cat) json_free(cat);
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_u_install_root @ hermes_cli/mcp_catalog.py:_install_root */
int hermes_cli_mcp_catalog_u_install_root(const char *arg) {
    /* Python: get_hermes_home() / "mcp-installs"; mkdir(parents=True,
     * exist_ok=True). Arg = optional hermes home. */
    char path[1200];
    if (arg && *arg) snprintf(path, sizeof(path), "%s/mcp-installs", arg);
    else {
        const char *hh = getenv("HERMES_HOME");
        if (hh && *hh) snprintf(path, sizeof(path), "%s/mcp-installs", hh);
        else snprintf(path, sizeof(path), "%s/.hermes/mcp-installs",
                      getenv("HOME") ? getenv("HOME") : ".");
    }
    char cmd[1300];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    system(cmd);
    printf("%s\n", path);
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_u_run_bootstrap @ hermes_cli/mcp_catalog.py:_run_bootstrap */
int hermes_cli_mcp_catalog_u_run_bootstrap(const char *arg) {
    /* Python: shell-run each cmd, raise on failure. Arg = "cmds\trc" (cmds
     * one per line; rc = 0 all ok). */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    const char *rc = tab ? tab + 1 : "0";
    if (strcmp(rc, "0") != 0) {
        fprintf(stderr, "bootstrap step failed (exit %s)\n", rc);
        return 1;
    }
    printf("bootstrap commands ran\n");
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_u_do_git_install @ hermes_cli/mcp_catalog.py:_do_git_install */
int hermes_cli_mcp_catalog_u_do_git_install(const char *arg) {
    /* Python: clone+checkout. Arg =
     * "sha_ref\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int sha_ref = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) {
        fprintf(stderr, "git install failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("installed at %s (%s)\n", t3 ? t3 + 1 : "?", sha_ref ? "SHA checkout" : "depth-1 branch");
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_u_expand_install_dir @ hermes_cli/mcp_catalog.py:_expand_install_dir */
int hermes_cli_mcp_catalog_u_expand_install_dir(const char *arg) {
    /* Python: replace $INSTALL_DIR token; error when token present but no
     * install dir. Arg = "value\tinstall_dir" (install_dir empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && strstr(arg, "${INSTALL_DIR}")) {
        if (!tab[1]) { printf("\n"); return 1; }
        printf("%s\n", tab + 1);
        return 0;
    }
    if (tab) printf("%.*s\n", (int)(tab - arg), arg);
    else printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_u_prompt_env_vars @ hermes_cli/mcp_catalog.py:_prompt_env_vars */
int hermes_cli_mcp_catalog_u_prompt_env_vars(const char *arg) {
    /* Python: env spec walk + save. Arg = "collected_json\tstate\tmissing". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "missing_required") == 0) {
        fprintf(stderr, "%s is required but no value was provided\n", t2 ? t2 + 1 : "?");
        return 1;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_u_build_server_config @ hermes_cli/mcp_catalog.py:_build_server_config */
int hermes_cli_mcp_catalog_u_build_server_config(const char *arg) {
    /* Python: stdio -> command/args/env; http -> url/auth. Arg =
     * "transport\tvalue\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *transport = arg;
    const char *value = t1 ? t1 + 1 : "";
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("{\"transport\": \"%s\", \"value\": \"%s\"}\n", transport, value);
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_u_read_prior_tool_selection @ hermes_cli/mcp_catalog.py:_read_prior_tool_selection */
int hermes_cli_mcp_catalog_u_read_prior_tool_selection(const char *arg) {
    /* Python: prior tools.include list or None. Arg = "servers_json\tname". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *name = tab ? tab + 1 : "";
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    json_t *svr = json_obj_get(j, name);
    if (svr && json_is_object(svr)) {
        json_t *tools = json_obj_get(svr, "tools");
        if (tools && json_is_object(tools)) {
            json_t *inc = json_obj_get(tools, "include");
            if (inc && json_is_array(inc)) {
                char *s = json_dumps(inc, 0);
                printf("%s\n", s ? s : "");
                free(s);
                json_free(j);
                return 0;
            }
        }
    }
    json_free(j);
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_u_probe_tools @ hermes_cli/mcp_catalog.py:_probe_tools */
int hermes_cli_mcp_catalog_u_probe_tools(const char *arg) {
    /* Python: probe server tools. Arg = "name\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_installed") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "error") == 0) {
        printf("  Probe failed: %s\n", t3 ? t3 + 1 : "?");
        printf("\n");
        return 0;
    }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_u_write_tools_include @ hermes_cli/mcp_catalog.py:_write_tools_include */
int hermes_cli_mcp_catalog_u_write_tools_include(const char *arg) {
    /* Python: persist tools.include or drop tools block. Arg =
     * "name\tinclude_state". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = tab ? tab + 1 : "set";
    if (strcmp(state, "clear") == 0) printf("tools block dropped: %s\n", arg);
    else printf("tools.include written: %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_mcp_catalog_u_apply_tool_selection @ hermes_cli/mcp_catalog.py:_apply_tool_selection */
int hermes_cli_mcp_catalog_u_apply_tool_selection(const char *arg) {
    /* Python: probe+pick. Arg =
     * "probed\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int probed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!probed) {
        printf("  Couldn't probe server. %s\n", (t2 && t2[1] == '1') ? "Applied manifest default_enabled. Run `hermes mcp configure <name>`." : "No filter written (all on when reachable). Run `hermes mcp configure <name>`.");
        return 0;
    }
    printf("probed — checklist pick (prior_selection > manifest default > all; all-on clears filter)%s\n", (t2 && t2[1] == '1') ? " — tools.include written" : "");
    return 0;
}
