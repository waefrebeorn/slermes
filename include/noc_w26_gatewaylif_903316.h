/*
 * noc_w26_gatewaylif_903316.h - C surface for upstream
 * hermes_cli/gateway_lifecycle_guards.py (gate w26_gatewaylif_903316).
 *
 * The refusal core of ``hermes gateway stop|restart``: how a live-work answer is
 * rendered into the lines that name every session about to be interrupted, which
 * of several candidate homes answers first, and the short formatters those lines
 * are built from.  Every I/O seam (the control socket, the state file, the cron
 * job registry, stdout/isatty) stays in the caller; this file owns only what the
 * values decide.
 */
#ifndef NOC_W26_GATEWAYLIF_903316_H
#define NOC_W26_GATEWAYLIF_903316_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* seconds -> "45s" | "1m05s" | "?" (int(float(seconds)), floor division). */
json_t *w26_gatewaylif_903316_elapsed(const json_t *seconds);

/* unit -> one human line ("chat turn s-1 pid 42, running 1m05s [model m]").
 * The cron job NAME is a file seam (the profile's jobs.json) and is absent here,
 * so a cron unit renders by id alone. */
json_t *w26_gatewaylif_903316_describe_unit(const json_t *unit);

/* req: {verb, home, flag_hint?, work:{active_agents, active_work[]}} ->
 * the refusal lines for one answering gateway, or [] when it names no live work.
 * ``home`` is only consulted for cron job names, which stay absent (seam). */
json_t *w26_gatewaylif_903316_work_lines(const json_t *req);

/* req: {verb, homes:[...], answers:[work|null, ...], flag_hint?} ->
 * the refusal lines for the FIRST home whose answer is not null, else [].
 * ``answers`` is the fixed input standing in for live_work()/control socket. */
json_t *w26_gatewaylif_903316_refusal_lines(const json_t *req);

/* Module-level literals. */
json_t *w26_gatewaylif_903316_constants(void);

#ifdef __cplusplus
}
#endif
#endif
