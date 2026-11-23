/*
 * mcp_liveness.c — real C11 port of upstream Hermes tools/mcp_liveness.py.
 *
 * PoP: mcpliveness_action @ tools/mcp_liveness.py:_action
 * PoP: mcpliveness_availability_problem @ tools/mcp_liveness.py:availability_problem
 * PoP: mcpliveness_parse @ tools/mcp_liveness.py:parse_liveness
 * PoP: mcpliveness_describe @ tools/mcp_liveness.py:describe
 *
 * location_is_rooted / KEY_NAME_RE / gpu_label are ported verbatim from
 * hermes_platform.declaration; the OS family is injected so the gate is
 * deterministic. Runtime status()/AppResolver probing is out of scope.
 */
#include "noc_w6_mcpliveness_08bec8.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(const char *s) {
    const char *a = s ? s : "", *end;
    size_t n;
    char *out;
    while (*a && isspace((unsigned char)*a)) a++;
    end = a + strlen(a);
    while (end > a && isspace((unsigned char)end[-1])) end--;
    n = (size_t)(end - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

static json_t *action_pair(const char *action, const char *retry) {
    json_t *r = json_array();
    json_append(r, json_string(action));
    json_append(r, json_string(retry));
    return r;
}

json_t *mcpliveness_action(const char *state, const char *app_name, const char *gpu_label) {
    const char *s = state ? state : "", *a = app_name ? app_name : "";
    char buf[1024];
    if (!strcmp(s, "app_not_running")) {
        snprintf(buf, sizeof(buf), "Start %s, then try again.", a);
        return action_pair(buf, "after_user_action");
    }
    if (!strcmp(s, "hermes_not_connected")) {
        snprintf(buf, sizeof(buf), "Reconnect %s in Hermes, then try again.", a);
        return action_pair(buf, "after_user_action");
    }
    if (!strcmp(s, "endpoint_unavailable")) {
        snprintf(buf, sizeof(buf), "Open %s and enable its local connection, then try again.", a);
        return action_pair(buf, "after_user_action");
    }
    if (!strcmp(s, "no_interactive_session")) {
        snprintf(buf, sizeof(buf), "Open an interactive desktop session and start %s, then try again.", a);
        return action_pair(buf, "never_here");
    }
    if (!strcmp(s, "version_too_old")) {
        snprintf(buf, sizeof(buf), "Update %s, then try again.", a);
        return action_pair(buf, "after_user_action");
    }
    if (!strcmp(s, "missing_app")) {
        snprintf(buf, sizeof(buf), "Install %s, then try again.", a);
        return action_pair(buf, "after_user_action");
    }
    if (!strcmp(s, "unsupported_gpu")) {
        snprintf(buf, sizeof(buf), "Use %s on a machine with %s.", a, gpu_label ? gpu_label : "");
        return action_pair(buf, "never_here");
    }
    return action_pair("", "");
}

json_t *mcpliveness_availability_problem(const char *state) {
    const char *s = state ? state : "";
    if (!strcmp(s, "missing_app") || !strcmp(s, "unsupported_os")) return json_string("missing_app");
    if (!strcmp(s, "unsupported_gpu")) return json_string("unsupported_gpu");
    if (!strcmp(s, "version_too_old")) return json_string("version_too_old");
    return json_null();
}

/* hermes_platform.declaration.location_is_rooted, ported verbatim. */
static int location_rooted(const char *loc, const char *osf) {
    size_t n, i;
    char *slash_copy;
    const char *p;
    int bad_component = 0;
    if (!loc || !*loc) return 0;
    n = strlen(loc);
    slash_copy = (char *)malloc(n + 1);
    if (!slash_copy) return 0;
    for (i = 0; i < n; i++) slash_copy[i] = (loc[i] == '\\') ? '/' : loc[i];
    slash_copy[n] = '\0';
    p = slash_copy;
    while (*p) {
        const char *seg = p, *end;
        while (*p && *p != '/') p++;
        end = p;
        if (*p == '/') p++;
        if ((size_t)(end - seg) == 2 && seg[0] == '.' && seg[1] == '.') { bad_component = 1; break; }
    }
    free(slash_copy);
    if (bad_component || strstr(loc, "://")) return 0;
    if (loc[0] == '~' || loc[0] == '%' || loc[0] == '$') return 1;
    if (!strcmp(osf, "win32"))
        return n >= 3 && isalpha((unsigned char)loc[0]) && loc[1] == ':' &&
               (loc[2] == '\\' || loc[2] == '/');
    return loc[0] == '/';
}

/* KEY_NAME_RE = [A-Za-z0-9_.-]+ (fullmatch) */
static int key_name_ok(const char *s) {
    size_t n;
    if (!s || !*s) return 0;
    n = strlen(s);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!(isalnum(c) || c == '_' || c == '.' || c == '-')) return 0;
    }
    return 1;
}

