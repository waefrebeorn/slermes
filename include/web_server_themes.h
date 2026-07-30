/*
 * web_server_themes.h — dashboard theme discovery + normalisation
 * (faithful C11 port of the theme cluster in hermes_cli/web_server.py).
 */
#ifndef WEB_SERVER_THEMES_H
#define WEB_SERVER_THEMES_H

#include <stdbool.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Python _parse_theme_layer: normalise a layer spec into {hex, alpha}.
 * Returns new json object or NULL on garbage input (caller falls back). */
json_t *ws_theme_parse_layer(const json_t *value, const char *default_hex,
                             double default_alpha);

/* Python _normalise_theme_definition: full theme YAML → wire format.
 * Returns new json object or NULL when unusable. */
json_t *ws_theme_normalise_definition(const json_t *data);

/* Python _BUILTIN_DASHBOARD_THEMES: new json array of the 8 builtins. */
json_t *ws_theme_builtin_list(void);

/* Python _discover_user_themes: scan <home>/dashboard-themes/*.yaml
 * (sorted), parse + normalise. `home` NULL → slermes_home(). Always
 * returns a (possibly empty) json array. */
json_t *ws_theme_discover_user_themes(const char *home);

/* Python get_dashboard_themes route body: merge builtins + user themes
 * (dedupe by name, builtins win), active from config. `active` may be
 * NULL → "default". Returns {"themes":[...],"active":...}. */
json_t *ws_theme_dashboard_themes_response(const char *home,
                                           const char *active);

/* Python _render_active_theme_bootstrap_css: critical-CSS shim for the
 * active user theme. Built-in / missing / invalid → "". Malloc'd. */
char *ws_theme_render_bootstrap_css(const char *home, const char *active);

/* Python _FONT_CHOICES allowlist (+ the "theme" default id). */
bool ws_theme_font_choice_allowed(const char *font_id);

#ifdef __cplusplus
}
#endif

#endif /* WEB_SERVER_THEMES_H */
