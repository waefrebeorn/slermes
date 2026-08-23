/* gate: agent_moonshot_schema_repair_schema — traced copy of
 * src/agent/moonshot_schema_ports.c::msc_repair_schema + _msc_repair_node
 * (linked for real via extra_src is not possible: the rung macros must fire
 * inside the recursion, so this is a verbatim traced copy; only ordering of
 * missing-type inference vs sub-schema recursion is swapped — semantics are
 * unaffected because inference touches node["type"] and recursion never
 * reads it).
 *
 * LIVE twin: agent/moonshot_schema.py::_repair_schema (LIVE module's
 * _SCHEMA_LIST_KEYS / _fill_missing_type supply the semantics).
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   R  repair_node in
 *   B1: array -> LOOP items, recurse object elements
 *   B2: scalar -> out
 *   LOOP schema-list keys (items/anyOf/oneOf/allOf):
 *     B3: child object -> recurse
 *     B4: child array  -> LOOP elements, B5: element object -> recurse
 *   B6: is_schema && no "type" -> infer:
 *       B7 properties->object / B8 items->array / else string
 *   R out
 * Inputs avoid LIVE-only repairs with no C counterpart (anyOf null-collapse,
 * nullable strip, required injection) and avoid "properties" maps, which
 * C recurses as a schema while LIVE walks per-value (documented gap).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "json.h"
#include "watercup.h"


static json_t *traced_repair_node(json_t *node, bool is_schema);

/* Single file-scope tracer: nested R/r depth lives in one stream.
 * The wrapper uses raw wcup_* calls because WCUP_BEGIN would declare a
 * shadowing local tracer and split the stream. */
static wcup_t _wc;

static json_t *traced_repair_node(json_t *node, bool is_schema) {
    WCUP_RECURSE_IN();
    if (!node) { WCUP_BRANCH(1); goto out; }
    WCUP_BRANCH(0);

    if (node->type == JSON_ARRAY) {                       /* B1 */
        WCUP_BRANCH(1);
        for (size_t i = 0; i < json_len(node); i++) {
            WCUP_LOOP_TICK();
            json_t *item = json_get(node, i);
            if (item && item->type == JSON_OBJECT) {
                WCUP_BRANCH(1);
                (void)traced_repair_node(item, true);
            } else {
                WCUP_BRANCH(0);
            }
        }
        goto out;
    }
    WCUP_BRANCH(0);
    if (node->type != JSON_OBJECT) {                      /* B2 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);

    /* Recurse into sub-schemas (C port key list). */
    const char *keys[] = {"properties", "items", "anyOf", "oneOf", "allOf", NULL};
    for (int k = 0; keys[k]; k++) {
        WCUP_LOOP_TICK();
        json_t *child = json_obj_get(node, keys[k]);
        if (child && child->type == JSON_OBJECT) {        /* B3 */
            WCUP_BRANCH(1);
            (void)traced_repair_node(child, true);
        } else {
            WCUP_BRANCH(0);
            if (child && child->type == JSON_ARRAY) {     /* B4 */
                WCUP_BRANCH(1);
                for (size_t i = 0; i < json_len(child); i++) {
                    WCUP_LOOP_TICK();
                    json_t *elt = json_get(child, i);
                    if (elt && elt->type == JSON_OBJECT) { /* B5 */
                        WCUP_BRANCH(1);
                        (void)traced_repair_node(elt, true);
                    } else {
                        WCUP_BRANCH(0);
                    }
                }
            } else {
                WCUP_BRANCH(0);
            }
        }
    }

    /* Missing-type inference for schema nodes (after recursion; see header). */
    if (is_schema && !json_obj_get(node, "type")) {       /* B6 */
        WCUP_BRANCH(1);
        if (json_obj_get(node, "properties")) {           /* B7 */
            WCUP_BRANCH(1);
            json_set(node, "type", json_string("object"));
        } else {
            WCUP_BRANCH(0);
            if (json_obj_get(node, "items")) {            /* B8 */
                WCUP_BRANCH(1);
                json_set(node, "type", json_string("array"));
            } else {
                WCUP_BRANCH(0);
                json_set(node, "type", json_string("string"));
            }
        }
    } else {
        WCUP_BRANCH(0);
    }

out:
    WCUP_RECURSE_OUT();
    return node;
}

/* Shape of msc_repair_schema wrapper (moonshot_schema_ports.c). */
static void wc_repair_schema(const char *node_json, bool is_schema) {
    wcup_begin(&_wc, "repair_schema");
    json_t *node = node_json ? json_parse(node_json, NULL) : NULL;
    if (!node) {
        /* unparseable/absent -> returned unchanged; guard branch only */
        wcup_branch(&_wc, !node_json);
    } else {
        wcup_branch(&_wc, false);
        (void)traced_repair_node(node, is_schema);
        char *ser = json_serialize(node);
        free(ser);
        json_free(node);
    }
    char cup[32];
    wcup_finalize(&_wc, cup, sizeof cup);
    printf("%s\n", cup);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s repair_schema <flag|json>...\n"
                        "  flag: '1' is_schema, '0' container\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        /* input form: "<is_schema:0|1>|<json>" */
        char *sep = strchr(argv[i], '|');
        if (!sep) { fprintf(stderr, "bad input: %s\n", argv[i]); return 2; }
        *sep = '\0';
        wc_repair_schema(sep + 1, argv[i][0] == '1');
    }
    return 0;
}
