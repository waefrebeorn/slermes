/*
 * web_server_dashboard.c - C11 port of upstream hermes_cli/web_server_dashboard.py
 * (gates w28_webserverd_919ecc_p1 / w28_webserverd_919ecc_p2).
 *
 * PoP: w28_webserverd_919ecc_p1_parse_theme_layer @ hermes_cli/web_server_dashboard.py:_parse_theme_layer
 * PoP: w28_webserverd_919ecc_p1_layer_hex @ hermes_cli/web_server_dashboard.py:_layer_hex
 * PoP: w28_webserverd_919ecc_p1_normalise_theme @ hermes_cli/web_server_dashboard.py:_normalise_theme_definition
 * PoP: w28_webserverd_919ecc_p1_bootstrap_css @ hermes_cli/web_server_dashboard.py:_render_active_theme_bootstrap_css
 * PoP: w28_webserverd_919ecc_p1_missing_message @ hermes_cli/web_server_dashboard.py:mount_spa._serve_index
 * PoP: w28_webserverd_919ecc_p1_wants_html @ hermes_cli/web_server_dashboard.py:_bundle_unavailable
 * PoP: w28_webserverd_919ecc_p1_headless @ hermes_cli/web_server_dashboard.py:mount_spa
 * PoP: w28_webserverd_919ecc_p1_root_token_page @ hermes_cli/web_server_dashboard.py:mount_spa.no_frontend
 * PoP: w28_webserverd_919ecc_p1_prefix_rewrite @ hermes_cli/web_server_dashboard.py:mount_spa._serve_index
 * PoP: w28_webserverd_919ecc_p1_css_url_rewrite @ hermes_cli/web_server_dashboard.py:mount_spa.serve_css
 * PoP: w28_webserverd_919ecc_p1_spa_route @ hermes_cli/web_server_dashboard.py:mount_spa.serve_spa
 * PoP: w28_webserverd_919ecc_p1_constants @ hermes_cli/web_server_dashboard.py:<theme/asset constants>
 * PoP: w28_webserverd_919ecc_p2_strip_manifest @ hermes_cli/web_server_dashboard.py:_strip_dashboard_manifest
 * PoP: w28_webserverd_919ecc_p2_runtime_status @ hermes_cli/web_server_dashboard.py:_plugin_runtime_status
 * PoP: w28_webserverd_919ecc_p2_mount_skip_reason @ hermes_cli/web_server_dashboard.py:_plugin_api_mount_skip_reason
 * PoP: w28_webserverd_919ecc_p2_plugin_entry @ hermes_cli/web_server_dashboard.py:_dashboard_plugin_entry
 * PoP: w28_webserverd_919ecc_p2_safe_api_relpath @ hermes_cli/web_server_dashboard.py:_safe_plugin_api_relpath
 * PoP: w28_webserverd_919ecc_p2_api_name @ hermes_cli/web_server_dashboard.py:_plugin_api_name
 * PoP: w28_webserverd_919ecc_p2_api_mounted @ hermes_cli/web_server_dashboard.py:_plugin_api_mounted
 * PoP: w28_webserverd_919ecc_p2_hot_name_ok @ hermes_cli/web_server_dashboard.py:_try_hot_mount_plugin_api
 * PoP: w28_webserverd_919ecc_p2_login_command @ hermes_cli/web_server_dashboard.py:_plugin_login_command
 * PoP: w28_webserverd_919ecc_p2_auth_hint @ hermes_cli/web_server_dashboard.py:_plugin_auth_hint
 * PoP: w28_webserverd_919ecc_p2_mount_decision @ hermes_cli/web_server_dashboard.py:_mount_one_plugin_api
 * PoP: w28_webserverd_919ecc_p2_env_truthy @ hermes_cli/web_server_dashboard.py:_dashboard_plugin_search_dirs
 * PoP: w28_webserverd_919ecc_p2_enable_sets @ hermes_cli/web_server_dashboard.py:_plugin_enable_sets_here
 * PoP: w28_webserverd_919ecc_p2_orphan_dashboard @ hermes_cli/web_server_dashboard.py:_merged_plugins_hub
 * PoP: w28_webserverd_919ecc_p2_cache_hit @ hermes_cli/web_server_dashboard.py:_merged_plugins_hub
 * PoP: w28_webserverd_919ecc_p2_splice_index @ hermes_cli/web_server_dashboard.py:_try_hot_mount_plugin_api
 * PoP: w28_webserverd_919ecc_p2_constants @ hermes_cli/web_server_dashboard.py:<plugin-hub constants>
 *
 * SEAMS (answers passed in as fixtures, never executed here):
 *   the FastAPI application object and its route table, request headers and
 *   scopes (accept / x-forwarded-prefix / route path), the filesystem
 *   (WEB_DIST, index.html, ffi samefile, font files), the config + theme YAML
 *   reader, the plugin registry's cached check_fn verdicts and the LEFT_CORE
 *   table, and the plugin-isolation import that actually executes plugin code.
 * What is ported is what those answers DECIDE: the theme wire format, the
 * critical-CSS bootstrap, the SPA fallback ladder, the prefix rewrites, the
 * manifest/plugin-entry shaping and every trust gate a plugin's backend has to
 * pass before it is imported.
 */
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w28_webserverd_919ecc_p1.h"
#include "noc_w28_webserverd_919ecc_p2.h"

/* ------------------------------------------------------------------ */
/* Shared helpers: Python value semantics over json_t.                 */
/* ------------------------------------------------------------------ */

static int dash_truthy(const json_t *v) {
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

static int dash_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int dash_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }

static json_t *dash_get(const json_t *o, const char *k) {
    return dash_is_obj(o) ? json_obj_get(o, k) : NULL;
}

static const char *dash_str(const json_t *v, const char *def) {
    return (v && v->type == JSON_STRING) ? v->str_val : def;
}

static char *dash_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python str.strip() over the ASCII whitespace set (fixtures are ASCII here). */
static char *dash_strip(const char *s) {
    const char *b = s ? s : "";
    const char *e;
    while (*b && strchr(" \t\n\r\f\v", *b)) b++;
    e = b + strlen(b);
    while (e > b && strchr(" \t\n\r\f\v", e[-1])) e--;
    {
        size_t n = (size_t)(e - b);
        char *out = (char *)malloc(n + 1);
        if (!out) return NULL;
        memcpy(out, b, n);
        out[n] = '\0';
        return out;
    }
}

/* _nonempty_str: isinstance(value, str) and bool(value.strip()). */
static int dash_nonempty_str(const json_t *v) {
    char *s;
    int ok;
    if (!v || v->type != JSON_STRING) return 0;
    s = dash_strip(v->str_val);
    if (!s) return 0;
    ok = s[0] != '\0';
    free(s);
    return ok;
}

/* _css_ident: key.replace("-","").replace("_","").isalnum(). */
static int dash_css_ident(const char *key) {
    int alnum_seen = 0;
    const char *p;
    if (!key) return 0;
    for (p = key; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '-' || c == '_') continue;
        if (!isalnum(c)) return 0;
        alnum_seen = 1;
    }
    return alnum_seen;
}

/* Python float(value) for the shapes a theme/YAML value can take.
 * Returns 0 where CPython raises TypeError/ValueError. */
