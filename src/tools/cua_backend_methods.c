#define _GNU_SOURCE
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
/* PoP: cua_ingest_windows @ tools/computer_use/cua_backend.py:_ingest_windows */
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
/* PoP: cua_daemon_child_env @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon.child_env */
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
/* PoP: cua_daemon_drain_stderr @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon._drain_stderr */
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
/* PoP: cua_daemon_proxy_invocation @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon.proxy_invocation */
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
/* PoP: cua_session_supports_input_property @ tools/computer_use/cua_backend.py:_CuaDriverSession.supports_input_property */
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
/* PoP: cua_session_logical_error_text @ tools/computer_use/cua_backend.py:_CuaDriverSession._logical_error_text */
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
/* PoP: cua_session_is_ended_session_result @ tools/computer_use/cua_backend.py:_CuaDriverSession._is_ended_session_result */
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
/* PoP: cua_windows_from_tool_result @ tools/computer_use/cua_backend.py:_windows_from_tool_result */
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
/* PoP: cua_apps_from_windows @ tools/computer_use/cua_backend.py:_apps_from_windows */
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


/* ===========================================================================
 * REAL_GAP closure batch — faithful ports of the remaining unported
 * tools/computer_use/cua_backend.py functions (config surface, launch-args
 * policy, session-lock probe, transport-reset / outcome helpers, drag).
 * =========================================================================== */

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "../../lib/libyaml/yaml.h"
#include "tools_config_cua.h"

static int (*cua_contract_install_hook)(bool upgrade, bool show_installer_progress) = NULL;
extern char **environ;


/* helper: strip + lowercase into a malloc'd buffer (caller frees) */
static char *cua_strip_lower(const char *raw)
{
    if (!raw)
        raw = "";
    while (*raw == ' ' || *raw == '\t' || *raw == '\n' || *raw == '\r')
        raw++;
    size_t len = strlen(raw);
    while (len > 0 && (raw[len-1]==' '||raw[len-1]=='\t'||raw[len-1]=='\n'||raw[len-1]=='\r'))
        len--;
    char *out = malloc(len + 1);
    if (!out)
        return NULL;
    for (size_t i = 0; i < len; i++)
        out[i] = (char)tolower((unsigned char)raw[i]);
    out[len] = '\0';
    return out;
}

/* ---------------------------------------------------------------------------
 * _mcp_field — read an MCP model field across the 1.x -> 2.x rename.
 * `obj` is a JSON object; an absent key is Python's _MISSING. Returns the
 * borrowed value for snake, else camel, else default_val (borrowed).
 * ------------------------------------------------------------------------- */
/* PoP: cua_mcp_field @ tools/computer_use/cua_backend.py:_mcp_field */
json_t *cua_mcp_field(const json_t *obj, const char *snake, const char *camel,
                      json_t *default_val)
{
    if (!obj || obj->type != JSON_OBJECT || !snake || !camel)
        return default_val;
    json_t *v = json_obj_get(obj, snake);
    if (v)
        return v;
    v = json_obj_get(obj, camel);
    return v ? v : default_val;
}

/* ---------------------------------------------------------------------------
 * _cua_configured_permission_mode — computer_use.permission_mode, default
 * "standard"; only standard/bounded honored, anything else falls closed.
 * Caller frees.
 * ------------------------------------------------------------------------- */
/* PoP: cua_configured_permission_mode @ tools/computer_use/cua_backend.py:_cua_configured_permission_mode */
char *cua_configured_permission_mode(const json_t *computer_use_cfg)
{
    const char *raw = json_get_str(computer_use_cfg, "permission_mode", "");
    /* Python: str(x or "").strip().lower() — None/empty collapses to "" */
    if (!raw || !*raw) {
        json_t *v = json_obj_get(computer_use_cfg, "permission_mode");
        raw = (v && v->type == JSON_STRING && v->str_val && *v->str_val) ? v->str_val : "";
    }
    char *mode = cua_strip_lower(raw);
    if (!mode || !*mode) {
        free(mode);
        return strdup("standard");
    }
    if (strcmp(mode, "standard") == 0 || strcmp(mode, "bounded") == 0)
        return mode;
    free(mode);
    return strdup("standard");
}

/* ---------------------------------------------------------------------------
 * _cua_capability_manifest — computer_use.capability_manifest path or NULL.
 * Caller frees a non-NULL result.
 * ------------------------------------------------------------------------- */
