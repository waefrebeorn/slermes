/*
 * port_web_server_themes.c — dashboard theme discovery + normalisation.
 * Faithful port of _parse_theme_layer, _normalise_theme_definition,
 * _discover_user_themes, get_dashboard_themes, and
 * _render_active_theme_bootstrap_css from hermes_cli/web_server.py.
 *
 * YAML files are parsed with libyaml and converted to json via
 * yaml_to_json_string(doc, "") so the normaliser works on one tree type.
 */

#include "web_server_themes.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "libyaml/yaml.h"
#include "slermes_home.h"
#include "web_server_events.h"  /* ws_events_theme_css_esc */

/* ── module constants (web_server.py) ───────────────────────────────────── */

#define THEME_CUSTOM_CSS_MAX (32 * 1024)

static const struct { const char *name, *label, *description; }
K_BUILTIN_THEMES[] = {
    {"default",       "Hermes Teal",         "Classic dark teal — the canonical Hermes look"},
    {"default-large", "Hermes Teal (Large)", "Hermes Teal with bigger fonts and roomier spacing"},
    {"nous-blue",     "Nous Blue",           "Light mode — vivid Nous-blue accents on cream canvas"},
    {"midnight",      "Midnight",            "Deep blue-violet with cool accents"},
    {"ember",         "Ember",               "Warm crimson and bronze — forge vibes"},
    {"mono",          "Mono",                "Clean grayscale — minimal and focused"},
    {"cyberpunk",     "Cyberpunk",           "Neon green on black — matrix terminal"},
    {"rose",          "Rosé",                "Soft pink and warm ivory — easy on the eyes"},
};
#define N_BUILTIN (sizeof(K_BUILTIN_THEMES) / sizeof(K_BUILTIN_THEMES[0]))

static const char *K_OVERRIDE_KEYS[] = {
    "card", "cardForeground", "popover", "popoverForeground",
    "primary", "primaryForeground", "secondary", "secondaryForeground",
    "muted", "mutedForeground", "accent", "accentForeground",
    "destructive", "destructiveForeground", "success", "warning",
    "border", "input", "ring", NULL,
};

static const char *K_NAMED_ASSET_KEYS[] = {
    "bg", "hero", "logo", "crest", "sidebar", "header", NULL,
};

static const char *K_COMPONENT_BUCKETS[] = {
    "card", "header", "footer", "sidebar", "tab",
    "progress", "badge", "backdrop", "page", NULL,
};

static const char *K_LAYOUT_VARIANTS[] = {"standard", "cockpit", "tiled", NULL};

static const char *K_DENSITIES[] = {"compact", "comfortable", "spacious", NULL};

static const char *K_FONT_CHOICES[] = {
    "system-sans", "system-serif", "system-mono",
    "inter", "ibm-plex-sans", "work-sans", "atkinson-hyperlegible", "dm-sans",
    "spectral", "fraunces", "source-serif",
    "jetbrains-mono", "ibm-plex-mono", "space-mono", NULL,
};

#define THEME_DEFAULT_FONT_SANS \
    "system-ui, -apple-system, \"Segoe UI\", Roboto, \"Helvetica Neue\", Arial, sans-serif"
#define THEME_DEFAULT_FONT_MONO \
    "ui-monospace, \"SF Mono\", \"Cascadia Mono\", Menlo, Consolas, monospace"
#define THEME_DEFAULT_BASE_SIZE "15px"
#define THEME_DEFAULT_LINE_HEIGHT "1.55"
#define THEME_DEFAULT_LETTER_SPACING "0"

static bool in_list(const char **list, const char *s) {
    for (size_t i = 0; list[i]; i++)
        if (strcmp(list[i], s) == 0) return true;
    return false;
}

/* Python: str.strip() truthiness — non-empty after strip. */
static bool str_nonblank(const char *s) {
    if (!s) return false;
    while (*s) {
        if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') return true;
        s++;
    }
    return false;
}

