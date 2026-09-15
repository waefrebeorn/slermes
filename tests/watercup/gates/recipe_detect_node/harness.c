/* gate: recipe_detect_node — traced mirror of the REAL C port
 * src/agent/verify_recipes.c::recipe_detect_node (linked via extra_src).
 * The harness traces the framework/script branch ladder (merged-dependency
 * semantics, mirroring LIVE Python), calls the REAL recipe_detect_node() and
 * builds the V event from its recipe_to_json() output.
 *
 * Twin: LIVE agent/verify/recipes.py::_detect_node_recipe, same rung order.
 *
 * Known port gaps (inputs avoided, documented in gate.json): the port's
 * "Scripts:" evidence enumerates only a FIXED key list in fixed order (LIVE
 * joins every script key in insertion order), and the port always emits a
 * numeric port (0 default) where LIVE yields None when no start script exists.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
#include "verify_recipes.h"

static const char *FRAMEWORKS[] = {
    "next", "@sveltejs/kit", "astro",
    "@remix-run/dev",   /* remix rung also matches @remix-run/react */
    "react-scripts", "vite", NULL
};
/* LIVE kind for each matched dep */
static const char *KINDS[] = {
    "nextjs", "sveltekit", "astro", "remix", "cra", "vite"
};

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s detect_node_recipe <pkg_json>...\n", argv[0]); return 2; }
    const char *root = "/nonexistent-wc-root";   /* deterministic: no lockfiles */
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("detect_node_recipe");
        json_t *pkg = json_parse(argv[i], NULL);

        /* merged deps view (Python deps.update(dependencies)+update(devDependencies)) */
        json_t *dep = pkg && pkg->type == JSON_OBJECT ? json_obj_get(pkg, "dependencies") : NULL;
        json_t *dev = pkg && pkg->type == JSON_OBJECT ? json_obj_get(pkg, "devDependencies") : NULL;
        int fw = -1;
        for (int k = 0; FRAMEWORKS[k]; k++) {
            int hit = ((dep && json_obj_get(dep, FRAMEWORKS[k])) ||
                       (dev && json_obj_get(dev, FRAMEWORKS[k])));
            if (k == 3 && !hit)   /* remix rung: also matches @remix-run/react */
                hit = ((dep && json_obj_get(dep, "@remix-run/react")) ||
                       (dev && json_obj_get(dev, "@remix-run/react")));
            WCUP_BRANCH(hit);
            if (hit) { fw = k; break; }
        }
        (void)KINDS;

        json_t *scripts = pkg && pkg->type == JSON_OBJECT ? json_obj_get(pkg, "scripts") : NULL;
        int has_dev = scripts && json_obj_get(scripts, "dev") != NULL;
        int has_start = scripts && json_obj_get(scripts, "start") != NULL;
        WCUP_BRANCH(has_dev != 0);             /* B6: dev script? */
        if (!has_dev) WCUP_BRANCH(has_start != 0);   /* B7: start script? */

        static const char *BUILD_KEYS[] = {"build", "typecheck", NULL};
        static const char *TEST_KEYS[] = {"test", "check", "lint", NULL};
        for (int k = 0; BUILD_KEYS[k]; k++)
            if (scripts && json_obj_get(scripts, BUILD_KEYS[k])) WCUP_LOOP_TICK();
        for (int k = 0; TEST_KEYS[k]; k++)
            if (scripts && json_obj_get(scripts, TEST_KEYS[k])) WCUP_LOOP_TICK();

        /* ---- real port ---- */
        recipe_t *r = recipe_detect_node(root, argv[i]);
        json_t *rj = r ? json_parse(recipe_to_json(r), NULL) : NULL;
        const char *name = rj ? json_get_str(rj, "name", "") : "";
        const char *kind = rj ? json_get_str(rj, "kind", "") : "";
        if (fw >= 0 && strcmp(kind, KINDS[fw]) != 0) {
            fprintf(stderr, "TRACED LADDER DIVERGES from real port: derived kind=%s real=%s\n",
                    KINDS[fw], kind);
            return 3;
        }

        char canon[4096];
        char boot[512], bld[512], tst[512], ev[1024], st[128], pt[32];
        boot[0] = bld[0] = tst[0] = ev[0] = '\0';
        json_t *arr;
        if (rj && (arr = json_obj_get(rj, "bootstrap")) && arr->type == JSON_ARRAY)
            for (size_t k = 0; k < arr->c.count; k++) {
                json_t *it = arr->c.items[k];
                strncat(boot, k ? "|" : "", sizeof boot - strlen(boot) - 1);
                strncat(boot, it && it->type == JSON_STRING && it->str_val ? it->str_val : "",
                        sizeof boot - strlen(boot) - 1);
            }
        if (rj && (arr = json_obj_get(rj, "build")) && arr->type == JSON_ARRAY)
            for (size_t k = 0; k < arr->c.count; k++) {
                json_t *it = arr->c.items[k];
                strncat(bld, k ? "|" : "", sizeof bld - strlen(bld) - 1);
                strncat(bld, it && it->type == JSON_STRING && it->str_val ? it->str_val : "",
                        sizeof bld - strlen(bld) - 1);
            }
        if (rj && (arr = json_obj_get(rj, "test")) && arr->type == JSON_ARRAY)
            for (size_t k = 0; k < arr->c.count; k++) {
                json_t *it = arr->c.items[k];
                strncat(tst, k ? "|" : "", sizeof tst - strlen(tst) - 1);
                strncat(tst, it && it->type == JSON_STRING && it->str_val ? it->str_val : "",
                        sizeof tst - strlen(tst) - 1);
            }
        json_t *sj = rj ? json_obj_get(rj, "start") : NULL;
        snprintf(st, sizeof st, "%s", (sj && sj->type == JSON_STRING && sj->str_val) ? sj->str_val : "null");
        json_t *pj = rj ? json_obj_get(rj, "port") : NULL;
        if (pj && pj->type == JSON_NUMBER) snprintf(pt, sizeof pt, "%d", (int)pj->num_val);
        else snprintf(pt, sizeof pt, "null");
        if (rj && (arr = json_obj_get(rj, "evidence")) && arr->type == JSON_ARRAY)
            for (size_t k = 0; k < arr->c.count; k++) {
                json_t *it = arr->c.items[k];
                strncat(ev, k ? "~" : "", sizeof ev - strlen(ev) - 1);
                strncat(ev, it && it->type == JSON_STRING && it->str_val ? it->str_val : "",
                        sizeof ev - strlen(ev) - 1);
            }
        const char *rp = rj ? json_get_str(rj, "readinessPath", "/") : "/";
        snprintf(canon, sizeof canon,
                 "name=%s|kind=%s|bootstrap=%s|build=%s|test=%s|start=%s|port=%s|readiness=%s|evidence=%s",
                 name, kind, boot, bld, tst, st, pt, rp ? rp : "/", ev);

        WCUP_VALSTR(canon);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("detect_node_recipe");
        if (rj) json_free(rj);
        if (r) recipe_free(r);
        if (pkg) json_free(pkg);
    }
    return 0;
}
