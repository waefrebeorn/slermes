/*
 * noc_w23_terminalto_b54e19.h - C surface for upstream
 * tools/terminal_tool_sudo_askpass.py (gate w23_terminalto_b54e19).
 *
 * Askpass delivery of the sudo password: the shell prologue that serves one
 * password line per counted sudo through a FIFO helper, the timeout token that
 * BaseEnvironment fills in once it knows the command's timeout, the option-word
 * reading that decides whether a command keeps the stdin form, and the cached
 * capability decision.
 *
 * Seams (injected by the harness, never decided here): the backend's
 * _run_bash/_wait_for_process probe, its capability flags, and platform.system.
 */
#ifndef NOC_W23_TERMINALTO_B54E19_H
#define NOC_W23_TERMINALTO_B54E19_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Extra seconds the watchdog waits past the command's own timeout. */
#define W23_TERMINALTO_B54E19_SLACK_S 30
/* Used when the effective timeout is not a number (the execute backstop uses 120 s then). */
#define W23_TERMINALTO_B54E19_FALLBACK_TIMEOUT_S 120

/* SUDO_ASKPASS_TIMEOUT_TOKEN, SUDO_ASKPASS_PREFIX, SUDO_ASKPASS_SUFFIX. */
const char *w23_terminalto_b54e19_timeout_token(void);
const char *w23_terminalto_b54e19_prefix(void);
const char *w23_terminalto_b54e19_suffix(void);
/* CAPABILITY_PROBE: the one-shot shell text the backend runs to select the form. */
const char *w23_terminalto_b54e19_capability_probe(void);

/* Shell text serving the stdin password line(s) to *count* askpass helper calls.
 * sudo_words is a JSON array of command words (NULL or empty -> ("sudo",)).
 * Caller frees the result. */
char *w23_terminalto_b54e19_prologue(long long count, const char *timeout_token,
                                     int background, const json_t *sudo_words);

/* Replace the prologue's timeout token with int(timeout)+30; every other command
 * text comes back byte-for-byte.  *timeout is any JSON scalar (number, bool,
 * string, null).  Caller frees the result. */
char *w23_terminalto_b54e19_fill_timeout(const char *command, const json_t *timeout);

/* True when sudo's option words (up to its command) include a stdin or askpass
 * option.  *words is a JSON array of raw words. */
json_t *w23_terminalto_b54e19_own_input(const json_t *words);

/* world keys: cached(null|bool), runtime_checked, probe_supported, is_local,
 * system, probe_timeout, run_rc, run_raises.
 * Returns {"supported":bool,"probed":bool,"cached_after":bool|null}. */
json_t *w23_terminalto_b54e19_supported_once(const json_t *world);

#ifdef __cplusplus
}
#endif
#endif