/* Python: key.replace("-","").replace("_","").isalnum() */
static bool key_isalnumish(const char *s) {
    bool any = false;
    for (const char *p = s; *p; p++) {
        if (*p == '-' || *p == '_') continue;
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9')))
            return false;
        any = true;
    }
    return any;
}

/* ── _parse_theme_layer ─────────────────────────────────────────────────── */
/* PoP: ws_theme_parse_layer @ hermes_cli/web_server.py:_parse_theme_layer */
json_t *ws_theme_parse_layer(const json_t *value, const char *default_hex,
                             double default_alpha) {
    if (!value || value->type == JSON_NULL) {
        json_t *o = json_object();
        json_set(o, "hex", json_string(default_hex));
        json_set(o, "alpha", json_number(default_alpha));
        return o;
    }
    if (value->type == JSON_STRING) {
        json_t *o = json_object();
        json_set(o, "hex", json_string(value->str_val));
        json_set(o, "alpha", json_number(default_alpha));
        return o;
    }
    if (value->type == JSON_OBJECT) {
        json_t *hex_v = json_object_get((json_t *)value, "hex");
        json_t *alpha_v = json_object_get((json_t *)value, "alpha");
        const char *hex_val = default_hex;
        if (hex_v) {
            if (hex_v->type != JSON_STRING) return NULL;
            hex_val = hex_v->str_val;
        }
        double alpha_f = default_alpha;
        if (alpha_v) {
            if (alpha_v->type == JSON_NUMBER) alpha_f = alpha_v->num_val;
            else if (alpha_v->type == JSON_BOOL) alpha_f = alpha_v->bool_val ? 1.0 : 0.0;
            else if (alpha_v->type == JSON_STRING) {
                char *end = NULL;
                double d = strtod(alpha_v->str_val, &end);
                if (end && end != alpha_v->str_val && *end == '\0') alpha_f = d;
                /* else float() raises → alpha_f stays default */
            }
            /* dict/list → TypeError → default */
        }
        if (alpha_f < 0.0) alpha_f = 0.0;
        if (alpha_f > 1.0) alpha_f = 1.0;
        json_t *o = json_object();
        json_set(o, "hex", json_string(hex_val));
        json_set(o, "alpha", json_number(alpha_f));
        return o;
    }
    return NULL;
}

/* ── _normalise_theme_definition ────────────────────────────────────────── */