/* PoP: cua_capability_manifest @ tools/computer_use/cua_backend.py:_cua_capability_manifest */
char *cua_capability_manifest(const json_t *computer_use_cfg)
{
    json_t *v = json_obj_get(computer_use_cfg, "capability_manifest");
    if (!v || v->type != JSON_STRING || !v->str_val)
        return NULL;
    const char *raw = v->str_val;
    while (*raw == ' ' || *raw == '\t' || *raw == '\n' || *raw == '\r')
        raw++;
    size_t len = strlen(raw);
    while (len > 0 && (raw[len-1]==' '||raw[len-1]=='\t'||raw[len-1]=='\n'||raw[len-1]=='\r'))
        len--;
    if (len == 0)
        return NULL;
    char *out = malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, raw, len);
    out[len] = '\0';
    return out;
}

/* ---------------------------------------------------------------------------
 * _cua_grant_existing_profile — computer_use.grant_existing_profile bool.
 * ------------------------------------------------------------------------- */
/* PoP: cua_grant_existing_profile @ tools/computer_use/cua_backend.py:_cua_grant_existing_profile */
bool cua_grant_existing_profile(const json_t *computer_use_cfg)
{
    return json_get_bool(computer_use_cfg, "grant_existing_profile", false);
}

/* ---------------------------------------------------------------------------
 * _manifest_is_mode_independent — true when a capability manifest is schema
 * v3+ (the mode-independent ceiling). Unreadable/unparseable/non-dict ->
 * false so bounded stays the driver-authoritative path.
 * ------------------------------------------------------------------------- */
/* PoP: cua_manifest_is_mode_independent @ tools/computer_use/cua_backend.py:_manifest_is_mode_independent */
bool cua_manifest_is_mode_independent(const char *path)
{
    if (!path || !*path)
        return false;
    yaml_doc_t *doc = yaml_parse_file(path, NULL);
    if (!doc)
        return false;  /* unreadable or unparseable manifest */
    /* must be a mapping: yaml_map_keys yields keys only for maps */
    size_t nkeys = 0;
    char **keys = yaml_map_keys(doc, "", &nkeys);
    bool ok = false;
    if (keys && nkeys > 0) {
        /* version must be an int >= 3; bools rejected by requiring an
         * all-digit scalar (true/false never parse as digits) */
        char *raw = yaml_to_json_string(doc, "version");
        if (raw) {
            const char *p = raw;
            while (*p == ' ' || *p == '"')
                p++;
            const char *end = raw + strlen(raw);
            while (end > p && (end[-1] == ' ' || end[-1] == '"'))
                end--;
            int alldigits = end > p;
            for (const char *q = p; q < end; q++)
                if (!isdigit((unsigned char)*q))
                    alldigits = 0;
            if (alldigits) {
                long ver = strtol(p, NULL, 10);
                ok = ver >= 3;
            }
            free(raw);
        }
    }
    if (keys) {
        for (size_t i = 0; i < nkeys; i++)
            free(keys[i]);
        free(keys);
    }
    yaml_free(doc);
    return ok;
}

/* ---------------------------------------------------------------------------
 * _standard_runtime_launch_args — MCP args plus any private runtime socket
 * owned by this transport. Copies base args and returns
 * {"args": [...], "socket": str-or-null}. Caller json_free()s.
 * ------------------------------------------------------------------------- */
/* PoP: cua_standard_runtime_launch_args @ tools/computer_use/cua_backend.py:_standard_runtime_launch_args */
json_t *cua_standard_runtime_launch_args(const json_t *args,
                                         bool grant_existing_profile,
                                         const char *platform,
                                         const char *socket_path)
{
    json_t *out_args = json_array();
    json_t *result = json_object();
    if (!out_args || !result) {
        json_free(out_args);
        json_free(result);
        return NULL;
    }
    if (args && args->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(args); i++) {
            json_t *a = json_get(args, i);
            if (a)
                json_append(out_args, json_copy(a));
        }
    }
    if (!grant_existing_profile) {
        json_set(result, "args", out_args);
        json_set(result, "socket", json_null());
        return result;
    }
    json_append(out_args, json_string("--grant"));
    json_append(out_args, json_string("existing-profile"));
    if (!platform || strcmp(platform, "darwin") != 0) {
        json_set(result, "args", out_args);
        json_set(result, "socket", json_null());
        return result;
    }
    char *private_socket = NULL;
    if (socket_path && *socket_path) {
        private_socket = strdup(socket_path);
    } else {
        unsigned int rnd = (unsigned int)getpid();
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd >= 0) {
            ssize_t got = read(fd, &rnd, sizeof(rnd));
            (void)got;
            close(fd);
        }
        const char *tmpdir = getenv("TMPDIR");
        char buf[256];
        snprintf(buf, sizeof(buf), "%s/hermes-cua-standard-%08x%08x.sock",
                 (tmpdir && *tmpdir) ? tmpdir : "/tmp",
                 rnd, (unsigned int)(rnd * 2654435761u));
        private_socket = strdup(buf);
    }
    if (!private_socket) {
        json_set(result, "args", out_args);
        json_set(result, "socket", json_null());
        return result;
    }
    json_append(out_args, json_string("--socket"));
    json_append(out_args, json_string(private_socket));
    json_set(result, "args", out_args);
    json_set(result, "socket", json_string(private_socket));
    free(private_socket);
    return result;
}