static int dash_float_of(const json_t *v, double *out) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NUMBER:
        *out = v->num_val;
        return 1;
    case JSON_BOOL:
        *out = v->bool_val ? 1.0 : 0.0;
        return 1;
    case JSON_STRING: {
        char *s = dash_strip(v->str_val);
        char *end = NULL;
        double d;
        int ok;
        if (!s) return 0;
        if (s[0] == '\0' || strchr(s, 'x') || strchr(s, 'X') || strchr(s, '_')) {
            free(s);   /* float() rejects hex literals and separators */
            return 0;
        }
        d = strtod(s, &end);
        ok = end && *end == '\0' && end != s;
        free(s);
        if (!ok) return 0;
        *out = d;
        return 1;
    }
    default:
        return 0;
    }
}

/* Python str(value) for the JSON scalars a theme can carry. */
static void dash_num_str(double v, char *buf, size_t cap) {
    if (v == (double)(long long)v && fabs(v) < 1e15) {
        snprintf(buf, cap, "%lld", (long long)v);
        return;
    }
    for (int prec = 1; prec <= 17; prec++) {
        char tmp[64];
        char *end = NULL;
        snprintf(tmp, sizeof(tmp), "%.*g", prec, v);
        if (strtod(tmp, &end) == v && end && *end == '\0') {
            snprintf(buf, cap, "%s", tmp);
            return;
        }
    }
    snprintf(buf, cap, "%.17g", v);
}

static char *dash_pystr(const json_t *v) {
    char buf[64];
    if (!v) return dash_dup("None");
    switch (v->type) {
    case JSON_STRING: return dash_dup(v->str_val);
    case JSON_BOOL:   return dash_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: dash_num_str(v->num_val, buf, sizeof(buf)); return dash_dup(buf);
    case JSON_NULL:   return dash_dup("None");
    case JSON_ARRAY:  return dash_dup("[...]");
    case JSON_OBJECT: return dash_dup("{...}");
    }
    return dash_dup("");
}

/* Small growable string buffer for the CSS/HTML rewrites. */
typedef struct {
    char  *p;
    size_t n;
    size_t cap;
} dash_sb;

static void dash_sb_init(dash_sb *b) { b->p = NULL; b->n = 0; b->cap = 0; }

static void dash_sb_reserve(dash_sb *b, size_t extra) {
    if (b->n + extra + 1 <= b->cap) return;
    {
        size_t cap = b->cap ? b->cap : 64;
        char *np;
        while (cap < b->n + extra + 1) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return;
        b->p = np;
        b->cap = cap;
    }
}

