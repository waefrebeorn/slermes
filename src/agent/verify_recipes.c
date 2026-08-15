/*
 * port_agent_verify_recipes.c — Faithful C11 port of agent/verify/recipes.py
 *
 * Static run-recipe detection for project verification. Mirrors grok-cli's
 * inferFallbackRecipe: package.json (lockfile-aware package manager +
 * framework detection) wins, then Python / Go / Rust / Java, then Makefile
 * and docker-compose fallbacks.
 *
 * The Recipe dataclass is modeled as the opaque-ish recipe_t (declared in
 * verify_recipes.h with owned char ptr and char-ptr-array fields). All
 * strings are strdup'd so recipe_free can own them.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>
#include "json.h"
#include "verify_recipes.h"

/* ---- small helpers ---- */

static bool file_exists(const char *root, const char *name)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", root, name);
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool dir_exists(const char *root, const char *name)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", root, name);
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static char *read_file_text(const char *root, const char *name)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", root, name);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

/* ---- Recipe construction / destruction ---- */

recipe_t *recipe_new(void)
{
    recipe_t *r = calloc(1, sizeof(*r));
    return r;
}

static char *sds(const char *s) { return s ? strdup(s ? s : "") : strdup(""); }
static char *sdstrim(const char *s) {
    if (!s) return strdup("");
    char *d = strdup(s);
    char *p = d;
    while (*p == ' ' || *p == '\t') p++;
    if (p != d) memmove(d, p, strlen(p) + 1);
    size_t n = strlen(d);
    while (n > 0 && (d[n-1] == ' ' || d[n-1] == '\t')) d[--n] = '\0';
    return d;
}

void recipe_add_bootstrap(recipe_t *r, const char *cmd)
{
    if (!r || !cmd || !*cmd) return;
    r->bootstrap = realloc(r->bootstrap, sizeof(char*) * (r->n_bootstrap + 2));
    r->bootstrap[r->n_bootstrap++] = strdup(cmd);
    r->bootstrap[r->n_bootstrap] = NULL;
}

void recipe_add_build(recipe_t *r, const char *cmd)
{
    if (!r || !cmd || !*cmd) return;
    r->build = realloc(r->build, sizeof(char*) * (r->n_build + 2));
    r->build[r->n_build++] = strdup(cmd);
    r->build[r->n_build] = NULL;
}

void recipe_add_test(recipe_t *r, const char *cmd)
{
    if (!r || !cmd || !*cmd) return;
    r->test = realloc(r->test, sizeof(char*) * (r->n_test + 2));
    r->test[r->n_test++] = strdup(cmd);
    r->test[r->n_test] = NULL;
}

void recipe_add_evidence(recipe_t *r, const char *msg)
{
    if (!r || !msg || !*msg) return;
    r->evidence = realloc(r->evidence, sizeof(char*) * (r->n_evidence + 2));
    r->evidence[r->n_evidence++] = strdup(msg);
    r->evidence[r->n_evidence] = NULL;
}

void recipe_set_start(recipe_t *r, const char *cmd)
{
    if (!r) return;
    free(r->start);
    r->start = cmd && *cmd ? strdup(cmd) : NULL;
}

void recipe_set_port(recipe_t *r, int p)
{
    if (r) r->port = p;
}

void recipe_set_readiness(recipe_t *r, const char *path)
{
    if (!r) return;
    free(r->readiness_path);
    r->readiness_path = sdstrim(path);
}

void recipe_set_kind(recipe_t *r, const char *k)
{
    if (!r) return;
    free(r->kind);
    r->kind = sdstrim(k);
}

void recipe_set_name(recipe_t *r, const char *n)
{
    if (!r) return;
    free(r->name);
    r->name = sdstrim(n);
}

void recipe_free(recipe_t *r)
{
    if (!r) return;
    free(r->name); free(r->kind); free(r->start); free(r->readiness_path);
    for (size_t i = 0; i < r->n_bootstrap; i++) free(r->bootstrap[i]);
    free(r->bootstrap);
    for (size_t i = 0; i < r->n_build; i++) free(r->build[i]);
    free(r->build);
    for (size_t i = 0; i < r->n_test; i++) free(r->test[i]);
    free(r->test);
    for (size_t i = 0; i < r->n_evidence; i++) free(r->evidence[i]);
    free(r->evidence);
    free(r);
}

/* ---- package.json / package manager ---- */

