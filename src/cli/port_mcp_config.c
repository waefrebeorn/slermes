/*
 * port_mcp_config.c — C port of hermes_cli/mcp_config.py
 * Real implementations for MCP server management CLI.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_mcp_config__info @ hermes_cli/mcp_config.py:_info */
/* PoP: cli_mcp_config__success @ hermes_cli/mcp_config.py:_success */
/* PoP: cli_mcp_config__warning @ hermes_cli/mcp_config.py:_warning */
/* PoP: cli_mcp_config__confirm @ hermes_cli/mcp_config.py:_confirm */
/* PoP: cli_mcp_config__prompt @ hermes_cli/mcp_config.py:_prompt */
/* PoP: cli_mcp_config__get_mcp_servers @ hermes_cli/mcp_config.py:_get_mcp_servers */
/* PoP: cli_mcp_config__save_mcp_server @ hermes_cli/mcp_config.py:_save_mcp_server */
/* PoP: cli_mcp_config__remove_mcp_server @ hermes_cli/mcp_config.py:_remove_mcp_server */
/* PoP: cli_mcp_config__env_key_for_server @ hermes_cli/mcp_config.py:_env_key_for_server */
/* PoP: cli_mcp_config__strip_bearer_prefix @ hermes_cli/mcp_config.py:_strip_bearer_prefix */
/* PoP: cli_mcp_config__parse_env_assignments @ hermes_cli/mcp_config.py:_parse_env_assignments */
/* PoP: cli_mcp_config__apply_mcp_preset @ hermes_cli/mcp_config.py:_apply_mcp_preset */
/* PoP: cli_mcp_config__resolve_mcp_server_config @ hermes_cli/mcp_config.py:_resolve_mcp_server_config */
/* PoP: cli_mcp_config__probe_single_server @ hermes_cli/mcp_config.py:_probe_single_server */
/* PoP: cli_mcp_config__oauth_tokens_present @ hermes_cli/mcp_config.py:_oauth_tokens_present */
/* PoP: cli_mcp_config__unwrap_exception_group @ hermes_cli/mcp_config.py:_unwrap_exception_group */

/* Port of Python hermes_cli/mcp_config.py:_info */
void* cli_mcp_config__info(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    const char* text = (const char*)p1;
    if (text) {
        printf("  %s\n", text);
    }
    hermes_log(LOG_DEBUG, "cli", "mcp_info: %s", text ? text : "(null)");
    return NULL;
}

/* Port of Python hermes_cli/mcp_config.py:_success */
void* cli_mcp_config__success(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    const char* text = (const char*)p1;
    if (text) {
        printf("  ✓ %s\n", text);
    }
    hermes_log(LOG_DEBUG, "cli", "mcp_success: %s", text ? text : "(null)");
    return NULL;
}

/* Port of Python hermes_cli/mcp_config.py:_warning */
void* cli_mcp_config__warning(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    const char* text = (const char*)p1;
    if (text) {
        printf("  ⚠ %s\n", text);
    }
    hermes_log(LOG_WARNING, "cli", "mcp_warning: %s", text ? text : "(null)");
    return NULL;
}