static void dash_sb_putn(dash_sb *b, const char *s, size_t n) {
    dash_sb_reserve(b, n);
    if (!b->p) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void dash_sb_put(dash_sb *b, const char *s) {
    dash_sb_putn(b, s, strlen(s));
}

static void dash_sb_putf(dash_sb *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    dash_sb_put(b, tmp);
}

static char *dash_sb_take(dash_sb *b) {
    if (!b->p) return dash_dup("");
    return b->p;
}

/* str.replace(needle, repl) — every occurrence (Python semantics). */
static char *dash_replace_all(const char *hay, const char *needle, const char *repl) {
    dash_sb b;
    const char *p = hay ? hay : "";
    size_t nl;
    dash_sb_init(&b);
    if (!needle || !needle[0]) return dash_dup(p);
    nl = strlen(needle);
    for (;;) {
        const char *hit = strstr(p, needle);
        if (!hit) break;
        dash_sb_putn(&b, p, (size_t)(hit - p));
        dash_sb_put(&b, repl ? repl : "");
        p = hit + nl;
    }
    dash_sb_put(&b, p);
    return dash_sb_take(&b);
}

/* ------------------------------------------------------------------ */
/* p1: theme layer normalisation                                       */
/* ------------------------------------------------------------------ */

static json_t *dash_layer(double default_alpha, const char *hex) {
    json_t *o = json_object();
    double a = default_alpha;
    if (a < 0.0) a = 0.0;
    if (a > 1.0) a = 1.0;
    json_set(o, "hex", json_string(hex ? hex : ""));
    json_set(o, "alpha", json_number(a));
    return o;
}

json_t *w28_webserverd_919ecc_p1_parse_theme_layer(const json_t *req) {
    json_t *value = dash_get(req, "value");
    const char *default_hex = dash_str(dash_get(req, "default_hex"), "#000000");
    json_t *da_node = dash_get(req, "default_alpha");
    double default_alpha = 1.0;
    if (da_node && da_node->type == JSON_NUMBER) default_alpha = da_node->num_val;

    if (!value || value->type == JSON_NULL) return dash_layer(default_alpha, default_hex);
    if (value->type == JSON_STRING) return dash_layer(default_alpha, value->str_val);
    if (value->type != JSON_OBJECT) return NULL;
    {
        json_t *hex_node = json_obj_get(value, "hex");
        json_t *alpha_node = json_obj_get(value, "alpha");
        const char *hex;
        double alpha = default_alpha;
        if (hex_node && hex_node->type != JSON_STRING) return NULL;
        hex = hex_node ? hex_node->str_val : default_hex;
        if (alpha_node) {
            double parsed;
            if (dash_float_of(alpha_node, &parsed)) alpha = parsed;
            else alpha = default_alpha;
        }
        return dash_layer(alpha, hex);
    }
}

json_t *w28_webserverd_919ecc_p1_layer_hex(const json_t *req) {
    json_t *palette = dash_get(req, "palette");
    const char *key = dash_str(dash_get(req, "key"), "");
    json_t *def = dash_get(req, "default");
    json_t *layer = dash_get(palette, key);
    if (!dash_truthy(layer)) layer = NULL;
    if (layer && layer->type == JSON_OBJECT) {
        json_t *hex = json_obj_get(layer, "hex");
        if (hex) return json_copy(hex);
    }
    if (def) return json_copy(def);
    return json_null();
}

/* _layer(key, default_hex, default_alpha) inside _normalise_theme_definition:
 * palette[key] wins over colors[key]; an unusable layer falls back whole. */
static json_t *dash_theme_layer(const json_t *palette_src, const json_t *colors_src,
                                const char *key, const char *default_hex, double default_alpha) {
    json_t *raw = dash_get(palette_src, key);
    double alpha = default_alpha;
    const char *hex = default_hex;
    if (!raw) raw = dash_get(colors_src, key);
    if (!raw || raw->type == JSON_NULL) {
        /* defaults */
    } else if (raw->type == JSON_STRING) {
        hex = raw->str_val;
    } else if (raw->type == JSON_OBJECT) {
        json_t *hex_node = json_obj_get(raw, "hex");
        json_t *alpha_node = json_obj_get(raw, "alpha");
        if (!hex_node || hex_node->type == JSON_STRING) {
            if (hex_node) hex = hex_node->str_val;
            if (alpha_node) {
                double parsed;
                if (dash_float_of(alpha_node, &parsed)) alpha = parsed;
            }
        }
        /* hex present but not a str -> the whole layer is None -> defaults */
    }
    /* any other type -> None -> defaults */
    return dash_layer(alpha, hex);
}

/* str(value).strip() is non-empty and value is (str, int, float) — bool is an
 * int in Python, so True/False stringify to "True"/"False" and pass. */
static int dash_style_scalar(const json_t *v) {
    char *s;
    char *t;
    int ok;
    if (!v) return 0;
    if (v->type != JSON_STRING && v->type != JSON_NUMBER && v->type != JSON_BOOL) return 0;
    s = dash_pystr(v);
    if (!s) return 0;
    t = dash_strip(s);
    ok = t && t[0] != '\0';
    free(t);
    free(s);
    return ok;
}

static int dash_in_set(const char *v, const char *const *set, size_t n) {
    size_t i;
    if (!v) return 0;
    for (i = 0; i < n; i++)
        if (!strcmp(v, set[i])) return 1;
    return 0;
}

static const char *const DASH_OVERRIDE_KEYS[] = {
    "card", "cardForeground", "popover", "popoverForeground",
    "primary", "primaryForeground", "secondary", "secondaryForeground",
    "muted", "mutedForeground", "accent", "accentForeground",
    "destructive", "destructiveForeground", "success", "warning",
    "border", "input", "ring",
};
static const char *const DASH_NAMED_ASSETS[] = {"bg", "hero", "logo", "crest", "sidebar", "header"};
static const char *const DASH_COMPONENT_BUCKETS[] = {
    "card", "header", "footer", "sidebar", "tab", "progress", "badge", "backdrop", "page",
};
static const char *const DASH_LAYOUT_VARIANTS[] = {"cockpit", "standard", "tiled"};

#define DASH_ARR_LEN(a) (sizeof(a) / sizeof((a)[0]))

json_t *w28_webserverd_919ecc_p1_normalise_theme(const json_t *data) {
    json_t *name_node, *palette_src, *colors_src, *out;
    json_t *palette, *typography, *layout, *assets_src, *assets_out = NULL;
    json_t *color_overrides = NULL, *component_styles = NULL, *custom_assets = NULL;
    double noise = 1.0;
    json_t *noise_raw;
    const char *layout_variant = "standard";

    if (!dash_is_obj(data)) return NULL;
    name_node = json_obj_get(data, "name");
    if (!dash_nonempty_str(name_node)) return NULL;

    palette_src = dash_get(data, "palette");
    if (!dash_is_obj(palette_src)) palette_src = NULL;
    colors_src = dash_get(data, "colors");
    if (!dash_is_obj(colors_src)) colors_src = NULL;

    palette = json_object();
    json_set(palette, "background", dash_theme_layer(palette_src, colors_src, "background", "#041c1c", 1.0));
    json_set(palette, "midground", dash_theme_layer(palette_src, colors_src, "midground", "#ffe6cb", 1.0));
    json_set(palette, "foreground", dash_theme_layer(palette_src, colors_src, "foreground", "#ffffff", 0.0));
    {
        json_t *glow = dash_get(palette_src, "warmGlow");
        if (!dash_truthy(glow)) glow = dash_get(data, "warmGlow");
        if (dash_truthy(glow)) json_set(palette, "warmGlow", json_copy(glow));
        else json_set(palette, "warmGlow", json_string("rgba(255, 189, 56, 0.35)"));
    }
    noise_raw = dash_get(palette_src, "noiseOpacity");
    if (!noise_raw) noise_raw = dash_get(data, "noiseOpacity");
    if (noise_raw && noise_raw->type != JSON_NULL) {
        double parsed;
        if (dash_float_of(noise_raw, &parsed)) noise = parsed;
    }
    json_set(palette, "noiseOpacity", json_number(noise));

    typography = json_object();
    json_set(typography, "fontSans",
             json_string("system-ui, -apple-system, \"Segoe UI\", Roboto, \"Helvetica Neue\", Arial, sans-serif"));
    json_set(typography, "fontMono",
             json_string("ui-monospace, \"SF Mono\", \"Cascadia Mono\", Menlo, Consolas, monospace"));
    json_set(typography, "baseSize", json_string("15px"));
    json_set(typography, "lineHeight", json_string("1.55"));
    json_set(typography, "letterSpacing", json_string("0"));
    {
        static const char *const TYPO_KEYS[] = {
            "fontSans", "fontMono", "fontDisplay", "fontUrl",
            "baseSize", "lineHeight", "letterSpacing",
        };
        json_t *typo_src = dash_get(data, "typography");
        size_t i;
        for (i = 0; i < DASH_ARR_LEN(TYPO_KEYS); i++) {
            json_t *v = dash_get(typo_src, TYPO_KEYS[i]);
            if (dash_nonempty_str(v)) json_set(typography, TYPO_KEYS[i], json_string(v->str_val));
        }
    }

    layout = json_object();
    json_set(layout, "radius", json_string("0.5rem"));
    json_set(layout, "density", json_string("comfortable"));
    {
        json_t *layout_src = dash_get(data, "layout");
        json_t *radius = dash_get(layout_src, "radius");
        json_t *density = dash_get(layout_src, "density");
        if (dash_nonempty_str(radius)) json_set(layout, "radius", json_string(radius->str_val));
        if (density && density->type == JSON_STRING &&
            dash_in_set(density->str_val, (const char *const[]){"compact", "comfortable", "spacious"}, 3))
            json_set(layout, "density", json_string(density->str_val));
    }

    {
        json_t *src = dash_get(data, "colorOverrides");
        size_t i;
        for (i = 0; i < json_object_size(src); i++) {
            const char *k = json_object_get_key_at(src, i);
            json_t *v = json_object_get_at(src, i);
            if (!k || !dash_in_set(k, DASH_OVERRIDE_KEYS, DASH_ARR_LEN(DASH_OVERRIDE_KEYS))) continue;
            if (!dash_nonempty_str(v)) continue;
            if (!color_overrides) color_overrides = json_object();
            json_set(color_overrides, k, json_copy(v));
        }
    }

    assets_src = dash_get(data, "assets");
    {
        size_t i;
        for (i = 0; i < DASH_ARR_LEN(DASH_NAMED_ASSETS); i++) {
            json_t *v = dash_get(assets_src, DASH_NAMED_ASSETS[i]);
            if (!dash_nonempty_str(v)) continue;
            if (!assets_out) assets_out = json_object();
            json_set(assets_out, DASH_NAMED_ASSETS[i], json_copy(v));
        }
    }
    {
        json_t *custom = dash_get(assets_src, "custom");
        size_t i;
        for (i = 0; i < json_object_size(custom); i++) {
            const char *k = json_object_get_key_at(custom, i);
            json_t *v = json_object_get_at(custom, i);
            if (!k || !dash_css_ident(k) || !dash_nonempty_str(v)) continue;
            if (!custom_assets) custom_assets = json_object();
            json_set(custom_assets, k, json_copy(v));
        }
    }
    if (custom_assets) {
        if (!assets_out) assets_out = json_object();
        json_set(assets_out, "custom", custom_assets);
    }

    {
        json_t *styles_src = dash_get(data, "componentStyles");
        size_t i;
        for (i = 0; i < json_object_size(styles_src); i++) {
            const char *bucket = json_object_get_key_at(styles_src, i);
            json_t *props = json_object_get_at(styles_src, i);
            json_t *clean;
            size_t j;
            if (!bucket || !dash_in_set(bucket, DASH_COMPONENT_BUCKETS, DASH_ARR_LEN(DASH_COMPONENT_BUCKETS))) continue;
            if (!dash_is_obj(props)) continue;
            clean = json_object();
            for (j = 0; j < json_object_size(props); j++) {
                const char *prop = json_object_get_key_at(props, j);
                json_t *val = json_object_get_at(props, j);
                char *s;
                if (!prop || !dash_css_ident(prop) || !dash_style_scalar(val)) continue;
                s = dash_pystr(val);
                if (!s) continue;
                json_set(clean, prop, json_string(s));
                free(s);
            }
            if (json_object_size(clean) == 0) {
                json_free(clean);
                continue;
            }
            if (!component_styles) component_styles = json_object();
            json_set(component_styles, bucket, clean);
        }
    }

    {
        json_t *lv = dash_get(data, "layoutVariant");
        if (lv && lv->type == JSON_STRING &&
            dash_in_set(lv->str_val, DASH_LAYOUT_VARIANTS, DASH_ARR_LEN(DASH_LAYOUT_VARIANTS)))
            layout_variant = lv->str_val;
    }

    out = json_object();
    json_set(out, "name", json_copy(name_node));
    {
        json_t *label = json_obj_get(data, "label");
        if (dash_truthy(label)) json_set(out, "label", json_copy(label));
        else json_set(out, "label", json_copy(name_node));
    }
    {
        json_t *desc = json_obj_get(data, "description");
        if (desc) json_set(out, "description", json_copy(desc));
        else json_set(out, "description", json_string(""));
    }
    json_set(out, "palette", palette);
    json_set(out, "typography", typography);
    json_set(out, "layout", layout);
    json_set(out, "layoutVariant", json_string(layout_variant));
    if (color_overrides) json_set(out, "colorOverrides", color_overrides);
    if (assets_out) json_set(out, "assets", assets_out);
    {
        json_t *css = json_obj_get(data, "customCSS");
        if (dash_nonempty_str(css)) {
            size_t n = strlen(css->str_val);
            char *clip;
            if (n > (size_t)W28_WEBSERVERD_919ECC_P1_CUSTOM_CSS_MAX) n = (size_t)W28_WEBSERVERD_919ECC_P1_CUSTOM_CSS_MAX;
            clip = (char *)malloc(n + 1);
            if (clip) {
                memcpy(clip, css->str_val, n);
                clip[n] = '\0';
                json_set(out, "customCSS", json_string(clip));
                free(clip);
            }
        }
    }
    if (component_styles) json_set(out, "componentStyles", component_styles);
    return out;
}

/* ------------------------------------------------------------------ */
/* p1: the critical-CSS bootstrap shim                                 */
/* ------------------------------------------------------------------ */

/* _esc: str(s).replace("</", "<\\/") */
static char *dash_esc(const char *s) {
    return dash_replace_all(s ? s : "", "</", "<\\/");
}

json_t *w28_webserverd_919ecc_p1_bootstrap_css(const json_t *req) {
    json_t *active = dash_get(req, "active");
    json_t *builtins = dash_get(req, "builtins");
    json_t *themes = dash_get(req, "themes");
    size_t i;

    if (!active || active->type != JSON_STRING || active->str_val[0] == '\0')
        return json_string("");
    for (i = 0; i < json_len(builtins); i++) {
        json_t *b = json_get(builtins, i);
        if (b && b->type == JSON_STRING && !strcmp(b->str_val, active->str_val))
            return json_string("");
    }
    for (i = 0; i < json_len(themes); i++) {
        json_t *theme = json_get(themes, i);
        json_t *palette, *typo, *fs, *bs;
        char *font_sans, *base_size;
        char *esc_bg, *esc_mg, *esc_fs, *esc_bs, *hex_bg, *hex_mg;
        json_t *hex_bg_node;
        dash_sb out;

        if (!dash_is_obj(theme)) return json_string("");   /* .get on a non-dict raises -> "" */
        {
            json_t *nm = json_obj_get(theme, "name");
            if (!nm || nm->type != JSON_STRING || strcmp(nm->str_val, active->str_val)) continue;
        }
        palette = json_obj_get(theme, "palette");
        if (!dash_truthy(palette)) palette = NULL;
        else if (!dash_is_obj(palette)) return json_string("");
        typo = json_obj_get(theme, "typography");
        if (!dash_truthy(typo)) typo = NULL;
        else if (!dash_is_obj(typo)) return json_string("");

        fs = dash_get(typo, "fontSans");
        if (!dash_truthy(fs)) fs = NULL;
        bs = dash_get(typo, "baseSize");
        if (!dash_truthy(bs)) bs = NULL;
        font_sans = fs ? dash_pystr(fs) : dash_dup(
            "system-ui, -apple-system, \"Segoe UI\", Roboto, \"Helvetica Neue\", Arial, sans-serif");
        base_size = bs ? dash_pystr(bs) : dash_dup("15px");

        hex_bg_node = dash_get(palette, "background");
        if (!dash_truthy(hex_bg_node)) hex_bg_node = NULL;
        {
            json_t *hex = dash_get(hex_bg_node, "hex");
            if (hex) hex_bg = dash_pystr(hex);
            else hex_bg = dash_dup("#0a0a0a");
        }
        {
            json_t *mg = dash_get(palette, "midground");
            json_t *hex;
            if (!dash_truthy(mg)) mg = NULL;
            hex = dash_get(mg, "hex");
            if (hex) hex_mg = dash_pystr(hex);
            else hex_mg = dash_dup("#e5e5e5");
        }
        esc_bg = dash_esc(hex_bg);
        esc_mg = dash_esc(hex_mg);
        esc_fs = dash_esc(font_sans);
        esc_bs = dash_esc(base_size);

        dash_sb_init(&out);
        dash_sb_put(&out, "<style id=\"hermes-theme-bootstrap\">:root{");
        dash_sb_put(&out, "--background-base:");
        dash_sb_put(&out, esc_bg);
        dash_sb_put(&out, ";--midground-base:");
        dash_sb_put(&out, esc_mg);
        dash_sb_put(&out, ";--theme-font-sans:");
        dash_sb_put(&out, esc_fs);
        dash_sb_put(&out, ";--theme-base-size:");
        dash_sb_put(&out, esc_bs);
        dash_sb_put(&out, ";}html,body{background-color:var(--background-base);");
        dash_sb_put(&out, "color:var(--midground-base);");
        dash_sb_put(&out, "font-family:var(--theme-font-sans);");
        dash_sb_put(&out, "font-size:var(--theme-base-size);}</style>");

        free(hex_bg); free(hex_mg); free(esc_bg); free(esc_mg);
        free(esc_fs); free(esc_bs);
        free(font_sans); free(base_size);
        {
            char *css = dash_sb_take(&out);
            json_t *r = json_string(css);
            free(css);
            return r;
        }
    }
    return json_string("");
}

/* ------------------------------------------------------------------ */
/* p1: SPA responses and rewrites                                      */
/* ------------------------------------------------------------------ */

json_t *w28_webserverd_919ecc_p1_missing_message(const json_t *req) {
    json_t *surface = dash_get(req, "ui_surface");
    if (surface && surface->type == JSON_STRING && !strcmp(surface->str_val, "webapp"))
        return json_string("The Webapp is not built. Run: hermes webapp");
    return json_string("Frontend not built. Run: pnpm --dir web run build");
}

json_t *w28_webserverd_919ecc_p1_wants_html(const json_t *req) {
    const char *accept = dash_str(dash_get(req, "accept"), "");
    return json_bool(strstr(accept, "text/html") != NULL);
}

json_t *w28_webserverd_919ecc_p1_headless(const json_t *req) {
    const char *value = dash_str(dash_get(req, "value"), "");
    return json_bool(!strcmp(value, "1"));
}

json_t *w28_webserverd_919ecc_p1_root_token_page(const json_t *req) {
    const char *full_path = dash_str(dash_get(req, "full_path"), "");
    int gated = dash_truthy(dash_get(req, "gated"));
    int withheld = dash_truthy(dash_get(req, "withheld"));
    int serve = full_path[0] == '\0' && !gated && !withheld;
    json_t *out = json_object();
    json_set(out, "serve", json_bool(serve));
    json_set(out, "status", json_number(serve ? 200.0 : 404.0));
    if (serve) {
        json_set(out, "error", json_null());
    } else {
        json_set(out, "error", json_string(W28_WEBSERVERD_919ECC_P1_HEADLESS_MSG));
    }
    return out;
}

json_t *w28_webserverd_919ecc_p1_prefix_rewrite(const json_t *req) {
    const char *html_in = dash_str(dash_get(req, "html"), "");
    const char *prefix = dash_str(dash_get(req, "prefix"), "");
    static const char *const ATTRS[] = {
        "href=\"/assets/", "src=\"/assets/", "href=\"/favicon.ico\"",
        "href=\"/fonts/", "href=\"/ds-assets/", "src=\"/ds-assets/",
    };
    char *html = dash_dup(html_in);
    size_t i;
    if (prefix[0] == '\0') return json_string(html ? html : "");
    for (i = 0; i < DASH_ARR_LEN(ATTRS); i++) {
        /* attr.replace('"/', f'"{prefix}/', 1): the FIRST '"/' in the attr */
        const char *attr = ATTRS[i];
        const char *q = strstr(attr, "\"/");
        char repl[512];
        size_t head = q ? (size_t)(q - attr) : 0;
        char *next;
        if (!q) continue;
        snprintf(repl, sizeof(repl), "%.*s\"%s/%s", (int)head, attr, prefix, q + 2);
        next = dash_replace_all(html, attr, repl);   /* str.replace: every occurrence */
        free(html);
        html = next;
    }
    {
        json_t *r = json_string(html ? html : "");
        free(html);
        return r;
    }
}

json_t *w28_webserverd_919ecc_p1_css_url_rewrite(const json_t *req) {
    const char *css_in = dash_str(dash_get(req, "css"), "");
    const char *prefix = dash_str(dash_get(req, "prefix"), "");
    static const char *const ASSET_DIRS[] = {"/fonts/", "/fonts-terminal/", "/ds-assets/", "/assets/"};
    static const char *const QUOTES[] = {"", "\"", "'"};
    char *css = dash_dup(css_in);
    size_t d, q;
    if (prefix[0] == '\0') return json_string(css ? css : "");
    for (d = 0; d < DASH_ARR_LEN(ASSET_DIRS); d++) {
        for (q = 0; q < DASH_ARR_LEN(QUOTES); q++) {
            char needle[64];
            char repl[512];
            char *next;
            snprintf(needle, sizeof(needle), "url(%s%s", QUOTES[q], ASSET_DIRS[d]);
            snprintf(repl, sizeof(repl), "url(%s%s%s", QUOTES[q], prefix, ASSET_DIRS[d]);
            next = dash_replace_all(css, needle, repl);
            free(css);
            css = next;
        }
    }
    {
        json_t *r = json_string(css ? css : "");
        free(css);
        return r;
    }
}

json_t *w28_webserverd_919ecc_p1_spa_route(const json_t *req) {
    const char *full_path = dash_str(dash_get(req, "full_path"), "");
    int html_token = dash_truthy(dash_get(req, "html_token"));
    int file_is_file = dash_truthy(dash_get(req, "file_is_file"));
    int index_exists = dash_truthy(dash_get(req, "index_exists"));
    int samefile = dash_truthy(dash_get(req, "samefile"));
    int under_dist = dash_truthy(dash_get(req, "under_dist"));
    int exists = dash_truthy(dash_get(req, "exists"));
    int is_static = dash_truthy(dash_get(req, "is_static"));
    int font_found = dash_truthy(dash_get(req, "font_found"));
    json_t *out = json_object();

    if (!strcmp(full_path, "api") || !strncmp(full_path, "api/", 4)) {
        char detail[4096];
        snprintf(detail, sizeof(detail), "No such API endpoint: /%s", full_path);
        json_set(out, "kind", json_string("api_404"));
        json_set(out, "detail", json_string(detail));
        return out;
    }
    if (!html_token && file_is_file && index_exists && samefile) {
        json_set(out, "kind", json_string("index"));
        json_set(out, "detail", json_null());
        return out;
    }
    if (full_path[0] && under_dist && exists && file_is_file) {
        json_set(out, "kind", json_string("file"));
        json_set(out, "detail", json_null());
        return out;
    }
    if (is_static) {
        json_set(out, "kind", json_string(font_found ? "font" : "static_404"));
        json_set(out, "detail", font_found ? json_null() : json_string("not found"));
        return out;
    }
    json_set(out, "kind", json_string("index_fallback"));
    json_set(out, "detail", json_null());
    return out;
}

json_t *w28_webserverd_919ecc_p1_constants(void) {
    json_t *o = json_object();
    json_t *builtins = json_array();
    static const char *const NAMES[] = {
        "default", "default-large", "nous-blue", "midnight",
        "ember", "mono", "cyberpunk", "rose",
    };
    size_t i;
    for (i = 0; i < DASH_ARR_LEN(NAMES); i++) json_append(builtins, json_string(NAMES[i]));
    json_set(o, "immutable_cache_control", json_string("public, max-age=31536000, immutable"));
    json_set(o, "no_store_cache_control", json_string("no-store, no-cache, must-revalidate"));
    json_set(o, "headless_message", json_string(W28_WEBSERVERD_919ECC_P1_HEADLESS_MSG));
    json_set(o, "theme_custom_css_max", json_number((double)W28_WEBSERVERD_919ECC_P1_CUSTOM_CSS_MAX));
    json_set(o, "theme_default_background", json_string("#041c1c"));
    json_set(o, "theme_default_midground", json_string("#ffe6cb"));
    json_set(o, "theme_default_foreground", json_string("#ffffff"));
    json_set(o, "theme_default_warm_glow", json_string("rgba(255, 189, 56, 0.35)"));
    json_set(o, "bootstrap_background_fallback", json_string("#0a0a0a"));
    json_set(o, "bootstrap_midground_fallback", json_string("#e5e5e5"));
    json_set(o, "theme_default_font_sans",
             json_string("system-ui, -apple-system, \"Segoe UI\", Roboto, \"Helvetica Neue\", Arial, sans-serif"));
    json_set(o, "theme_default_font_mono",
             json_string("ui-monospace, \"SF Mono\", \"Cascadia Mono\", Menlo, Consolas, monospace"));
    json_set(o, "theme_default_base_size", json_string("15px"));
    json_set(o, "theme_default_line_height", json_string("1.55"));
    json_set(o, "theme_default_letter_spacing", json_string("0"));
    json_set(o, "theme_default_radius", json_string("0.5rem"));
    json_set(o, "theme_default_density", json_string("comfortable"));
    json_set(o, "builtin_themes", builtins);
    {
        json_t *set = json_array();
        for (i = 0; i < DASH_ARR_LEN(DASH_LAYOUT_VARIANTS); i++)
            json_append(set, json_string(DASH_LAYOUT_VARIANTS[i]));
        json_set(o, "layout_variants", set);
    }
    return o;
}

/* ------------------------------------------------------------------ */
/* p2: shared set / path / route-table helpers                         */
/* ------------------------------------------------------------------ */

/* name in a JSON array of strings (the enable/disable sets). */
static int dash_set_has(const json_t *arr, const char *name) {
    size_t i;
    if (!name) return 0;
    for (i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, name)) return 1;
    }
    return 0;
}

