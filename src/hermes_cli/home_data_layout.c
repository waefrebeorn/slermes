/*
 * home_data_layout.c — C port of upstream Hermes hermes_cli/home_data_layout.py.
 * Gate: w15_homedatal_47b777
 *
 * PoP: w15_homedatal_47b777_profile_root_entry @ hermes_cli/home_data_layout.py:profile_root_entry
 *
 * profile_root_entry(parts):
 *   if len(parts) >= 3 and parts[0] == "profiles": return parts[2]
 *   return parts[0] if parts else None
 * Values are copied verbatim from the JSON array (Python does not coerce).
 */
#include "noc_w15_homedatal_47b777.h"
#include "hermes_json.h"
#include <string.h>

struct json_t *w15_homedatal_47b777_profile_root_entry(const struct json_t *parts) {
    if (!json_is_array(parts)) return json_null();
    size_t n = json_len(parts);
    if (n == 0) return json_null();
    json_t *first = json_get(parts, 0);
    if (n >= 3 && first && first->type == JSON_STRING && first->str_val &&
        !strcmp(first->str_val, "profiles")) {
        json_t *third = json_get(parts, 2);
        return third ? json_copy(third) : json_null();
    }
    return first ? json_copy(first) : json_null();
}
