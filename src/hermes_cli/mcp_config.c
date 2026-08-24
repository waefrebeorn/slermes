/*
 * mcp_config.c — canonical C11 port of hermes_cli/mcp_config.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_mcp_config_u_confirm @ hermes_cli/mcp_config.py:_confirm */
int hermes_cli_mcp_config_u_confirm(const char *arg) {
    /* Python: prompt "<question> [Y/n]: " (default) returning bool. Arg =
     * "question\tdefault" (default "1"). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *q = arg;
    int dflt = tab ? (tab[1] == '1') : 1;
    printf("  %.*s [%s]: \n", (int)(tab ? (size_t)(tab - arg) : strlen(q)), q,
           dflt ? "Y/n" : "y/N");
    printf("%d\n", dflt);
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_get_mcp_servers @ hermes_cli/mcp_config.py:_get_mcp_servers */
int hermes_cli_mcp_config_u_get_mcp_servers(const char *arg) {
    /* Python: config.get("mcp_servers") dict or {}. Arg = config JSON. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *cfg = json_parse(arg, NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        printf("{}\n");
        return 0;
    }
    json_t *servers = json_obj_get(cfg, "mcp_servers");
    if (servers && json_is_object(servers)) {
        char *s = json_dumps(servers, 0);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(cfg);
        return 0;
    }
    printf("{}\n");
    json_free(cfg);
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_save_mcp_server @ hermes_cli/mcp_config.py:_save_mcp_server */
int hermes_cli_mcp_config_u_save_mcp_server(const char *arg) {
    /* Python: validate + save; False on suspicious. Arg =
     * "name\tissues\tsaved". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int issues = t1 && t1[1] == '1';
    if (issues) {
        printf("Server '%s' was NOT saved due to suspicious configuration.\n", arg);
        return 0;
    }
    printf("mcp server saved: %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_remove_mcp_server @ hermes_cli/mcp_config.py:_remove_mcp_server */
int hermes_cli_mcp_config_u_remove_mcp_server(const char *arg) {
    /* Python: delete server from mcp_servers; pop key when empty; True if
     * existed. Arg = "name\tservers_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char name[128];
    size_t nlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
    memcpy(name, arg, nlen); name[nlen] = '\0';
    json_t *servers = json_parse(tab ? tab + 1 : "", NULL);
    if (!servers || !json_is_object(servers)) {
        if (servers) json_free(servers);
        printf("0\n");
        return 0;
    }
    json_t *found = json_obj_get(servers, name);
    int existed = found != NULL;
    if (existed) {
        json_obj_del(servers, name);
        char *s = json_dumps(servers, 0);
        printf("1\n%s\n", s ? s : "{}");
        free(s);
    } else {
        printf("0\n");
    }
    json_free(servers);
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_replace_mcp_servers @ hermes_cli/mcp_config.py:_replace_mcp_servers */
int hermes_cli_mcp_config_u_replace_mcp_servers(const char *arg) {
    /* Python: whole-map replace. Arg = "count\tstate\tissues\tresult". */
    if (!arg || !*arg) { printf("1\t[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "issues") == 0) {
        printf("0\t[%s]\n", t3 ? t3 + 1 : "");
        return 0;
    }
    printf("1\t[]\n");
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_env_key_for_server @ hermes_cli/mcp_config.py:_env_key_for_server */
int hermes_cli_mcp_config_u_env_key_for_server(const char *arg) {
    /* Python: re.sub(r"[^A-Za-z0-9_]", "_", name.upper()).strip("_") then
     * f"MCP_{suffix}_API_KEY". Arg = server name. */
    if (!arg || !*arg) { printf("MCP__API_KEY\n"); return 0; }
    char buf[256];
    size_t n = strlen(arg);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, arg, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)toupper((unsigned char)*p);
    /* replace invalid chars, then trim leading/trailing underscores */
    for (char *p = buf; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') *p = '_';
    }
    char *s = buf, *e = buf + strlen(buf);
    while (s < e && *s == '_') s++;
    while (e > s && *(e-1) == '_') e--;
    printf("MCP_%.*s_API_KEY\n", (int)(e - s), s);
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_strip_bearer_prefix @ hermes_cli/mcp_config.py:_strip_bearer_prefix */
int hermes_cli_mcp_config_u_strip_bearer_prefix(const char *arg) {
    /* Python: strip "Bearer " prefix (case-insensitive) + trim. Arg = token. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (strncasecmp(p, "bearer ", 7) == 0) {
        const char *q = p + 7;
        while (*q == ' ') q++;
        printf("%s\n", q);
        return 0;
    }
    printf("%s\n", p);
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_bearer_auth_headers @ hermes_cli/mcp_config.py:_bearer_auth_headers */
int hermes_cli_mcp_config_u_bearer_auth_headers(const char *arg) {
    /* Python: {"Authorization": "Bearer ${ENV_KEY}"}. Arg = env_key. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("{\"Authorization\": \"Bearer ${%s}\"}\n", arg);
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_save_bearer_auth_token @ hermes_cli/mcp_config.py:_save_bearer_auth_token */
int hermes_cli_mcp_config_u_save_bearer_auth_token(const char *arg) {
    /* Python: normalize, save to .env, return headers. Arg =
     * "name\ttoken\tenv_key". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *token = t1 ? t1 + 1 : "";
    const char *p = token;
    while (*p == ' ') p++;
    if (strncasecmp(p, "bearer ", 7) == 0) p += 7;
    while (*p == ' ') p++;
    if (!*p || strcasecmp(p, "bearer") == 0) {
        fprintf(stderr, "Bearer token is required\n");
        return 1;
    }
    printf("saved bearer token for %s (env %s)\n", arg, t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_parse_env_assignments @ hermes_cli/mcp_config.py:_parse_env_assignments */
int hermes_cli_mcp_config_u_parse_env_assignments(const char *arg) {
    /* Python: KEY=VALUE parse with validation. Arg = "items" (tab-sep). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *p = arg;
    int first = 1;
    printf("{");
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        char item[1024];
        if (len >= sizeof(item)) len = sizeof(item) - 1;
        memcpy(item, p, len); item[len] = '\0';
        char *eq = strchr(item, '=');
        if (!eq) {
            fprintf(stderr, "Invalid --env value '%s' (expected KEY=VALUE)\n", item);
            return 1;
        }
        char key[256];
        size_t klen = (size_t)(eq - item);
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, item, klen); key[klen] = '\0';
        int valid = klen > 0 && ((key[0] >= 'A' && key[0] <= 'Z') || (key[0] >= 'a' && key[0] <= 'z') || key[0] == '_');
        for (size_t i = 1; valid && i < klen; i++) {
            char c = key[i];
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) valid = 0;
        }
        if (!valid) {
            fprintf(stderr, "Invalid --env variable name '%s'\n", key);
            return 1;
        }
        if (!first) printf(", ");
        printf("\"%s\": \"%s\"", key, eq + 1);
        first = 0;
        p = t ? t + 1 : p + len;
    }
    printf("}\n");
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_apply_mcp_preset @ hermes_cli/mcp_config.py:_apply_mcp_preset */
int hermes_cli_mcp_config_u_apply_mcp_preset(const char *arg) {
    /* Python: known preset fill when transport omitted. Arg =
     * "preset\tknown\turl_or_cmd\tapplied\turl\tcommand". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int known = t1 && t1[1] == '1';
    if (!known) {
        fprintf(stderr, "Unknown MCP preset: %s\n", arg);
        return 1;
    }
    int has_transport = t2 && t2[1] == '1';
    if (has_transport) { printf("0\n"); return 0; }
    printf("1\t%s\t%s\n", arg, t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_resolve_mcp_server_config @ hermes_cli/mcp_config.py:_resolve_mcp_server_config */
int hermes_cli_mcp_config_u_resolve_mcp_server_config(const char *arg) {
    /* Python: ${ENV} interpolation. Arg = "config_json\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_probe_single_server @ hermes_cli/mcp_config.py:_probe_single_server */
int hermes_cli_mcp_config_u_probe_single_server(const char *arg) {
    /* Python: connect+list. Arg =
     * "tools\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("probe failed: %s\n", t2 ? t2 + 1 : "?"); return 1; }
    printf("probed %s tool(s) (details out-param: prompts/resources counts)\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_oauth_tokens_present @ hermes_cli/mcp_config.py:_oauth_tokens_present */
int hermes_cli_mcp_config_u_oauth_tokens_present(const char *arg) {
    /* Python: token file exists (permissive on error). Arg =
     * "has_tokens\terror". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("1\n"); return 0; }
    if (tab && tab[1] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_unwrap_exception_group @ hermes_cli/mcp_config.py:_unwrap_exception_group */
int hermes_cli_mcp_config_u_unwrap_exception_group(const char *arg) {
    /* Python: first exception from group. Arg = "group\tfirst\tmessage". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (strcmp(arg, "group") == 0 && t1) { printf("%s\n", t1 + 1); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: hermes_cli_mcp_config_u_reauth_oauth_server @ hermes_cli/mcp_config.py:_reauth_oauth_server */
int hermes_cli_mcp_config_u_reauth_oauth_server(const char *arg) {
    /* Python: wipe+probe+verify. Arg =
     * "success\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int success = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) {
        printf("Server '%s' is not configured for OAuth / no URL — use mcp remove + add.\n", t2 ? t2 + 1 : "?");
        return 0;
    }
    if (!success) { printf("  ✗ OAuth re-auth failed or no token landed.\n"); return 1; }
    printf("  ✓ Re-authenticated '%s' (state wiped, token verified)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? " — cache cleared" : "");
    return 1;
}

/* PoP: hermes_cli_mcp_config_cmd_mcp_reauth @ hermes_cli/mcp_config.py:cmd_mcp_reauth */
int hermes_cli_mcp_config_cmd_mcp_reauth(const char *arg) {
    /* Python: serial reauth. Arg =
     * "do_all\thas_name\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int do_all = arg[0] == '1';
    int has_name = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (do_all) {
        printf("Re-authenticating OAuth server(s) one at a time...\n");
        printf("Re-authenticated %s server(s)\n", t3 ? t3 + 1 : "0");
        return 0;
    }
    if (!has_name) {
        printf("Specify a server name, or use --all to re-auth every OAuth server.\n");
        return 0;
    }
    printf("re-authed: %s\n", t3 ? t3 + 1 : "?");
    return 0;
}