static json_t *parse_error(const char *msg) {
    json_t *r = json_object();
    json_set(r, "ok", json_bool(false));
    json_set(r, "error", json_string(msg));
    return r;
}

static json_t *parse_ok(const char *kind, const char *path,
                        const char *url, const char *token, const char *pid) {
    json_t *r = json_object();
    json_set(r, "ok", json_bool(true));
    json_set(r, "kind", json_string(kind));
    json_set(r, "path", json_string(path ? path : ""));
    json_set(r, "url_field", json_string(url ? url : "http"));
    json_set(r, "token_field", json_string(token ? token : "token"));
    json_set(r, "pid_field", json_string(pid ? pid : "pid"));
    return r;
}

static const char *kind_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "None";
    if (v->type == JSON_STRING) return v->str_val;
    return "";
}

json_t *mcpliveness_parse(const json_t *facts) {
    const json_t *raw, *fields_raw;
    const char *osf, *kind, *path;
    char *path_t = NULL, *u_t = NULL, *k_t = NULL, *p_t = NULL;
    const char *url = "http", *token = "token", *pid = "pid";
    json_t *out;
    int extra = 0, bad_value = 0, bad_key = 0;
    size_t i;

    if (!facts || facts->type != JSON_OBJECT) return parse_error("liveness must be an object");
    raw = json_obj_get(facts, "raw");
    osf = json_get_str(facts, "osf", "win32");
    if (!raw || raw->type != JSON_OBJECT) return parse_error("liveness must be an object");

    kind = json_get_str(raw, "kind", "");
    if (!strcmp(kind, "static") || !strcmp(kind, "interactive_session")) {
        if (json_object_size(raw) != 1) {
            return parse_error(!strcmp(kind, "static")
                ? "static liveness only accepts 'kind'"
                : "interactive_session liveness only accepts 'kind'");
        }
        return parse_ok(kind, "", "http", "token", "pid");
    }
    if (strcmp(kind, "server_json")) {
        char buf[256];
        snprintf(buf, sizeof(buf), "unknown liveness kind: '%s'", kind_repr(json_obj_get(raw, "kind")));
        return parse_error(buf);
    }
    for (i = 0; i < json_object_size(raw); i++) {
        const char *k = json_object_get_key_at(raw, i);
        if (k && strcmp(k, "kind") && strcmp(k, "path") && strcmp(k, "fields"))
            return parse_error("server_json liveness has unknown fields");
    }
    fields_raw = json_obj_get(raw, "fields");
    if (fields_raw && fields_raw->type != JSON_NULL && fields_raw->type != JSON_OBJECT)
        return parse_error("server_json liveness fields must be an object");
    path = json_get_str(raw, "path", "");
    path_t = trim(path);
    if (!path_t || !*path_t) {
        free(path_t);
        return parse_error("server_json liveness requires a non-empty path");
    }
    if (!location_rooted(path_t, osf)) {
        free(path_t);
        return parse_error("server_json liveness path must be absolute or start with ~ / %VAR% / $VAR, without '..' or a URL scheme");
    }
    if (fields_raw && fields_raw->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(fields_raw); i++) {
            const char *k = json_object_get_key_at(fields_raw, i);
            json_t *v = json_object_get_at(fields_raw, i);
            char *vt;
            if (!k) continue;
            if (strcmp(k, "url") && strcmp(k, "token") && strcmp(k, "pid")) { extra = 1; continue; }
            if (!v || v->type != JSON_STRING) { bad_value = 1; continue; }
            vt = trim(v->str_val);
            if (!vt || !*vt) { bad_value = 1; free(vt); continue; }
            if (!strcmp(k, "url")) { u_t = vt; url = u_t; }
            else if (!strcmp(k, "token")) { k_t = vt; token = k_t; }
            else { p_t = vt; pid = p_t; }
        }
    }
    if (extra) { free(path_t); free(u_t); free(k_t); free(p_t);
        return parse_error("server_json liveness fields may only override url, token, and pid"); }
    if (bad_value) { free(path_t); free(u_t); free(k_t); free(p_t);
        return parse_error("server_json liveness field names must be non-empty strings"); }
    /* KEY_NAME_RE check over the three final (stripped) field values. */
    {
        char *vals[3];
        int j;
        vals[0] = trim(url); vals[1] = trim(token); vals[2] = trim(pid);
        for (j = 0; j < 3; j++) if (!key_name_ok(vals[j])) bad_key = 1;
        for (j = 0; j < 3; j++) free(vals[j]);
    }
    if (bad_key) { free(path_t); free(u_t); free(k_t); free(p_t);
        return parse_error("server_json liveness field names must be simple key names"); }
    out = parse_ok("server_json", path_t, url, token, pid);
    free(path_t); free(u_t); free(k_t); free(p_t);
    return out;
}