/* PoP: recipe_read_package_json @ agent/verify/recipes.py:_read_package_json */
char *recipe_read_package_json(const char *root)
{
    char *raw = read_file_text(root, "package.json");
    if (!raw) return NULL;
    char *err = NULL;
    json_t *j = json_parse(raw, &err);
    free(raw);
    if (err) { free(err); return NULL; }
    if (!j || j->type != JSON_OBJECT) { json_free(j); return NULL; }
    /* keep canonical JSON text */
    /* Use the parsed form: re-serialize is overkill; return the raw text
     * re-parsed+serialized? Python returns the dict. C returns parsed JSON
     * text so callers can parse it. */
    char *out = json_serialize(j);
    json_free(j);
    return out;
}

/* PoP: recipe_detect_package_manager @ agent/verify/recipes.py:detect_package_manager */
char *recipe_detect_package_manager(const char *root)
{
    static const char *candidates[][2] = {
        {"pnpm-lock.yaml", "pnpm"},
        {"bun.lock", "bun"},
        {"bun.lockb", "bun"},
        {"yarn.lock", "yarn"},
        {"package-lock.json", "npm"},
        {"uv.lock", "uv"},
        {"poetry.lock", "poetry"},
        {"Pipfile.lock", "pipenv"},
        {NULL, NULL},
    };
    for (int i = 0; candidates[i][0]; i++) {
        if (file_exists(root, candidates[i][0]))
            return strdup(candidates[i][1]);
    }
    return NULL;
}

/* ---- port inference ---- */

/* regex-free: scan for --port <n> or -p <n>, then PORT=<n> */
/* PoP: recipe_infer_port_from_command @ agent/verify/recipes.py:_infer_port_from_command */
int recipe_infer_port_from_command(const char *command)
{
    if (!command || !*command) return 0;
    /* --port <n> */
    const char *p = strstr(command, "--port");
    if (!p) p = strstr(command, "-p");
    if (p && p > command && isalnum((unsigned char)p[-1])) p = NULL;
    if (p) {
        p += strspn(p, "-pP\r\t ");
        if (*p == ' ' || *p == '\t') p++;
        char *end = NULL;
        long v = strtol(p, &end, 10);
        if (end != p && v > 0 && v < 65536) return (int)v;
    }
    /* PORT=<n> */
    const char *pp = command;
    while ((pp = strstr(pp, "PORT=")) != NULL) {
        pp += 5;
        char *end = NULL;
        long v = strtol(pp, &end, 10);
        if (end != pp && v > 0 && v < 65536) return (int)v;
        pp = end;
    }
    return 0;
}

/* PoP: OSError @ agent/verify/recipes.py:_read_text */
/* Read a file's UTF-8 text, or NULL on OSError (mirrors _read_text). */
char *recipe_read_text(const char *root, const char *name)
{
    return read_file_text(root, name);
}

/* PoP: recipe_dedupe @ agent/verify/recipes.py:_dedupe */
/* Dedupe a list of strings, dropping blanks, preserving first-seen order.
 * Returns a NULL-terminated char** (caller frees with the normal helper). */
char **recipe_dedupe(const char *const *values, size_t n)
{
    if (!values) return NULL;
    char **out = NULL;
    size_t cnt = 0;
    for (size_t i = 0; i < n; i++) {
        if (!values[i]) continue;
        const char *v = values[i];
        while (*v == ' ' || *v == '\t') v++;
        if (!*v) continue;
        /* strip trailing */
        char *trimmed = strdup(v);
        size_t len = strlen(trimmed);
        while (len > 0 && (trimmed[len-1] == ' ' || trimmed[len-1] == '\t')) trimmed[--len] = '\0';
        if (!*trimmed) { free(trimmed); continue; }
        bool dup = false;
        for (size_t j = 0; j < cnt; j++) {
            if (strcmp(out[j], trimmed) == 0) { dup = true; break; }
        }
        if (!dup) {
            out = realloc(out, sizeof(char*) * (cnt + 2));
            out[cnt++] = trimmed;
            out[cnt] = NULL;
        } else {
            free(trimmed);
        }
    }
    return out;
}

/* PoP: recipe_script_runner @ agent/verify/recipes.py:_script_runner */
/* Build the package-manager script-run invocation for an entry script. */
char *recipe_script_runner(const char *package_manager, const char *entry)
{
    if (!entry) return NULL;
    const char *pm = package_manager ? package_manager : "npm";
    if (strcmp(pm, "pnpm") == 0) { char *r=NULL; asprintf(&r,"pnpm %s",entry); return r; }
    if (strcmp(pm, "bun") == 0) { char *r=NULL; asprintf(&r,"bun run %s",entry); return r; }
    if (strcmp(pm, "yarn") == 0) { char *r=NULL; asprintf(&r,"yarn %s",entry); return r; }
    char *r=NULL; asprintf(&r, "npm run %s", entry); return r;
}