/* ---------------------------------------------------------------------------
 * subprocess capture helper — shell-quoted argv, stdout slurped, exit code.
 * ------------------------------------------------------------------------- */
static char *cua_slurp_cmd(const char *const argv[], int *exit_code)
{
    size_t cap = 64;
    for (int i = 0; argv[i]; i++)
        cap += strlen(argv[i]) * 4 + 8;
    char *cmd = malloc(cap);
    if (!cmd)
        return NULL;
    cmd[0] = '\0';
    for (int i = 0; argv[i]; i++) {
        if (i)
            strcat(cmd, " ");
        strcat(cmd, "'");
        for (const char *q = argv[i]; *q; q++) {
            if (*q == '\'')
                strcat(cmd, "'\\''");
            else
                strncat(cmd, q, 1);
        }
        strcat(cmd, "'");
    }
    FILE *fp = popen(cmd, "r");
    free(cmd);
    if (!fp)
        return NULL;
    size_t bufsz = 4096, len = 0;
    char *buf = malloc(bufsz);
    if (!buf) { pclose(fp); return NULL; }
    size_t n;
    while ((n = fread(buf + len, 1, bufsz - len - 1, fp)) > 0) {
        len += n;
        if (len + 1 >= bufsz) {
            bufsz *= 2;
            char *nb = realloc(buf, bufsz);
            if (!nb) { free(buf); pclose(fp); return NULL; }
            buf = nb;
        }
    }
    buf[len] = '\0';
    int rc = pclose(fp);
    if (exit_code)
        *exit_code = (rc == -1) ? -1 : WEXITSTATUS(rc);
    return buf;
}

/* ---------------------------------------------------------------------------
 * _linux_session_locked — best-effort loginctl probe. Returns 1 (locked),
 * 0 (unlocked), -1 (unknown: non-Linux / no logind / probe failure).
 * ------------------------------------------------------------------------- */