/* _plugin_api_mount_skip_reason over array-shaped sets. */
static const char *dash_skip_reason(const json_t *plugin, const json_t *enabled, const json_t *disabled) {
    const char *source = dash_str(dash_get(plugin, "source"), NULL);
    const char *name = dash_str(dash_get(plugin, "name"), "");
    int is_user = source && !strcmp(source, "user");
    int is_bundled = source && !strcmp(source, "bundled");
    if ((is_user || is_bundled) && dash_set_has(disabled, name)) return "explicitly disabled";
    if (is_user && !dash_set_has(enabled, name)) return "not in plugins.enabled";
    return NULL;
}

/* _plugin_api_mounted over a route-table fixture. */
static int dash_api_mounted_routes(const json_t *routes, const char *name) {
    char prefix[512];
    size_t i;
    size_t plen;
    snprintf(prefix, sizeof(prefix), "/api/plugins/%s", name ? name : "");
    plen = strlen(prefix);
    for (i = 0; i < json_len(routes); i++) {
        json_t *r = json_get(routes, i);
        const char *path = dash_str(dash_get(r, "path"), "");
        if (dash_truthy(dash_get(r, "hot"))) continue;
        if (!strcmp(path, prefix)) return 1;
        if (!strncmp(path, prefix, plen) && path[plen] == '/') return 1;
    }
    return 0;
}

