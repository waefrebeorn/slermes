#ifndef NOC_W22_CONVENTION_A3BE4B_H
#define NOC_W22_CONVENTION_A3BE4B_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/core/conventions.py (gate w22_convention_a3be4b).
 *
 * The module is the cross-capability contract for stored instants: an instant
 * crosses the persistence boundary as epoch seconds, and a stored cell that no
 * instant decodes from must degrade to unset instead of failing the page. The
 * logging sink (the WARNING naming the session) and the caller's datetime
 * objects are the seam; the decoding rules are ported here:
 *
 *   - ensure_utc:    normalize an aware ISO-8601 instant to UTC; a naive one
 *                    raises ValueError (reported as "naive").
 *   - stored_epoch:  a stored cell as epoch seconds, or null when unset /
 *                    untrusted. Numbers, numeric strings (Python float()
 *                    syntax) and `{"__dt__": iso}` datetimes are accepted;
 *                    null and "" are unset; anything else, non-finite values
 *                    and values outside [0, 4200000000] read as null.
 *   - visibility:    the RowVisibility members, in declaration order.
 *   - limits:        the trust window and UNTRUSTED_INSTANT (its floor).
 */

/* PoP: w22_convention_a3be4b_ensure_utc @ persistence/core/conventions.py:ensure_utc
 * -> {"ok": true, "value": "<UTC isoformat>", "error": null}
 *  | {"ok": false, "value": null, "error": "naive" | "parse"} */
json_t *w22_convention_a3be4b_ensure_utc(const char *iso);

/* PoP: w22_convention_a3be4b_stored_epoch @ persistence/core/conventions.py:stored_instant
 * -> epoch seconds (number) or null. A datetime is carried as
 * {"__dt__": "<iso8601>", ...}; a naive one is untrusted. */
json_t *w22_convention_a3be4b_stored_epoch(const json_t *value);

/* PoP: w22_convention_a3be4b_visibility @ persistence/core/conventions.py:RowVisibility
 * [{"name": "ACTIVE", "value": "active"}, ...] in declaration order. */
json_t *w22_convention_a3be4b_visibility(void);

/* PoP: w22_convention_a3be4b_limits @ persistence/core/conventions.py:_STORED_EPOCH_MIN
 * {"floor": 0.0, "ceiling": 4200000000.0, "untrusted_epoch": 0.0,
 *  "untrusted_iso": "1970-01-01T00:00:00+00:00"} */
json_t *w22_convention_a3be4b_limits(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_CONVENTION_A3BE4B_H */
