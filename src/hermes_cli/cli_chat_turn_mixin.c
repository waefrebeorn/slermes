/*
 * cli_chat_turn_mixin.c — C home for upstream hermes_cli/cli_chat_turn_mixin.py.
 *
 * PoP: w18_clichattu_3f16f7_chat_resolve_interrupt @ hermes_cli/cli_chat_turn_mixin.py:CLIChatTurnMixin._chat_resolve_interrupt
 *
 * Self-contained, libjson-only.  The observable return of the method is ported
 * faithfully; the two assignments to `self` and the conditional
 * `self.agent.clear_interrupt()` side effect have no C object to act on and are
 * represented by the injected thread_alive / interrupt_requested flags.
 */
#include "noc_w18_clichattu_3f16f7.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

static int jtruth(const struct json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}
static int ctruth(const char *s) { return s && s[0] != '\0'; }

struct json_t *w18_clichattu_3f16f7_chat_resolve_interrupt(
    const struct json_t *result, const char *interrupt_msg, const char *response,
    int thread_alive, int interrupt_requested)
{
    (void)thread_alive;
    (void)interrupt_requested;   /* side-effect-only in the Python method */

    int interrupted_this_turn = jtruth(result) && jtruth(json_obj_get(result, "interrupted"));
    int show = 0;
    struct json_t *pending = NULL;

    if (interrupted_this_turn) {
        struct json_t *im = json_obj_get(result, "interrupt_message");
        if (jtruth(im)) pending = json_copy(im);
        else pending = ctruth(interrupt_msg) ? json_string(interrupt_msg) : json_null();
        show = (ctruth(response) && jtruth(pending)) ? 1 : 0;
    } else if (ctruth(interrupt_msg)) {
        pending = json_string(interrupt_msg);
    } else {
        pending = json_null();
    }

    struct json_t *out = json_array();
    json_append(out, pending);
    json_append(out, json_bool(show ? true : false));
    return out;
}
