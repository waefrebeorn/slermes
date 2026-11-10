/*
 * port_producer_lifecycle.c — pure helpers from
 * gateway/session_producer_lifecycle.py.
 */

#include "port_producer_lifecycle.h"
#include "hermes_json.h"

#include <stddef.h>
#include <string.h>

/* Python truthiness of a JSON value. */
static int spl_truthy(const json_t *n)
{
    if (!n) return 0;
    switch (n->type) {
    case JSON_NULL: return 0;
    case JSON_BOOL: return n->bool_val ? 1 : 0;
    case JSON_NUMBER: return n->num_val != 0.0;
    case JSON_STRING: return n->str_val && n->str_val[0] != '\0';
    default: return n->c.count != 0; /* object/array */
    }
}

const char *spl_executed_outcome(const char *captured_json)
{
    json_t *cap = captured_json ? json_parse(captured_json, NULL) : NULL;
    json_t *terminal = NULL, *result = NULL, *r = NULL;

    if (!cap) return "success";
    terminal = json_obj_get(cap, "terminal_result");
    result = json_obj_get(cap, "result");
    /* captured.get('terminal_result', captured.get('result')): the first key,
     * present-even-if-null, wins; else the second; then `or {}`. */
    if (terminal != NULL)
        r = terminal;
    else if (result != NULL)
        r = result;
    if (!r || r->type == JSON_NULL) {
        json_free(cap);
        return "success";
    }
    if (json_obj_get(r, "interrupted") && spl_truthy(json_obj_get(r, "interrupted"))) {
        json_free(cap);
        return "cancelled";
    }
    if ((json_obj_get(r, "failed") && spl_truthy(json_obj_get(r, "failed"))) ||
        (json_obj_get(r, "error") && spl_truthy(json_obj_get(r, "error")))) {
        json_free(cap);
        return "failure";
    }
    json_free(cap);
    return "success";
}