/* PoP: names @ agent/verify/recipes.py:_parse_make_targets */
/* Parse Makefile target names: lines matching ^[A-Za-z0-9_.-]+:(space|\$).
 * Returns a NULL-terminated char** of target names (caller frees). */
char **recipe_parse_make_targets(const char *raw)
{
    if (!raw) return NULL;
    char **targets = NULL;
    size_t cnt = 0;
    const char *line = raw;
    while (line && *line) {
        const char *nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        /* copy line (no trailing \r) */
        char *copy = malloc(len + 1);
        size_t cl = 0;
        for (size_t i = 0; i < len; i++) {
            if (line[i] != '\r') copy[cl++] = line[i];
        }
        copy[cl] = '\0';
        /* match ^([A-Za-z0-9_.-]+):(\s|\$) */
        char *p = copy;
        while (*p == ' ' || *p == '\t') p++;
        char *colon = strchr(p, ':');
        if (colon && colon > p) {
            char name[256];
            size_t namelen = (size_t)(colon - p);
            if (namelen < sizeof(name)) {
                memcpy(name, p, namelen);
                name[namelen] = '\0';
                bool valid = name[0] != '\0';
                for (size_t i = 0; i < namelen; i++) {
                    char c = name[i];
                    if (!isalnum((unsigned char)c) && c != '_' && c != '.' && c != '-') { valid = false; break; }
                }
                char after = colon[1];
                if (valid && (after == ' ' || after == '\t' || after == '\0')) {
                    targets = realloc(targets, sizeof(char*) * (cnt + 2));
                    targets[cnt++] = strdup(name);
                    targets[cnt] = NULL;
                }
            }
        }
        free(copy);
        if (!nl) break;
        line = nl + 1;
    }
    return targets;
}

/* ---- Node ---- */

static char *script_runner(const char *pm, const char *entry)
{
    if (!entry) return NULL;
    if (strcmp(pm ? pm : "npm", "pnpm") == 0) { char *r=NULL; asprintf(&r,"pnpm %s",entry); return r; }
    if (strcmp(pm ? pm : "npm", "bun") == 0) { char *r=NULL; asprintf(&r,"bun run %s",entry); return r; }
    if (strcmp(pm ? pm : "npm", "yarn") == 0) { char *r=NULL; asprintf(&r,"yarn %s",entry); return r; }
    char *r=NULL; asprintf(&r, "npm run %s", entry); return r;
}