/* PoP: cua_linux_session_locked @ tools/computer_use/cua_backend.py:_linux_session_locked */
int cua_linux_session_locked(void)
{
#ifdef __linux__
    int rc = -1;
    const char *list_argv[] = {"loginctl", "list-sessions", "--no-legend", NULL};
    char *out = cua_slurp_cmd(list_argv, &rc);
    if (!out)
        return -1;
    if (rc != 0) {
        free(out);
        return -1;
    }
    int any_seat = 0;
    char *saveptr = NULL;
    for (char *line = strtok_r(out, "\n", &saveptr); line;
         line = strtok_r(NULL, "\n", &saveptr)) {
        if (!strstr(line, "seat"))
            continue;
        char tmp[512];
        strncpy(tmp, line, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        char *sp2 = NULL;
        char *first = strtok_r(tmp, " \t", &sp2);
        char *second = first ? strtok_r(NULL, " \t", &sp2) : NULL;
        if (!first || !second)
            continue;
        any_seat = 1;
        const char *show_argv[6];
        show_argv[0] = "loginctl";
        show_argv[1] = "show-session";
        show_argv[2] = first;
        show_argv[3] = "-p";
        show_argv[4] = "LockedHint";
        show_argv[5] = NULL;
        char *pout = cua_slurp_cmd(show_argv, NULL);
        if (pout) {
            if (strstr(pout, "LockedHint=no")) {
                free(pout);
                free(out);
                return 0;
            }
            free(pout);
        }
    }
    free(out);
    return any_seat ? 1 : -1;
#else
    return -1;
#endif
}

/* ---------------------------------------------------------------------------
 * _empty_discovery_reason — one-line diagnosis for empty window discovery.
 * Caller frees.
 * ------------------------------------------------------------------------- */
/* PoP: cua_empty_discovery_reason @ tools/computer_use/cua_backend.py:_empty_discovery_reason */
char *cua_empty_discovery_reason(void)
{
    const char *msg;
#if defined(__linux__)
    int locked = cua_linux_session_locked();
#elif defined(__APPLE__)
    int locked = -1;
#else
    int locked = -1;
#endif
#if defined(__linux__)
    int is_linux = 1;
#else
    int is_linux = 0;
#endif
#if defined(__APPLE__)
    int is_darwin = 1;
#else
    int is_darwin = 0;
#endif
    if (is_linux && locked == 1) {
        msg = "the desktop session is LOCKED (loginctl LockedHint=yes) — "
              "unlock the screen; a locked compositor hides windows and "
              "freezes app renderers";
    } else if (is_linux && !getenv("DISPLAY")) {
        msg = "no DISPLAY is set — X11/XWayland is not reachable from this process";
    } else if (is_darwin) {
        msg = "window discovery returned no windows; on macOS this usually "
              "means no shareable display (headless Mac or panel asleep) — "
              "wake the display or attach a monitor/HDMI dummy, then run "
              "`hermes computer-use doctor`";
    } else {
        msg = "window discovery returned no windows; run `hermes computer-use "
              "doctor` (display reachability, AX capability)";
    }
    return strdup(msg);
}

/* ---------------------------------------------------------------------------
 * env JSON object -> execve-style envp array ("K=V" strings). Returns NULL
 * when env is absent/empty (caller then inherits environ).
 * ------------------------------------------------------------------------- */
static char **cua_env_json_to_envp(const json_t *env)
{
    if (!env || env->type != JSON_OBJECT || env->c.count == 0)
        return NULL;
    size_t n = env->c.count;
    char **envp = calloc(n + 1, sizeof(char *));
    if (!envp)
        return NULL;
    for (size_t i = 0; i < n; i++) {
        const char *k = env->c.keys ? env->c.keys[i] : NULL;
        json_t *v = env->c.items ? env->c.items[i] : NULL;
        if (!k || !v || v->type != JSON_STRING)
            continue;
        size_t klen = strlen(k), vlen = v->str_val ? strlen(v->str_val) : 0;
        char *entry = malloc(klen + vlen + 2);
        if (!entry)
            continue;
        memcpy(entry, k, klen);
        entry[klen] = '=';
        if (vlen)
            memcpy(entry + klen + 1, v->str_val, vlen);
        entry[klen + 1 + vlen] = '\0';
        envp[i] = entry;
    }
    return envp;
}

static void cua_free_envp(char **envp)
{
    if (!envp)
        return;
    for (char **e = envp; *e; e++)
        free(*e);
    free(envp);
}

/* run "<command> stop --socket <socket_path>" with sanitized child env,
 * fire-and-forget like subprocess.run(..., timeout=3). */
static void cua_run_driver_stop(const char *command, const char *socket_path,
                                char **envp)
{
    if (!command || !*command)
        return;
    pid_t worker = fork();
    if (worker < 0)
        return;
    if (worker == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, 0);
            dup2(devnull, 1);
            dup2(devnull, 2);
            if (devnull > 2)
                close(devnull);
        }
        char *argv[5];
        argv[0] = (char *)command;
        argv[1] = (char *)"stop";
        argv[2] = (char *)"--socket";
        argv[3] = (char *)(socket_path ? (char *)socket_path : "");
        argv[4] = NULL;
        if (envp)
            execvpe(command, argv, envp);
        else
            execvp(command, argv);
        _exit(127);
    }
    int status = 0;
    for (int i = 0; i < 30; i++) {
        if (waitpid(worker, &status, WNOHANG) == worker)
            return;
        usleep(100 * 1000);
    }
    kill(worker, SIGKILL);
    waitpid(worker, &status, 0);
}

/* ---------------------------------------------------------------------------
 * stop (_EmbeddedCuaDaemon.stop) — graceful driver stop, reaping of the owned
 * child with the wait/terminate/kill ladder, then socket removal on POSIX.
 * `owned_pid` is the daemon process handle (<= 0 when not running).
 * ------------------------------------------------------------------------- */
