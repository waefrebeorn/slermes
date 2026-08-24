/*
 * computer_use_schema.c — canonical C11 port of tools/computer_use/schema.py.
 *
 * One consolidated `computer_use` tool schema with an `action` discriminator
 * (keeps the schema compact and the per-turn token cost low). The full JSON
 * schema is served by the existing tool-dispatch layer; this module ports the
 * accessor surface and validates action names against the Python set.
 */

#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

/* PoP: cus_schema_name @ tools/computer_use/schema.py:COMPUTER_USE_SCHEMA.name */
const char *cus_schema_name(void) { return "computer_use"; }

/* PoP: cus_actions @ tools/computer_use/schema.py (action enum in the schema)
 * The consolidated action discriminator values. */
static const char *const ACTIONS[] = {
    "screenshot", "click", "double_click", "right_click", "type",
    "key", "scroll", "wait", "capture", NULL
};

/* PoP: cus_is_valid_action @ tools/computer_use/schema.py (action enum check) */
int cus_is_valid_action(const char *action) {
    if (!action) return 0;
    for (int i = 0; ACTIONS[i]; i++)
        if (strcmp(action, ACTIONS[i]) == 0) return 1;
    return 0;
}

/*
 * Compact tool-schema summary for the tool list. Full parameter detail is
 * PoP: cus_schema @ tools/computer_use/schema.py:COMPUTER_USE_SCHEMA
 */
json_t *cus_schema(void) {
    json_t *s = json_object();
    json_set(s, "name", json_string("computer_use"));
    json_set(s, "description", json_string(
        "Drive the desktop in the background via cua-driver — screenshots, "
        "mouse, keyboard, scroll, drag — without stealing the user's cursor"));
    json_t *actions = json_array();
    for (int i = 0; ACTIONS[i]; i++)
        json_append(actions, json_string(ACTIONS[i]));
    json_set(s, "actions", actions);
    return s;
}