static json_t *get_dict(const json_t *data, const char *key) {
    json_t *v = json_object_get((json_t *)data, key);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

static json_t *theme_layer(const json_t *palette_src, const json_t *colors_src,
                           const char *key, const char *default_hex,
                           double default_alpha) {
    json_t *spec = NULL;
    if (palette_src) spec = json_object_get((json_t *)palette_src, key);
    if (!spec && colors_src) spec = json_object_get((json_t *)colors_src, key);
    json_t *parsed = ws_theme_parse_layer(spec, default_hex, default_alpha);
    if (parsed) return parsed;
    json_t *o = json_object();
    json_set(o, "hex", json_string(default_hex));
    json_set(o, "alpha", json_number(default_alpha));
    return o;
}

/* PoP: ws_theme_normalise_definition @ hermes_cli/web_server.py:_normalise_theme_definition */
json_t *ws_theme_normalise_definition(const json_t *data) {
    if (!data || data->type != JSON_OBJECT) return NULL;
    json_t *name_v = json_object_get((json_t *)data, "name");
    if (!name_v || name_v->type != JSON_STRING || !str_nonblank(name_v->str_val))
        return NULL;
    const char *name = name_v->str_val;

    json_t *palette_src = get_dict(data, "palette");
    json_t *colors_src = get_dict(data, "colors");

    /* Palette */
    json_t *palette = json_object();
    json_set(palette, "background",
             theme_layer(palette_src, colors_src, "background", "#041c1c", 1.0));
    json_set(palette, "midground",
             theme_layer(palette_src, colors_src, "midground", "#ffe6cb", 1.0));
    json_set(palette, "foreground",
             theme_layer(palette_src, colors_src, "foreground", "#ffffff", 0.0));
    /* warmGlow: palette_src.get() or data.get() or default — Python `or`
     * treats empty string / None as falsy. */
    const char *warm = NULL;
    if (palette_src) {
        json_t *w = json_object_get(palette_src, "warmGlow");
        if (w && w->type == JSON_STRING && *w->str_val) warm = w->str_val;
    }
    if (!warm) {
        json_t *w = json_object_get((json_t *)data, "warmGlow");
        if (w && w->type == JSON_STRING && *w->str_val) warm = w->str_val;
    }
    json_set(palette, "warmGlow",
             json_string(warm ? warm : "rgba(255, 189, 56, 0.35)"));
    /* noiseOpacity */
    double noise = 1.0;
    json_t *raw_noise = palette_src ? json_object_get(palette_src, "noiseOpacity") : NULL;
    if (!raw_noise) raw_noise = json_object_get((json_t *)data, "noiseOpacity");
    if (raw_noise) {
        if (raw_noise->type == JSON_NUMBER) noise = raw_noise->num_val;
        else if (raw_noise->type == JSON_BOOL) noise = raw_noise->bool_val ? 1.0 : 0.0;
        else if (raw_noise->type == JSON_STRING) {
            char *end = NULL;
            double d = strtod(raw_noise->str_val, &end);
            if (end && end != raw_noise->str_val && *end == '\0') noise = d;
            else noise = 1.0;
        } else noise = 1.0;
    }
    json_set(palette, "noiseOpacity", json_number(noise));

    /* Typography */
    json_t *typo_src = get_dict(data, "typography");
    json_t *typography = json_object();
    json_set(typography, "fontSans", json_string(THEME_DEFAULT_FONT_SANS));
    json_set(typography, "fontMono", json_string(THEME_DEFAULT_FONT_MONO));
    json_set(typography, "baseSize", json_string(THEME_DEFAULT_BASE_SIZE));
    json_set(typography, "lineHeight", json_string(THEME_DEFAULT_LINE_HEIGHT));
    json_set(typography, "letterSpacing", json_string(THEME_DEFAULT_LETTER_SPACING));
    static const char *typo_keys[] = {"fontSans", "fontMono", "fontDisplay",
                                      "fontUrl", "baseSize", "lineHeight",
                                      "letterSpacing", NULL};
    if (typo_src) {
        for (size_t i = 0; typo_keys[i]; i++) {
            json_t *v = json_object_get(typo_src, typo_keys[i]);
            if (v && v->type == JSON_STRING && str_nonblank(v->str_val))
                json_set(typography, typo_keys[i], json_string(v->str_val));
        }
    }

    /* Layout */
    json_t *layout_src = get_dict(data, "layout");
    json_t *layout = json_object();
    json_set(layout, "radius", json_string("0.5rem"));
    json_set(layout, "density", json_string("comfortable"));
    if (layout_src) {
        json_t *radius = json_object_get(layout_src, "radius");
        if (radius && radius->type == JSON_STRING && str_nonblank(radius->str_val))
            json_set(layout, "radius", json_string(radius->str_val));
        json_t *density = json_object_get(layout_src, "density");
        if (density && density->type == JSON_STRING &&
            in_list(K_DENSITIES, density->str_val))
            json_set(layout, "density", json_string(density->str_val));
    }

    /* Color overrides */
    json_t *overrides_src = get_dict(data, "colorOverrides");
    json_t *color_overrides = json_object();
    if (overrides_src) {
        for (size_t i = 0; i < json_object_size(overrides_src); i++) {
            const char *k = json_object_get_key_at(overrides_src, i);
            json_t *v = json_object_get_at(overrides_src, i);
            if (k && in_list(K_OVERRIDE_KEYS, k) && v &&
                v->type == JSON_STRING && str_nonblank(v->str_val))
                json_set(color_overrides, k, json_string(v->str_val));
        }
    }

    /* Assets */
    json_t *assets_out = json_object();
    json_t *assets_src = get_dict(data, "assets");
    if (assets_src) {
        for (size_t i = 0; K_NAMED_ASSET_KEYS[i]; i++) {
            json_t *v = json_object_get(assets_src, K_NAMED_ASSET_KEYS[i]);
            if (v && v->type == JSON_STRING && str_nonblank(v->str_val))
                json_set(assets_out, K_NAMED_ASSET_KEYS[i],
                         json_string(v->str_val));
        }
        json_t *custom_src = json_object_get(assets_src, "custom");
        if (custom_src && custom_src->type == JSON_OBJECT) {
            json_t *custom = json_object();
            for (size_t i = 0; i < json_object_size(custom_src); i++) {
                const char *k = json_object_get_key_at(custom_src, i);
                json_t *v = json_object_get_at(custom_src, i);
                if (k && key_isalnumish(k) && v && v->type == JSON_STRING &&
                    str_nonblank(v->str_val))
                    json_set(custom, k, json_string(v->str_val));
            }
            if (json_object_size(custom) > 0)
                json_set(assets_out, "custom", custom);
            else
                json_free(custom);
        }
    }

    /* customCSS */
    json_t *custom_css_v = json_object_get((json_t *)data, "customCSS");
    char *custom_css = NULL;
    if (custom_css_v && custom_css_v->type == JSON_STRING &&
        str_nonblank(custom_css_v->str_val)) {
        size_t l = strlen(custom_css_v->str_val);
        if (l > THEME_CUSTOM_CSS_MAX) l = THEME_CUSTOM_CSS_MAX;
        custom_css = malloc(l + 1);
        memcpy(custom_css, custom_css_v->str_val, l);
        custom_css[l] = '\0';
    }

    /* componentStyles */
    json_t *comp_src = get_dict(data, "componentStyles");
    json_t *component_styles = json_object();
    if (comp_src) {
        for (size_t i = 0; i < json_object_size(comp_src); i++) {
            const char *bucket = json_object_get_key_at(comp_src, i);
            json_t *props = json_object_get_at(comp_src, i);
            if (!bucket || !in_list(K_COMPONENT_BUCKETS, bucket) || !props ||
                props->type != JSON_OBJECT)
                continue;
            json_t *clean = json_object();
            for (size_t j = 0; j < json_object_size(props); j++) {
                const char *prop = json_object_get_key_at(props, j);
                json_t *val = json_object_get_at(props, j);
                if (!prop || !key_isalnumish(prop) || !val) continue;
                if (val->type == JSON_STRING) {
                    if (str_nonblank(val->str_val))
                        json_set(clean, prop, json_string(val->str_val));
                } else if (val->type == JSON_NUMBER) {
                    /* str(value) — int stays int-looking */
                    char buf[64];
                    if (val->num_val == (double)(long long)val->num_val)
                        snprintf(buf, sizeof(buf), "%lld",
                                 (long long)val->num_val);
                    else
                        snprintf(buf, sizeof(buf), "%.17g", val->num_val);
                    json_set(clean, prop, json_string(buf));
                }
                /* bool is isinstance(int) in Python but str(True)="True";
                 * YAML themes never use bools here — treat as skip since
                 * isinstance(value,(str,int,float)) includes bool: emit. */
                else if (val->type == JSON_BOOL) {
                    json_set(clean, prop,
                             json_string(val->bool_val ? "True" : "False"));
                }
            }
            if (json_object_size(clean) > 0)
                json_set(component_styles, bucket, clean);
            else
                json_free(clean);
        }
    }

    /* layoutVariant */
    const char *layout_variant = "standard";
    json_t *lv = json_object_get((json_t *)data, "layoutVariant");
    if (lv && lv->type == JSON_STRING && in_list(K_LAYOUT_VARIANTS, lv->str_val))
        layout_variant = lv->str_val;

    /* Assemble in Python dict-literal order. */
    json_t *result = json_object();
    json_set(result, "name", json_string(name));
    json_t *label_v = json_object_get((json_t *)data, "label");
    json_set(result, "label",
             json_string(label_v && label_v->type == JSON_STRING &&
                                 *label_v->str_val
                             ? label_v->str_val
                             : name));
    json_t *desc_v = json_object_get((json_t *)data, "description");
    json_set(result, "description",
             json_string(desc_v && desc_v->type == JSON_STRING
                             ? desc_v->str_val
                             : ""));
    json_set(result, "palette", palette);
    json_set(result, "typography", typography);
    json_set(result, "layout", layout);
    json_set(result, "layoutVariant", json_string(layout_variant));
    if (json_object_size(color_overrides) > 0)
        json_set(result, "colorOverrides", color_overrides);
    else
        json_free(color_overrides);
    if (json_object_size(assets_out) > 0)
        json_set(result, "assets", assets_out);
    else
        json_free(assets_out);
    if (custom_css) {
        json_set(result, "customCSS", json_string(custom_css));
        free(custom_css);
    }
    if (json_object_size(component_styles) > 0)
        json_set(result, "componentStyles", component_styles);
    else
        json_free(component_styles);
    return result;
}

/* ── builtin list ───────────────────────────────────────────────────────── */
/* PoP: ws_theme_builtin_list @ hermes_cli/web_server.py:get_dashboard_themes */
json_t *ws_theme_builtin_list(void) {
    json_t *arr = json_array();
    for (size_t i = 0; i < N_BUILTIN; i++) {
        json_t *o = json_object();
        json_set(o, "name", json_string(K_BUILTIN_THEMES[i].name));
        json_set(o, "label", json_string(K_BUILTIN_THEMES[i].label));
        json_set(o, "description", json_string(K_BUILTIN_THEMES[i].description));
        json_append(arr, o);
    }
    return arr;
}

/* ── _discover_user_themes ──────────────────────────────────────────────── */

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* PoP: ws_theme_discover_user_themes @ hermes_cli/web_server.py:_discover_user_themes */
json_t *ws_theme_discover_user_themes(const char *home) {
    json_t *result = json_array();
    char dirpath[1024];
    snprintf(dirpath, sizeof(dirpath), "%s/dashboard-themes",
             home && *home ? home : slermes_home());
    DIR *d = opendir(dirpath);
    if (!d) return result;

    /* collect *.yaml, sorted like Path.glob sorted() */
    char **names = NULL;
    size_t n = 0, cap = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        size_t l = strlen(de->d_name);
        if (l < 6 || strcmp(de->d_name + l - 5, ".yaml") != 0) continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 8;
            names = realloc(names, cap * sizeof *names);
        }
        names[n++] = strdup(de->d_name);
    }
    closedir(d);
    qsort(names, n, sizeof *names, cmp_str);

    for (size_t i = 0; i < n; i++) {
        char fpath[1400];
        snprintf(fpath, sizeof(fpath), "%s/%s", dirpath, names[i]);
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse_file(fpath, &err);
        free(err);
        if (!doc) { free(names[i]); continue; }
        char *jstr = yaml_to_json_string(doc, "");
        yaml_free(doc);
        if (!jstr) { free(names[i]); continue; }
        json_t *data = json_parse(jstr, NULL);
        free(jstr);
        if (!data) { free(names[i]); continue; }
        json_t *norm = ws_theme_normalise_definition(data);
        json_free(data);
        if (norm) json_append(result, norm);
        free(names[i]);
    }
    free(names);
    return result;
}