/* PoP: recipe_detect_node @ agent/verify/recipes.py:_detect_node_recipe */
recipe_t *recipe_detect_node(const char *root, const char *pkg_json)
{
    recipe_t *r = recipe_new();
    recipe_set_kind(r, "node");
    recipe_set_name(r, "Node.js app");
    char *pm = recipe_detect_package_manager(root);
    char *pm_used = pm ? pm : strdup("npm");
    char *install = NULL;
    if (strcmp(pm_used, "pnpm") == 0) install = strdup("pnpm install");
    else if (strcmp(pm_used, "bun") == 0) install = strdup("bun install");
    else if (strcmp(pm_used, "yarn") == 0) install = strdup("yarn install");
    else install = strdup("npm install");
    recipe_add_bootstrap(r, install);

    /* extract scripts */
    json_t *pkg = pkg_json ? json_parse(pkg_json, NULL) : NULL;
    bool has_dev = false, has_start = false, has_build = false,
         has_typecheck = false, has_test = false, has_check = false,
         has_lint = false;
    char first_framework[64] = "";
    if (pkg && pkg->type == JSON_OBJECT) {
        json_t *deps = json_obj_get(pkg, "dependencies");
        json_t *devdeps = json_obj_get(pkg, "devDependencies");
        json_t *scripts = json_obj_get(pkg, "scripts");
        if (scripts && scripts->type == JSON_OBJECT) {
            has_dev = json_obj_get(scripts, "dev") != NULL;
            has_start = json_obj_get(scripts, "start") != NULL;
            has_build = json_obj_get(scripts, "build") != NULL;
            has_typecheck = json_obj_get(scripts, "typecheck") != NULL;
            has_test = json_obj_get(scripts, "test") != NULL;
            has_check = json_obj_get(scripts, "check") != NULL;
            has_lint = json_obj_get(scripts, "lint") != NULL;
        }
        /* framework detection via deps keys */
        if (deps && deps->type == JSON_OBJECT) {
            /* iterate keys for framework deps */
            /* libjson json_t for objects: we check specific deps */
            if (json_obj_get(deps, "next") != NULL) snprintf(first_framework, sizeof(first_framework), "nextjs");
            else if (json_obj_get(deps, "@sveltejs/kit") != NULL) snprintf(first_framework, sizeof(first_framework), "sveltekit");
            else if (json_obj_get(deps, "astro") != NULL) snprintf(first_framework, sizeof(first_framework), "astro");
            else if (json_obj_get(deps, "@remix-run/dev") != NULL) snprintf(first_framework, sizeof(first_framework), "remix");
            else if (json_obj_get(deps, "@remix-run/react") != NULL) snprintf(first_framework, sizeof(first_framework), "remix");
            else if (json_obj_get(deps, "react-scripts") != NULL) snprintf(first_framework, sizeof(first_framework), "cra");
            else if (json_obj_get(deps, "vite") != NULL) snprintf(first_framework, sizeof(first_framework), "vite");
        }
        if (!*first_framework && devdeps && devdeps->type == JSON_OBJECT) {
            if (json_obj_get(devdeps, "next") != NULL) snprintf(first_framework, sizeof(first_framework), "nextjs");
            else if (json_obj_get(devdeps, "@sveltejs/kit") != NULL) snprintf(first_framework, sizeof(first_framework), "sveltekit");
            else if (json_obj_get(devdeps, "astro") != NULL) snprintf(first_framework, sizeof(first_framework), "astro");
            else if (json_obj_get(devdeps, "@remix-run/dev") != NULL) snprintf(first_framework, sizeof(first_framework), "remix");
            else if (json_obj_get(devdeps, "react-scripts") != NULL) snprintf(first_framework, sizeof(first_framework), "cra");
            else if (json_obj_get(devdeps, "vite") != NULL) snprintf(first_framework, sizeof(first_framework), "vite");
        }
    }

    const char *label = "Node.js app";
    int default_port = 0;
    if (strcmp(first_framework, "nextjs") == 0) { label = "Next.js"; default_port = 3000; recipe_set_kind(r, "nextjs"); }
    else if (strcmp(first_framework, "sveltekit") == 0) { label = "SvelteKit"; default_port = 5173; recipe_set_kind(r, "sveltekit"); }
    else if (strcmp(first_framework, "astro") == 0) { label = "Astro"; default_port = 4321; recipe_set_kind(r, "astro"); }
    else if (strcmp(first_framework, "remix") == 0) { label = "Remix"; default_port = 3000; recipe_set_kind(r, "remix"); }
    else if (strcmp(first_framework, "cra") == 0) { label = "Create React App"; default_port = 3000; recipe_set_kind(r, "cra"); }
    else if (strcmp(first_framework, "vite") == 0) { label = "Vite"; default_port = 5173; recipe_set_kind(r, "vite"); }
    recipe_set_name(r, label);

    const char *start_script = has_dev ? "dev" : (has_start ? "start" : NULL);
    char *start_body = NULL;
    if (start_script && pkg && pkg->type == JSON_OBJECT) {
        json_t *scripts = json_obj_get(pkg, "scripts");
        if (scripts) {
            json_t *sb = json_obj_get(scripts, start_script);
            if (sb && sb->type == JSON_STRING) start_body = sb->str_val;
        }
    }
    char *start_cmd = start_script ? script_runner(pm_used, start_script) : NULL;
    if (start_cmd) recipe_set_start(r, start_cmd);

    int port = 0;
    if (start_body) port = recipe_infer_port_from_command(start_body);
    if (!port) port = default_port;
    recipe_set_port(r, port);

    if (has_build) { char *c = script_runner(pm_used, "build"); recipe_add_build(r, c); free(c); }
    if (has_typecheck) { char *c = script_runner(pm_used, "typecheck"); recipe_add_build(r, c); free(c); }
    if (has_test) { char *c = script_runner(pm_used, "test"); recipe_add_test(r, c); free(c); }
    if (has_check) { char *c = script_runner(pm_used, "check"); recipe_add_test(r, c); free(c); }
    if (has_lint) { char *c = script_runner(pm_used, "lint"); recipe_add_test(r, c); free(c); }

    char ev[256];
    recipe_add_evidence(r, "Detected package.json");
    if (pm) { snprintf(ev, sizeof(ev), "Package manager: %s", pm); recipe_add_evidence(r, ev); }
    /* Scripts list */
    {
        char slist[512] = "";
        if (pkg && pkg->type == JSON_OBJECT) {
            json_t *scripts = json_obj_get(pkg, "scripts");
            if (scripts && scripts->type == JSON_OBJECT) {
                /* enumerate keys */
                /* libjson objects: we don't have key-iteration in the minimal
                 * API; emit the known scripts we matched instead */
                char *names[8] = {"dev","start","build","typecheck","test","check","lint",NULL};
                for (int i = 0; names[i]; i++) {
                    if (json_obj_get(scripts, names[i]) != NULL) {
                        if (slist[0]) strncat(slist, ", ", sizeof(slist)-strlen(slist)-1);
                        strncat(slist, names[i], sizeof(slist)-strlen(slist)-1);
                    }
                }
            }
        }
        snprintf(ev, sizeof(ev), "Scripts: %s", slist[0] ? slist : "(none)");
        recipe_add_evidence(r, ev);
    }

    if (pkg) json_free(pkg);
    free(pm_used); free(install); free(start_cmd);
    return r;
}

