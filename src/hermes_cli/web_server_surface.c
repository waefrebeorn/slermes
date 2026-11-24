/*
 * web_server_surface.c — C home for upstream Hermes hermes_cli/web_server_surface.py.
 *
 * PoP: w17_webserver_7f8b25_surface @ hermes_cli/web_server_surface.py:surface
 * PoP: w17_webserver_7f8b25_policy @ hermes_cli/web_server_surface.py:policy
 * PoP: w17_webserver_7f8b25_private_launch @ hermes_cli/web_server_surface.py:private_launch
 *
 * Faithful port of the frozen SurfacePolicy table and the three pure state
 * predicates. The webapp row's gateway_connections=True (all other rows False).
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w17_webserver_7f8b25.h"

/* dupstr — never name a helper `dup` (critical C bug class). */
static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

typedef struct {
    const char *name;
    bool serves_spa;
    bool html_token;
    bool host_terminal;
    bool launch_link;
    bool gateway_ticket;
    bool own_origin_cookie_writes;
    bool web_access;
    bool browser_files;
    bool desktop_plugins;
    bool ui_preferences;
    bool client_direct_voice;
    bool gateway_connections;
} surface_policy_t;

/* POLICIES — verbatim from hermes_cli/web_server_surface.py. */
static const surface_policy_t POLICIES[] = {
    /* serve */
    { "serve", false, true, false, false, false, false, false, false,
      false, false, true, false },
    /* dashboard */
    { "dashboard", true, true, false, false, false, false, false, false,
      false, false, true, false },
    /* gateway */
    { "gateway", true, true, false, false, false, false, true, false,
      false, false, true, false },
    /* webapp */
    { "webapp", true, false, true, true, true, true, true, true,
      true, true, false, true },
};
static const size_t POLICIES_N = sizeof(POLICIES) / sizeof(POLICIES[0]);

/* surface(): `if recorded: return recorded` — empty string is falsy, so it
 * falls through to the gateway_runner probe just like Python truthiness. */
char *w17_webserver_7f8b25_surface(const char *ui_surface, bool has_gateway_runner) {
    if (ui_surface && ui_surface[0] != '\0') return dupstr(ui_surface);
    return dupstr(has_gateway_runner ? "gateway" : "dashboard");
}

static const surface_policy_t *find_policy(const char *ui_surface, bool has_gateway_runner) {
    char *name = w17_webserver_7f8b25_surface(ui_surface, has_gateway_runner);
    if (!name) return NULL;
    const surface_policy_t *found = NULL;
    for (size_t i = 0; i < POLICIES_N; i++) {
        if (!strcmp(POLICIES[i].name, name)) { found = &POLICIES[i]; break; }
    }
    free(name);
    return found;
}

json_t *w17_webserver_7f8b25_policy(const char *ui_surface, bool has_gateway_runner) {
    const surface_policy_t *p = find_policy(ui_surface, has_gateway_runner);
    json_t *out = json_object();
    if (!p) return out;  /* unknown surface: Python POLICIES[surface] would KeyError */
    json_set(out, "serves_spa", json_bool(p->serves_spa));
    json_set(out, "html_token", json_bool(p->html_token));
    json_set(out, "host_terminal", json_bool(p->host_terminal));
    json_set(out, "launch_link", json_bool(p->launch_link));
    json_set(out, "gateway_ticket", json_bool(p->gateway_ticket));
    json_set(out, "own_origin_cookie_writes", json_bool(p->own_origin_cookie_writes));
    json_set(out, "web_access", json_bool(p->web_access));
    json_set(out, "browser_files", json_bool(p->browser_files));
    json_set(out, "desktop_plugins", json_bool(p->desktop_plugins));
    json_set(out, "ui_preferences", json_bool(p->ui_preferences));
    json_set(out, "client_direct_voice", json_bool(p->client_direct_voice));
    json_set(out, "gateway_connections", json_bool(p->gateway_connections));
    return out;
}

bool w17_webserver_7f8b25_private_launch(const char *ui_surface, bool has_gateway_runner,
                                         bool auth_required) {
    const surface_policy_t *p = find_policy(ui_surface, has_gateway_runner);
    return p ? (p->launch_link && !auth_required) : false;
}
