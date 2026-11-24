#ifndef NOC_W22_SESSIONSCM_632617_H
#define NOC_W22_SESSIONSCM_632617_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/sessions_cmd_repair_profiles.py (gate w22_sessionscm_632617).
 *
 * `hermes sessions repair-profiles` is the CLI face of
 * hermes_cli.sessions_repair_profiles: it opens every profile's store itself,
 * reports what it finds, and repairs only under `--apply`. The stores, the
 * scan and the confirmation prompt are its seams; everything the command
 * *decides and prints* is pure and ported here:
 *
 *   - _KIND_LABELS: kind -> human label (unknown kinds print themselves).
 *   - _group: findings grouped by kind, kinds in first-seen order.
 *   - _print_report: the per-kind report block.
 *   - cmd_repair_profiles: the whole flow -- the json/report branches, the
 *     live-gateway refusal, the "N of M can be repaired" tail, the confirm
 *     gate, the snapshot/totals/failure output and the exit code.
 *
 * A finding travels as the object Finding.as_dict() produces, plus
 * "repairable" (Finding._fix is set):
 *   {"kind", "store", "subject", "detail", "action", "reason", "repairable"}
 */

/* PoP: w22_sessionscm_632617_kind_label @ hermes_cli/sessions_cmd_repair_profiles.py:_KIND_LABELS
 * The label for a finding kind; an unknown kind is its own label
 * (`_KIND_LABELS.get(kind, kind)`). NULL in -> "". */
const char *w22_sessionscm_632617_kind_label(const char *kind);

/* PoP: w22_sessionscm_632617_group @ hermes_cli/sessions_cmd_repair_profiles.py:_group
 * {"<kind>": ["<subject>", ...], ...} in first-seen kind order. */
json_t *w22_sessionscm_632617_group(const json_t *findings);

/* PoP: w22_sessionscm_632617_report @ hermes_cli/sessions_cmd_repair_profiles.py:_print_report
 * The report text (`print` adds the newline, so every line here ends with
 * one). Caller frees the result. */
char *w22_sessionscm_632617_report(const json_t *findings);

/* PoP: w22_sessionscm_632617_run @ hermes_cli/sessions_cmd_repair_profiles.py:cmd_repair_profiles
 * The command, with its seams injected:
 *   {"stores": [profile], "live": [[profile, pid], ...],
 *    "findings": [finding, ...],
 *    "apply": bool, "json": bool, "yes": bool, "legacy_main": str|null,
 *    "result": {"snapshots": {..}, "totals": {..}, "failures": [..]},
 *    "confirm": null (accepted) | int (the refused exit code)}
 * -> {"exit": int, "stdout": str, "stderr": str, "prompts": [[p, declined], ...]}
 */
json_t *w22_sessionscm_632617_run(const json_t *request);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_SESSIONSCM_632617_H */