static int dash_ieq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* Windows path arithmetic, lexical (resolve() without touching the disk). */
#define DASH_MAX_COMP 96
typedef struct {
    char *comp[DASH_MAX_COMP];
    int   n;
    char  drive[4];
} dash_path;

static void dash_path_free(dash_path *p) {
    int i;
    for (i = 0; i < p->n; i++) free(p->comp[i]);
    p->n = 0;
    p->drive[0] = '\0';
}

static void dash_path_push(dash_path *p, const char *s, size_t len) {
    char *c;
    if (p->n >= DASH_MAX_COMP || len == 0) return;
    c = (char *)malloc(len + 1);
    if (!c) return;
    memcpy(c, s, len);
    c[len] = '\0';
    p->comp[p->n++] = c;
}

static void dash_path_normalise(const char *path, dash_path *out) {
    const char *p = path ? path : "";
    out->n = 0;
    out->drive[0] = '\0';
    while (*p) {
        const char *start;
        size_t len;
        while (*p == '/' || *p == '\\') p++;
        if (!*p) break;
        start = p;
        while (*p && *p != '/' && *p != '\\') p++;
        len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') continue;
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (out->n > 0) {
                free(out->comp[--out->n]);
                out->comp[out->n] = NULL;
            }
            continue;
        }
        if (out->n == 0 && !out->drive[0] && len == 2 &&
            isalpha((unsigned char)start[0]) && start[1] == ':') {
            out->drive[0] = (char)toupper((unsigned char)start[0]);
            out->drive[1] = ':';
            out->drive[2] = '\0';
            continue;
        }
        dash_path_push(out, start, len);
    }
}