/* PoP: cua_daemon_stop @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon.stop */
void cua_daemon_stop(const char *command, const char *socket_path, pid_t owned_pid,
                     const json_t *child_env)
{
    char **envp = cua_env_json_to_envp(child_env);
    if (owned_pid > 0 && command && *command)
        cua_run_driver_stop(command, socket_path, envp);
    if (owned_pid > 0) {
        /* process.wait(timeout=5) -> terminate -> kill ladder */
        int status = 0;
        int reaped = 0;
        for (int i = 0; i < 100 && !reaped; i++)
            reaped = (waitpid(owned_pid, &status, WNOHANG) == owned_pid),
            usleep(50 * 1000);
        if (!reaped) {
            kill(owned_pid, SIGTERM);
            for (int i = 0; i < 40 && !reaped; i++)
                reaped = (waitpid(owned_pid, &status, WNOHANG) == owned_pid),
                usleep(50 * 1000);
        }
        if (!reaped) {
            kill(owned_pid, SIGKILL);
            for (int i = 0; i < 40 && !reaped; i++)
                reaped = (waitpid(owned_pid, &status, WNOHANG) == owned_pid),
                usleep(50 * 1000);
        }
    }
#ifndef _WIN32
    if (socket_path && *socket_path && access(socket_path, F_OK) == 0)
        unlink(socket_path);
#endif
    cua_free_envp(envp);
}

/* ---------------------------------------------------------------------------
 * stop (_AsyncBridge.stop) — signal the loop thread to stop and join it
 * (join(timeout=2.0)); clears both handles exactly once.
 * ------------------------------------------------------------------------- */
/* PoP: cua_bridge_stop @ tools/computer_use/cua_backend.py:_AsyncBridge.stop */
void cua_bridge_stop(pthread_t *thread, volatile int *loop_running,
                     void (*loop_stop_hook)(void *), void *loop_userdata)
{
    if (loop_running && *loop_running && loop_stop_hook)
        loop_stop_hook(loop_userdata);   /* call_soon_threadsafe(self._loop.stop) */
    if (loop_running)
        *loop_running = 0;
    if (thread && *thread) {
#if defined(__linux__) && defined(_GNU_SOURCE)
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        if (pthread_timedjoin_np(*thread, NULL, &ts) == 0)
            *thread = (pthread_t)0;
#else
        pthread_join(*thread, NULL);
        *thread = (pthread_t)0;
#endif
    }
}


/* ===========================================================================
 * _CuaDriverSession transport-reset / outcome helpers and CuaDriverBackend
 * drag — REAL_GAP closure batch part 2.
 * =========================================================================== */

/* ---------------------------------------------------------------------------
 * stop (_CuaDriverSession.stop) — under the lifecycle lock: when never
 * started only the owned standard runtime teardown runs; otherwise mark
 * stopped, tear down the MCP lifecycle coroutine, then the owned runtime.
 * ------------------------------------------------------------------------- */
/* PoP: cua_session_stop @ tools/computer_use/cua_backend.py:_CuaDriverSession.stop */
void cua_session_stop(bool *started_flag,
                      void (*stop_lifecycle_locked)(void *), void *lifecycle_ud,
                      char **owned_socket_slot)
{
    if (!started_flag || !*started_flag) {
        cua_session_stop_owned_standard_runtime_locked(owned_socket_slot, NULL);
        return;
    }
    *started_flag = false;
    if (stop_lifecycle_locked)
        stop_lifecycle_locked(lifecycle_ud);
    cua_session_stop_owned_standard_runtime_locked(owned_socket_slot, NULL);
}

/* ---------------------------------------------------------------------------
 * set_transport_reset_callback — register a synchronous cache-invalidation
 * hook for transport swaps.
 * ------------------------------------------------------------------------- */
/* PoP: cua_session_set_transport_reset_callback @ tools/computer_use/cua_backend.py:_CuaDriverSession.set_transport_reset_callback */
void cua_session_set_transport_reset_callback(
    void (**out_callback)(void *), void **out_userdata,
    void (*callback)(void *), void *userdata)
{
    if (out_callback)
        *out_callback = callback;
    if (out_userdata)
        *out_userdata = userdata;
}

/* ---------------------------------------------------------------------------
 * _notify_transport_reset — invoke the registered hook, swallowing/logging
 * any failure like Python's except branch.
 * ------------------------------------------------------------------------- */
/* PoP: cua_session_notify_transport_reset @ tools/computer_use/cua_backend.py:_CuaDriverSession._notify_transport_reset */
void cua_session_notify_transport_reset(void (**callback)(void *), void **userdata)
{
    if (!callback || !*callback)
        return;
    /* Python wraps callback() in try/except Exception -> logger.debug */
    (*callback)(userdata ? *userdata : NULL);
}

/* ---------------------------------------------------------------------------
 * _stop_owned_standard_runtime_locked — stop the exact private macOS app
 * daemon launched for a grant. socket_path_slot is the session's
 * `_owned_standard_runtime_socket` field (cleared on exit).
 * ------------------------------------------------------------------------- */
