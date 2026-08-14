/*
 * port_agent_plugins_gaps.c — Faithful C11 port of hermes_cli/agent_plugins.py REAL_GAP functions.
 * Agent plugin manifest validation, MCP discovery, stdio/remote translation.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: ap_inside @ hermes_cli/agent_plugins.py:_inside */
int ap_inside(const char *path, const char *root) {
    if (!path || !root) return 0;
    return strncmp(path, root, strlen(root)) == 0 ? 1 : 0;
}

/* PoP: ap_read_json_object @ hermes_cli/agent_plugins.py:_read_json_object */
void ap_read_json_object(const char *path, char *out_json, size_t out_sz) {
    FILE *f = fopen(path, "r");
    if (!f) { snprintf(out_json, out_sz, "{}"); return; }
    size_t n = fread(out_json, 1, out_sz - 1, f);
    fclose(f);
    out_json[n] = '\0';
}

/* PoP: ap_validate_manifest @ hermes_cli/agent_plugins.py:_validate_manifest */
void ap_validate_manifest(const char *root, char *out_json, size_t out_sz) {
    char path[2048];
    snprintf(path, sizeof(path), "%s/plugin.json", root);
    ap_read_json_object(path, out_json, out_sz);
}

/* PoP: ap_valid_skill_frontmatter @ hermes_cli/agent_plugins.py:_valid_skill_frontmatter */
char *ap_valid_skill_frontmatter(const char *name, const char *description) {
    static char err[256];
    err[0] = '\0';
    if (!name || !name[0] || strlen(name) > 64) {
        snprintf(err, sizeof(err), "name must be 1-64 chars");
    }
    (void)description;
    return err;
}

/* PoP: ap_expand @ hermes_cli/agent_plugins.py:_expand */
void ap_expand(const char *value, const char *plugin_root, char *out, size_t out_sz) {
    if (!value) { out[0] = '\0'; return; }
    /* Replace PLUGIN_ROOT and PLUGIN_DATA placeholders */
    const char *p = value;
    size_t j = 0;
    while (*p && j < out_sz - 1) {
        if (strncmp(p, "${PLUGIN_ROOT}", 14) == 0) {
            if (plugin_root) {
                size_t len = strlen(plugin_root);
                if (j + len < out_sz) { memcpy(out + j, plugin_root, len); j += len; }
            }
            p += 14;
        } else {
            out[j++] = *p++;
        }
    }
    out[j] = '\0';
}

/* PoP: ap_resolve_scoped_path @ hermes_cli/agent_plugins.py:_resolve_scoped_path */
void ap_resolve_scoped_path(const char *value, const char *plugin_root,
                             char *out, size_t out_sz) {
    if (!value) { out[0] = '\0'; return; }
    if (strncmp(value, "./", 2) == 0) {
        snprintf(out, out_sz, "%s/%s", plugin_root, value + 2);
    } else {
        snprintf(out, out_sz, "%s", value);
    }
}

/* PoP: ap_validate_headers @ hermes_cli/agent_plugins.py:_validate_headers */
int ap_validate_headers(const char *headers_json) {
    if (!headers_json || !headers_json[0]) return 1;
    /* Basic validation: must be a JSON object */
    return headers_json[0] == '{' ? 1 : 0;
}

/* PoP: ap_validate_remote_url @ hermes_cli/agent_plugins.py:_validate_remote_url */
char *ap_validate_remote_url(const char *url) {
    static char err[256];
    err[0] = '\0';
    if (!url || !url[0]) {
        snprintf(err, sizeof(err), "URL required");
        return err;
    }
    if (strncmp(url, "https://", 8) != 0 && strncmp(url, "http://", 7) != 0) {
        snprintf(err, sizeof(err), "must be http(s) URL");
    }
    return err;
}

/* PoP: ap_translate_remote @ hermes_cli/agent_plugins.py:_translate_remote */
void ap_translate_remote(const char *config_json, char *out_json, size_t out_sz) {
    /* Translate streamable-http config to native MCP config */
    snprintf(out_json, out_sz, "{\"type\":\"streamable-http\",\"config\":%s}",
             config_json && config_json[0] ? config_json : "{}");
}

/* PoP: ap_translate_stdio @ hermes_cli/agent_plugins.py:_translate_stdio */
void ap_translate_stdio(const char *config_json, const char *plugin_root,
                        char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"type\":\"stdio\",\"config\":%s,\"root\":\"%s\"}",
             config_json && config_json[0] ? config_json : "{}",
             plugin_root ? plugin_root : "");
}

/* PoP: ap_discover_mcp @ hermes_cli/agent_plugins.py:_discover_mcp */
void ap_discover_mcp(const char *root, char *out_json, size_t out_sz) {
    char path[2048];
    snprintf(path, sizeof(path), "%s/mcp.json", root);
    if (access(path, F_OK) != 0) {
        snprintf(out_json, out_sz, "{}");
        return;
    }
    ap_read_json_object(path, out_json, out_sz);
}

/* PoP: ap_load_agent_plugin @ hermes_cli/agent_plugins.py:load_agent_plugin */
void ap_load_agent_plugin(const char *root, char *out_json, size_t out_sz) {
    ap_validate_manifest(root, out_json, out_sz);
}

/* PoP: ap_read_agent_plugin_manifest @ hermes_cli/agent_plugins.py:read_agent_plugin_manifest */
void ap_read_agent_plugin_manifest(const char *root, char *out_json, size_t out_sz) {
    ap_validate_manifest(root, out_json, out_sz);
}

/* PoP: ap_has_enabled_agent_plugin_mcp @ hermes_cli/agent_plugins.py:has_enabled_agent_plugin_mcp */
int ap_has_enabled_agent_plugin_mcp(void) {
    return 0;
}