static int dash_path_same_prefix(const dash_path *base, const dash_path *full) {
    int i;
    if (!base->drive[0] && base->n == 0) return 1;      /* base "." is the cwd */
    if (base->drive[0] && !dash_ieq(base->drive, full->drive)) return 0;
    if (full->n < base->n) return 0;
    for (i = 0; i < base->n; i++)
        if (!dash_ieq(base->comp[i], full->comp[i])) return 0;
    return 1;
}

/* Path(...).is_absolute() on Windows. */
static int dash_is_abs_path(const char *s) {
    if (!s || !s[0]) return 0;
    if (s[0] == '/' || s[0] == '\\') return 1;
    if (isalpha((unsigned char)s[0]) && s[1] == ':' && (s[2] == '/' || s[2] == '\\')) return 1;
    return 0;
}

/* _safe_plugin_api_relpath: the original string when it stays inside the
 * plugin's dashboard dir, else NULL. */
static char *dash_safe_relpath(const json_t *api_field, const char *dashboard_dir) {
    char *stripped;
    char *joined;
    dash_path base, full;
    int inside;

    if (!api_field || api_field->type != JSON_STRING) return NULL;
    stripped = dash_strip(api_field->str_val);
    if (!stripped) return NULL;
    if (stripped[0] == '\0') { free(stripped); return NULL; }
    if (dash_is_abs_path(stripped) ||
        (isalpha((unsigned char)stripped[0]) && stripped[1] == ':')) {
        free(stripped);   /* absolute, or a drive-relative path that replaces the base */
        return NULL;
    }
    {
        size_t n = strlen(dashboard_dir ? dashboard_dir : "") + strlen(stripped) + 2;
        joined = (char *)malloc(n);
        if (!joined) { free(stripped); return NULL; }
        snprintf(joined, n, "%s/%s", dashboard_dir ? dashboard_dir : "", stripped);
    }
    dash_path_normalise(dashboard_dir ? dashboard_dir : "", &base);
    dash_path_normalise(joined, &full);
    inside = dash_path_same_prefix(&base, &full);
    dash_path_free(&base);
    dash_path_free(&full);
    free(joined);
    free(stripped);
    return inside ? dash_dup(api_field->str_val) : NULL;
}

/* ------------------------------------------------------------------ */
/* p2: manifest shaping and runtime status                             */
/* ------------------------------------------------------------------ */

json_t *w28_webserverd_919ecc_p2_strip_manifest(const json_t *p) {
    json_t *out = json_object();
    size_t i;
    for (i = 0; i < json_object_size(p); i++) {
        const char *k = json_object_get_key_at(p, i);
        if (!k || k[0] == '_') continue;
        json_set(out, k, json_copy(json_object_get_at(p, i)));
    }
    return out;
}

json_t *w28_webserverd_919ecc_p2_runtime_status(const json_t *req) {
    json_t *aliases = dash_get(req, "aliases");
    json_t *enabled = dash_get(req, "enabled");
    json_t *disabled = dash_get(req, "disabled");
    size_t i;
    for (i = 0; i < json_len(aliases); i++) {
        json_t *a = json_get(aliases, i);
        if (a && a->type == JSON_STRING && dash_set_has(disabled, a->str_val))
            return json_string("disabled");
    }
    for (i = 0; i < json_len(aliases); i++) {
        json_t *a = json_get(aliases, i);
        if (a && a->type == JSON_STRING && dash_set_has(enabled, a->str_val))
            return json_string("enabled");
    }
    return json_string("inactive");
}

json_t *w28_webserverd_919ecc_p2_mount_skip_reason(const json_t *req) {
    const char *reason = dash_skip_reason(dash_get(req, "plugin"),
                                          dash_get(req, "enabled"), dash_get(req, "disabled"));
    return reason ? json_string(reason) : json_null();
}

