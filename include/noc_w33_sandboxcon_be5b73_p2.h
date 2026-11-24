/*
 * noc_w33_sandboxcon_be5b73_p2.h - C surface for upstream
 * tools/environments/sandbox_conformance.py, part 2 (gate
 * w33_sandboxcon_be5b73_p2).
 *
 * What the host-touch checks decide from what they observed: the path
 * predicate that says a host path is watched, the audit filter that turns a
 * host operation into a report line, the audit event table, and the verdict of
 * the two "never touch the host" invariants over recorded observations.
 *
 * The observation itself (audit hook, os.stat/lstat/readlink wrapping, the
 * planted file, the tools) is a seam fed as fixture input.
 */
#ifndef NOC_W33_SANDBOXCON_BE5B73_P2_H
#define NOC_W33_SANDBOXCON_BE5B73_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of audit events that record a host touch. */
#define W33_SANDBOXCON_BE5B73_P2_AUDIT_EVENTS 3

/* req: {"roots": [str, ...], "path": str | {"bytes": str} | {"pathlike": str}
 *       | {"fd": n} | null} -> bool (upstream _watched). */
json_t *w33_sandboxcon_be5b73_p2_watched(const json_t *req);

/* req: {"roots": [str, ...], "events": [{"event": str, "arg": <path>} | {"event": str}]}
 * -> [{"event", "watched": bool, "recorded": bool, "record": str|null}, ...]
 * where record is Python's f"{event} {args[0]}" for a recorded event
 * (upstream _audit). */
json_t *w33_sandboxcon_be5b73_p2_audit(const json_t *req);

/* The audit event names that record: ["open", "os.listdir", "os.scandir"]
 * (upstream _audit's event test). */
json_t *w33_sandboxcon_be5b73_p2_audit_events(void);

/* obs: {"mode": "file_tools"|"host_backend", "agent": {"tenant","agent"},
 *       "touched": [...], "outputs": [...], "marker": str,
 *       "content_after": str, "stat_before": {..}, "stat_after": {..},
 *       "calls": [{"op","sandbox":{"tenant","agent"},"command","stdin"}],
 *       "paths": [str, ...]} -> bool
 * The verdict of check_file_tools_never_touch_host (file_tools) or
 * check_host_backend_refused (host_backend) over those observations. */
json_t *w33_sandboxcon_be5b73_p2_host_touch_verdict(const json_t *obs);

#ifdef __cplusplus
}
#endif
#endif
