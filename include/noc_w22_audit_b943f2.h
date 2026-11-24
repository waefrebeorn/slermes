#ifndef NOC_W22_AUDIT_B943F2_H
#define NOC_W22_AUDIT_B943F2_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/dashboard_auth/audit.py (gate w22_audit_b943f2).
 *
 * `audit_log` appends one JSON object per line to
 * `$HERMES_HOME/logs/dashboard-auth.log`. Everything the write path decides is
 * pure and ported here:
 *
 *   - _bounded_value: strings bounded to _MAX_FIELD_LENGTH characters with the
 *     _TRUNCATION_MARKER tail, recursively through dicts/lists.
 *   - _REDACTED_FIELDS: token-like kwargs dropped before serialisation.
 *   - the entry itself (`ts`, `event`, then the surviving fields in call order)
 *     serialized with `separators=(",", ":")`.
 *   - _rotate_if_needed's decision and its `.1`/`.2`/`.3` rename order.
 *
 * The file handle, the clock (`ts` is injected) and the directory creation are
 * the write-side seams and stay upstream.
 */

/* PoP: w22_audit_b943f2_bounded @ hermes_cli/dashboard_auth/audit.py:_bounded_value
 * Bound strings, including nested values. A decoded JSON value can only carry
 * str/int/float/bool/None/list/dict, so the `repr()` branch has no reachable
 * input here. Lengths count characters, like Python's len(). */
json_t *w22_audit_b943f2_bounded(const json_t *value);

/* PoP: w22_audit_b943f2_event_value @ hermes_cli/dashboard_auth/audit.py:AuditEvent
 * The literal `event` field for an `AuditEvent` member name ("LOGIN_START"),
 * or NULL for a name the enum does not have. */
const char *w22_audit_b943f2_event_value(const char *name);

/* PoP: w22_audit_b943f2_is_redacted @ hermes_cli/dashboard_auth/audit.py:_REDACTED_FIELDS
 * 1 when the field name is in `_REDACTED_FIELDS` (case matters: both
 * "Authorization" and "authorization" are listed). */
int w22_audit_b943f2_is_redacted(const char *key);

/* PoP: w22_audit_b943f2_line @ hermes_cli/dashboard_auth/audit.py:audit_log
 * The serialized line (no trailing newline) for one event:
 *   {"ts":<ts>,"event":<value>,<fields...>}
 * `fields` is the kwargs object; redacted names are dropped, the rest bounded.
 * A field literally named "ts"/"event" overrides the prefix, exactly like the
 * `{**fields}` spread. Caller frees the result. */
char *w22_audit_b943f2_line(const char *ts, const char *event_name, const json_t *fields);

/* PoP: w22_audit_b943f2_rotation @ hermes_cli/dashboard_auth/audit.py:_rotate_if_needed
 * Rotation plan for appending `pending` bytes to `name` whose current size is
 * `size` (both int64 decimal strings; `size` NULL = stat raised OSError, which
 * means no rotation):
 *   ["name.2 -> name.3", "name.1 -> name.2", "name -> name.1"]
 * `existing` lists the backup suffixes (1..3) whose file exists. */
json_t *w22_audit_b943f2_rotation(const char *name, const char *size, const char *pending,
                                  const json_t *existing);

/* PoP: w22_audit_b943f2_limits @ hermes_cli/dashboard_auth/audit.py:_MAX_FIELD_LENGTH
 * _MAX_FIELD_LENGTH / _MAX_BYTES / _BACKUP_COUNT and the marker. */
#define W22_AUDIT_B943F2_MAX_FIELD_LENGTH 256
#define W22_AUDIT_B943F2_MAX_BYTES 5242880LL
#define W22_AUDIT_B943F2_BACKUP_COUNT 3

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_AUDIT_B943F2_H */
