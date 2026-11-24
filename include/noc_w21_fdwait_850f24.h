/*
 * noc_w21_fdwait_850f24.h -- POSIX fd-readiness helper shared by the stdout
 * drains and the PTY bridge (port of tools/environments/fd_wait.py).
 *
 * PoP: w21_fdwait_850f24_open_plan @ tools/environments/fd_wait.py:open_fd_waiter
 * PoP: w21_fdwait_850f24_wait_plan @ tools/environments/fd_wait.py:FdWaiter.wait
 *
 * The OS calls are the seam (registering an fd with the real epoll/kqueue/
 * select selector, fstat, sys.platform); the mapping from their outcome to a
 * safe plan is the module's own logic and is what this gate pins:
 *   - DefaultSelector succeeds            -> use it;
 *   - ValueError (fd out of range for select) -> None, nothing to drain;
 *   - OSError EBADF                        -> None, the fd is closed;
 *   - any other OSError (EMFILE)           -> PollSelector needs no fd of its
 *     own, but on macOS a device fd (PTY) would report POLLNVAL immediately and
 *     spin the reader -> raise there instead.
 */
#ifndef NOC_W21_FDWAIT_850F24_H
#define NOC_W21_FDWAIT_850F24_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* open_fd_waiter as a plan.
 * in: {"register": "ok"|"ValueError"|"EBADF"|"other",
 *      "errno_name": "EBADF"|"EMFILE"|..., "platform": "darwin"|"linux"|...,
 *      "is_char_device": bool}
 * -> {"kind": "select"|"poll"|null, "raised": null|"OSError",
 *     "closed": bool} */
json_t *w21_fdwait_850f24_open_plan(const json_t *in);

/* FdWaiter.wait as a plan: select() on a timeout clamped to at least 0,
 * and bool() of the ready list.
 * in: {"timeout": number, "ready": bool} -> {"arg": number, "result": bool} */
json_t *w21_fdwait_850f24_wait_plan(const json_t *in);

/* The timeout clamp on its own (both wait implementations share it). */
json_t *w21_fdwait_850f24_clamp_timeout(const json_t *timeout_s);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_FDWAIT_850F24_H */