static const char *gpu_label_of(const char *gpu) {
    return (gpu && !strcmp(gpu, "nvidia")) ? "an NVIDIA GPU" : "a supported GPU";
}

json_t *mcpliveness_describe(const json_t *facts) {
    const json_t *av;
    const char *name, *state, *display, *gpu, *minv, *ver, *app;
    char action[1024], reason[1024], buf[2048];
    json_t *pair;
    if (!facts || facts->type != JSON_OBJECT) return json_string("");
    name = json_get_str(facts, "name", "");
    gpu = gpu_label_of(json_get_str(facts, "required_gpu", ""));
    av = json_obj_get(facts, "available");
    state = json_get_str(facts, "liveness_state", "");
    minv = av ? json_get_str(av, "min_version", "") : "";
    ver = av ? json_get_str(av, "version", "") : "";
    display = json_get_str(facts, "display_name", "");
    app = (display && *display) ? display : name;

    pair = mcpliveness_action(state, app, gpu);
    snprintf(action, sizeof(action), "%s", json_get_str(pair, "x", "") ? "" : "");
    /* mcpliveness_action returns an array; read it directly. */
    if (pair && pair->type == JSON_ARRAY && json_len(pair) >= 1 && json_get(pair, 0)->type == JSON_STRING)
        snprintf(action, sizeof(action), "%s", json_get(pair, 0)->str_val);
    json_free(pair);

    if (!strcmp(state, "missing_app") && av && !strcmp(json_get_str(av, "state", ""), "unsupported_os")) {
        snprintf(reason, sizeof(reason), "%s is not supported on this operating system.", app);
        snprintf(action, sizeof(action), "Use a supported operating system.");
    } else if (!strcmp(state, "missing_app")) {
        snprintf(reason, sizeof(reason), "%s is not installed.", app);
        if (minv && *minv) snprintf(action, sizeof(action), "Install %s %s or newer, then try again.", app, minv);
    } else if (!strcmp(state, "unsupported_gpu")) {
        snprintf(reason, sizeof(reason), "%s needs %s; none was found on this machine.", app, gpu);
    } else if (!strcmp(state, "version_too_old")) {
        char found[512], minimum[512];
        snprintf(found, sizeof(found), "%s", (ver && *ver) ? " version" : "");
        char fval[512] = "";
        if (ver && *ver) snprintf(fval, sizeof(fval), " version %s", ver);
        snprintf(minimum, sizeof(minimum), "%s", (minv && *minv) ? "minimum" : "");
        char mval[512] = "";
        if (minv && *minv) snprintf(mval, sizeof(mval), "; version %s or newer is required", minv);
        snprintf(reason, sizeof(reason), "%s%s is too old%s.", app, fval, mval);
        (void)found; (void)minimum;
    } else if (!strcmp(state, "no_interactive_session")) {
        snprintf(reason, sizeof(reason), "%s needs an interactive desktop session.", app);
    } else if (!strcmp(state, "endpoint_unavailable")) {
        snprintf(reason, sizeof(reason), "%s's local endpoint is unavailable.", app);
    } else if (!strcmp(state, "hermes_not_connected")) {
        snprintf(reason, sizeof(reason), "%s's MCP connection is missing.", app);
    } else {
        snprintf(reason, sizeof(reason), "%s is not running.", app);
    }
    snprintf(buf, sizeof(buf), "%s %s", reason, action);
    return json_string(buf);
}
