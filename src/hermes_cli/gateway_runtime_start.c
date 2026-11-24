/*
 * gateway_runtime_start.c - C11 port of upstream
 * hermes_cli/gateway_runtime_start.py (gate w26_gatewayrun_558360).
 *
 * PoP: w26_gatewayrun_558360_strip_markers @ hermes_cli/gateway_runtime_start.py:strip_supervisor_markers
 * PoP: w26_gatewayrun_558360_os_refusal @ hermes_cli/gateway_runtime_start.py:os_refusal
 * PoP: w26_gatewayrun_558360_stdio_log_path @ hermes_cli/gateway_runtime_start.py:stdio_log_path
 * PoP: w26_gatewayrun_558360_constants @ hermes_cli/gateway_runtime_start.py:<module constants>
 *
 * spawn_unmanaged_gateway's seams -- os.environ, sys.platform, the deadline,
 * the Popen/launchd hand-off and the elevation probe -- are not ported: the
 * env is handed in as values and only the decisions are made here.  carry_
 * gateway_profile_pin (tools/environments/local_env_policy.py) is inlined
 * because the strip depends on it: the markers go, the pin they imply stays.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_gatewayrun_558360.h"

/* SUPERVISOR_MARKER_ENV */
static const char *const GW_MARKERS[W26_GATEWAYRUN_558360_MARKERS] = {
    "INVOCATION_ID", "JOURNAL_STREAM",
    "XPC_SERVICE_NAME", "LAUNCHD_SOCKET", "HERMES_LAUNCHD_LABEL",
    "HERMES_GATEWAY_EXTERNAL_SUPERVISOR", "HERMES_SUPERVISED_CHILD",
    "NOTIFY_SOCKET", "WATCHDOG_USEC", "WATCHDOG_PID",
};

/* DAEMON_STRIPPED_ENV: the daemon-lifetime policy pins and markers */
static const char *const GW_STRIPPED[W26_GATEWAYRUN_558360_STRIPPED] = {
    "HERMES_PROFILE", "HERMES_YOLO_MODE", "HERMES_IGNORE_RULES", "HERMES_SAFE_MODE",
    "HERMES_IGNORE_USER_CONFIG", "HERMES_ACCEPT_HOOKS", "HERMES_SESSION_SOURCE", "TERMINAL_CWD",
    "HERMES_KANBAN_TASK", "HERMES_KANBAN_WORKSPACE", "HERMES_KANBAN_BRANCH", "HERMES_KANBAN_RUN_ID",
    "HERMES_KANBAN_CLAIM_LOCK", "HERMES_KANBAN_CLAIM_TTL_SECONDS", "HERMES_KANBAN_GOAL_MODE",
    "HERMES_KANBAN_GOAL_MAX_TURNS", "HERMES_KANBAN_DB", "HERMES_KANBAN_WORKSPACES_ROOT",
    "HERMES_KANBAN_BOARD", "HERMES_TENANT", "HERMES_SESSION_SOURCE_EXPLICIT", "HERMES_TURN_AUTHOR",
    "HERMES_INTERACTIVE", "HERMES_GATEWAY_SESSION", "HERMES_SINGLE_QUERY_SESSION",
    "HERMES_EPHEMERAL_SYSTEM_PROMPT", "HERMES_PREFILL_MESSAGES_FILE", "HERMES_INFERENCE_PROVIDER",
    "HERMES_ENABLE_PROJECT_PLUGINS", "HERMES_TUI_CLIENT_ENV",
    /* *SUPERVISOR_MARKER_ENV */
    "INVOCATION_ID", "JOURNAL_STREAM",
    "XPC_SERVICE_NAME", "LAUNCHD_SOCKET", "HERMES_LAUNCHD_LABEL",
    "HERMES_GATEWAY_EXTERNAL_SUPERVISOR", "HERMES_SUPERVISED_CHILD",
    "NOTIFY_SOCKET", "WATCHDOG_USEC", "WATCHDOG_PID",
};

/* GATEWAY_PROFILE_PIN_ENV */
#define GW_PROFILE_PIN "HERMES_GATEWAY_PROFILE_PIN"

static const char *const GW_BANNER_PREFIXES[] = {
    "\xe2\x94\x8c", "\xe2\x94\x82", "\xe2\x94\x9c", "\xe2\x94\x94",  /* ┌ │ ├ └ */
};

static const char GW_ELEVATED_REMEDY[] =
    "Hermes is running as administrator, and a gateway it started would run as administrator too, "
    "which locks every normal Hermes window out of it. Start Hermes from a normal (non-administrator) window";

/* Python truthiness of a JSON value. */
static int gw_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str(value or "").strip().lower(), for the shapes an env entry can hold. */
static void gw_lower_text(const json_t *v, char *buf, size_t cap) {
    const char *text = "";
    char raw[128];
    size_t i = 0, n;
    if (v) {
        if (v->type == JSON_STRING) text = v->str_val ? v->str_val : "";
        else if (v->type == JSON_BOOL) text = v->bool_val ? "True" : "False";
        else if (v->type == JSON_NUMBER) {
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(raw, sizeof(raw), "%lld", (long long)v->num_val);
            else
                snprintf(raw, sizeof(raw), "%.17g", v->num_val);
            text = raw;
        }
    }
    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') text++;
    n = strlen(text);
    while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t' || text[n - 1] == '\n' || text[n - 1] == '\r')) n--;
    for (; i < n && i + 1 < cap; i++) buf[i] = (char)tolower((unsigned char)text[i]);
    buf[i] = '\0';
}

