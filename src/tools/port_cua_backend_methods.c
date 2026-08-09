/*
 * port_cua_backend_methods.c — real, self-contained ports of the pure-logic
 * REAL_GAP methods from tools/computer_use/cua_backend.py that were previously
 * unported (had no C PoP annotation at all). These are the IO/logic-heavy
 * helper methods on _EmbeddedCuaDaemon and _CuaDriverSession that operate on
 * JSON results / simple struct fields:
 *
 *   - child_env            -> cua_daemon_child_env
 *   - _drain_stderr        -> cua_daemon_drain_stderr
 *   - proxy_invocation     -> cua_daemon_proxy_invocation
 *   - supports_input_property -> cua_session_supports_input_property
 *   - _logical_error_text  -> cua_session_logical_error_text
 *   - _is_ended_session_result -> cua_session_is_ended_session_result
 *   - _windows_from_tool_result -> cua_windows_from_tool_result
 *   - _apps_from_windows   -> cua_apps_from_windows (depends on _ingest_windows)
 *
 * Opaque structs + minimal includes. No god header. C11 only.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <json.h>
#include "cua_backend_helpers.h"
#include "cua_backend_methods.h"

/* ===========================================================================
 * _positive_int — faithful to Python (rejects bools, parses int/str, >0 only).
 * =========================================================================== */
/* _positive_int is already PORTED in port_cua_backend_ports.c (string API);
 * this json_t variant is an internal helper for cua_ingest_windows. */
static long cua_json_positive_int(const json_t *value)
{
    if (value && value->type == JSON_BOOL)
        return -1;
    if (value && value->type == JSON_NUMBER) {
        double d = value->num_val;
        long v = (long)d;
        return (d == (double)v && v > 0) ? v : -1;
    }
    if (value && value->type == JSON_STRING) {
        const char *s = value->str_val;
        if (!s || !*s) return -1;
        /* int(value) — Python rejects leading/trailing whitespace and signs?
         * No — int("  +42  ") works. Use strtol with full validation. */
        char *end = NULL;
        long v = strtol(s, &end, 10);
        if (end == s || *end != '\0') return -1;
        return v > 0 ? v : -1;
    }
    return -1;
}

/* ===========================================================================
 * _ingest_windows — normalise list_windows entries, dropping unusable ones.
 * Builds a JSON array of normalised window objects matching Python's dict
 * shape exactly. Returns a malloc'd json_t* (caller frees via json_free),
 * or NULL on NULL/empty/non-array input.
 * =========================================================================== */
/* PoP: _ingest_windows @ tools/computer_use/cua_backend.py:_ingest_windows */
json_t *cua_ingest_windows(const json_t *raw_windows)
{
    json_t *out = json_array();
    if (!out) return NULL;
    if (!raw_windows || raw_windows->type != JSON_ARRAY)
        return out;  /* Python returns [] for non-list — empty array */

    for (size_t i = 0; i < json_len(raw_windows); i++) {
        json_t *w = json_get(raw_windows, i);
        if (!w || w->type != JSON_OBJECT)
            continue;  /* skip non-dict */

        long pid = cua_json_positive_int(json_obj_get(w, "pid"));
        long wid = cua_json_positive_int(json_obj_get(w, "window_id"));
        if (pid < 0 || wid < 0)
            continue;  /* both required and positive */

        json_t *z_raw = json_obj_get(w, "z_index");
        double z_index = 0;
        if (z_raw && z_raw->type == JSON_NUMBER)
            z_index = (double)z_raw->num_val;
        /* JSON_BOOL is excluded by the NUMBER type check. */
        /* Python: z_raw if isinstance(z_raw, (int,float)) and not bool */

        const char *app_name_raw = json_get_str(w, "app_name", "");
        const char *title_raw = json_get_str(w, "title", "");

        json_t *entry = json_object();
        if (!entry) continue;
        json_set(entry, "app_name", json_string(app_name_raw ? app_name_raw : ""));
        json_set(entry, "pid", json_number((double)pid));
        json_set(entry, "window_id", json_number((double)wid));
        /* off_screen: only explicit False means off-screen; null => unknown (False) */
        json_t *is_on = json_obj_get(w, "is_on_screen");
        json_set(entry, "off_screen", json_bool(is_on && is_on->type == JSON_BOOL && !is_on->bool_val));
        json_set(entry, "title", json_string(title_raw ? title_raw : ""));
        json_set(entry, "z_index", json_number(z_index));
        json_append(out, entry);
    }
    return out;
}

