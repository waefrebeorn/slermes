#include "hermes_json.h"
/*
 * recipes.c — C home for upstream Hermes agent/verify/recipes.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: agent_verify_recipes_pending @ agent/verify/recipes.py:<module>
 */
#include <stddef.h>

void agent_verify_recipes_port_pending(void) { (void)0; }

/* PoP: agent_verify_recipes_u_script_runner @ agent/verify/recipes.py:_script_runner */
/* _SCRIPT_RUNNERS.get(package_manager or "", "npm run {}").format(entry).
 * Input JSON {"package_manager": str|null, "entry": str}. */
char *agent_verify_recipes_u_script_runner(const char *arg) {
    json_t *o = arg ? json_parse(arg, NULL) : NULL;
    const char *tmpl = "npm run {}";
    const char *entry = "";
    if (o && json_is_object(o)) {
        json_t *pm = json_obj_get(o, "package_manager");
        json_t *en = json_obj_get(o, "entry");
        if (en && json_is_string(en) && en->str_val) entry = en->str_val;
        if (pm && json_is_string(pm) && pm->str_val) {
            if (strcmp(pm->str_val, "pnpm") == 0) tmpl = "pnpm {}";
            else if (strcmp(pm->str_val, "bun") == 0) tmpl = "bun run {}";
            else if (strcmp(pm->str_val, "yarn") == 0) tmpl = "yarn {}";
        }
    }
    /* substitute every "{}" occurrence in tmpl (python str.format with one arg
     * replaces each {} field) */
    size_t cap = strlen(tmpl) + strlen(entry) * 2 + 32, w = 0;
    char *out = malloc(cap); out[0] = 0;
    for (const char *p = tmpl; *p; ) {
        if (p[0] == '{' && p[1] == '}') {
            size_t need = w + strlen(entry) + 2;
            if (need > cap) { cap = need * 2; out = realloc(out, cap); }
            memcpy(out + w, entry, strlen(entry)); w += strlen(entry); p += 2;
        } else { out[w++] = *p++; }
    }
    out[w] = 0;
    if (o) json_free(o);
    return out;
}
