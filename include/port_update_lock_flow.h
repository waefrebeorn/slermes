/* port_update_lock_flow.h — FLOW-core port of hermes_cli/update_lock.py.
 *
 * Mission: match the program's FLOW (decision sequences, call order, branch
 * conditions, state transitions, error paths) of the update-lock state
 * machine: marker parsing, the incarnation identity rule, judge/release
 * verdicts, stale-marker recovery under the mutex, and the marker-claim
 * acquire loop. File/clock/process I/O is INJECTED through seams (a generic
 * callback dispatcher + json_t state), never performed inside the ported
 * decision core.
 *
 * Seam model
 * ----------
 * Every live external call goes through `ulf_io_t.call(io, name, args)`:
 *   - appends {"name": name, "args": args} to io->trace (ordered);
 *   - returns the scripted result (a NEW json_t* the caller frees);
 *   - models a raised exception with {"__raise__": "<Type>", "message": "...",
 *     "errno": <int>?}. OSError-family types (OSError, PermissionError,
 *     FileNotFoundError, MarkerBusy, ...) are caught by the live except
 *     clauses exactly as their Python classes are.
 *
 * Live counterpart of each seam name:
 *   real_world                 _real_world() -> _World (world=None call sites);
 *                              ret {"pid","ct","now"}; alive/ct_of stay seams
 *   world.alive                _World.alive(pid) (the _pid_alive probe)
 *   world.ct_of                _World.ct_of(pid) (process_create_time)
 *   time.time                  time.time()
 *   os.getpid                  os.getpid()
 *   mutex.enter / mutex.exit   marker_mutex(path) context manager enter/exit
 *                              (enter may raise MarkerBusy/OSError; exit is
 *                              recorded on EVERY body exit, never after a
 *                              failed enter)
 *   _read_marker               _read_marker(path) -> marker spec | null
 *   checkout_lock_held         checkout_lock_held(install_root)
 *   path.unlink                Path.unlink() (FileNotFoundError suppressed at
 *                              the live call sites that suppress it)
 *   path.parent.mkdir          Path.parent.mkdir(parents=True, exist_ok=True)
 *   _sweep_dead_tmp_siblings   _sweep_dead_tmp_siblings(path)
 *   legacy_profile_claims      legacy_profile_claims(root) -> [holder specs]
 *   _identity_line             _identity_line() -> "ct:<s>" | ""
 *   _publish_exclusive         _publish_exclusive(path, body) -> bool
 *   _live_partners             _live_partners(marker) -> [pids] (seamed here;
 *                              itself a claimed port of this same core)
 *   read_live_update           read_live_update(path=..., install_root=...)
 *   self._is_partner           UpdateLock._is_partner(pid)
 *   self._adopt_or_refuse      UpdateLock._adopt_or_refuse(marker); a script
 *                              entry may carry "holder": the refusal holder
 *                              the live method assigns to self.holder (the
 *                              SAME state effect is applied on both sides)
 *   self._checkout_held_elsewhere  UpdateLock._checkout_held_elsewhere()
 *
 * Data model / representation notes
 * ---------------------------------
 * - marker spec: {"raw"|"raw_hex", "pid", "started_at", "create_time",
 *   "delegate_pid", "delegate_create_time", "in_flight", "ct_text",
 *   "delegate_ct_text", "runs"}. started_at is a JSON number, or
 *   {"__bigint__": "<decimal>"} when > 2^53 (libjson doubles cannot hold
 *   u64 exactly); the port keeps u64 internally and projects the same way.
 * - holder spec: {"pid", "age_seconds", "reason"?, "held"?} (UpdateHolder).
 * - world: {"pid", "ct": float|null, "now"}.
 * - Paths are POSIX strings in fixtures; the oracle normalizes every Path
 *   through as_posix() (str(Path) flips slashes on Windows).
 * - Scripted OSError str() parity: with "errno" -> "[Errno N] message"
 *   (OSError(errno, message)); without -> "message". No filename component
 *   is scripted (both sides construct the exception identically).
 * - Marker byte parsing tolerates BOM/CRLF/space-tab padding; invalid UTF-8
 *   is replaced per-byte with U+FFFD (fixtures avoid truncated multi-byte
 *   sequences, where Python groups replacements — documented).
 * - The regex surface is small ASCII line grammars ([0-9]+, ct:/delegate:/
 *   run: lines) hand-transcribed as exact scanners (documented).
 *
 * Ownership: every ulf_* function returns a NEW json_t* (caller frees).
 * Args passed to ulf_call are borrowed. Inputs are borrowed. The claim port
 * mutates its in.self / in.claims state in place; the harness owns them.
 */
#ifndef PORT_UPDATE_LOCK_FLOW_H
#define PORT_UPDATE_LOCK_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ulf_io_t {
    json_t *(*call)(struct ulf_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"name","args"}: ordered seam sequence */
    void   *ctx;     /* harness backend (scripted results) */
} ulf_io_t;

json_t *ulf_call(ulf_io_t *io, const char *name, json_t *args);
bool ulf_raised(const json_t *result);

/* Uniform port contract: json in -> json out (NEW node, caller frees). */
json_t *ulf_bounded_int(ulf_io_t *io, json_t *in);
json_t *ulf_as_ct(ulf_io_t *io, json_t *in);
json_t *ulf_incarnation(ulf_io_t *io, json_t *in);
json_t *ulf_identity_live(ulf_io_t *io, json_t *in);
json_t *ulf_parse_marker(ulf_io_t *io, json_t *in);
json_t *ulf_judge_marker(ulf_io_t *io, json_t *in);
json_t *ulf_release_decision(ulf_io_t *io, json_t *in);
json_t *ulf_live_partners(ulf_io_t *io, json_t *in);
json_t *ulf_reclaim_dead(ulf_io_t *io, json_t *in);
/* State-mutating port: in.self / in.claims are mutated in place. */
json_t *ulf_claim_marker(ulf_io_t *io, json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* PORT_UPDATE_LOCK_FLOW_H */
