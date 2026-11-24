#ifndef NOC_W18_WEBREADCO_957679_H
#define NOC_W18_WEBREADCO_957679_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C port of hermes_cli/web_read_coalescing.py cache-key freezing.
 * PoP: w18_webreadco_957679_freeze @ hermes_cli/web_read_coalescing.py:_freeze
 */

/* _freeze(value) rendered as a canonical JSON value:
 *   {"t":<type name>,"v":<payload>}
 * type names: dict, list, tuple, set, frozenset, int, float, str, bool, NoneType.
 * dict/set/frozenset payloads are sorted by their canonical JSON rendering.
 * Input markers select container types JSON cannot express:
 *   {"__tuple__":[...]}, {"__set__":[...]}, {"__frozenset__":[...]}.
 * Plain JSON arrays/objects are list/dict; integral numbers are int, others float. */
json_t *w18_webreadco_957679_freeze(const json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_WEBREADCO_957679_H */