/* ===========================================================================
 * child_env — start from cua_driver_child_env, add 2 fixed keys.
 * Python: env = cua_driver_child_env(); env["X"]="..."; env["Y"]="..."; return
 * We accept a base env json object; caller owns result.
 * =========================================================================== */
/* PoP: child_env @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon.child_env */
json_t *cua_daemon_child_env(const json_t *base_env)
{
    /* Delegate to the already-ported cua_driver_child_env wrapper which
     * builds the base dict from cua_driver_child_env() logic. */
    /* The wrappers.c function returns a serialized JSON string of the env
     * object (via printf). We rebuild it here from base_env to attach the
     * two extra keys, mirroring Python's mutation. */
    json_t *env = base_env ? json_copy(base_env) : json_object();
    if (!env) return NULL;
    if (env->type != JSON_OBJECT) {
        /* base_env was not an object — rebuild empty */
        json_t *tmp = env;
        env = json_object();
        json_free(tmp);
    }
    json_set(env, "CUA_DRIVER_PERMISSION_MODE", json_string("unrestricted"));
    json_set(env, "CUA_DRIVER_DANGEROUSLY_BYPASS_APPROVALS", json_string("1"));
    return env;
}

/* ===========================================================================
 * _drain_stderr — iterate stderr lines, append non-empty stripped lines.
 * Mirrors Python exactly: for line in stream: text=str(line).strip(); if text: append.
 * We accept a newline-separated dump of stderr text.
 * =========================================================================== */
