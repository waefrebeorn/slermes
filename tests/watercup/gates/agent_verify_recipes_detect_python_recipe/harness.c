/* gate: agent_verify_recipes_detect_python_recipe — traced ladder of
 * src/agent/verify_recipes.c::recipe_detect_python (REAL port linked via
 * extra_src and called per input for the value event; branch rungs traced
 * from the same fixture the port reads).
 * LIVE twin: agent/verify/recipes.py::_detect_python_recipe.
 *
 * Input encoding: one JSON object per argv item describing a throwaway
 * fixture dir: {"files": {"pyproject.toml": "...", ...}, "dirs": ["tests"]}.
 * Both sides materialize an identical mkdtemp dir, run detection, clean up.
 *
 * Ladder (twin.py mirrors EXACTLY):
 *   B1: any marker (pyproject|requirements|manage.py|setup.py)
 *   install chain: B2 uv / B3 poetry / B4 pipenv / B5 (pyproject&&!requirements)
 *   B6: is_django -> V(django recipe)
 *   else:
 *     B7: is_fastapi
 *       B8 main.py exists / B9 app.py exists (app_module choice) -> V
 *     else:
 *       B10: is_flask
 *         B11 app.py exists / B12 main.py exists -> V
 *       else:
 *         B13: has_tests -> V(generic python)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include "watercup.h"
#include "json.h"
#include "verify_recipes.h"

static bool rd_text(const char *root, const char *name, char **out)
{
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", root, name);
    FILE *f = fopen(path, "rb");
    *out = NULL;
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    *out = malloc((size_t)n + 1);
    size_t got = fread(*out, 1, (size_t)n, f);
    (*out)[got] = '\0';
    fclose(f);
    return true;
}

static bool fexists(const char *root, const char *name)
{
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", root, name);
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool dexists(const char *root, const char *name)
{
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", root, name);
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* lowercase concat of pyproject + "\n" + requirements (Python .lower()) */

/* lowercase concat of pyproject + "\n" + requirements (Python .lower()) */
static void lower_combined(const char *a, const char *b, char *dst, size_t cap)
{
    size_t k = 0;
    for (const char *s = a ? a : ""; *s && k + 1 < cap; s++)
        dst[k++] = (char)((*s >= 'A' && *s <= 'Z') ? *s + 32 : *s);
    if (k + 1 < cap) dst[k++] = '\n';
    for (const char *s = b ? b : ""; *s && k + 1 < cap; s++)
        dst[k++] = (char)((*s >= 'A' && *s <= 'Z') ? *s + 32 : *s);
    dst[k] = '\0';
}

static const char *detect_pm(const char *root)
{
    static const char *cands[][2] = {
        {"pnpm-lock.yaml","pnpm"},{"bun.lock","bun"},{"bun.lockb","bun"},
        {"yarn.lock","yarn"},{"package-lock.json","npm"},{"uv.lock","uv"},
        {"poetry.lock","poetry"},{"Pipfile.lock","pipenv"},{NULL,NULL}};
    for (int i = 0; cands[i][0]; i++)
        if (fexists(root, cands[i][0])) return cands[i][1];
    return NULL;
}

static void val_from_recipe(const recipe_t *r, char *val, size_t cap)
{
    if (!r) { snprintf(val, cap, "none"); return; }
    char tests[512] = "", ev[1024] = "";
    for (size_t i = 0; r->test && i < r->n_test; i++) {
        if (i) strncat(tests, ",", sizeof tests - strlen(tests) - 1);
        strncat(tests, r->test[i], sizeof tests - strlen(tests) - 1);
    }
    for (size_t i = 0; r->evidence && i < r->n_evidence; i++) {
        if (i) strncat(ev, ";", sizeof ev - strlen(ev) - 1);
        strncat(ev, r->evidence[i], sizeof ev - strlen(ev) - 1);
    }
    snprintf(val, cap, "%s|%s|%s|%s|%s|%d|%s",
             r->kind ? r->kind : "-", r->name ? r->name : "-",
             (r->n_bootstrap && r->bootstrap[0]) ? r->bootstrap[0] : "-",
             tests, r->start ? r->start : "-", r->port, ev);
}

static void rmrf(const char *path)
{
    char cmd[4200];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", path);
    int saved = system(cmd);
    (void)saved;
}

