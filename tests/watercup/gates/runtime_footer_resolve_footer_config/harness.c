/* gate: runtime_footer_resolve_footer_config — traced harness over the REAL
 * C port src/gateway/runtime_footer.c::resolve_footer_config (linked via
 * extra_src). LIVE twin: gateway/runtime_footer.py::resolve_footer_config.
 * Input encoding per arg: <config_json>|<platform_key> ('|' splits; may omit).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"
#include "hermes_json.h"

json_node_t *resolve_footer_config(json_node_t *user_config,
                                   const char *platform_key);

static void trace_one(json_node_t *user_config, const char *platform_key) {
    WCUP_BEGIN("resolve_footer_config");
    json_node_t *resolved = resolve_footer_config(user_config, platform_key);

    /* mirror the ladder rungs for the trace */
    json_node_t *display = user_config && json_is_object(user_config)
        ? json_object_get(user_config, "display") : NULL;
    WCUP_BRANCH(json_is_object(display));
    bool has_global = false, has_en = false, has_fields = false;
    if (json_is_object(display)) {
        json_node_t *g = json_object_get(display, "runtime_footer");
        has_global = json_is_object(g);
        WCUP_BRANCH(has_global);
        if (has_global) {
            has_en = json_object_get(g, "enabled") != NULL;
            WCUP_BRANCH(has_en);
            json_node_t *f = json_object_get(g, "fields");
            has_fields = json_is_array(f) && json_array_size(f) > 0;
            WCUP_BRANCH(has_fields);
        }
        if (platform_key && platform_key[0]) {
            /* LIVE rung: plat_cfg = (cfg.get("platforms") or {}).get(key);
             * a single object-presence branch. */
            json_node_t *plats = json_object_get(display, "platforms");
            json_node_t *pc = json_is_object(plats)
                ? json_object_get(plats, platform_key) : NULL;
            WCUP_BRANCH(json_is_object(pc));
            json_node_t *pf = json_is_object(pc)
                ? json_object_get(pc, "runtime_footer") : NULL;
            WCUP_BRANCH(json_is_object(pf));
            if (json_is_object(pf)) {
                WCUP_BRANCH(json_object_get(pf, "enabled") != NULL);
                json_node_t *ff = json_object_get(pf, "fields");
                WCUP_BRANCH(json_is_array(ff) && json_array_size(ff) > 0);
            }
        } else {
            /* LIVE: `if platform_key:` — four rungs skipped without events */
        }
    }

    bool enabled = false;
    char fields_buf[512];
    fields_buf[0] = '\0';
    json_node_t *en = json_object_get(resolved, "enabled");
    enabled = json_bool_value(en);
    json_node_t *flds = json_object_get(resolved, "fields");
    if (json_is_array(flds)) {
        size_t n = json_array_size(flds);
        for (size_t i = 0; i < n; i++) {
            if (i) strcat(fields_buf, ",");
            const char *s = json_string_value(json_array_get(flds, i));
            strncat(fields_buf, s ? s : "", sizeof(fields_buf) - strlen(fields_buf) - 1);
        }
    }
    char vbuf[600];
    snprintf(vbuf, sizeof vbuf, "enabled=%s|fields=%s", enabled ? "True" : "False",
             fields_buf);
    WCUP_VALSTR(vbuf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("resolve_footer_config");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resolve_footer_config '<cfg>|<key>'...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[2048];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        char *sep = strchr(buf, '|');
        json_node_t *cfg = NULL;
        const char *key = NULL;
        if (sep) {
            *sep = '\0';
            key = sep + 1;
            if (!key[0]) key = NULL;
        }
        if (buf[0]) cfg = json_parse(buf, NULL);
        trace_one(cfg, key);
    }
    return 0;
}
