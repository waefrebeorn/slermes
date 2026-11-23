/*
 * search_policy.c — real C port of upstream Hermes agent/search_policy.py constant tables.
 * PoP: cn_66abc92c44_constants @ agent/search_policy.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_66abc92c44.h"

json_t *cn_66abc92c44_constants(void) {
    json_t *out = json_object();
    json_set(out, "SEARCH_PRUNE_DIR_NAMES", ({ json_t *__a = json_array();
        json_append(__a, json_string(".Trash"));
        json_append(__a, json_string(".archive"));
        json_append(__a, json_string(".backups"));
        json_append(__a, json_string(".cache"));
        json_append(__a, json_string(".git"));
        json_append(__a, json_string(".gradle"));
        json_append(__a, json_string(".hg"));
        json_append(__a, json_string(".m2"));
        json_append(__a, json_string(".mypy_cache"));
        json_append(__a, json_string(".next"));
        json_append(__a, json_string(".nox"));
        json_append(__a, json_string(".npm"));
        json_append(__a, json_string(".nuget"));
        json_append(__a, json_string(".nuxt"));
        json_append(__a, json_string(".parcel-cache"));
        json_append(__a, json_string(".pnpm-store"));
        json_append(__a, json_string(".pytest_cache"));
        json_append(__a, json_string(".ruff_cache"));
        json_append(__a, json_string(".svelte-kit"));
        json_append(__a, json_string(".svn"));
        json_append(__a, json_string(".tox"));
        json_append(__a, json_string(".turbo"));
        json_append(__a, json_string(".venv"));
        json_append(__a, json_string(".yarn"));
        json_append(__a, json_string("__pycache__"));
        json_append(__a, json_string("backup"));
        json_append(__a, json_string("backups"));
        json_append(__a, json_string("build"));
        json_append(__a, json_string("coverage"));
        json_append(__a, json_string("dist"));
        json_append(__a, json_string("dist-packages"));
        json_append(__a, json_string("node_modules"));
        json_append(__a, json_string("out"));
        json_append(__a, json_string("site-packages"));
        json_append(__a, json_string("target"));
        json_append(__a, json_string("third_party"));
        json_append(__a, json_string("vendor"));
        json_append(__a, json_string("venv"));
      __a; }));
    return out;
}