/* ---- Python ---- */

/* PoP: recipe_detect_python @ agent/verify/recipes.py:_detect_python_recipe */
recipe_t *recipe_detect_python(const char *root)
{
    char *pyproject = read_file_text(root, "pyproject.toml");
    char *requirements = read_file_text(root, "requirements.txt");
    bool setup_py = file_exists(root, "setup.py");
    bool manage_py = file_exists(root, "manage.py");
    if (!pyproject && !requirements && !manage_py && !setup_py) {
        free(pyproject); free(requirements); return NULL;
    }
    char *pm = recipe_detect_package_manager(root);
    char *lower_combined = NULL;
    {
        size_t need = (pyproject ? strlen(pyproject) : 0) + (requirements ? strlen(requirements) : 0) + 1;
        lower_combined = malloc(need);
        lower_combined[0] = '\0';
        if (pyproject) strcat(lower_combined, pyproject);
        if (requirements) strcat(lower_combined, requirements);
        for (char *c = lower_combined; *c; c++) *c = tolower((unsigned char)*c);
    }
    bool is_django = manage_py || strstr(lower_combined, "django") != NULL;
    bool is_fastapi = strstr(lower_combined, "fastapi") != NULL || strstr(lower_combined, "uvicorn") != NULL;
    bool is_flask = strstr(lower_combined, "flask") != NULL;
    bool has_tests = dir_exists(root, "tests");

    char *install = NULL;
    if (strcmp(pm ? pm : "", "uv") == 0) install = strdup("uv sync");
    else if (strcmp(pm ? pm : "", "poetry") == 0) install = strdup("poetry install");
    else if (strcmp(pm ? pm : "", "pipenv") == 0) install = strdup("pipenv install");
    else if (pyproject && !requirements) install = strdup("pip install -e .");
    else install = strdup("pip install -r requirements.txt");

    recipe_t *r = recipe_new();

    if (is_django) {
        recipe_set_kind(r, "django");
        recipe_set_name(r, "Django app");
        recipe_add_bootstrap(r, install);
        recipe_add_test(r, "python manage.py test");
        recipe_set_start(r, "python manage.py runserver 0.0.0.0:8000");
        recipe_set_port(r, 8000);
        recipe_set_readiness(r, "/");
        if (manage_py) recipe_add_evidence(r, "Detected manage.py");
        else recipe_add_evidence(r, "Detected Django dependency");
        if (pyproject) recipe_add_evidence(r, "Detected pyproject.toml");
        free(install); free(pm); free(pyproject); free(requirements); free(lower_combined);
        return r;
    }
    if (is_fastapi) {
        const char *app_module = "main:app";
        if (file_exists(root, "main.py")) app_module = "main:app";
        else if (file_exists(root, "app.py")) app_module = "app:app";
        char start[128];
        snprintf(start, sizeof(start), "uvicorn %s --host 0.0.0.0 --port 8000", app_module);
        recipe_set_kind(r, "fastapi");
        recipe_set_name(r, "FastAPI app");
        recipe_add_bootstrap(r, install);
        if (has_tests) recipe_add_test(r, "pytest");
        recipe_set_start(r, start);
        recipe_set_port(r, 8000);
        recipe_set_readiness(r, "/");
        recipe_add_evidence(r, "Detected Python project");
        recipe_add_evidence(r, "Detected FastAPI/Uvicorn dependency");
        free(install); free(pm); free(pyproject); free(requirements); free(lower_combined);
        return r;
    }
    if (is_flask) {
        const char *app_module = "app.py";
        if (file_exists(root, "app.py")) app_module = "app.py";
        else if (file_exists(root, "main.py")) app_module = "main.py";
        char start[128];
        snprintf(start, sizeof(start), "flask --app %s run --host 0.0.0.0 --port 5000", app_module);
        recipe_set_kind(r, "flask");
        recipe_set_name(r, "Flask app");
        recipe_add_bootstrap(r, install);
        if (has_tests) recipe_add_test(r, "pytest");
        recipe_set_start(r, start);
        recipe_set_port(r, 8000);
        recipe_set_readiness(r, "/");
        recipe_add_evidence(r, "Detected Python project");
        recipe_add_evidence(r, "Detected Flask dependency");
        free(install); free(pm); free(pyproject); free(requirements); free(lower_combined);
        return r;
    }
    /* generic python */
    recipe_set_kind(r, "python");
    recipe_set_name(r, "Python project");
    recipe_add_bootstrap(r, install);
    if (has_tests) recipe_add_test(r, "pytest");
    else recipe_add_test(r, "python -m unittest discover");
    recipe_set_readiness(r, "/");
    recipe_add_evidence(r, "Detected Python project");
    free(install); free(pm); free(pyproject); free(requirements); free(lower_combined);
    return r;
}

