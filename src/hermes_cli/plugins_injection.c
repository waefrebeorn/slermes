/*
 * plugins_injection.c — real C11 port of upstream Hermes
 * hermes_cli/plugins_injection.py.
 *
 * PoP: w16_pluginsinj_e56fea_inject @ hermes_cli/plugins_injection.py:inject_plugin_message
 *
 * Faithful replay of inject_plugin_message's routing decision.  The Python
 * function mutates collaborator objects; the C port takes the observable state
 * of those collaborators explicitly and emits the same verdict, the route it
 * took and the message it built.
 */
#include "noc_w16_pluginsinj_e56fea.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* dupstr: POSIX has dup(2); never shadow it. */
static char *dupstr(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int nonempty(const char *s) { return s && s[0] != '\0'; }

static json_t *make_result(int ok, const char *route, const char *msg, json_t *target) {
    json_t *obj = json_object();
    json_set(obj, "ok", json_bool(ok ? 1 : 0));
    json_set(obj, "route", json_string(route ? route : ""));
    json_set(obj, "msg", json_string(msg ? msg : ""));
    json_set(obj, "target", target ? target : json_null());
    return obj;
}

json_t *w16_pluginsinj_e56fea_inject(
    const char *content,
    const char *role,
    int has_cli,
    int agent_running,
    const char *session_key,
    json_t *origin,
    int has_gw_injector,
    int has_tui_injector,
    int injection_allowed,
    int tui_result,
    int tui_raises,
    int gw_result,
    int gw_raises,
    const char *calling_session_key,
    const char *plugin_home) {
    const char *c = content ? content : "";
    const char *r = role ? role : "";
    char *msg;
    size_t n;
    int keyless;
    json_t *target;
    json_t *out;

    /* msg = content if role == "user" else f"[{role}] {content}" */
    if (!strcmp(r, "user")) {
        msg = dupstr(c);
    } else {
        n = strlen(r) + strlen(c) + 4; /* '[' + role + ']' + ' ' + NUL */
        msg = (char *)malloc(n);
        if (!msg) return make_result(0, "oom", "", NULL);
        snprintf(msg, n, "[%s] %s", r, c);
    }

    /* An origin names a messaging chat; the local REPL is never that chat. */
    if (has_cli && origin == NULL) {
        out = make_result(1, agent_running ? "cli_interrupt" : "cli_pending", msg, NULL);
        free(msg);
        return out;
    }

    /* session_key and origin are mutually exclusive. */
    if (nonempty(session_key) && origin != NULL) {
        out = make_result(0, "refused_both", msg, NULL);
        free(msg);
        return out;
    }

    keyless = !nonempty(session_key) && origin == NULL;
    if (keyless && !has_gw_injector) {
        out = make_result(0, "refused_keyless_no_gw", msg, NULL);
        free(msg);
        return out;
    }

    /* Mapping check: a JSON object is the only Mapping shape. */
    if (origin != NULL && origin->type != JSON_OBJECT) {
        out = make_result(0, "refused_invalid_origin", msg, NULL);
        free(msg);
        return out;
    }

    if (!injection_allowed) {
        out = make_result(0, "refused_denied", msg, NULL);
        free(msg);
        return out;
    }

    /* TUI/desktop slot owns only its own session_key; an exception fails closed. */
    if (nonempty(session_key) && has_tui_injector) {
        if (tui_raises) {
            out = make_result(0, "refused_tui_exception", msg, NULL);
            free(msg);
            return out;
        }
        if (tui_result) {
            out = make_result(1, "tui", msg, NULL);
            free(msg);
            return out;
        }
        /* falsy TUI result falls through to the gateway */
    }

    if (!has_gw_injector) {
        out = make_result(0, "refused_no_gw", msg, NULL);
        free(msg);
        return out;
    }
    if (gw_raises) {
        out = make_result(0, "refused_gw_exception", msg, NULL);
        free(msg);
        return out;
    }

    target = json_object();
    if (keyless) {
        json_set(target, "keyless", json_bool(1));
        json_set(target, "calling_session_key",
                 json_string(calling_session_key ? calling_session_key : ""));
        json_set(target, "plugin_home", json_string(plugin_home ? plugin_home : ""));
        out = make_result(gw_result ? 1 : 0, "gw_keyless", msg, target);
    } else if (origin == NULL) {
        json_set(target, "session_key", json_string(session_key));
        json_set(target, "plugin_home", json_string(plugin_home ? plugin_home : ""));
        out = make_result(gw_result ? 1 : 0, "gw_keyed", msg, target);
    } else {
        json_set(target, "origin", json_copy(origin));
        json_set(target, "plugin_home", json_string(plugin_home ? plugin_home : ""));
        out = make_result(gw_result ? 1 : 0, "gw_origin", msg, target);
    }
    free(msg);
    return out;
}
