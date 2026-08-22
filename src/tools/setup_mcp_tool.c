/*
 * setup_mcp_tool.c — Port of tools/setup_mcp_tool.py.
 * Schema + thin dispatcher over the platform-injected blocking-prompt bridge.
 */
#define _POSIX_C_SOURCE 200809L

#include "setup_mcp_tool.h"
#include "hermes_json.h"
#include "hermes_tool_result.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* strip helper: returns malloc'd trimmed copy */
static char *trim_dup(const char *s)
{
    if (!s) s = "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                       s[len - 1] == '\n' || s[len - 1] == '\r'))
        len--;
    char *out = malloc(len + 1);
    if (out) { memcpy(out, s, len); out[len] = '\0'; }
    return out;
}

/* PoP: setup_mcp_tool @ tools/setup_mcp_tool.py:setup_mcp_tool */
char *setup_mcp_tool(const char *server, const char *action, const char *reason,
                     setup_mcp_callback_fn callback, void *cb_ctx)
{
    if (!callback) {
        return tool_error(
            "setup_mcp is only available in the Hermes desktop app. Use the "
            "terminal instead: `hermes mcp install <name>` for catalog entries, "
            "`hermes mcp login <name>` for OAuth.", NULL);
    }

    char *name = trim_dup(server);
    if (!name || !*name) {
        free(name);
        return tool_error("server is required — the catalog or config name of the MCP server.", NULL);
    }

    /* action = (action or "install").strip().lower(); must be in _ACTIONS */
    const char *act_raw = (action && *action) ? action : "install";
    char *act = trim_dup(act_raw);
    for (char *p = act; *p; p++)
        if (*p >= 'A' && *p <= 'Z')
            *p = (char)(*p + 32);
    if (!(strcmp(act, "install") == 0 || strcmp(act, "enable") == 0 ||
          strcmp(act, "authorize") == 0)) {
        size_t need = snprintf(NULL, 0,
            "action must be one of install, enable, authorize.") + 1;
        char *msg = malloc(need);
        snprintf(msg, need, "action must be one of install, enable, authorize.");
        char *err = tool_error(msg, NULL);
        free(msg);
        free(name);
        free(act);
        return err;
    }

    char *why = trim_dup(reason);
    char *raw = NULL;
    int rc = callback(name, act, why ? why : "", &raw, cb_ctx);
    free(why);
    free(act);

    if (rc != 0) {
        /* except Exception as exc: tool_error(f"MCP setup flow failed: {exc}") */
        const char *detail = "callback raised";
        size_t need = snprintf(NULL, 0, "MCP setup flow failed: %s", detail) + 1;
        char *msg = malloc(need);
        snprintf(msg, need, "MCP setup flow failed: %s", detail);
        char *err = tool_error(msg, NULL);
        free(msg);
        free(name);
        return err;
    }

    if (!raw) {
        /* renderer never answered — distinct from an explicit decline */
        json_t *o = json_new_object();
        json_object_set(o, "status", json_string("unanswered"));
        json_object_set(o, "server", json_string(name));
        json_object_set(o, "note", json_string(
            "The user did not respond to the setup card. Do not retry "
            "immediately; continue without the server or ask in chat."));
        char *out = json_serialize(o);
        json_free(o);
        free(name);
        return out;
    }

    /* Desktop answers with a JSON object; pass it through, else wrap raw text. */
    json_t *parsed = json_parse(raw, NULL);
    char *out;
    if (parsed) {
        out = json_serialize(parsed);
        json_free(parsed);
    } else {
        json_t *o = json_new_object();
        json_object_set(o, "status", json_string("error"));
        json_object_set(o, "detail", json_string(raw));
        out = json_serialize(o);
        json_free(o);
    }
    free(raw);
    free(name);
    return out;
}

/* PoP: SETUP_MCP_SCHEMA @ tools/setup_mcp_tool.py */
char *setup_mcp_schema(void)
{
    static const char *schema =
"{\"name\":\"setup_mcp\","
"\"description\":\"Propose an MCP server to the user as an inline consent card in the "
"Hermes desktop chat. The card lets them install a catalog entry, "
"re-enable a disabled server, or run an OAuth login — right there, "
"without opening the Capabilities tab — and blocks until they act or "
"decline. Use when the user asks to add/set up an MCP (e.g. \\\"add the "
"linear mcp\\\"), or when a task clearly needs one that is missing or "
"unauthorized. Never call it twice for the same server after a "
"decline. Returns JSON {status: installed|enabled|authorized|declined|"
"unanswered|error, server, detail?, tools?}. On declined/unanswered, "
"continue without the server. Catalog names: run `hermes mcp catalog` "
"in the terminal to list them.\","
"\"parameters\":{\"type\":\"object\",\"properties\":{"
"\"server\":{\"type\":\"string\",\"description\":\"The server's catalog name (for install) or its name in mcp_servers config (for enable/authorize).\"},"
"\"action\":{\"type\":\"string\",\"enum\":[\"install\",\"enable\",\"authorize\"],\"description\":\"install: add a catalog entry (prompts for any required keys). enable: re-enable a disabled configured server. authorize: run the OAuth browser flow for a configured server. Defaults to install.\"},"
"\"reason\":{\"type\":\"string\",\"description\":\"One short sentence shown on the card: why this server helps right now (e.g. \\\"To read the JIRA ticket you linked\\\").\"}},"
"\"required\":[\"server\"]}}";
    return strdup(schema);
}
