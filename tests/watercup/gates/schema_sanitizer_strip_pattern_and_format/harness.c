/* gate: schema_sanitizer_strip_pattern_and_format — traced copy of
 * src/tools/schema_sanitizer.c::cli_tools_schema_sanitizer__strip_pattern_and_format
 * (+ its static strip_pf_walk core).
 * LIVE twin: tools/schema_sanitizer.py::strip_pattern_and_format.
 *
 * WCUP_* records the SAME control structure so the cup matches iff the C port
 * executes the identical branch ladder as LIVE Python.
 *
 * Rung order (must match twin.py EXACTLY):
 *   B1: empty tools input
 *   B2: JSON parse failed
 *   B3: root is array
 *   per tool:  L(tick) B(tool is dict) B(fn is dict) B(fn params dict)
 *              B(tool params dict)
 *   walk(node): B(is dict) B(is array) [object: L per key until schema marker,
 *              B(is_schema), L per key, B(strippable key) w/ recursion R/r]
 *   VSTR: total stripped count
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hermes_json.h"
#include "watercup.h"

static const char *STRIP_KEYS[] = {"pattern", "format", NULL};

/* file-scope tracer (recursion spans two functions) */
static wcup_t _wc;

static json_t *wc_walk(json_t *node, int *stripped)
{
    WCUP_RECURSE_IN();
    if (!node) { WCUP_RECURSE_OUT(); return NULL; }
    if (node->type == JSON_OBJECT) {
        WCUP_BRANCH(1);
        int is_schema = 0;
        for (size_t i = 0; i < node->c.count; i++) {
            WCUP_LOOP_TICK();
            const char *k = node->c.keys[i];
            if (k && (strcmp(k, "type") == 0 || strcmp(k, "anyOf") == 0 ||
                      strcmp(k, "oneOf") == 0 || strcmp(k, "allOf") == 0)) {
                WCUP_BRANCH(1);
                is_schema = 1;
                break;
            }
            WCUP_BRANCH(0);
        }
        WCUP_BRANCH(is_schema);
        json_t *out = json_object();
        for (size_t i = 0; i < node->c.count; i++) {
            WCUP_LOOP_TICK();
            const char *k = node->c.keys[i];
            if (!k) continue;
            int hit = 0;
            if (is_schema)
                for (int s = 0; STRIP_KEYS[s]; s++)
                    if (strcmp(k, STRIP_KEYS[s]) == 0) { hit = 1; break; }
            WCUP_BRANCH(is_schema && hit);
            if (hit) { (*stripped)++; continue; }
            json_t *v = wc_walk(node->c.items[i], stripped);
            json_set(out, k, v ? v : json_null());
        }
        WCUP_RECURSE_OUT();
        return out;
    }
    WCUP_BRANCH(0); /* not object */
    if (node->type == JSON_ARRAY) {
        WCUP_BRANCH(1);
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(node); i++) {
            WCUP_LOOP_TICK();
            json_append(out, wc_walk(json_get(node, i), stripped));
        }
        WCUP_RECURSE_OUT();
        return out;
    }
    WCUP_BRANCH(0); /* scalar */
    WCUP_RECURSE_OUT();
    return json_copy(node);
}

static void wc_strip_pf(const char *tools_json)
{
    wcup_begin(&_wc, "strip_pattern_and_format");
    int stripped = 0;
    if (!tools_json || !tools_json[0]) {
        WCUP_BRANCH(1);
        char b[16]; snprintf(b, sizeof b, "%d", stripped); WCUP_VALSTR(b);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("strip_pattern_and_format");
        return;
    }
    WCUP_BRANCH(0);
    json_t *root = json_parse(tools_json, NULL);
    if (!root) {
        WCUP_BRANCH(1);
        char b[16]; snprintf(b, sizeof b, "%d", stripped); WCUP_VALSTR(b);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("strip_pattern_and_format");
        return;
    }
    WCUP_BRANCH(0);
    if (root->type == JSON_ARRAY) {
        WCUP_BRANCH(1);
        size_t n = json_len(root);
        for (size_t i = 0; i < n; i++) {
            WCUP_LOOP_TICK();
            json_t *tool = json_get(root, i);
            if (!tool || tool->type != JSON_OBJECT) { WCUP_BRANCH(0); continue; }
            WCUP_BRANCH(1);
            json_t *params = NULL, *fn = json_obj_get(tool, "function");
            WCUP_BRANCH(fn && fn->type == JSON_OBJECT);
            if (fn && fn->type == JSON_OBJECT) {
                params = json_obj_get(fn, "parameters");
                WCUP_BRANCH(params && params->type == JSON_OBJECT);
            }
            if (!params) {
                params = json_obj_get(tool, "parameters");
                WCUP_BRANCH(params && params->type == JSON_OBJECT);
            }
            if (params && params->type == JSON_OBJECT)
                wc_walk(params, &stripped);
        }
    } else {
        WCUP_BRANCH(0);
    }
    json_free(root);
    char b[16]; snprintf(b, sizeof b, "%d", stripped); WCUP_VALSTR(b);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("strip_pattern_and_format");
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s strip_pattern_and_format <tools-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_strip_pf(argv[i]);
    return 0;
}
