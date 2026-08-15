/*
 * port_agent_verify_environment.c — Faithful C11 port of agent/verify/environment.py
 *
 * Manifest I/O: <root>/.hermes/environment.json is the user-editable source
 * of truth for a project's verification recipe. Any read/parse/shape problem
 * degrades to fresh detection instead of breaking `hermes verify`.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "json.h"
#include "verify_recipes.h"
#include "verify_environment.h"

#ifndef MANIFEST_VERSION
#define MANIFEST_VERSION 1
#endif

/* now_iso is declared in hermes_core_types.h / exposed via the agent
 * timestamp port — used for the manifest updatedAt field. */
/* PoP: verify_manifest_path @ agent/verify/environment.py:manifest_path */
char *verify_manifest_path(const char *root)
{
    if (!root || !*root) return NULL;
    char *p = NULL;
    asprintf(&p, "%s/.hermes/environment.json", root);
    return p;
}

static char *read_file_all(const char *path)
{
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

/* PoP: verify_load_manifest @ agent/verify/environment.py:load_manifest */
recipe_t *verify_load_manifest(const char *root)
{
    char *path = verify_manifest_path(root);
    if (!path) return NULL;
    char *raw = read_file_all(path);
    free(path);
    if (!raw) return NULL;
    char *err = NULL;
    json_t *manifest = json_parse(raw, &err);
    free(raw);
    if (err) free(err);
    if (!manifest) return NULL;
    recipe_t *recipe = NULL;
    if (manifest->type == JSON_OBJECT) {
        json_t *r = json_obj_get(manifest, "recipe");
        if (!r) r = manifest;
        char *s = json_serialize(r);
        if (s) {
            recipe = recipe_from_dict(s);
            free(s);
        }
    }
    json_free(manifest);
    return recipe;
}

static int mkdir_p(const char *path)
{
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len-1] == '/') tmp[len-1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            char saved = *p;
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) { *p = saved; return -1; }
            *p = saved;
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

/* PoP: verify_save_manifest @ agent/verify/environment.py:save_manifest */
char *verify_save_manifest(const char *root, const recipe_t *recipe)
{
    if (!recipe) return NULL;
    char *path = verify_manifest_path(root);
    if (!path) return NULL;
    /* parent dir */
    char parent[4096];
    snprintf(parent, sizeof(parent), "%s/.hermes", root ? root : ".");
    if (mkdir_p(parent) != 0) { free(path); return NULL; }
    char *recipe_json = recipe_to_json(recipe);
    if (!recipe_json) { free(path); return NULL; }
    extern char *now_iso(void);
    char *ts = now_iso();
    /* build the versioned wrapper {version, recipe, updatedAt} */
    json_t *o = json_object();
    json_set(o, "version", json_number((double)MANIFEST_VERSION));
    json_t *r = json_parse(recipe_json, NULL);
    json_set(o, "recipe", r ? r : json_object());
    json_set(o, "updatedAt", json_string(ts ? ts : ""));
    char *out = json_serialize(o);
    json_free(o);
    free(recipe_json);
    free(ts);
    if (!out) { free(path); return NULL; }
    FILE *f = fopen(path, "wb");
    if (!f) { free(out); free(path); return NULL; }
    fprintf(f, "%s\n", out);
    fclose(f);
    free(out);
    return path;
}

/* PoP: verify_load_or_detect @ agent/verify/environment.py:load_or_detect */
recipe_t *verify_load_or_detect(const char *root, char *source_out)
{
    recipe_t *saved = verify_load_manifest(root);
    if (saved) {
        if (source_out) snprintf(source_out, 16, "manifest");
        return saved;
    }
    if (source_out) snprintf(source_out, 16, "detected");
    return recipe_detect(root);
}