static int gw_is_marker(const char *key) {
    int i;
    for (i = 0; i < W26_GATEWAYRUN_558360_MARKERS; i++)
        if (!strcmp(key, GW_MARKERS[i])) return 1;
    return 0;
}

json_t *w26_gatewayrun_558360_strip_markers(const json_t *env) {
    json_t *out = json_object();
    char lower[128];
    int pin = 0;
    size_t i;

    if (!env || env->type != JSON_OBJECT) return out;

    /* carry_gateway_profile_pin: a pinning supervisor marker implies the pin. */
    if (gw_truthy(json_obj_get(env, "HERMES_SUPERVISED_CHILD"))) pin = 1;
    gw_lower_text(json_obj_get(env, "HERMES_GATEWAY_EXTERNAL_SUPERVISOR"), lower, sizeof(lower));
    if (!strcmp(lower, "1") || !strcmp(lower, "true") || !strcmp(lower, "yes") || !strcmp(lower, "on"))
        pin = 1;
    /* A `gateway run` under INVOCATION_ID skips the sticky active_profile. */
    if (gw_truthy(json_obj_get(env, "INVOCATION_ID"))) pin = 1;

    for (i = 0; i < json_object_size(env); i++) {
        const char *key = json_object_get_key_at(env, i);
        const json_t *v = json_object_get_at(env, i);
        if (!key || gw_is_marker(key)) continue;
        json_set(out, key, json_copy(v));
    }
    if (pin) json_set(out, GW_PROFILE_PIN, json_string("1"));
    return out;
}

json_t *w26_gatewayrun_558360_os_refusal(const json_t *exc) {
    const json_t *winerror = exc ? json_obj_get(exc, "winerror") : NULL;
    const json_t *errno_v = exc ? json_obj_get(exc, "errno") : NULL;
    const json_t *strerror = exc ? json_obj_get(exc, "strerror") : NULL;
    char code[64];
    char buf[512];
    size_t used = 0;

    code[0] = '\0';
    if (winerror && winerror->type == JSON_NUMBER)
        snprintf(code, sizeof(code), "WinError %lld", (long long)winerror->num_val);
    else if (errno_v && errno_v->type == JSON_NUMBER)
        snprintf(code, sizeof(code), "errno %lld", (long long)errno_v->num_val);

    buf[0] = '\0';
    if (code[0]) {
        snprintf(buf, sizeof(buf), "%s", code);
        used = strlen(buf);
    }
    if (strerror && strerror->type == JSON_STRING && strerror->str_val && strerror->str_val[0]) {
        if (used && used + 2 < sizeof(buf)) { buf[used++] = ':'; buf[used++] = ' '; buf[used] = '\0'; }
        if (used < sizeof(buf)) snprintf(buf + used, sizeof(buf) - used, "%s", strerror->str_val);
        used = strlen(buf);
    }
    if (!used) return json_null();
    return json_string(buf);
}

json_t *w26_gatewayrun_558360_stdio_log_path(const json_t *req) {
    const char *home = req ? json_get_str(req, "home", "") : "";
    char sep = '/';
    char *buf;
    size_t cap, used = 0;
    const char *p;

#ifdef _WIN32
    sep = '\\';
#endif

    cap = strlen(home) + 64;
    buf = (char *)malloc(cap);
    if (!buf) return json_string("");
    for (p = home; *p; p++) {
        int is_sep = (*p == '/' || *p == '\\');
        if (is_sep) {
            /* collapse runs; a root-leading separator is kept once */
            if (used > 0 && buf[used - 1] == sep) continue;
            buf[used++] = sep;
            continue;
        }
        buf[used++] = *p;
    }
    while (used > 0 && buf[used - 1] == sep) used--;
    buf[used] = '\0';

    {
        const char *tail = "logs/gateway-stdio.log";
        char tail_buf[64];
        size_t i;
        for (i = 0; tail[i] && i + 1 < sizeof(tail_buf); i++)
            tail_buf[i] = (tail[i] == '/') ? sep : tail[i];
        tail_buf[i] = '\0';
        if (used && used + 1 < cap) buf[used++] = sep;
        if (used < cap) snprintf(buf + used, cap - used, "%s", tail_buf);
    }

    {
        json_t *out = json_string(buf);
        free(buf);
        return out;
    }
}

json_t *w26_gatewayrun_558360_constants(void) {
    json_t *o = json_object();
    json_t *markers = json_array();
    json_t *stripped = json_array();
    json_t *banners = json_array();
    size_t i;

    for (i = 0; i < W26_GATEWAYRUN_558360_MARKERS; i++)
        json_append(markers, json_string(GW_MARKERS[i]));
    for (i = 0; i < W26_GATEWAYRUN_558360_STRIPPED; i++)
        json_append(stripped, json_string(GW_STRIPPED[i]));
    for (i = 0; i < sizeof(GW_BANNER_PREFIXES) / sizeof(GW_BANNER_PREFIXES[0]); i++)
        json_append(banners, json_string(GW_BANNER_PREFIXES[i]));

    json_set(o, "supervisor_marker_env", markers);
    json_set(o, "daemon_stripped_env", stripped);
    json_set(o, "client_elevated_remedy", json_string(GW_ELEVATED_REMEDY));
    json_set(o, "banner_prefixes", banners);
    json_set(o, "tail_lines", json_number(15.0));
    json_set(o, "tail_bytes", json_number(65536.0));
    return o;
}
