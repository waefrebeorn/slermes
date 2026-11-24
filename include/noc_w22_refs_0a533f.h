#ifndef NOC_W22_REFS_0A533F_H
#define NOC_W22_REFS_0A533F_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/checkpoints/refs.py (gate w22_refs_0a533f).
 *
 * The one reading of a `/rollback` argument every surface shares: a list
 * number or a checkpoint hash. A hash copied from the `/rollback` list keeps
 * naming the same checkpoint after another is taken (a list position moves),
 * and a short hash can be all digits. A digit string without a leading zero is
 * a list number while it is in range (`1..N`); any other string of 4+
 * characters (the hash minimum) is a hash. Shorter out-of-range digits stay
 * main's "Invalid checkpoint number" error.
 *
 * `checkpoints` is the decoded `list_checkpoints()` result, newest first: an
 * array of objects. Every branch is pure.
 */

/* PoP: w22_refs_0a533f_resolve @ agent/checkpoints/refs.py:resolve_checkpoint_ref
 * The commit `ref` names in `checkpoints`.
 *   {"ok": true,  "value": <the checkpoint hash, or `ref` itself>}
 *   {"ok": false, "error": "Invalid checkpoint number. Use 1-N.", "type": "ValueError"}
 *   {"ok": false, "error": "'int' object has no attribute 'strip'",
 *    "type": "AttributeError"}
 * The value travels as the raw JSON value: a checkpoint entry without a "hash"
 * key hands back `ref` unchanged, exactly like `entry.get("hash", ref)`. `ref`
 * is the JSON value Python was handed, because `(ref or "").strip()` is
 * truthiness first: 0/False/[]/{} become "", while a truthy non-string raises
 * AttributeError (reported, not crashed). */
json_t *w22_refs_0a533f_resolve(const json_t *checkpoints, const json_t *ref);

/* PoP: w22_refs_0a533f_min_hash @ agent/checkpoints/refs.py:_MIN_HASH
 * `_MIN_HASH`: the shortest string that is read as a checkpoint hash. */
#define W22_REFS_0A533F_MIN_HASH 4

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_REFS_0A533F_H */
