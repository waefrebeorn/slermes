/*
 * gateway_cli_commands.c — C11 port of upstream Hermes
 * hermes_cli/gateway_cli_commands.py (gate w25_gatewaycli_8b715b).
 *
 * PoP: w25_gatewaycli_8b715b_diff @ hermes_cli/gateway_cli_commands.py:_diff
 * PoP: w25_gatewaycli_8b715b_handoff @ hermes_cli/gateway_cli_commands.py:_handoff
 * PoP: w25_gatewaycli_8b715b_run @ hermes_cli/gateway_cli_commands.py:run
 * PoP: w25_gatewaycli_8b715b_constants @ hermes_cli/gateway_cli_commands.py:_ROUTES
 *
 * Rewired, the session's tools, checkpoints, environment and handoff watcher run
 * in the gateway, so /diff session, /reload and /handoff are answered there.  The
 * connection, the rollback.session_diff payload, the two-phase handoff wait and
 * the terminal printers are seams; what is ported is the decision they feed: the
 * argument read, the branch taken, and whether the local handler still runs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_gatewaycli_8b715b.h"

/* Python truthiness of a JSON value. */
static int sc_truthy(const json_t *v) {
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

static int sc_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char sc_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/* Everything after the first space of *cmd*, with Python's .strip(). */
static char *sc_after_first_space(const char *cmd) {
    const char *p = cmd, *end;
    char *out;
    size_t n;
    while (*p && *p != ' ') p++;
    if (*p == ' ') p++;
    while (*p && sc_space(*p)) p++;
    end = p + strlen(p);
    while (end > p && sc_space(end[-1])) end--;
    n = (size_t)(end - p);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

#define SC_MAX_ARGS 64

/* Python .split() over the already-stripped text: runs of whitespace separate. */
static size_t sc_split(char *text, char **tokens, size_t max) {
    size_t count = 0;
    char *p = text;
    while (*p && count < max) {
        while (*p && sc_space(*p)) p++;
        if (!*p) break;
        tokens[count++] = p;
        while (*p && !sc_space(*p)) p++;
        if (*p) *p++ = '\0';
    }
    return count;
}

static int sc_has(char **tokens, size_t count, const char *word) {
    size_t i;
    for (i = 0; i < count; i++)
        if (strcmp(tokens[i], word) == 0) return 1;
    return 0;
}

json_t *w25_gatewaycli_8b715b_diff(const json_t *req) {
    char *text = sc_after_first_space(json_get_str(req, "cmd", ""));
    char *tokens[SC_MAX_ARGS];
    size_t i, count;
    json_t *out = json_object();
    int stat_only = 0;

    count = text ? sc_split(text, tokens, SC_MAX_ARGS) : 0;
    for (i = 0; i < count; i++) {
        char *p;
        for (p = tokens[i]; *p; p++) *p = sc_lower(*p);
    }
    if (!sc_has(tokens, count, "session")) {
        json_set(out, "handled", json_bool(0));
        json_set(out, "branch", json_string("none"));
        json_set(out, "stat_only", json_null());
        free(text);
        return out;
    }
    stat_only = sc_has(tokens, count, "--stat") || sc_has(tokens, count, "stat");
    json_set(out, "handled", json_bool(1));
    if (!sc_truthy(json_obj_get(json_obj_get(req, "result"), "enabled"))) {
        json_set(out, "branch", json_string("not_enabled"));
        json_set(out, "stat_only", json_null());
    } else {
        json_set(out, "branch", json_string("session_diff"));
        json_set(out, "stat_only", json_bool(stat_only));
    }
    free(text);
    return out;
}

/* hermes_cli.cli_commands_session_tools._command_arg(cmd, lower=True) */
static char *sc_command_arg(const char *cmd) {
    const char *p = cmd, *start, *end;
    char *out;
    size_t n, i;
    while (*p && sc_space(*p)) p++;
    while (*p && !sc_space(*p)) p++;
    while (*p && sc_space(*p)) p++;
    start = p;
    end = start + strlen(start);
    while (end > start && sc_space(end[-1])) end--;
    n = (size_t)(end - start);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) out[i] = sc_lower(start[i]);
    out[n] = '\0';
    return out;
}

json_t *w25_gatewaycli_8b715b_handoff(const json_t *req) {
    char *platform = sc_command_arg(json_get_str(req, "cmd", ""));
    json_t *out = json_object();

    json_set(out, "platform", json_string(platform ? platform : ""));
    if (!platform || platform[0] == '\0') {
        /* usage: three printed lines, nothing sent to the gateway */
        json_set(out, "result", json_string("USAGE"));
        json_set(out, "lines", json_number(3));
    } else if (sc_truthy(json_obj_get(req, "agent_running"))) {
        json_set(out, "result", json_string("BUSY"));
        json_set(out, "lines", json_number(1));
    } else if (sc_truthy(json_obj_get(req, "kept"))) {
        json_set(out, "result", json_string("KEPT"));
        json_set(out, "lines", json_number(2));
    } else {
        json_set(out, "result", json_string("EXIT"));
        json_set(out, "lines", json_number(2));
    }
    free(platform);
    return out;
}

json_t *w25_gatewaycli_8b715b_run(const json_t *req) {
    const char *canonical = json_get_str(req, "canonical", "");
    const char *route = json_get_str(req, "route", "");
    json_t *out = json_object();
    int known = !strcmp(canonical, "diff") || !strcmp(canonical, "reload")
                || !strcmp(canonical, "handoff");

    if (!known || !sc_truthy(json_obj_get(req, "conn"))) {
        json_set(out, "result", json_string("FALSE"));
        json_set(out, "printed", json_null());
        return out;
    }
    if (!strcmp(route, "exit")) {
        json_set(out, "result", json_string("EXIT_SESSION"));
        json_set(out, "printed", json_null());
    } else if (!strcmp(route, "raise")) {
        /* except GatewayClientError: f"  /{canonical}: {detail or exc}" */
        const json_t *detail = json_obj_get(req, "detail");
        const json_t *error = json_obj_get(req, "error");
        char buf[1024];
        const char *why = sc_truthy(detail) ? json_get_str(req, "detail", "")
                        : (error && error->type == JSON_STRING ? error->str_val : "");
        snprintf(buf, sizeof(buf), "  /%s: %s", canonical, why ? why : "");
        json_set(out, "result", json_string("TRUE"));
        json_set(out, "printed", json_string(buf));
    } else if (!strcmp(route, "false")) {
        json_set(out, "result", json_string("FALSE"));
        json_set(out, "printed", json_null());
    } else {
        json_set(out, "result", json_string("TRUE"));
        json_set(out, "printed", json_null());
    }
    return out;
}

json_t *w25_gatewaycli_8b715b_constants(void) {
    json_t *out = json_object(), *routes = json_array(), *refused = json_array();
    json_append(routes, json_string("diff"));
    json_append(routes, json_string("reload"));
    json_append(routes, json_string("handoff"));
    json_set(out, "routes", routes);
    json_set(out, "refused", refused);
    return out;
}