/* ---- Go / Rust / Java ---- */

/* PoP: recipe_detect_go @ agent/verify/recipes.py:_detect_go_recipe */
recipe_t *recipe_detect_go(const char *root)
{
    if (!file_exists(root, "go.mod")) return NULL;
    recipe_t *r = recipe_new();
    recipe_set_kind(r, "go");
    recipe_set_name(r, "Go project");
    recipe_add_build(r, "go build ./...");
    recipe_add_test(r, "go test ./...");
    if (file_exists(root, "main.go")) recipe_set_start(r, "go run .");
    recipe_set_readiness(r, "/");
    recipe_add_evidence(r, "Detected go.mod");
    return r;
}

/* PoP: recipe_detect_rust @ agent/verify/recipes.py:_detect_rust_recipe */
recipe_t *recipe_detect_rust(const char *root)
{
    if (!file_exists(root, "Cargo.toml")) return NULL;
    recipe_t *r = recipe_new();
    recipe_set_kind(r, "rust");
    recipe_set_name(r, "Rust project");
    recipe_add_build(r, "cargo build");
    recipe_add_test(r, "cargo test");
    if (file_exists(root, "src/main.rs")) recipe_set_start(r, "cargo run");
    recipe_set_readiness(r, "/");
    recipe_add_evidence(r, "Detected Cargo.toml");
    return r;
}

/* PoP: recipe_detect_java @ agent/verify/recipes.py:_detect_java_recipe */
recipe_t *recipe_detect_java(const char *root)
{
    if (file_exists(root, "pom.xml")) {
        recipe_t *r = recipe_new();
        recipe_set_kind(r, "maven");
        recipe_set_name(r, "Maven project");
        recipe_add_build(r, "mvn package");
        recipe_add_test(r, "mvn test");
        recipe_set_readiness(r, "/");
        recipe_add_evidence(r, "Detected pom.xml");
        return r;
    }
    bool has_gradle = file_exists(root, "build.gradle") || file_exists(root, "build.gradle.kts");
    if (has_gradle) {
        recipe_t *r = recipe_new();
        const char *gradle = file_exists(root, "gradlew") ? "./gradlew" : "gradle";
        recipe_set_kind(r, "gradle");
        recipe_set_name(r, "Gradle project");
        char b[64], t[64];
        snprintf(b, sizeof(b), "%s build", gradle);
        snprintf(t, sizeof(t), "%s test", gradle);
        recipe_add_build(r, b);
        recipe_add_test(r, t);
        recipe_set_readiness(r, "/");
        recipe_add_evidence(r, "Detected Gradle build file");
        return r;
    }
    return NULL;
}

/* ---- Makefile ---- */