/* ── get_dashboard_themes response ──────────────────────────────────────── */
/* PoP: ws_theme_dashboard_themes_response @ hermes_cli/web_server.py:get_dashboard_themes */
json_t *ws_theme_dashboard_themes_response(const char *home,
                                           const char *active) {
    json_t *themes = ws_theme_builtin_list();
    json_t *user = ws_theme_discover_user_themes(home);
    for (size_t i = 0; i < json_len(user); i++) {
        json_t *t = json_get(user, i);
        const char *nm = json_get_str(t, "name", "");
        bool dup = false;
        for (size_t j = 0; j < N_BUILTIN && !dup; j++)
            if (strcmp(K_BUILTIN_THEMES[j].name, nm) == 0) dup = true;
        /* also dedupe against earlier user themes already appended */
        for (size_t j = N_BUILTIN; j < json_len(themes) && !dup; j++)
            if (strcmp(json_get_str(json_get(themes, j), "name", ""), nm) == 0)
                dup = true;
        if (dup) continue;
        json_t *entry = json_object();
        json_set(entry, "name", json_string(nm));
        json_set(entry, "label", json_string(json_get_str(t, "label", "")));
        json_set(entry, "description",
                 json_string(json_get_str(t, "description", "")));
        json_set(entry, "definition", json_copy(t));
        json_append(themes, entry);
    }
    json_free(user);
    json_t *res = json_object();
    json_set(res, "themes", themes);
    json_set(res, "active", json_string(active && *active ? active : "default"));
    return res;
}

