/*
 * anon_sign_in_cli.c - C home for upstream Hermes hermes_cli/anon_sign_in_cli.py.
 * PoP: w15_anonsigni_6f4a1e_drain_sign_in_copy @ hermes_cli/anon_sign_in_cli.py:drain_sign_in_copy
 *
 * Faithful port of the pure generator drain: every non-terminal state is skipped
 * and the LAST terminal state's copy wins. `chat` selects `.copy` (in-chat) vs
 * `.copy_terminal`. The `on_terminal` hook and the rendering entry points resolve
 * live anon_auth / device-flow state and stay with the live Python owner.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w15_anonsigni_6f4a1e.h"

struct json_t *w15_anonsigni_6f4a1e_drain_sign_in_copy(const struct json_t *states, int chat) {
    const char *result = "";
    if (states && states->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(states); i++) {
            json_t *st = json_get(states, i);
            if (!st || st->type != JSON_OBJECT) continue;
            if (!json_get_bool(st, "terminal", 0)) continue;
            const char *chat_copy = json_get_str(st, "copy", "");
            result = chat ? chat_copy : json_get_str(st, "copy_terminal", chat_copy);
        }
    }
    return json_string(result);
}
