/*
 * build_agent.c — C port of docker/build_agent.py (gate w6_buildagent_361323).
 * PoP: w6_buildagent_361323_assemble_inputs @ docker/build_agent.py:assemble_image
 * PoP: w6_buildagent_361323_link_target @ docker/build_agent.py:assemble_image
 * PoP: w6_buildagent_361323_assemble_image @ docker/build_agent.py:assemble_image
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_buildagent_361323.h"

/* Join with a single '/', tolerating a trailing '/' on the prefix. */
static char *path_join(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a);
    int slash = la && a[la - 1] == '/';
    char *r = (char *)malloc(la + strlen(b) + 2);
    if (!r) return NULL;
    memcpy(r, a, la);
    if (!slash) r[la++] = '/';
    strcpy(r + la, b);
    return r;
}
static void set_path(struct json_t *o, const char *key, const char *p) {
    json_set(o, key, json_string(p ? p : ""));
}
/* pathlib.Path drops a trailing separator when it parses a path. */
static const char *norm_root(const char *root) {
    static char buf[8192];
    if (!root) root = "";
    strncpy(buf, root, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    size_t n = strlen(buf);
    while (n > 1 && buf[n - 1] == '/') buf[--n] = 0;
    return buf;
}

struct json_t *w6_buildagent_361323_assemble_inputs(const char *root, const char *target,
                                                     const char *site) {
    root = norm_root(root);
    if (!target) target = "";
    if (!site) site = "";
    struct json_t *o = json_object();

    char *p = path_join(root, "pyproject.toml");
    set_path(o, "project", p); free(p);
    set_path(o, "code", root);
    json_set(o, "repo", json_string("."));
    json_set(o, "placement", json_string("fixed"));
    set_path(o, "target", target);

    char *venv = path_join(root, ".venv");
    char *binpy = path_join(venv, "bin/python");
    set_path(o, "python", binpy);  /* (environment / "bin/python").absolute() */
    free(binpy);

    set_path(o, "site_packages", site);
    set_path(o, "environment", venv);
    char *q = path_join(root, "tools"); set_path(o, "tools", q); free(q);
    q = path_join(root, "pm-runtime"); set_path(o, "pm_runtime", q); free(q);
    json_set(o, "bin_dir", json_string("libexec"));

    /* RESOURCE_ENV (scripts/build/inputs.py), in insertion order. */
    static const char *const res_names[] = {
        "skills", "optional-skills", "plugins", "locales", "optional-mcps"
    };
    struct json_t *res = json_object();
    for (size_t i = 0; i < sizeof(res_names) / sizeof(res_names[0]); i++) {
        char *rp = path_join(root, res_names[i]);
        json_set(res, res_names[i], json_string(rp ? rp : ""));
        free(rp);
    }
    json_set(o, "resources", res);

    struct json_t *fr = json_object();
    q = path_join(root, "ui-tui"); json_set(fr, "tui", json_string(q ? q : "")); free(q);
    q = path_join(root, "hermes_cli/web_dist"); json_set(fr, "web", json_string(q ? q : "")); free(q);
    json_set(o, "frontends", fr);
    free(venv);
    return o;
}

char *w6_buildagent_361323_link_target(const char *command) {
    return path_join("../..", command ? command : "");
}

struct json_t *w6_buildagent_361323_assemble_image_ex(const char *root, const char *target,
        const char *site, const struct json_t *commands) {
    struct json_t *out = json_object();
    json_set(out, "inputs", w6_buildagent_361323_assemble_inputs(root, target, site));
    struct json_t *links = json_array();
    char *venv = path_join(norm_root(root), ".venv");
    if (commands && commands->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(commands); i++) {
            const char *name = json_object_get_key_at(commands, i);
            const struct json_t *cv = json_object_get_at(commands, i);
            if (!name) continue;
            const char *cmd = (cv && cv->type == JSON_STRING) ? cv->str_val : "";
            json_t *l = json_string(name);
            char *lp = path_join(venv, "bin");
            char *full = path_join(lp, name);
            char *tgt = w6_buildagent_361323_link_target(cmd);
            struct json_t *e = json_object();
            json_set(e, "name", l);
            set_path(e, "path", full);
            set_path(e, "target", tgt);
            json_append(links, e);
            free(full); free(lp); free(tgt);
        }
    }
    free(venv);
    json_set(out, "links", links);
    return out;
}