/* PoP: cua_session_stop_owned_standard_runtime_locked @ tools/computer_use/cua_backend.py:_CuaDriverSession._stop_owned_standard_runtime_locked */
void cua_session_stop_owned_standard_runtime_locked(char **socket_path_slot,
                                                    char *driver_command_override)
{
    if (!socket_path_slot || !*socket_path_slot || !**socket_path_slot)
        return;
    char *socket_path = *socket_path_slot;
    *socket_path_slot = NULL;
    char *driver_command = driver_command_override;
    if (!driver_command) {
        driver_command = cua_backend_resolve_cua_driver_cmd(NULL);
    }
    if (driver_command && *driver_command) {
        /* subprocess.run([...,"stop","--socket",socket], env=sanitized) */
        char **envp = NULL;
        {
            /* tools/environments/local._sanitize_subprocess_env core: drop
             * display/input-injecting vars from the child environment */
            static const char *drop[] = {"DISPLAY", "WAYLAND_DISPLAY",
                                         "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS",
                                         "XDG_RUNTIME_DIR"};
            size_t keep = 0;
            for (size_t i = 0; environ[i]; i++)
                keep++;
            char **merged = calloc(keep + 1, sizeof(char *));
            if (merged) {
                size_t t = 0;
                for (size_t i = 0; i < keep; i++) {
                    int skip = 0;
                    for (size_t d = 0; d < sizeof(drop)/sizeof(drop[0]); d++) {
                        size_t dl = strlen(drop[d]);
                        if (strncmp(environ[i], drop[d], dl) == 0 && environ[i][dl] == '=')
                            skip = 1;
                    }
                    if (!skip)
                        merged[t++] = environ[i];
                }
                envp = merged;
            }
        }
        cua_run_driver_stop(driver_command, socket_path, envp);
        free(envp);
        if (!driver_command_override)
            free(driver_command);
    }
    if (access(socket_path, F_OK) == 0)
        unlink(socket_path);
    free(socket_path);
}

/* ---------------------------------------------------------------------------
 * _restore_declared_session_after_transport_reset — re-attach the public
 * label inside a replacement private lifecycle. `call_tool` performs one MCP
 * tool invocation ("start_session") synchronously and returns its result
 * object (borrowed-shape; caller keeps ownership); mirrors
 * self._bridge.run(self._call_tool_async(...), timeout=timeout).
 * ------------------------------------------------------------------------- */
/* PoP: cua_session_restore_declared_session_after_transport_reset @ tools/computer_use/cua_backend.py:_CuaDriverSession._restore_declared_session_after_transport_reset */
void cua_session_restore_declared_session_after_transport_reset(
    const char *declared_session_id, double timeout,
    json_t *(*call_tool)(const char *tool, const json_t *args, void *ud),
    void *ud)
{
    (void)timeout;  /* enforced by the embedder's bridge implementation */
    if (!declared_session_id || !*declared_session_id)
        return;
    if (!call_tool)
        return;
    json_t *args = json_object();
    json_set(args, "session", json_string(declared_session_id));
    json_t *result = call_tool("start_session", args, ud);
    json_free(args);
    if (result && result->type == JSON_OBJECT) {
        json_t *err = json_obj_get(result, "isError");
        bool is_err = err && ((err->type == JSON_BOOL && err->bool_val) ||
                              (err->type == JSON_NUMBER && err->num_val == 1));
        if (is_err) {
            char *text = cua_session_logical_error_text(result);
            fprintf(stderr,
                    "WARNING computer_use: cua-driver public session label %s "
                    "could not be restored: %s\n",
                    declared_session_id, text ? text : "");
            free(text);
        }
    }
}

/* ---------------------------------------------------------------------------
 * _TRANSPORT_REPLAY_SAFE_TOOLS frozenset + membership test.
 * ------------------------------------------------------------------------- */
static const char *cua_transport_replay_safe_tools[] = {
    "get_cursor_position",
    "get_displays",
    "get_screen_size",
    "get_window_state",
    "list_apps",
    "list_windows",
    NULL,
};

/* PoP: cua_transport_replay_is_safe @ tools/computer_use/cua_backend.py:_CuaDriverSession._transport_replay_is_safe */
bool cua_transport_replay_is_safe(const char *name)
{
    if (!name)
        return false;
    for (size_t i = 0; cua_transport_replay_safe_tools[i]; i++)
        if (strcmp(cua_transport_replay_safe_tools[i], name) == 0)
            return true;
    return false;
}

/* shared outcome-dict builder: data/images/image_mime_types/
 * structuredContent{ok,code,message,operation,next_step,detail}/isError */