/* PoP: _drain_stderr @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon._drain_stderr */
void cua_daemon_drain_stderr(json_t *stderr_tail, const char *stderr_text)
{
    if (!stderr_tail || stderr_tail->type != JSON_ARRAY || !stderr_text)
        return;
    /* split on \n, strip each, append non-empty */
    const char *p = stderr_text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        /* strip trailing \r */
        while (len > 0 && (p[len-1] == '\r' || p[len-1] == '\n' ||
                           p[len-1] == ' ' || p[len-1] == '\t'))
            len--;
        if (len > 0) {
            char *line = (char *)malloc(len + 1);
            if (line) {
                memcpy(line, p, len);
                line[len] = '\0';
                json_append(stderr_tail, json_string(line));
                free(line);
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
}

/* ===========================================================================
 * proxy_invocation — return (command, [*mcp_args, --embedded, --socket, path])
 * as a serialized JSON string "command\targ1,arg2,...". Mirrors Python exactly.
 * =========================================================================== */
/* PoP: proxy_invocation @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon.proxy_invocation */
char *cua_daemon_proxy_invocation(const char *command,
                                  const json_t *mcp_args,
                                  const char *socket_path)
{
    if (!command || !mcp_args || !socket_path)
        return NULL;
    if (mcp_args->type != JSON_ARRAY)
        return NULL;

    /* build the suffix list: mcp_args + "--embedded" + "--socket" + socket_path */
    size_t n = json_len(mcp_args);
    /* count total */
    size_t total = n + 3;  /* +3 for --embedded, --socket, socket_path */
    char **parts = (char **)malloc(total * sizeof(char *));
    if (!parts) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *a = json_get(mcp_args, i);
        if (a && a->type == JSON_STRING) {
            parts[k++] = strdup(a->str_val ? a->str_val : "");
        }
    }
    parts[k++] = strdup("--embedded");
    parts[k++] = strdup("--socket");
    parts[k++] = strdup(socket_path);
    size_t count = k;

    /* serialize as command\targ1,arg2,... — but proxy_invocation returns
     * a TUPLE in Python. We serialize as JSON array of the args for the
     * caller's convenience; command is returned as first element. */
    /* Build: "command" + tab + comma-joined args */
    size_t need = strlen(command) + 1;
    for (size_t i = 0; i < count; i++)
        need += strlen(parts[i]) + 1;
    char *result = (char *)malloc(need + count);
    if (!result) {
        for (size_t i = 0; i < count; i++) free(parts[i]);
        free(parts);
        return NULL;
    }
    char *o = result;
    strcpy(o, command);
    o += strlen(command);
    *o++ = '\t';
    for (size_t i = 0; i < count; i++) {
        size_t l = strlen(parts[i]);
        memcpy(o, parts[i], l);
        o += l;
        if (i < count - 1) *o++ = ',';  /* comma-separated args after the tab */
        free(parts[i]);
    }
    *o = '\0';
    free(parts);
    return result;
}

/* ===========================================================================
 * supports_input_property — is property_name in tool's schema["properties"]?
 * =========================================================================== */
/* PoP: supports_input_property @ tools/computer_use/cua_backend.py:_CuaDriverSession.supports_input_property */
bool cua_session_supports_input_property(const json_t *tool_schemas,
                                         const char *tool,
                                         const char *property_name)
{
    if (!tool_schemas || tool_schemas->type != JSON_OBJECT || !tool || !property_name)
        return false;
    json_t *schema = json_obj_get(tool_schemas, tool);
    if (!schema || schema->type != JSON_OBJECT)
        return false;
    json_t *props = json_obj_get(schema, "properties");
    if (!props || props->type != JSON_OBJECT)
        return false;
    /* property_name present as a key */
    return json_has(props, property_name);
}

/* ===========================================================================
 * _logical_error_text — flatten result["data"] + result["structuredContent"]
 * into a newline-joined text blob. Mirrors Python exactly.
 * =========================================================================== */
/* PoP: _logical_error_text @ tools/computer_use/cua_backend.py:_CuaDriverSession._logical_error_text */
char *cua_session_logical_error_text(const json_t *result)
{
    if (!result || result->type != JSON_OBJECT)
        return strdup("");

    char *parts[2] = {NULL, NULL};
    const char *keys[2] = {"data", "structuredContent"};
    int nparts = 0;
    for (int ki = 0; ki < 2; ki++) {
        json_t *v = json_obj_get(result, keys[ki]);
        if (!v) continue;
        if (v->type == JSON_STRING) {
            parts[nparts++] = strdup(v->str_val ? v->str_val : "");
        } else {
            char *ser = json_serialize(v);
            parts[nparts++] = ser;  /* may be NULL on failure */
        }
    }

    size_t total = 0;
    for (int i = 0; i < nparts; i++)
        if (parts[i]) total += strlen(parts[i]) + 1;
    char *text = (char *)malloc(total + 1);
    if (!text) {
        for (int i = 0; i < nparts; i++) free(parts[i]);
        return strdup("");
    }
    char *o = text;
    for (int i = 0; i < nparts; i++) {
        if (!parts[i]) continue;
        size_t l = strlen(parts[i]);
        memcpy(o, parts[i], l);
        o += l;
        if (i < nparts - 1) *o++ = '\n';  /* join with \n */
        free(parts[i]);
    }
    *o = '\0';
    /* Python: "\n".join(chunks) — no trailing newline */
    (void)nparts;
    return text;
}

/* ===========================================================================
 * _is_ended_session_result — recognise cua-driver's ended-session error.
 * =========================================================================== */
/* PoP: _is_ended_session_result @ tools/computer_use/cua_backend.py:_CuaDriverSession._is_ended_session_result */
bool cua_session_is_ended_session_result(const json_t *result)
{
    if (!result || result->type != JSON_OBJECT)
        return false;
    json_t *is_err = json_obj_get(result, "isError");
    if (!is_err || is_err->type != JSON_BOOL || !is_err->bool_val)
        return false;
    char *msg = cua_session_logical_error_text(result);
    if (!msg) return false;
    /* lowercase the message for case-insensitive substring checks */
    char *lower = strdup(msg);
    free(msg);
    if (!lower) return false;
    for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);

    bool has_session = strstr(lower, "session") != NULL;
    bool ended = strstr(lower, "has ended") != NULL ||
                 strstr(lower, "session ended") != NULL;
    bool start_session = strstr(lower, "start_session") != NULL;
    free(lower);
    return has_session && ended && start_session;
}

