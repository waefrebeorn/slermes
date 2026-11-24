#ifndef NOC_W20_SLASHWORKE_CDC456_H
#define NOC_W20_SLASHWORKE_CDC456_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tui_gateway/slash_worker_client.py (gate w20_slashworke_cdc456).
 *
 * The gateway side of the persistent slash worker: the child's PATH is
 * composed from its parts, the JSON-lines reply loop is replayed against a
 * scripted message queue, and the stop plan is chosen from the platform and
 * the child's liveness.  Spawning, pipes and signals stay with the caller.
 */

/* PoP: w20_slashworke_cdc456_prepend_tool_paths @ tui_gateway/slash_worker_client.py:_prepend_tool_paths
 * Request: {"env_path": str|null, "managed_home": str|null, "managed_error": bool,
 *           "executable": str, "home_dir": str, "store_first": bool}
 *   "managed_bin" is `str(Path(get_hermes_home())/"bin")` resolved by the
 *   caller; it is empty when that read failed.  "executable" is
 *   sys.executable and "home_dir" is Path.home(), from which the venv bin dir
 *   and ~/.local/bin are derived.
 * Answer: {"path": "<managed bin>;<venv bin>;...", "store_first": bool}.
 * Empty parts are dropped (Python's `if p`), the managed-bin read is suppressed
 * on failure, and `;` is this host's os.pathsep.  A present `pm` store applies
 * its store-first ordering to the joined PATH. */
json_t *w20_slashworke_cdc456_prepend_tool_paths(const json_t *req);

/* PoP: w20_slashworke_cdc456_run_reply @ tui_gateway/slash_worker_client.py:_SlashWorker.run
 * Request: {"seq": int, "exited": bool, "stderr_tail": [str, ...],
 *           "messages": [null | {"id": int, "ok": bool, "error": str,
 *                                "seed": <json>, "output": <json>}, ...]}
 * Answer: {"status": "ok", "output": <rstripped str>, "seed": <str>} or
 *         {"status": "raised", "type": "RuntimeError", "message": <text>}.
 * A None message ends the stream (closed pipe, with the last 8 stderr lines),
 * a foreign id is skipped, ok falsy raises, and an empty queue times out. */
json_t *w20_slashworke_cdc456_run_reply(const json_t *req);

/* PoP: w20_slashworke_cdc456_closed_pipe @ tui_gateway/slash_worker_client.py:_SlashWorker.run
 * `slash worker closed pipe[: <last 8 stderr lines>]`.
 * Request: {"stderr_tail": [str, ...]}. */
json_t *w20_slashworke_cdc456_closed_pipe(const json_t *req);

/* PoP: w20_slashworke_cdc456_stop_plan @ tui_gateway/slash_worker_client.py:_stop_worker_group
 * Request: {"posix": bool, "alive": bool}.
 * Answer: {"signals": ["terminate","kill"] | ["SIGTERM","SIGKILL"] | [],
 *          "released": bool} -- the child is stopped (never raises) and, on
 * POSIX, its process group is released once empty. */
json_t *w20_slashworke_cdc456_stop_plan(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_SLASHWORKE_CDC456_H */