static json_t *cua_outcome_dict(const char *code, const char *operation,
                                const char *detail, const char *message)
{
    json_t *result = json_object();
    json_t *sc = json_object();
    if (!result || !sc) {
        json_free(result);
        json_free(sc);
        return NULL;
    }
    json_set(result, "data", json_string(message));
    json_set(result, "images", json_array());
    json_set(result, "image_mime_types", json_array());
    json_set(sc, "ok", json_bool(false));
    json_set(sc, "code", json_string(code));
    json_set(sc, "message", json_string(message));
    json_set(sc, "operation", json_string(operation ? operation : ""));
    json_set(sc, "next_step", json_string("fresh_state"));
    json_set(sc, "detail", json_string(detail ? detail : ""));
    json_set(result, "structuredContent", sc);
    json_set(result, "isError", json_bool(true));
    return result;
}

/* ---------------------------------------------------------------------------
 * _unknown_transport_outcome — fail-closed result when a transport failure
 * left the action outcome unknown; never replayed automatically.
 * ------------------------------------------------------------------------- */
/* PoP: cua_unknown_transport_outcome @ tools/computer_use/cua_backend.py:_CuaDriverSession._unknown_transport_outcome */
json_t *cua_unknown_transport_outcome(const char *name, const char *exc_text)
{
    char message[1024];
    snprintf(message, sizeof(message),
             "cua-driver transport failed during %s; the action outcome is "
             "unknown, so Hermes did not replay it. Take fresh state before "
             "deciding whether to act again.", name ? name : "");
    return cua_outcome_dict("transport_outcome_unknown", name, exc_text, message);
}

/* ---------------------------------------------------------------------------
 * _timeout_outcome — fail-closed result for an MCP call that hit its
 * deadline (#74799); same effect_disposition=unknown principle.
 * ------------------------------------------------------------------------- */
/* PoP: cua_timeout_outcome @ tools/computer_use/cua_backend.py:_CuaDriverSession._timeout_outcome */
json_t *cua_timeout_outcome(const char *name, const char *exc_text)
{
    char message[1024];
    snprintf(message, sizeof(message),
             "cua-driver MCP call %s timed out; the action outcome is "
             "unknown and may still have taken effect on the remote screen. "
             "The session has been marked suspect and will be recreated before "
             "the next computer-use call. Take fresh state before deciding "
             "whether to act again.", name ? name : "");
    return cua_outcome_dict("timeout_outcome_unknown", name, exc_text, message);
}

/* ---------------------------------------------------------------------------
 * _maybe_repair_runtime_contract — one automatic driver repair attempt for a
 * failed runtime contract. Never raises. An explicit HERMES_CUA_DRIVER_CMD
 * override is authoritative even when broken; a missing binary means install
 * was never requested. The actual installer is injected via
 * cua_backend_set_contract_install_hook() (the Python side imports
 * hermes_cli.tools_config.install_cua_driver lazily).
 * ------------------------------------------------------------------------- */
static int cua_contract_repair_attempted = 0;

/* PoP: cua_backend_set_contract_install_hook @ tools/computer_use/cua_backend.py:_maybe_repair_runtime_contract */
void cua_backend_set_contract_install_hook(int (*hook)(bool upgrade, bool show_installer_progress))
{
    cua_contract_install_hook = hook;
}

/* PoP: cua_maybe_repair_runtime_contract @ tools/computer_use/cua_backend.py:_maybe_repair_runtime_contract */
json_t *cua_maybe_repair_runtime_contract(const json_t *contract)
{
    if (contract && contract->type == JSON_OBJECT) {
        json_t *ready = json_obj_get(contract, "ready");
        bool is_ready = ready && ((ready->type == JSON_BOOL && ready->bool_val));
        if (is_ready)
            return contract ? json_copy(contract) : NULL;
    }
    if (cua_contract_repair_attempted)
        return contract ? json_copy(contract) : NULL;
    const char *override_cmd = getenv("HERMES_CUA_DRIVER_CMD");
    if (override_cmd && *override_cmd && strspn(override_cmd, " \t") < strlen(override_cmd))
        return contract ? json_copy(contract) : NULL;
    const char *binary = contract ? json_get_str(contract, "binary", "") : "";
    if (!binary || !*binary)
        return contract ? json_copy(contract) : NULL;
    cua_contract_repair_attempted = 1;
    if (!cua_contract_install_hook ||
        !cua_contract_install_hook(false, false))
        return contract ? json_copy(contract) : NULL;
    /* return cua_driver_runtime_contract_status() */
    json_t *fresh = cua_backend_cua_driver_runtime_contract_status(binary);
    return fresh ? fresh : (contract ? json_copy(contract) : NULL);
}