/* PoP: recipe_detect_make @ agent/verify/recipes.py:_detect_make_recipe */
recipe_t *recipe_detect_make(const char *root)
{
    char *makefile = read_file_text(root, "Makefile");
    if (!makefile) return NULL;
    /* parse targets: lines ^[A-Za-z0-9_.-]+:(space|\$) */
    char *install = NULL, *build = NULL, *test = NULL, *run = NULL;
    char targets_line[512] = "";
    char *line = makefile;
    while (*line) {
        char *nl = strchr(line, '\n');
        char saved = nl ? '\n' : '\0';
        if (nl) *nl = '\0';
        char *c = line;
        while (*c == ' ' || *c == '\t') c++;
        /* check first token */
        char *colon = strchr(c, ':');
        if (colon && colon > c) {
            *colon = '\0';
            char *name = c;
            size_t nlen = strlen(name);
            if (nlen > 0) {
                char *e = name + nlen - 1;
                while (e > name && (*e == ' ' || *e == '\t' || *e == '\r')) *e-- = '\0', nlen--;
                /* valid target name chars */
                bool valid = true;
                for (char *p = name; *p; p++) {
                    if (!isalnum((unsigned char)*p) && *p != '_' && *p != '.' && *p != '-') { valid = false; break; }
                }
                if (valid && *name) {
                    if (strcmp(name, "install") == 0 || strcmp(name, "setup") == 0 || strcmp(name, "bootstrap") == 0) install = strdup(name);
                    else if (strcmp(name, "build") == 0 || strcmp(name, "compile") == 0) build = strdup(name);
                    else if (strcmp(name, "test") == 0 || strcmp(name, "check") == 0) test = strdup(name);
                    else if (strcmp(name, "run") == 0 || strcmp(name, "start") == 0 || strcmp(name, "serve") == 0 || strcmp(name, "dev") == 0) run = strdup(name);
                    if (targets_line[0]) strncat(targets_line, ", ", sizeof(targets_line) - strlen(targets_line) - 1);
                    strncat(targets_line, name, sizeof(targets_line) - strlen(targets_line) - 1);
                }
            }
        }
        if (!saved) break;
        line = nl + 1;
        *nl = saved;
    }
    recipe_t *r = recipe_new();
    recipe_set_kind(r, "make");
    recipe_set_name(r, "Makefile-driven project");
    if (install) { char c[64]; snprintf(c, sizeof(c), "make %s", install); recipe_add_bootstrap(r, c); free(install); }
    if (build) { char c[64]; snprintf(c, sizeof(c), "make %s", build); recipe_add_build(r, c); free(build); }
    if (test) { char c[64]; snprintf(c, sizeof(c), "make %s", test); recipe_add_test(r, c); free(test); }
    if (run) { char c[64]; snprintf(c, sizeof(c), "make %s", run); recipe_set_start(r, c); free(run); }
    recipe_set_readiness(r, "/");
    char ev[512];
    snprintf(ev, sizeof(ev), "Detected Makefile");
    recipe_add_evidence(r, ev);
    snprintf(ev, sizeof(ev), "Targets: %s", targets_line[0] ? targets_line : "(none)");
    recipe_add_evidence(r, ev);
    free(makefile);
    return r;
}

/* ---- docker-compose ---- */

/* PoP: recipe_detect_compose @ agent/verify/recipes.py:_detect_compose_recipe */
recipe_t *recipe_detect_compose(const char *root)
{
    static const char *files[] = {"docker-compose.yml", "docker-compose.yaml", "compose.yml", "compose.yaml", NULL};
    const char *found = NULL;
    for (int i = 0; files[i]; i++) {
        if (file_exists(root, files[i])) { found = files[i]; break; }
    }
    if (!found) return NULL;
    recipe_t *r = recipe_new();
    recipe_set_kind(r, "compose");
    recipe_set_name(r, "docker-compose project");
    recipe_add_build(r, "docker compose build");
    recipe_set_start(r, "docker compose up");
    recipe_set_readiness(r, "/");
    char ev[128];
    snprintf(ev, sizeof(ev), "Detected %s", found);
    recipe_add_evidence(r, ev);
    return r;
}

/* ---- entry point ---- */

/* PoP: recipe_detect @ agent/verify/recipes.py:detect_recipe */
recipe_t *recipe_detect(const char *root)
{
    char *pkg = recipe_read_package_json(root);
    if (pkg) { recipe_t *r = recipe_detect_node(root, pkg); free(pkg); return r; }
    recipe_t *r = recipe_detect_python(root);
    if (r) return r;
    r = recipe_detect_go(root);
    if (r) return r;
    r = recipe_detect_rust(root);
    if (r) return r;
    r = recipe_detect_java(root);
    if (r) return r;
    r = recipe_detect_make(root);
    if (r) return r;
    return recipe_detect_compose(root);
}

/* ---- from_dict / to_json ---- */

