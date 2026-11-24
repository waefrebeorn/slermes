/*
 * noc_w21_computerus_9582b9.h -- public C API for the C11 port of upstream Hermes
 *   hermes_cli/subcommands/computer_use_screen.py
 *
 * `hermes computer-use screen` status/start/stop/install. The handlers read a
 * live Bot Desktop runtime (tools.bot_desktop.runtime/lease/install), so the
 * port takes that state as an injected JSON object -- the same seam the Python
 * code pulls through module attributes -- and reproduces the printed report and
 * the process exit code verbatim.
 *
 * Every exported symbol carries the w21_computerus_9582b9_ prefix.
 */
#ifndef NOC_W21_COMPUTERUS_9582B9_H
#define NOC_W21_COMPUTERUS_9582B9_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * _screen_status(). Input keys:
 *   json(bool), supported, installed, running, blocker(str|null),
 *   missing(array of str), install_command(str|null), profile, display,
 *   geometry, pid(number), socket, as_dict(object), lease_view(object)
 *   (lease_view is lease.public_view(lease.get()); holder lives inside it)
 * Text mode -> {"rc":int,"text":str}; --json mode -> {"rc":int,"payload":object}
 * (payload = {**st.as_dict(), "lease": lease.public_view(lease.get())}).
 */
json_t *w21_computerus_9582b9_status(const json_t *state);

/* _screen_start(): error(str|null) + profile/display/geometry -> {"rc","text"}. */
json_t *w21_computerus_9582b9_start(const json_t *state);

/* _screen_stop(): supported, released_holder, stopped, force -> {"rc","text"}. */
json_t *w21_computerus_9582b9_stop(const json_t *state);

/*
 * _screen_install(): supported, missing(array), after_missing(array),
 * command(str|null), rc(number), busy(bool), busy_message, no_sudo(bool),
 * tty(bool), answer(str), yes(bool) -> {"rc","text"}.
 */
json_t *w21_computerus_9582b9_install(const json_t *state);

/* SCREEN_ACTIONS: action -> handler name, sorted keys. */
json_t *w21_computerus_9582b9_actions(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_COMPUTERUS_9582B9_H */
