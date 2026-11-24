#ifndef NOC_W16_TERMINATION_EF34AC_H
#define NOC_W16_TERMINATION_EF34AC_H

#include "hermes_json.h"

/* C port of selected pieces of hermes_cli/termination_guard.py.
 * Returned json_t* values are caller-owned (json_free). */

/* _exit_on_signal(signum, frame) raises SystemExit(128 + signum). */
int w16_termination_ef34ac_exit_status(int signum);

/* TERMINATION_SIGNALS: SIGTERM(15) then SIGHUP(1), each only when present in
 * the injected `available` array (Windows has no SIGHUP). */
json_t *w16_termination_ef34ac_termination_signals(json_t *available);

/* The installation decision of unwind_on_termination():
 * {"installed": [signum, ...]}.  prev_kinds is parallel to signals with
 * "DFL" (SIG_DFL), "IGN" (SIG_IGN) or a custom-handler label. */
json_t *w16_termination_ef34ac_guard_plan(int is_main, json_t *signals,
                                          json_t *prev_kinds);

#endif