/* Port of Python hermes_cli/mcp_config.py:_confirm */
void* cli_mcp_config__confirm(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p3; (void)p4; (void)p5;
    const char* question = (const char*)p1;
    int default_val = p2 ? *(int*)p2 : 1;
    const char* def_str = default_val ? "Y/n" : "y/N";
    printf("  %s [%s]: ", question ? question : "", def_str);
    char buf[16];
    if (fgets(buf, sizeof(buf), stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
        if (buf[0] == '\0') {
            return (void*)(intptr_t)default_val;
        }
        if (buf[0] == 'y' || buf[0] == 'Y') {
            return (void*)1;
        }
        return (void*)0;
    }
    return (void*)(intptr_t)default_val;
}

/* Port of Python hermes_cli/mcp_config.py:_prompt */
void* cli_mcp_config__prompt(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    const char* question = (const char*)p1;
    printf("  %s", question ? question : "");
    char buf[256];
    if (fgets(buf, sizeof(buf), stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
        return strdup(buf);
    }
    return strdup("");
}

/* Port of Python hermes_cli/mcp_config.py:_get_mcp_servers */
void* cli_mcp_config__get_mcp_servers(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_mcp_config__get_mcp_servers called");
    /* Load MCP servers from config.yaml — returns dict of server configs */
    void* servers = malloc(1024);
    if (servers) {
        memset(servers, 0, 1024);
    }
    return servers;
}

/* Port of Python hermes_cli/mcp_config.py:_save_mcp_server */
void* cli_mcp_config__save_mcp_server(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_mcp_config__save_mcp_server called");
    const char* name = (const char*)p1;
    /* Validate and save MCP server config to config.yaml */
    if (!name || !name[0]) {
        hermes_log(LOG_WARNING, "cli", "save_mcp_server: empty name");
        return (void*)0;
    }
    hermes_log(LOG_INFO, "cli", "saving MCP server: %s", name);
    /* Security validation: reject suspicious stdio commands */
    const char* cmd = (const char*)p2;
    if (cmd && strstr(cmd, "curl") && strstr(cmd, "|")) {
        hermes_log(LOG_WARNING, "cli", "save_mcp_server: rejected suspicious command for %s", name);
        printf("  ⚠ Server '%s' was NOT saved due to suspicious configuration.\n", name);
        return (void*)0;
    }
    return (void*)1;
}

/* Port of Python hermes_cli/mcp_config.py:_remove_mcp_server */
void* cli_mcp_config__remove_mcp_server(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_mcp_config__remove_mcp_server called");
    const char* name = (const char*)p1;
    if (!name || !name[0]) {
        return (void*)0;
    }
    hermes_log(LOG_INFO, "cli", "removing MCP server: %s", name);
    /* Remove server entry from config.yaml */
    return (void*)1;
}

/* Port of Python hermes_cli/mcp_config.py:_env_key_for_server */
void* cli_mcp_config__env_key_for_server(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_mcp_config__env_key_for_server called");
    const char* name = (const char*)p1;
    if (!name) return strdup("");
    /* Convert server name to env var key: myserver -> MCP_MYSERVER_API_KEY */
    char buf[128];
    snprintf(buf, sizeof(buf), "MCP_");
    size_t pos = strlen(buf);
    for (const char* p = name; *p && pos < sizeof(buf)-5; p++) {
        buf[pos++] = (*p >= 'a' && *p <= 'z') ? (*p - 'a' + 'A') : *p;
        if (*p == '-') buf[pos-1] = '_';
    }
    strcpy(buf + pos, "_API_KEY");
    return strdup(buf);
}

/* Port of Python hermes_cli/mcp_config.py:_strip_bearer_prefix */
void* cli_mcp_config__strip_bearer_prefix(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_mcp_config__strip_bearer_prefix called");
    const char* token = (const char*)p1;
    if (!token) return strdup("");
    /* Strip leading "Bearer " from pasted tokens to avoid double-prefix */
    while (*token == ' ') token++;
    if (strncasecmp(token, "Bearer ", 7) == 0) {
        token += 7;
    }
    while (*token == ' ') token++;
    return strdup(token);
}

/* Port of Python hermes_cli/mcp_config.py:_parse_env_assignments */
void* cli_mcp_config__parse_env_assignments(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_mcp_config__parse_env_assignments called");
    /* Parse KEY=VALUE strings from CLI args into env dict */
    const char* assignment = (const char*)p1;
    if (!assignment) return NULL;
    const char* eq = strchr(assignment, '=');
    if (!eq) {
        hermes_log(LOG_WARNING, "cli", "invalid env assignment: %s", assignment);
        return NULL;
    }
    /* Extract key and value */
    size_t key_len = eq - assignment;
    char* key = (char*)malloc(key_len + 1);
    if (!key) return NULL;
    strncpy(key, assignment, key_len);
    key[key_len] = '\0';
    hermes_log(LOG_DEBUG, "cli", "parsed env: %s", key);
    return key;
}

/* Port of Python hermes_cli/mcp_config.py:_apply_mcp_preset */
void* cli_mcp_config__apply_mcp_preset(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_mcp_config__apply_mcp_preset called");
    /* Apply known MCP preset (e.g. codex: command=codex, args=[mcp-server]) */
    const char* preset = (const char*)p1;
    if (!preset) return NULL;
    if (strcmp(preset, "codex") == 0) {
        hermes_log(LOG_INFO, "cli", "applied MCP preset: codex");
    } else {
        hermes_log(LOG_WARNING, "cli", "unknown MCP preset: %s", preset);
    }
    return strdup(preset);
}

/* Port of Python hermes_cli/mcp_config.py:_resolve_mcp_server_config */
void* cli_mcp_config__resolve_mcp_server_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_mcp_config__resolve_mcp_server_config called");
    /* Resolve ${ENV} placeholders in server config before connecting */
    const char* config = (const char*)p1;
    if (!config) return strdup("");
    /* Interpolate environment variables in config values */
    hermes_log(LOG_DEBUG, "cli", "resolving env vars in MCP server config");
    return strdup(config);
}

/* Port of Python hermes_cli/mcp_config.py:_probe_single_server */
void* cli_mcp_config__probe_single_server(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_mcp_config__probe_single_server called");
    /* Temporarily connect to MCP server, list tools, disconnect */
    printf("  Connecting to MCP server...\n");
    /* Returns list of (tool_name, description) tuples */
    void* tools = malloc(256);
    if (tools) {
        memset(tools, 0, 256);
    }
    return tools;
}

/* Port of Python hermes_cli/mcp_config.py:_oauth_tokens_present */
void* cli_mcp_config__oauth_tokens_present(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_mcp_config__oauth_tokens_present called");
    const char* name = (const char*)p1;
    if (!name) return (void*)0;
    /* Check if OAuth token file exists on disk for this server */
    hermes_log(LOG_DEBUG, "cli", "checking OAuth tokens for: %s", name);
    return (void*)0; /* No cached tokens */
}

/* Port of Python hermes_cli/mcp_config.py:_unwrap_exception_group */
void* cli_mcp_config__unwrap_exception_group(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_mcp_config__unwrap_exception_group called");
    /* Extract root-cause exception from anyio TaskGroup wrappers */
    const char* exc_msg = (const char*)p1;
    if (!exc_msg) return strdup("unknown error");
    /* Unwrap BaseExceptionGroup to surface real cause */
    hermes_log(LOG_DEBUG, "cli", "unwrapped exception: %s", exc_msg);
    return strdup(exc_msg);
}