json_t *w28_webserverd_919ecc_p2_plugin_entry(const json_t *req) {
    json_t *data = dash_get(req, "data");
    const char *name = dash_str(dash_get(req, "name"), "");
    const char *dashboard_dir = dash_str(dash_get(req, "dashboard_dir"), "");
    const char *source = dash_str(dash_get(req, "source"), "");
    json_t *out = json_object();
    json_t *tab_info = json_object();
    json_t *raw_tab = dash_get(data, "tab");
    json_t *slots_out = json_array();
    char *safe_api;
    char default_path[512];

    if (!dash_is_obj(raw_tab)) raw_tab = NULL;
    snprintf(default_path, sizeof(default_path), "/%s", name);
    {
        json_t *path = dash_get(raw_tab, "path");
        json_t *pos = dash_get(raw_tab, "position");
        if (path) json_set(tab_info, "path", json_copy(path));
        else json_set(tab_info, "path", json_string(default_path));
        if (pos) json_set(tab_info, "position", json_copy(pos));
        else json_set(tab_info, "position", json_string("end"));
    }
    {
        json_t *override = dash_get(raw_tab, "override");
        if (override && override->type == JSON_STRING && override->str_val[0] == '/')
            json_set(tab_info, "override", json_copy(override));
        if (dash_truthy(dash_get(raw_tab, "hidden")))
            json_set(tab_info, "hidden", json_bool(1));
    }
    {
        json_t *slots_src = dash_get(data, "slots");
        size_t i;
        for (i = 0; i < json_len(slots_src); i++) {
            json_t *s = json_get(slots_src, i);
            if (s && s->type == JSON_STRING && s->str_val[0]) json_append(slots_out, json_copy(s));
        }
    }
    safe_api = dash_safe_relpath(dash_get(data, "api"), dashboard_dir);

    json_set(out, "name", json_string(name));
    {
        json_t *label = dash_get(data, "label");
        if (label) json_set(out, "label", json_copy(label));
        else json_set(out, "label", json_string(name));
    }
    {
        json_t *desc = dash_get(data, "description");
        if (desc) json_set(out, "description", json_copy(desc));
        else json_set(out, "description", json_string(""));
    }
    {
        json_t *icon = dash_get(data, "icon");
        if (icon) json_set(out, "icon", json_copy(icon));
        else json_set(out, "icon", json_string("Puzzle"));
    }
    {
        json_t *version = dash_get(data, "version");
        if (version) json_set(out, "version", json_copy(version));
        else json_set(out, "version", json_string("0.0.0"));
    }
    json_set(out, "tab", tab_info);
    json_set(out, "slots", slots_out);
    {
        json_t *entry = dash_get(data, "entry");
        if (entry) json_set(out, "entry", json_copy(entry));
        else json_set(out, "entry", json_string("dist/index.js"));
    }
    {
        json_t *css = dash_get(data, "css");
        json_set(out, "css", css ? json_copy(css) : json_null());
    }
    json_set(out, "has_api", json_bool(safe_api != NULL));
    json_set(out, "source", json_string(source));
    json_set(out, "_dir", json_string(dashboard_dir));
    json_set(out, "_api_file", safe_api ? json_string(safe_api) : json_null());
    free(safe_api);
    return out;
}

