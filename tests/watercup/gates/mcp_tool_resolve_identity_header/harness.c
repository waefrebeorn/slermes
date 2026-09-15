/* gate: mcp_tool_resolve_identity_header
 * Calls the REAL C port src/tools/mcp_tool_pure.c::mcp_resolve_identity_header
 * against LIVE Python tools/mcp_tool.py::_resolve_identity_header.
 * Input: config JSON, e.g. {"identity_header":{"name":"X-User-Id","value":"a"}}
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"

json_t *mcp_resolve_identity_header(const char *server_name, const json_t *config);

/* out-of-path externals (not on traced inputs' path) */
void hermes_log(int lvl, const char *fmt, ...) {(void)lvl; (void)fmt;}
void slermes_home_ensure(void) {}
const char *slermes_home(void) { return "/tmp/wc-fake-hermes-home"; }
const char *gw_active_profile_name(void) { return "default"; }

static void run_one(const char *cfg_json) {
    WCUP_BEGIN("_resolve_identity_header");
    json_t *config = cfg_json && *cfg_json ? json_parse(cfg_json, NULL) : NULL;

    /* Trace LIVE's ladder */
    json_t *resolved = mcp_resolve_identity_header("srv", config);
    {
        json_t *raw = (!config || config->type != JSON_OBJECT)
            ? NULL : json_obj_get(config, "identity_header");
        int has_raw = raw && raw->type != JSON_NULL;
        WCUP_BRANCH(has_raw);
        if (has_raw) {
            WCUP_BRANCH(raw->type == JSON_OBJECT);
            if (raw->type == JSON_OBJECT) {
                json_t *name = json_obj_get(raw, "name");
                int name_ok = name && name->type == JSON_STRING && name->str_val && *name->str_val;
                WCUP_BRANCH(name_ok);
                if (name_ok) {
                    json_t *vf = json_obj_get(raw, "value_from");
                    const char *vfs = (vf && vf->type == JSON_STRING && vf->str_val && *vf->str_val)
                        ? vf->str_val : "static";
                    int is_profile = strcmp(vfs, "profile") == 0;
                    WCUP_BRANCH(is_profile);
                    if (!is_profile) {
                        WCUP_BRANCH(strcmp(vfs, "static") != 0);
                        if (strcmp(vfs, "static") == 0) {
                            json_t *val = json_obj_get(raw, "value");
                            int val_ok = 0;
                            if (val && val->type == JSON_STRING && val->str_val) {
                                /* LIVE: isinstance str and value.strip() truthy */
                                const char *v = val->str_val;
                                while (*v == ' ' || *v == '\t') v++;
                                size_t vlen = strlen(v);
                                while (vlen > 0 && (v[vlen-1] == ' ' || v[vlen-1] == '\t')) vlen--;
                                val_ok = vlen > 0;
                            }
                            WCUP_BRANCH(val_ok);
                        }
                    }
                }
            }
        }
    }

    char res[512];
    if (resolved && resolved->type == JSON_ARRAY && resolved->c.count >= 2 &&
        resolved->c.items[0]->type == JSON_STRING && resolved->c.items[1]->type == JSON_STRING) {
        snprintf(res, sizeof res, "%s|%s",
                 resolved->c.items[0]->str_val, resolved->c.items[1]->str_val);
    } else {
        snprintf(res, sizeof res, "%s", "None");
    }
    WCUP_VALSTR(res);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_resolve_identity_header");
    if (resolved) json_free(resolved);
    if (config) json_free(config);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _resolve_identity_header <config-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
