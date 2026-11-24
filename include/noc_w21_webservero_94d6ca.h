#ifndef NOC_W21_WEBSERVERO_94D6CA_H
#define NOC_W21_WEBSERVERO_94D6CA_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/web_server_owner_exit.py (gate w21_webservero_94d6ca).
 *
 * Supersession retirement for Desktop-owned `hermes serve --isolated` backends
 * reached over SSH: the retire predicate (a VALID lock names another spawn and
 * this process has outlived the reaper's settle window), the observe message,
 * and the shared retirement watchdog loop (two consecutive observations, then
 * the fence must both prepare and commit before `should_exit`).
 *
 * Not ported (bound to the filesystem, threads and the backend retirement
 * fence): `read_valid_backend_lock` (reads and validates backend.lock.json) and
 * `fence.prepare()/commit()` (the live admission/teardown fence). Both are the
 * simulator's scripted inputs; `time.sleep` is scripted as a count.
 */

/* hermes_cli/web_server_owner_exit.py: DEFAULT_OWNER_POLL_S */
double w21_webservero_94d6ca_default_poll(void);

/* PoP: w21_webservero_94d6ca_should_retire @ hermes_cli/web_server_owner_exit.py:should_retire_superseded
 * `lock is not None and lock["spawnNonce"] != my_nonce and age_s >= 180.0`.
 *   true | false | {"raised":"KeyError","message":"'spawnNonce'"} */
json_t *w21_webservero_94d6ca_should_retire(const json_t *lock, const char *my_nonce,
                                            double age_s);

/* PoP: w21_webservero_94d6ca_observe_message @ hermes_cli/web_server_owner_exit.py:start_owner_watchdog
 * `f"SSH-isolated backend {nonce} was superseded by spawn {lock['spawnNonce']}; retiring."` */
char *w21_webservero_94d6ca_observe_message(const char *nonce, const json_t *spawn_nonce);

/* PoP: w21_webservero_94d6ca_simulate @ hermes_cli/web_server_owner_exit.py:_observe
 * The watchdog as a bounded, replayable loop. `req` carries:
 *   nonce, max_polls, polls[{lock, age_s}], fences[{prepare{ok,token},commit{ok}}]
 * The fence attempts are handed out in order, one per `prepare()`.
 *   {"retired":bool,"polls":N,"prepare_calls":N,"commit_calls":N,"sleeps":N,
 *    "message":"..."|null} */
json_t *w21_webservero_94d6ca_simulate(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_WEBSERVERO_94D6CA_H */
