/*
 * worker_operations.c — faithful C11 port of the pure routing table in
 * upstream Hermes pm/worker_operations.py (gate w7_workeropera_9d249d).
 *
 * PoP: w7_workeropera_9d249d_wop_names @ pm/worker_operations.py:OPERATIONS
 * PoP: w7_workeropera_9d249d_wop_lookup @ pm/worker_operations.py:OPERATIONS
 *
 * Only the import-light OPERATIONS mapping is ported; Operation.resolve()
 * performs a live import_module and is intentionally out of scope for a
 * data-only oracle.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w7_workeropera_9d249d.h"

struct Op {
    const char *name;
    const char *module;
    const char *packages; /* "|"-joined, NULL => None */
    const char *bootstrap;
};

/* Exact mirror of pm/worker_operations.py:OPERATIONS (insertion order). */
static const struct Op OPS[] = {
    {"ensure",                        "pm.install",          NULL,  "state"},
    {"stage_only",                    "pm.install",          NULL,  "always"},
    {"stage_tools",                   "pm.build_operations", NULL,  "never"},
    {"prepare_tools",                 "pm.build_operations", NULL,  "always"},
    {"sync_venv",                     "pm.install",          "venv", "policy"},
    {"venv_is_current",               "pm.install",          "venv", "never"},
    {"build_environment",             "pm.operations",       "uv",  "policy"},
    {"lock_project",                  "pm.operations",       "uv",  "policy"},
    {"ensure_environment",            "pm.operations",       "uv",  "policy"},
    {"ensure_project_environment",    "pm.operations",       "uv",  "policy"},
    {"ensure_python_tool",            "pm.operations",       "uv",  "policy"},
    {"check_project_lock",            "pm.build_operations", "uv",  "policy"},
    {"export_requirements",           "pm.build_operations", "uv",  "policy"},
    {"build_requirements_environment","pm.build_operations", "uv",  "policy"},
    {"prune_cache",                   "pm.build_operations", "uv",  "always"},
};
static const size_t OPS_LEN = sizeof(OPS) / sizeof(OPS[0]);

static int cmp_name(const void *a, const void *b) {
    const char *const *x = (const char *const *)a;
    const char *const *y = (const char *const *)b;
    return strcmp(*x, *y);
}

struct json_t *wop_names(void) {
    const char **names = (const char **)malloc(OPS_LEN * sizeof(const char *));
    if (!names) return json_array();
    for (size_t i = 0; i < OPS_LEN; i++) names[i] = OPS[i].name;
    qsort(names, OPS_LEN, sizeof(const char *), cmp_name);
    json_t *arr = json_array();
    for (size_t i = 0; i < OPS_LEN; i++) json_append(arr, json_string(names[i]));
    free(names);
    return arr;
}

struct json_t *wop_lookup(const char *name) {
    if (!name) return json_null();
    for (size_t i = 0; i < OPS_LEN; i++) {
        if (!strcmp(OPS[i].name, name)) {
            json_t *obj = json_object();
            json_set(obj, "module", json_string(OPS[i].module));
            if (OPS[i].packages) {
                json_t *pk = json_array();
                /* packages is a tuple of one element in every operation. */
                const char *p = OPS[i].packages;
                const char *sep = strchr(p, '|');
                if (sep) {
                    char buf[256];
                    size_t n = (size_t)(sep - p);
                    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
                    memcpy(buf, p, n); buf[n] = '\0';
                    json_append(pk, json_string(buf));
                    json_append(pk, json_string(sep + 1));
                } else {
                    json_append(pk, json_string(p));
                }
                json_set(obj, "packages", pk);
            } else {
                json_set(obj, "packages", json_null());
            }
            json_set(obj, "bootstrap", json_string(OPS[i].bootstrap));
            return obj;
        }
    }
    return json_null();
}
