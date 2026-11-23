/* post_update.c — C home for upstream Hermes hermes_cli/post_update.py.
 *
 * PoP: w8_postupdate_run_steps_ex @ hermes_cli/post_update.py:run_steps
 *
 * Faithful port of run_steps(steps):
 *   results = {}
 *   for name, func in steps:
 *       try:    results[name] = func()
 *       except Exception as exc: results[name] = {"ok": False, "error": str(exc)}
 *   return results
 * A step is injected as {"name": str, "out": <json>} (returns <json>) or
 * {"name": str, "raise": str} (raises Exception(<str>)).  One failing step
 * never stops the rest; the results object preserves step order.
 */
#include "noc_w8_postupdate_284e24.h"
#include "hermes_json.h"
#include <string.h>

json_t *w8_postupdate_run_steps_ex(const json_t *steps) {
    json_t *out = json_object();
    if (!steps || steps->type != JSON_ARRAY) return out;

    for (size_t i = 0; i < json_len(steps); i++) {
        const json_t *s = json_get(steps, i);
        if (!s || s->type != JSON_OBJECT) continue;
        const char *name = json_get_str(s, "name", "");

        const json_t *raised = json_obj_get(s, "raise");
        if (raised && raised->type == JSON_STRING) {
            json_t *r = json_object();
            json_set(r, "ok", json_bool(false));
            json_set(r, "error", json_string(raised->str_val ? raised->str_val : ""));
            json_set(out, name, r);
        } else {
            const json_t *value = json_obj_get(s, "out");
            json_set(out, name, value ? json_copy(value) : json_null());
        }
    }
    return out;
}

