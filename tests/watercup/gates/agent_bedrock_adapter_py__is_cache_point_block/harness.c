/* gate: agent_bedrock_adapter_py__is_cache_point_block — traced C port of
 * agent/bedrock_adapter.py::_is_cache_point_block.
 *
 * Python reference:
 *   def _is_cache_point_block(block: Any) -> bool:
 *       return isinstance(block, dict) and set(block.keys()) == {"cachePoint"}
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: parsed object is non-NULL
 *   B2: type == JSON_OBJECT
 *   B3: object has exactly 1 key
 *   B4: that key == "cachePoint"
 *   VSTR("true"/"false")
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "watercup.h"
#include "hermes_json.h"

/* trace helper: takes explicit wcup_t so we can call wcup_* directly
 * (WCUP_BRANCH macro binds to the _wc local in main) */
static bool wc_is_cache_point_block(wcup_t *wc, const json_t *block)
{
    /* B1: must be a non-NULL parsed value */
    if (!block) {
        wcup_branch(wc, 1);
        return false;
    }
    wcup_branch(wc, 0);

    /* B2: must be a JSON object (Python: isinstance(block, dict)) */
    if (block->type != JSON_OBJECT) {
        wcup_branch(wc, 0);
        return false;
    }
    wcup_branch(wc, 1);

    /* B3: must have exactly 1 key (Python: len(set(block.keys())) == 1) */
    size_t nkeys = json_object_size(block);
    if (nkeys != 1) {
        wcup_branch(wc, 0);
        return false;
    }
    wcup_branch(wc, 1);

    /* B4: that single key must be "cachePoint" */
    const char *key = json_object_get_key_at(block, 0);
    bool match = (key && strcmp(key, "cachePoint") == 0);
    wcup_branch(wc, match ? 1 : 0);

    return match;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <fn> <json...>\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        char *err = NULL;
        json_t *obj = json_parse(argv[i], &err);
        WCUP_BEGIN("_is_cache_point_block");
        bool result = wc_is_cache_point_block(&_wc, obj);
        WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        if (obj) json_free(obj);
        WCUP_END("_is_cache_point_block");
    }
    return 0;
}