json_t *w28_webserverd_919ecc_p2_safe_api_relpath(const json_t *req) {
    char *rel = dash_safe_relpath(dash_get(req, "api_field"),
                                  dash_str(dash_get(req, "dashboard_dir"), ""));
    json_t *out;
    if (!rel) return json_null();
    out = json_string(rel);
    free(rel);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: route-table decisions                                           */
/* ------------------------------------------------------------------ */

json_t *w28_webserverd_919ecc_p2_api_name(const json_t *req) {
    const char *path = dash_str(dash_get(req, "path"), "");
    const char *root = dash_str(dash_get(req, "root_path"), "");
    const char *p = path;

    if (root[0] && !strncmp(path, root, strlen(root))) p = path + strlen(root);
    if (strncmp(p, "/api/plugins/", 13)) return json_string("");
    {
        /* parts = p.split("/"): ['', 'api', 'plugins', '<name>', ...] — the
         * empty field before the leading '/' counts. */
        const char *cursor = p;
        int field = 0;
        for (;;) {
            const char *start = cursor;
            size_t len;
            while (*cursor && *cursor != '/') cursor++;
            len = (size_t)(cursor - start);
            if (field == 3) {
                char buf[512];
                if (len >= sizeof(buf)) len = sizeof(buf) - 1;
                memcpy(buf, start, len);
                buf[len] = '\0';
                return json_string(buf);
            }
            field++;
            if (!*cursor) break;
            cursor++;
        }
        return json_string("");   /* a trailing empty name is parts[3] == "" */
    }
}

json_t *w28_webserverd_919ecc_p2_api_mounted(const json_t *req) {
    return json_bool(dash_api_mounted_routes(dash_get(req, "routes"),
                                             dash_str(dash_get(req, "name"), "")));
}

json_t *w28_webserverd_919ecc_p2_hot_name_ok(const json_t *req) {
    const char *name = dash_str(dash_get(req, "name"), "");
    if (!name[0]) return json_bool(0);
    if (!strcmp(name, "..")) return json_bool(0);
    if (strpbrk(name, "/\\{}")) return json_bool(0);
    return json_bool(1);
}

/* _plugin_login_command(name) over the LEFT_CORE table. */
static char *dash_login_command(const char *name, const json_t *left_core) {
    size_t i;
    dash_sb b;
    for (i = 0; i < json_len(left_core); i++) {
        json_t *entry = json_get(left_core, i);
        const char *plugin = dash_str(dash_get(entry, "plugin"), NULL);
        json_t *cli = dash_get(entry, "cli");
        if (plugin && !strcmp(plugin, name) && dash_truthy(cli)) {
            dash_sb_init(&b);
            dash_sb_putf(&b, "hermes %s login", dash_str(cli, ""));
            return dash_sb_take(&b);
        }
    }
    dash_sb_init(&b);
    dash_sb_putf(&b, "hermes auth %s", name);
    return dash_sb_take(&b);
}

json_t *w28_webserverd_919ecc_p2_login_command(const json_t *req) {
    char *cmd = dash_login_command(dash_str(dash_get(req, "name"), ""),
                                   dash_get(req, "left_core"));
    json_t *out = json_string(cmd);
    free(cmd);
    return out;
}

json_t *w28_webserverd_919ecc_p2_auth_hint(const json_t *req) {
    const char *name = dash_str(dash_get(req, "name"), "");
    json_t *tools = dash_get(req, "provides_tools");
    json_t *registry = dash_get(req, "registry");
    json_t *cached = dash_get(req, "cached");
    json_t *left_core = dash_get(req, "left_core");
    json_t *probes = json_array();
    json_t *out = json_object();
    size_t i;

    for (i = 0; i < json_len(tools); i++) {
        json_t *tname = json_get(tools, i);
        json_t *entry, *check_fn, *verdict;
        const char *fn;
        if (!tname || tname->type != JSON_STRING) continue;
        entry = dash_get(registry, tname->str_val);
        if (!dash_truthy(entry)) continue;
        check_fn = dash_get(entry, "check_fn");
        if (!dash_truthy(check_fn)) continue;
        fn = dash_str(check_fn, NULL);
        if (!fn) continue;
        verdict = dash_get(cached, fn);
        if (!verdict || verdict->type == JSON_NULL) {
            json_append(probes, json_string(fn));
            continue;
        }
        if (verdict->type == JSON_BOOL && !verdict->bool_val) {
            char *cmd = dash_login_command(name, left_core);
            json_set(out, "auth_required", json_bool(1));
            json_set(out, "auth_command", json_string(cmd));
            json_set(out, "probes", probes);
            free(cmd);
            return out;
        }
    }
    json_set(out, "auth_required", json_bool(0));
    json_set(out, "auth_command", json_string(""));
    json_set(out, "probes", probes);
    return out;
}

json_t *w28_webserverd_919ecc_p2_mount_decision(const json_t *req) {
    json_t *plugin = dash_get(req, "plugin");
    json_t *routes = dash_get(req, "routes");
    const char *isolation = dash_str(dash_get(req, "isolation"), "in-process");
    const char *import_mode = dash_str(dash_get(req, "import_mode"), "ok");
    const char *name = dash_str(dash_get(plugin, "name"), "");
    const char *source = dash_str(dash_get(plugin, "source"), NULL);
    json_t *out = json_object();

    json_set(out, "ok", json_bool(0));
    json_set(out, "mode", json_string("none"));
    json_set(out, "imported", json_bool(0));
    json_set(out, "forwarded", json_bool(0));

    if (!dash_truthy(dash_get(plugin, "_api_file"))) return out;
    if (dash_api_mounted_routes(routes, name)) return out;
    if (dash_skip_reason(plugin, dash_get(req, "enabled"), dash_get(req, "disabled")) != NULL)
        return out;
    if (!(source && (!strcmp(source, "bundled") || !strcmp(source, "project")))) {
        if (!strcmp(isolation, "host")) {
            json_set(out, "ok", json_bool(1));
            json_set(out, "mode", json_string("hosted"));
            json_set(out, "forwarded", json_bool(1));
            return out;
        }
    }
    if (source && !strcmp(source, "project")) return out;
    if (!dash_truthy(dash_get(req, "api_inside"))) return out;
    if (!dash_truthy(dash_get(req, "api_exists"))) return out;
    /* Past the file checks every branch calls import_user_code: the call itself
     * is the observable, so "imported" is 1 even when it fails or finds no router. */
    json_set(out, "imported", json_bool(1));
    if (!strcmp(import_mode, "none") || !strcmp(import_mode, "raise") ||
        !strcmp(import_mode, "norouter"))
        return out;
    json_set(out, "ok", json_bool(1));
    json_set(out, "mode", json_string("in-process"));
    json_set(out, "imported", json_bool(1));
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: config sets, hub shaping, cache/splice decisions                */
/* ------------------------------------------------------------------ */

json_t *w28_webserverd_919ecc_p2_env_truthy(const json_t *req) {
    const char *value = dash_str(dash_get(req, "value"), "");
    char *s = dash_strip(value);
    char *p;
    int truthy = 0;
    if (!s) return json_bool(0);
    for (p = s; *p; p++) *p = (char)tolower((unsigned char)*p);
    truthy = !strcmp(s, "1") || !strcmp(s, "true") || !strcmp(s, "yes") || !strcmp(s, "on");
    free(s);
    return json_bool(truthy);
}

static int dash_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* sorted(set(list)) with non-strings dropped (a config list of names). */
static json_t *dash_sorted_set(const json_t *src) {
    json_t *out = json_array();
    json_t *list;
    size_t i, n;
    if (!dash_is_arr(src) || json_len(src) == 0) return out;
    n = json_len(src);
    list = json_array();
    for (i = 0; i < n; i++) {
        json_t *v = json_get(src, i);
        if (v && v->type == JSON_STRING) json_append(list, json_copy(v));
    }
    n = json_len(list);
    if (n) {
        const char **items = (const char **)malloc(n * sizeof(char *));
        size_t keep = 0;
        if (items) {
            for (i = 0; i < n; i++) items[i] = json_get(list, i)->str_val;
            qsort(items, n, sizeof(char *), dash_cmp_str);
            for (i = 0; i < n; i++) {
                if (keep && !strcmp(items[keep - 1], items[i])) continue;
                items[keep++] = items[i];
            }
            for (i = 0; i < keep; i++) json_append(out, json_string(items[i]));
            free(items);
        }
    }
    json_free(list);
    return out;
}

json_t *w28_webserverd_919ecc_p2_enable_sets(const json_t *req) {
    json_t *config = dash_get(req, "config");
    json_t *plugins = dash_get(config, "plugins");
    json_t *out = json_object();
    json_set(out, "enabled", dash_sorted_set(dash_get(plugins, "enabled")));
    json_set(out, "disabled", dash_sorted_set(dash_get(plugins, "disabled")));
    return out;
}

json_t *w28_webserverd_919ecc_p2_orphan_dashboard(const json_t *req) {
    json_t *rows = dash_get(req, "rows");
    json_t *dashboard = dash_get(req, "dashboard_list");
    json_t *out = json_array();
    size_t i, j;

    for (i = 0; i < json_len(dashboard); i++) {
        json_t *entry = json_get(dashboard, i);
        json_t *name_node = dash_get(entry, "name");
        char *name = name_node ? dash_pystr(name_node) : NULL;
        int found = 0;
        for (j = 0; j < json_len(rows); j++) {
            json_t *row_name = dash_get(json_get(rows, j), "name");
            if (row_name && name && row_name->type == JSON_STRING && !strcmp(row_name->str_val, name)) {
                found = 1;
                break;
            }
        }
        free(name);
        if (!found) json_append(out, w28_webserverd_919ecc_p2_strip_manifest(entry));
    }
    return out;
}

json_t *w28_webserverd_919ecc_p2_cache_hit(const json_t *req) {
    int force = dash_truthy(dash_get(req, "force"));
    int has_entry = dash_truthy(dash_get(req, "has_entry"));
    double now = 0.0, expiry = 0.0;
    dash_float_of(dash_get(req, "now"), &now);
    dash_float_of(dash_get(req, "expiry"), &expiry);
    return json_bool(!force && has_entry && now < expiry);
}

json_t *w28_webserverd_919ecc_p2_splice_index(const json_t *req) {
    json_t *routes = dash_get(req, "routes");
    size_t i;
    for (i = 0; i < json_len(routes); i++) {
        if (dash_truthy(dash_get(json_get(routes, i), "hot"))) return json_number((double)i);
    }
    return json_number((double)json_len(routes));
}

json_t *w28_webserverd_919ecc_p2_constants(void) {
    json_t *o = json_object();
    json_set(o, "hub_cache_ttl_seconds", json_number(W28_WEBSERVERD_919ECC_P2_HUB_CACHE_TTL_SECONDS));
    json_set(o, "skip_reason_disabled", json_string("explicitly disabled"));
    json_set(o, "skip_reason_not_enabled", json_string("not in plugins.enabled"));
    json_set(o, "isolation_host", json_string("host"));
    json_set(o, "default_entry", json_string("dist/index.js"));
    json_set(o, "default_icon", json_string("Puzzle"));
    json_set(o, "default_version", json_string("0.0.0"));
    json_set(o, "default_tab_position", json_string("end"));
    json_set(o, "auth_command_prefix", json_string("hermes auth "));
    return o;
}
