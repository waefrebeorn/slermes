/*
 * noc_w22_sessionwor_277372.h - C surface for upstream
 * gateway/session_worker_reservation.py (gate w22_sessionwor_277372).
 *
 * Owner-side proof that the process behind an admission is the interpreter the
 * worker claims to be, plus the reservation calls built on that proof.
 *
 * Seams (injected by the harness, never decided here): process liveness
 * (Popen.poll), the process table facts (ppid/status/running/parents), the
 * kernel birth from hermes_cli.process_identity, the OS name/uid/SID, the
 * admission-open guard and the persistence store.
 */
#ifndef NOC_W22_SESSIONWOR_277372_H
#define NOC_W22_SESSIONWOR_277372_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MAX_LAUNCHER_HOPS: a venv launcher may sit between the owner and the worker. */
#define W22_SESSIONWOR_277372_MAX_HOPS 3

int w22_sessionwor_277372_hops(void);

/* world keys: process_live, process_pid, owner_pid, hello{type,pid,birth,ancestors},
 * handle_ppid, handle_zombie, worker_birth, worker_running, worker_zombie,
 * psutil_error, parents[].  Returns {"status":"verified","pid","birth"} or
 * {"status":"refused","reason"}. */
json_t *w22_sessionwor_277372_verify(const json_t *world);

/* nt -> "sid:<sid>", anything else -> "uid:<uid>".  Caller frees. */
char *w22_sessionwor_277372_owner_subject(const char *os_name, const char *uid,
                                          const char *sid);

/* req keys: epoch, admission_id, principal_id, profile_id, pid, birth, secret,
 * owner, worker_subject -> the exact store kwargs. */
json_t *w22_sessionwor_277372_reserve(const json_t *req);

/* req keys: row{admission_id}, scope{epoch,execution_id,generation}. */
json_t *w22_sessionwor_277372_lose(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
