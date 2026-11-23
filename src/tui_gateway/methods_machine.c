/*
 * methods_machine.c - C home for upstream Hermes tui_gateway/methods_machine.py.
 * PoP: w15_methodsmac_3a4c7c_machine_facts @ tui_gateway/methods_machine.py:_
 *
 * The live handler is unscoped on purpose (hardware / OS account belong to the host,
 * not a profile home). Its one pure operation is the dict comprehension that drops
 * `full_name` from `summary()` before the payload goes back through `_ok`. The host
 * probe itself stays in the live Python owner; the fixture supplies the summary.
 */
#include "hermes_json.h"
#include <string.h>
#include "noc_w15_methodsmac_3a4c7c.h"

struct json_t *w15_methodsmac_3a4c7c_machine_facts(const struct json_t *summary) {
    json_t *out = json_object();
    if (summary && summary->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(summary); i++) {
            const char *k = json_object_get_key_at(summary, i);
            if (!k || !strcmp(k, "full_name")) continue;
            json_t *v = json_object_get_at(summary, i);
            json_set(out, k, v ? json_copy(v) : json_null());
        }
    }
    return out;
}