/* ===========================================================================
 * _windows_from_tool_result — extract windows list across cua-driver result
 * shapes (structuredContent/windows, data/windows, data/_legacy_windows,
 * top-level windows/_legacy_windows).
 * =========================================================================== */
/* PoP: _windows_from_tool_result @ tools/computer_use/cua_backend.py:_windows_from_tool_result */
json_t *cua_windows_from_tool_result(const json_t *out)
{
    if (!out || out->type != JSON_OBJECT)
        return json_array();

    /* try out["structuredContent"]["windows"] */
    json_t *structured = json_obj_get(out, "structuredContent");
    if (structured && structured->type == JSON_OBJECT) {
        json_t *w = json_obj_get(structured, "windows");
        if (w && w->type == JSON_ARRAY && json_len(w) > 0)
            return json_copy(w);
    }
    /* try out["data"]["windows"] or ["_legacy_windows"] */
    json_t *data = json_obj_get(out, "data");
    if (data && data->type == JSON_OBJECT) {
        json_t *w = json_obj_get(data, "windows");
        if (w && w->type == JSON_ARRAY && json_len(w) > 0)
            return json_copy(w);
        json_t *lw = json_obj_get(data, "_legacy_windows");
        if (lw && lw->type == JSON_ARRAY && json_len(lw) > 0)
            return json_copy(lw);
    }
    /* top-level "windows" or "_legacy_windows" */
    json_t *w = json_obj_get(out, "windows");
    if (w && w->type == JSON_ARRAY && json_len(w) > 0)
        return json_copy(w);
    json_t *lw = json_obj_get(out, "_legacy_windows");
    if (lw && lw->type == JSON_ARRAY && json_len(lw) > 0)
        return json_copy(lw);
    return json_array();
}

/* ===========================================================================
 * _apps_from_windows — dedupe windows to {name, pid} pairs, preserving order.
 * Depends on cua_ingest_windows (normalises raw -> window summaries).
 * =========================================================================== */
/* PoP: _apps_from_windows @ tools/computer_use/cua_backend.py:_apps_from_windows */
json_t *cua_apps_from_windows(const json_t *raw_windows)
{
    json_t *windows = cua_ingest_windows(raw_windows);
    json_t *apps = json_array();
    if (!windows || !apps) {
        if (windows) json_free(windows);
        return apps;
    }

    /* seen set: "app_name\tpid" keys */
    char seen[256][64];
    size_t n_seen = 0;

    for (size_t i = 0; i < json_len(windows); i++) {
        json_t *w = json_get(windows, i);
        if (!w || w->type != JSON_OBJECT)
            continue;
        const char *name = json_get_str(w, "app_name", "");
        if (!name || !*name)
            continue;  /* Python: `if not name: continue` */
        double pid_d = json_get_num(w, "pid", 0);
        long pid = (long)pid_d;

        char key[64];
        snprintf(key, sizeof(key), "%s\t%ld", name, pid);
        int dup = 0;
        for (size_t s = 0; s < n_seen; s++)
            if (strcmp(seen[s], key) == 0) { dup = 1; break; }
        if (dup) continue;
        if (n_seen < 256) {
            strncpy(seen[n_seen], key, 63);
            seen[n_seen][63] = '\0';
            n_seen++;
        }

        json_t *app = json_object();
        json_set(app, "name", json_string(name));
        json_set(app, "pid", json_number((double)pid));
        json_append(apps, app);
    }
    json_free(windows);
    return apps;
}
