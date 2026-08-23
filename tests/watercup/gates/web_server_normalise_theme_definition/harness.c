/* gate: web_server_normalise_theme_definition — drives the REAL
 * src/hermes_cli/web_server_themes.c::ws_theme_normalise_definition (linked
 * via extra_src). LIVE twin:
 * hermes_cli/web_server.py::_normalise_theme_definition.
 *
 * Input: one theme-definition JSON per cup. Rung order (must match twin.py):
 *   B1 data is object / B2 name nonblank str / B3 palette dict /
 *   B4 noiseOpacity present / B5 typography override present /
 *   B6 density valid / B7 colorOverrides kept / B8 assets kept /
 *   B9 customCSS set / B10 componentStyles kept / B11 layoutVariant valid.
 *   V = canonical serialized result ("" when NULL).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"

json_t *ws_theme_normalise_definition(const json_t *data);

static int in_list(const char **list, const char *s) {
    for (int i = 0; list[i]; i++) if (strcmp(list[i], s) == 0) return 1;
    return 0;
}
static int key_alnumish(const char *k) {
    for (const char *p = k; *p; p++)
        if (!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9'))) {
            if (*p=='-'||*p=='_') continue;
            return 0;
        }
    return 1;
}

static const char *L_DENSITY[] = {"compact","comfortable","spacious",NULL};
static const char *L_VARIANT[] = {"standard","cockpit","tiled",NULL};
static const char *L_OVERRIDE[] = {"card","cardForeground","popover","popoverForeground",
    "primary","primaryForeground","secondary","secondaryForeground","muted","mutedForeground",
    "accent","accentForeground","destructive","destructiveForeground","success","warning",
    "border","input","ring",NULL};
static const char *L_ASSET[] = {"bg","hero","logo","crest","sidebar","header",NULL};
static const char *L_BUCKET[] = {"card","header","footer","sidebar","tab","progress",
    "badge","backdrop","page",NULL};

/* does the theme keep at least one entry of this filtered map? */
static int kept_count(json_t *src, const char **keys, int need_str_nonblank) {
    if (!src || src->type != JSON_OBJECT) return -1;
    int n = 0;
    for (size_t i = 0; i < json_object_size(src); i++) {
        const char *k = json_object_get_key_at(src, i);
        json_t *v = json_object_get_at(src, i);
        if (!k) continue;
        if (keys && !in_list(keys, k)) continue;
        if (need_str_nonblank && (!v || v->type != JSON_STRING || !v->str_val[0])) continue;
        n++;
    }
    return n;
}

static int nonblank_str(json_t *v) {
    return v && v->type == JSON_STRING && v->str_val[0] &&
           strspn(v->str_val, " \t\r\n") != strlen(v->str_val);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalise <theme-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("normalise_theme_definition");
        json_t *data = json_parse(argv[i], NULL);
        int is_obj = data && data->type == JSON_OBJECT;
        WCUP_BRANCH(is_obj);                                        /* B1 */
        json_t *name_v = is_obj ? json_object_get(data, "name") : NULL;
        int name_ok = is_obj && name_v && name_v->type == JSON_STRING &&
                      name_v->str_val[0] && strspn(name_v->str_val, " \t\r\n") != strlen(name_v->str_val);
        WCUP_BRANCH(name_ok);                                       /* B2 */
        json_t *palette_src = (is_obj && data) ? json_object_get(data, "palette") : NULL;
        int palette_dict = palette_src && palette_src->type == JSON_OBJECT;
        WCUP_BRANCH(palette_dict);                                  /* B3 */
        json_t *noise = palette_dict ? json_object_get(palette_src, "noiseOpacity")
                                     : (is_obj ? json_object_get(data, "noiseOpacity") : NULL);
        WCUP_BRANCH(noise && noise->type != JSON_NULL);             /* B4 */
        json_t *typo_src = (is_obj && data) ? json_object_get(data, "typography") : NULL;
        static const char *TYPO_KEYS[] = {"fontSans","fontMono","fontDisplay","fontUrl",
                                          "baseSize","lineHeight","letterSpacing",NULL};
        int typo_hit = 0;
        if (typo_src && typo_src->type == JSON_OBJECT)
            for (size_t k = 0; k < json_object_size(typo_src) && !typo_hit; k++)
                if (nonblank_str(json_object_get_at(typo_src, k))) typo_hit = 1;
        WCUP_BRANCH(typo_hit);                                      /* B5 */
        json_t *layout_src = (is_obj && data) ? json_object_get(data, "layout") : NULL;
        json_t *density = layout_src && layout_src->type == JSON_OBJECT
                              ? json_object_get(layout_src, "density") : NULL;
        WCUP_BRANCH(density && density->type == JSON_STRING &&
                    in_list(L_DENSITY, density->str_val));          /* B6 */
        json_t *ovr = (is_obj && data) ? json_object_get(data, "colorOverrides") : NULL;
        WCUP_BRANCH(kept_count(ovr, L_OVERRIDE, 1) > 0);            /* B7 */
        json_t *assets = (is_obj && data) ? json_object_get(data, "assets") : NULL;
        int assets_kept = kept_count(assets, L_ASSET, 1);
        if (assets && assets->type == JSON_OBJECT)
            for (size_t k = 0; k < json_object_size(assets); k++) {
                json_t *custom = json_object_get_at(assets, k);
                if (custom && custom->type == JSON_OBJECT && kept_count(custom, NULL, 1) > 0)
                    assets_kept++;
            }
        WCUP_BRANCH(assets_kept > 0);                               /* B8 */
        json_t *css = (is_obj && data) ? json_object_get(data, "customCSS") : NULL;
        WCUP_BRANCH(nonblank_str(css));                             /* B9 */
        json_t *comp = (is_obj && data) ? json_object_get(data, "componentStyles") : NULL;
        int comp_kept = 0;
        if (comp && comp->type == JSON_OBJECT)
            for (size_t k = 0; k < json_object_size(comp); k++) {
                const char *bucket = json_object_get_key_at(comp, k);
                json_t *props = json_object_get_at(comp, k);
                if (!bucket || !in_list(L_BUCKET, bucket) ||
                    !props || props->type != JSON_OBJECT) continue;
                for (size_t j = 0; j < json_object_size(props); j++) {
                    json_t *val = json_object_get_at(props, j);
                    const char *prop = json_object_get_key_at(props, j);
                    if (prop && val && (val->type == JSON_NUMBER ||
                        ((val->type == JSON_STRING) && nonblank_str(val))))
                        { comp_kept++; break; }
                }
            }
        WCUP_BRANCH(comp_kept > 0);                                 /* B10 */
        json_t *lv = (is_obj && data) ? json_object_get(data, "layoutVariant") : NULL;
        WCUP_BRANCH(lv && lv->type == JSON_STRING && in_list(L_VARIANT, lv->str_val)); /* B11 */

        json_t *res = ws_theme_normalise_definition(is_obj ? data : NULL);
        char *ser = res ? json_serialize(res) : NULL;
        char valbuf[8192];
        snprintf(valbuf, sizeof valbuf, "%s", ser ? ser : "");
        WCUP_VALSTR(valbuf);                                        /* V */
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("normalise_theme_definition");
    }
    return 0;
}
