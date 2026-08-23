/* gate: gateway_display_resolve_display_setting — traced copy of
 * src/gateway/display_config.c::resolve_display_setting
 * (PoP: gateway/display_config.py::resolve_display_setting).
 *
 * Input encoding: "<platform>|<setting>|<fallback|->|<config_json>"
 *   "-" = NULL fallback / empty platform.
 *
 * Rung ladder (must match twin.py EXACTLY):
 *   B1 display config object exists
 *   B2 platform key nonempty -> B3 platforms obj -> B4 plat obj -> B5 val set
 *   B6 global display.<setting> set
 *   V(resolved value as string; "null" when nothing matched)
 *
 * KNOWN DIVERGENCES avoided by input choice (see gate.json notes):
 *   LIVE has legacy tool_progress_overrides + built-in defaults rungs the C
 *   port lacks; LIVE's normaliser differs on tool_progress/bool-ish settings.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"
#include "json.h"

/* traced copy of normalise_display_value (settings used by this gate only
 * exercise the grouping/reasoning/int/passthrough paths). */
static char *norm_value(const char *setting, const char *value) {
    if (!value) value = "";
    if (!setting) setting = "";
    if (strcmp(setting, "tool_progress") == 0) {
        char *buf = strdup(value);
        for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
        return buf;
    }
    if (strcmp(setting, "tool_progress_grouping") == 0) {
        char *buf = strdup(value);
        for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strcmp(buf, "separate") == 0) return buf;
        free(buf);
        return strdup("accumulate");
    }
    if (strcmp(setting, "reasoning_style") == 0) {
        char *buf = strdup(value);
        for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strcmp(buf, "blockquote") == 0 || strcmp(buf, "subtext") == 0) return buf;
        free(buf);
        return strdup("code");
    }
    if (strcmp(setting, "tool_preview_length") == 0) {
        char *end = NULL;
        long n = strtol(value, &end, 10);
        if (end == value || *end != '\0') return strdup("0");
        char buf[32]; snprintf(buf, sizeof buf, "%ld", n);
        return strdup(buf);
    }
    return strdup(value);
}

static char *render_val(json_t *val, const char *setting) {
    if (val->type == JSON_STRING) return norm_value(setting, val->str_val ? val->str_val : "");
    if (val->type == JSON_NUMBER) { char b[64]; snprintf(b, sizeof b, "%.0f", val->num_val); return strdup(b); }
    if (val->type == JSON_BOOL)   return strdup(val->bool_val ? "true" : "false");
    return NULL;
}


int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_resolve_display_setting <plat|setting|fb|json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char probe[2048]; snprintf(probe, sizeof probe, "%s", argv[i]);
        char *p1 = strchr(probe, '|');
        char *p2 = p1 ? strchr(p1 + 1, '|') : NULL;
        char *p3 = p2 ? strchr(p2 + 1, '|') : NULL;
        if (!p3) { fprintf(stderr, "bad probe\n"); return 2; }
        *p1 = *p2 = *p3 = '\0';
        const char *platform = strcmp(probe, "-") == 0 ? "" : probe;
        const char *setting = p1 + 1;
        const char *fb_s = p2 + 1;
        const char *fb = strcmp(fb_s, "-") == 0 ? NULL : fb_s;
        json_t *cfg = json_parse(p3 + 1, NULL);

        WCUP_BEGIN("resolve_display_setting");
        char *vres = NULL;
        json_t *display = cfg && cfg->type == JSON_OBJECT ? json_obj_get(cfg, "display") : NULL;
        WCUP_BRANCH(display && display->type == JSON_OBJECT);            /* B1 */
        if (!(display && display->type == JSON_OBJECT)) { vres = fb ? strdup(fb) : NULL; goto done; }

        int pk = platform && *platform;
        WCUP_BRANCH(pk);                                                  /* B2 */
        if (!pk) goto global_rung;
        {
            json_t *platforms = json_obj_get(display, "platforms");
            WCUP_BRANCH(platforms && platforms->type == JSON_OBJECT);     /* B3 */
            if (!(platforms && platforms->type == JSON_OBJECT)) goto global_rung;
            json_t *plat = json_obj_get(platforms, platform);
            WCUP_BRANCH(plat && plat->type == JSON_OBJECT);               /* B4 */
            if (!(plat && plat->type == JSON_OBJECT)) goto global_rung;
            json_t *val = json_obj_get(plat, setting);
            WCUP_BRANCH(val != NULL && val->type != JSON_NULL);           /* B5 */
            if (val && val->type != JSON_NULL) { vres = render_val(val, setting); goto done; }
        }
global_rung:;
        json_t *g = json_obj_get(display, setting);
        WCUP_BRANCH(g != NULL && g->type != JSON_NULL);                   /* B6 */
        if (g && g->type != JSON_NULL) { vres = render_val(g, setting); goto done; }
        vres = fb ? strdup(fb) : NULL;
done:;
        WCUP_VALSTR(vres ? vres : "null");
        free(vres);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("resolve_display_setting");
    }
    return 0;
}