/* ── _render_active_theme_bootstrap_css ─────────────────────────────────── */
/* PoP: ws_theme_render_bootstrap_css @ hermes_cli/web_server.py:_render_active_theme_bootstrap_css */
char *ws_theme_render_bootstrap_css(const char *home, const char *active) {
    if (!active || !*active) return strdup("");
    for (size_t i = 0; i < N_BUILTIN; i++)
        if (strcmp(K_BUILTIN_THEMES[i].name, active) == 0) return strdup("");

    json_t *user = ws_theme_discover_user_themes(home);
    char *out = NULL;
    for (size_t i = 0; i < json_len(user) && !out; i++) {
        json_t *theme = json_get(user, i);
        if (strcmp(json_get_str(theme, "name", ""), active) != 0) continue;
        json_t *palette = json_object_get(theme, "palette");
        json_t *bg = palette ? json_object_get(palette, "background") : NULL;
        json_t *mg = palette ? json_object_get(palette, "midground") : NULL;
        const char *bg_hex = bg ? json_get_str(bg, "hex", "#0a0a0a") : "#0a0a0a";
        const char *mg_hex = mg ? json_get_str(mg, "hex", "#e5e5e5") : "#e5e5e5";
        json_t *typo = json_object_get(theme, "typography");
        const char *font_sans =
            typo ? json_get_str(typo, "fontSans", THEME_DEFAULT_FONT_SANS)
                 : THEME_DEFAULT_FONT_SANS;
        const char *base_size =
            typo ? json_get_str(typo, "baseSize", THEME_DEFAULT_BASE_SIZE)
                 : THEME_DEFAULT_BASE_SIZE;

        char *e_bg = ws_events_theme_css_esc(bg_hex);
        char *e_mg = ws_events_theme_css_esc(mg_hex);
        char *e_fs = ws_events_theme_css_esc(font_sans);
        char *e_bs = ws_events_theme_css_esc(base_size);

        size_t need = strlen(e_bg) + strlen(e_mg) + strlen(e_fs) +
                      strlen(e_bs) + 512;
        out = malloc(need);
        snprintf(out, need,
                 "<style id=\"hermes-theme-bootstrap\">"
                 ":root{"
                 "--background-base:%s;"
                 "--midground-base:%s;"
                 "--theme-font-sans:%s;"
                 "--theme-base-size:%s;"
                 "}"
                 "html,body{background-color:var(--background-base);"
                 "color:var(--midground-base);"
                 "font-family:var(--theme-font-sans);"
                 "font-size:var(--theme-base-size);}"
                 "</style>",
                 e_bg, e_mg, e_fs, e_bs);
        free(e_bg); free(e_mg); free(e_fs); free(e_bs);
    }
    json_free(user);
    return out ? out : strdup("");
}

/* ── font allowlist ─────────────────────────────────────────────────────── */
/* PoP: ws_theme_font_choice_allowed @ hermes_cli/web_server.py:set_dashboard_font */
bool ws_theme_font_choice_allowed(const char *font_id) {
    if (!font_id) return false;
    if (strcmp(font_id, "theme") == 0) return true; /* _FONT_DEFAULT_ID */
    return in_list(K_FONT_CHOICES, font_id);
}