/* ---------------------------------------------------------------------------
 * _is_placeholder_id — true when *value* is a schema-filler id rather than a
 * real target. Bools are never placeholders; non-numeric strings are NOT
 * placeholders either (they reach validation errors instead).
 * ------------------------------------------------------------------------- */
/* PoP: cua_is_placeholder_id @ tools/computer_use/cua_backend.py:_is_placeholder_id */
bool cua_is_placeholder_id(const json_t *value)
{
    if (!value)
        return false;   /* not an int/str -> False */
    if (value->type == JSON_STRING) {
        const char *s = value->str_val;
        if (!s)
            return false;
        char *end = NULL;
        long v = strtol(s, &end, 10);
        if (end == s)
            return false;      /* ValueError -> False */
        while (*end == ' ' || *end == '\t')
            end++;
        if (*end != '\0')
            return false;
        return v <= 0;
    }
    if (value->type != JSON_NUMBER)
        return false;          /* bool/None/float-only shapes -> False */
    long v = (long)value->num_val;
    return v <= 0;
}

/* ---------------------------------------------------------------------------
 * _handle_transport_reset — invalidate every capability minted by the
 * replaced transport: clear the active capture target and reset the typed
 * browser route state.
 * ------------------------------------------------------------------------- */
/* PoP: cua_backend_handle_transport_reset @ tools/computer_use/cua_backend.py:CuaDriverBackend._handle_transport_reset */
void cua_backend_handle_transport_reset(void (*clear_active_target)(void *),
                                        void *clear_ud,
                                        json_t *typed_browser_route_state)
{
    if (clear_active_target)
        clear_active_target(clear_ud);
    if (typed_browser_route_state && typed_browser_route_state->type == JSON_OBJECT) {
        /* route.state.clear(): remove every key in place */
        while (typed_browser_route_state->c.count > 0) {
            const char *k = typed_browser_route_state->c.keys[0];
            if (!k || !json_obj_del(typed_browser_route_state, k))
                break;
        }
    }
}

/* helper: error ActionResult for drag preconditions */
static json_t *cua_drag_error(const char *message)
{
    json_t *r = json_object();
    if (!r)
        return NULL;
    json_set(r, "ok", json_bool(false));
    json_set(r, "action", json_string("drag"));
    json_set(r, "message", json_string(message));
    return r;
}

/* ---------------------------------------------------------------------------
 * drag — element- or coordinate-based drag routed through _run_input_action.
 * Pass exactly one pair of sources/targets; returns the ActionResult JSON
 * (caller frees) — an {ok:false,action:"drag",message:...} precondition
 * error or whatever run_input_action produced.
 * ------------------------------------------------------------------------- */
/* PoP: cua_backend_drag @ tools/computer_use/cua_backend.py:CuaDriverBackend.drag */
json_t *cua_backend_drag(long active_pid, long active_window_id,
                         long from_element, long to_element, int use_elements,
                         long from_x, long from_y, long to_x, long to_y,
                         int use_coordinates,
                         json_t *(*run_input_action)(const char *action,
                                                     const json_t *args,
                                                     void *ud),
                         void *ud)
{
    if (active_pid <= 0)
        return cua_drag_error("No active window — call capture() first.");
    json_t *args = json_object();
    if (!args)
        return NULL;
    json_set(args, "pid", json_number((double)active_pid));
    if (use_elements) {
        if (active_window_id <= 0) {
            json_free(args);
            return cua_drag_error("No active window_id for element-based drag.");
        }
        json_set(args, "from_element", json_number((double)from_element));
        json_set(args, "to_element", json_number((double)to_element));
        json_set(args, "window_id", json_number((double)active_window_id));
    } else if (use_coordinates) {
        if (active_window_id <= 0) {
            json_free(args);
            return cua_drag_error("No active window_id for coordinate drag.");
        }
        json_set(args, "from_x", json_number((double)from_x));
        json_set(args, "from_y", json_number((double)from_y));
        json_set(args, "to_x", json_number((double)to_x));
        json_set(args, "to_y", json_number((double)to_y));
        json_set(args, "window_id", json_number((double)active_window_id));
    } else {
        json_free(args);
        return cua_drag_error("drag requires from_element/to_element or "
                              "from_coordinate/to_coordinate.");
    }
    json_t *result = run_input_action ? run_input_action("drag", args, ud) : NULL;
    json_free(args);
    return result;
}