/* as_strings analog: accept a string or a JSON array of strings */
static void ingest_strings(recipe_t *r, void (*setter)(recipe_t *, const char *),
                           char ***arr, size_t *n, const json_t *v)
{
    (void)setter;
    if (!v) return;
    if (v->type == JSON_STRING && v->str_val && *v->str_val) {
        *arr = realloc(*arr, sizeof(char*) * (*n + 2));
        (*arr)[(*n)++] = strdup(v->str_val);
        (*arr)[*n] = NULL;
    } else if (v->type == JSON_ARRAY) {
        size_t cnt = json_len(v);
        for (size_t i = 0; i < cnt; i++) {
            json_t *elt = json_get(v, i);
            if (elt && elt->type == JSON_STRING && elt->str_val && *elt->str_val) {
                *arr = realloc(*arr, sizeof(char*) * (*n + 2));
                (*arr)[(*n)++] = strdup(elt->str_val);
                (*arr)[*n] = NULL;
            }
        }
    }
}

/* PoP: recipe_from_dict @ agent/verify/recipes.py:Recipe.from_dict */
recipe_t *recipe_from_dict(const char *raw)
{
    if (!raw) return NULL;
    char *err = NULL;
    json_t *j = json_parse(raw, &err);
    if (err) free(err);
    if (!j || j->type != JSON_OBJECT) { json_free(j); return NULL; }
    const char *name = json_get_str(j, "name", NULL);
    if (!name || !*name) name = json_get_str(j, "appLabel", NULL);
    if (!name || !*name) { json_free(j); return NULL; }
    recipe_t *r = recipe_new();
    recipe_set_name(r, name);
    const char *kind = json_get_str(j, "kind", NULL);
    if (!kind || !*kind) kind = json_get_str(j, "appKind", NULL);
    recipe_set_kind(r, kind ? kind : "unknown");
    const char *start = json_get_str(j, "start", NULL);
    if (!start || !*start) start = json_get_str(j, "startCommand", NULL);
    recipe_set_start(r, start ? start : "");
    double port_raw = json_get_num(j, "port", 0);
    if (port_raw <= 0) port_raw = json_get_num(j, "startPort", 0);
    recipe_set_port(r, (int)port_raw);
    const char *readiness = json_get_str(j, "readinessPath", NULL);
    if (!readiness || !*readiness) readiness = json_get_str(j, "readiness_path", NULL);
    recipe_set_readiness(r, readiness && *readiness ? readiness : "/");
    json_t *b = json_obj_get(j, "bootstrap");
    if (!b) b = json_obj_get(j, "installCommands");
    ingest_strings(r, NULL, &r->bootstrap, &r->n_bootstrap, b);
    json_t *bd = json_obj_get(j, "build");
    if (!bd) bd = json_obj_get(j, "buildCommands");
    ingest_strings(r, NULL, &r->build, &r->n_build, bd);
    json_t *t = json_obj_get(j, "test");
    if (!t) t = json_obj_get(j, "testCommands");
    ingest_strings(r, NULL, &r->test, &r->n_test, t);
    ingest_strings(r, NULL, &r->evidence, &r->n_evidence, json_obj_get(j, "evidence"));
    json_free(j);
    return r;
}

/* PoP: recipe_to_json @ agent/verify/recipes.py:Recipe.to_dict */
char *recipe_to_json(const recipe_t *r)
{
    json_t *o = json_object();
    json_set(o, "name", json_string(r->name ? r->name : ""));
    json_set(o, "kind", json_string(r->kind ? r->kind : "unknown"));
    json_t *b = json_array();
    for (size_t i = 0; i < r->n_bootstrap; i++) json_append(b, json_string(r->bootstrap[i]));
    json_set(o, "bootstrap", b);
    json_t *bd = json_array();
    for (size_t i = 0; i < r->n_build; i++) json_append(bd, json_string(r->build[i]));
    json_set(o, "build", bd);
    json_t *t = json_array();
    for (size_t i = 0; i < r->n_test; i++) json_append(t, json_string(r->test[i]));
    json_set(o, "test", t);
    json_set(o, "start", json_string(r->start ? r->start : ""));
    json_set(o, "port", json_number((double)(r->port ? r->port : 0)));
    json_set(o, "readinessPath", json_string(r->readiness_path ? r->readiness_path : "/"));
    json_t *e = json_array();
    for (size_t i = 0; i < r->n_evidence; i++) json_append(e, json_string(r->evidence[i]));
    json_set(o, "evidence", e);
    char *s = json_serialize(o);
    json_free(o);
    return s;
}
