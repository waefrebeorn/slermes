/*
 * plugin_rpc.c — C home for upstream Hermes hermes_cli/plugin_rpc.py.
 *
 * PoP: w8_pluginrpc_14cb01_plugin_rpc_handler @ hermes_cli/plugin_rpc.py:plugin_rpc_handler
 * PoP: w8_pluginrpc_14cb01_register_rpc @ hermes_cli/plugin_rpc.py:register_rpc
 *
 * The plugin registry and callable(handler) are injected seams. All exports
 * are w8_pluginrpc_14cb01_ prefixed.
 */
#include "noc_w8_pluginrpc_14cb01.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_t *w8_pluginrpc_14cb01_plugin_rpc_handler(const struct json_t *method,
                                              const struct json_t *table) {
    if (!method || method->type != JSON_STRING || !method->str_val) return json_null();
    if (strncmp(method->str_val, "plugin.", 7) != 0) return json_null();
    json_t *v = json_obj_get(table, method->str_val);
    return v ? json_copy(v) : json_null();
}

static json_t *pr_error(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg));
    return o;
}
static int pr_is_id_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}
static int pr_is_seg_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
}
/* plugin_events._EVENT_RE: dot-separated non-empty [A-Za-z0-9_-] segments. */
static int pr_valid_event(const char *s) {
    if (!s || !*s || strlen(s) > 128) return 0;
    const char *p = s;
    while (*p) {
        const char *start = p;
        while (*p && pr_is_seg_char(*p)) p++;
        if (p == start) return 0;
        if (*p == '.') { p++; if (!*p) return 0; }
        else if (*p) return 0;
    }
    return 1;
}

json_t *w8_pluginrpc_14cb01_register_rpc(const char *plugin_id, const char *name,
                                        int callable_handler) {
    const char *pid = plugin_id ? plugin_id : "";
    const char *nm = name ? name : "";
    if (!callable_handler) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Plugin '%s' register_rpc() handler must be callable", pid);
        return pr_error(msg);
    }
    size_t plen = strlen(pid);
    if (plen < 1 || plen > 64) {
        char msg[512];
        snprintf(msg, sizeof(msg), "invalid plugin id '%s': expected [a-z0-9_-]{1,64}", pid);
        return pr_error(msg);
    }
    for (size_t i = 0; i < plen; ++i) {
        if (!pr_is_id_char(pid[i])) {
            char msg[512];
            snprintf(msg, sizeof(msg), "invalid plugin id '%s': expected [a-z0-9_-]{1,64}", pid);
            return pr_error(msg);
        }
    }
    if (!pr_valid_event(nm)) {
        char msg[768];
        snprintf(msg, sizeof(msg),
                 "invalid plugin RPC method '%s': expected dot-separated segments of "
                 "[A-Za-z0-9_-] (the plugin id already namespaces the name)", nm);
        return pr_error(msg);
    }
    char method[512];
    snprintf(method, sizeof(method), "plugin.%s.%s", pid, nm);
    json_t *o = json_object();
    json_set(o, "bucket", json_string("rpc_method"));
    json_set(o, "method", json_string(method));
    return o;
}