static void trace_one(const char *spec_json)
{
    json_t *spec = spec_json ? json_parse(spec_json, NULL) : NULL;
    char tmpl[] = "/tmp/wcup_recipe_XXXXXX";
    char *root = mkdtemp(tmpl);
    if (!root || !spec || spec->type != JSON_OBJECT) {
        printf("error\n");
        if (spec) json_free(spec);
        return;
    }
    json_t *files = json_obj_get(spec, "files");
    if (files && files->type == JSON_OBJECT)
        for (size_t i = 0; i < files->c.count; i++) {
            char p[4096];
            snprintf(p, sizeof p, "%s/%s", root, files->c.keys[i]);
            FILE *f = fopen(p, "wb");
            if (f && files->c.items[i]->type == JSON_STRING)
                fwrite(files->c.items[i]->str_val, 1,
                       strlen(files->c.items[i]->str_val), f);
            if (f) fclose(f);
        }
    json_t *dirs = json_obj_get(spec, "dirs");
    if (dirs && dirs->type == JSON_ARRAY)
        for (size_t i = 0; i < dirs->c.count; i++) {
            char p[4096];
            snprintf(p, sizeof p, "%s/%s", root, dirs->c.items[i]->str_val);
            mkdir(p, 0755);
        }

    WCUP_BEGIN("detect_python_recipe");

    /* fixture reads (mirrors _read_text/exists rungs in the port) */
    char *pyproject = NULL, *requirements = NULL;
    bool has_pyproject = rd_text(root, "pyproject.toml", &pyproject);
    bool has_requirements = rd_text(root, "requirements.txt", &requirements);
    bool manage_py = fexists(root, "manage.py");
    bool setup_py = fexists(root, "setup.py");

    char lower[8192];
    lower_combined(pyproject, requirements, lower, sizeof lower);

    char val[4096];
    recipe_t *r = NULL;

    if (!(has_pyproject || has_requirements || manage_py || setup_py)) { /* B1 */
        WCUP_BRANCH(0);
        snprintf(val, sizeof val, "none");
    } else {
        WCUP_BRANCH(1);

        /* lockfile walk (L per candidate examined, early exit — mirrors
         * detect_package_manager / recipe_detect_package_manager) */
        static const char *pm_cands[][2] = {
            {"pnpm-lock.yaml","pnpm"},{"bun.lock","bun"},{"bun.lockb","bun"},
            {"yarn.lock","yarn"},{"package-lock.json","npm"},{"uv.lock","uv"},
            {"poetry.lock","poetry"},{"Pipfile.lock","pipenv"},{NULL,NULL}};
        const char *pm = NULL;
        for (int i = 0; pm_cands[i][0]; i++) {
            WCUP_LOOP_TICK();
            if (fexists(root, pm_cands[i][0])) { pm = pm_cands[i][1]; break; }
        }

        /* install chain: uv / poetry / pipenv / (pyproject && !requirements) */
        WCUP_BRANCH(pm && strcmp(pm, "uv") == 0);           /* B2 */
        if (!(pm && strcmp(pm, "uv") == 0)) {
            WCUP_BRANCH(pm && strcmp(pm, "poetry") == 0);   /* B3 */
            if (!(pm && strcmp(pm, "poetry") == 0)) {
                WCUP_BRANCH(pm && strcmp(pm, "pipenv") == 0); /* B4 */
                if (!(pm && strcmp(pm, "pipenv") == 0))
                    WCUP_BRANCH(has_pyproject && !has_requirements); /* B5 */
            }
        }

        bool is_django = manage_py || strstr(lower, "django") != NULL;
        bool is_fastapi = strstr(lower, "fastapi") != NULL ||
                          strstr(lower, "uvicorn") != NULL;
        bool is_flask = strstr(lower, "flask") != NULL;

        if (is_django) {                                    /* B6 */
            WCUP_BRANCH(1);
            WCUP_BRANCH(manage_py);                         /* B7 evidence pick */
        } else {
            WCUP_BRANCH(0);
            if (is_fastapi) {                               /* B8? no: B7 */
                WCUP_BRANCH(1);
                WCUP_BRANCH(fexists(root, "main.py"));      /* app_module */
                WCUP_BRANCH(!fexists(root, "main.py") &&
                            fexists(root, "app.py"));
            } else {
                WCUP_BRANCH(0);
                if (is_flask) {
                    WCUP_BRANCH(1);
                    WCUP_BRANCH(fexists(root, "app.py"));   /* app_module */
                    WCUP_BRANCH(!fexists(root, "app.py") &&
                                fexists(root, "main.py"));
                } else {
                    WCUP_BRANCH(0);
                    WCUP_BRANCH(dexists(root, "tests"));    /* has_tests */
                }
            }
        }

        r = recipe_detect_python(root);   /* REAL linked port for the value */
        val_from_recipe(r, val, sizeof val);
    }

    WCUP_VALSTR(val);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("detect_python_recipe");

    if (r) recipe_free(r);
    free(pyproject);
    free(requirements);
    json_free(spec);
    rmrf(root);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s detect_python_recipe <fixture-json>...\n",
                argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
