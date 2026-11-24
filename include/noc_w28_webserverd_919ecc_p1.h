/*
 * noc_w28_webserverd_919ecc_p1.h - C surface for the THEMES + SPA-ASSET
 * partition of upstream hermes_cli/web_server_dashboard.py
 * (gate w28_webserverd_919ecc_p1).
 *
 * What the served bytes are BEFORE any request is routed: the theme wire
 * format a user's dashboard-themes YAML files normalise to, the critical-CSS
 * bootstrap shim for a non-builtin active theme, and the SPA fallback ladder
 * (headless token page, prefix rewrites, index-vs-file-vs-static-404).
 *
 * Every seam (the config read, the theme directory, WEB_DIST and its files,
 * the request headers, the session token) arrives as a fixture value.
 */
#ifndef NOC_W28_WEBSERVERD_919ECC_P1_H
#define NOC_W28_WEBSERVERD_919ECC_P1_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _HEADLESS_MSG — the 404 body/title when the headless backend refuses the SPA. */
#define W28_WEBSERVERD_919ECC_P1_HEADLESS_MSG \
    "Headless backend (hermes serve): web UI disabled \xE2\x80\x94 use " \
    "`hermes dashboard` for the browser UI."

/* _THEME_CUSTOM_CSS_MAX */
#define W28_WEBSERVERD_919ECC_P1_CUSTOM_CSS_MAX (32 * 1024)

/* {value, default_hex, default_alpha} ->
 * {"hex": str, "alpha": float in [0,1]} or null for garbage
 * (bare-hex shorthand, {hex, alpha} dict, null -> defaults). */
json_t *w28_webserverd_919ecc_p1_parse_theme_layer(const json_t *req);

/* {palette, key, default} -> palette[key]["hex"], else the default.
 * A non-dict layer falls back; a non-string hex is passed through. */
json_t *w28_webserverd_919ecc_p1_layer_hex(const json_t *req);

/* A user theme YAML (already parsed) -> the wire format ThemeProvider reads:
 * {name, label, description, palette{background,midground,foreground,warmGlow,
 * noiseOpacity}, typography, layout, layoutVariant [, colorOverrides, assets,
 * customCSS, componentStyles]}. null when the theme is unusable. */
json_t *w28_webserverd_919ecc_p1_normalise_theme(const json_t *data);

/* {active, builtins:[name], themes:[normalised theme]} -> the
 * <style id="hermes-theme-bootstrap"> shim for `active`, or "" (builtin,
 * unset, or not found). */
json_t *w28_webserverd_919ecc_p1_bootstrap_css(const json_t *req);

/* {ui_surface} -> the "not built" sentence: Webapp vs dashboard build. */
json_t *w28_webserverd_919ecc_p1_missing_message(const json_t *req);

/* {accept} -> "text/html" in Accept (the browser-vs-API branch). */
json_t *w28_webserverd_919ecc_p1_wants_html(const json_t *req);

/* {value} -> HERMES_SERVE_HEADLESS == "1" exactly. */
json_t *w28_webserverd_919ecc_p1_headless(const json_t *req);

/* {full_path, gated, withheld} -> {serve, status, error}: the headless root
 * token page is served only at "" and only when the token is not withheld. */
json_t *w28_webserverd_919ecc_p1_root_token_page(const json_t *req);

/* {html, prefix} -> index.html with absolute asset URLs rewritten under the
 * X-Forwarded-Prefix (no prefix: unchanged). */
json_t *w28_webserverd_919ecc_p1_prefix_rewrite(const json_t *req);

/* {css, prefix} -> a built CSS sheet with url(/fonts/...) etc. rewritten
 * under the prefix (all four asset dirs, bare/quoted). */
json_t *w28_webserverd_919ecc_p1_css_url_rewrite(const json_t *req);

/* {full_path, html_token, file_is_file, index_exists, samefile, under_dist,
 *  exists, is_static, font_found} ->
 * {kind: api_404|index|file|font|static_404|index_fallback, detail}. */
json_t *w28_webserverd_919ecc_p1_spa_route(const json_t *req);

/* The theme/asset constants of the module, as one object. */
json_t *w28_webserverd_919ecc_p1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
