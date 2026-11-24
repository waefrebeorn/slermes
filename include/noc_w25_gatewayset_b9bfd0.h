/*
 * noc_w25_gatewayset_b9bfd0.h - C surface for upstream
 * hermes_cli/gateway_setting_commands.py (gate w25_gatewayset_b9bfd0).
 *
 * The pure decision core of the classic CLI's per-session setting commands on
 * the shared gateway.  The connection, the socket and the gateway's own
 * handler stay in the caller (the seam): what is ported here is what the
 * values decide -- which canonical names the gateway owns, how the command
 * line splits into (canonical, arg), which args mirror local client state,
 * which forms lose their deadline, and how a reply's lines are printed.
 */
#ifndef NOC_W25_GATEWAYSET_B9BFD0_H
#define NOC_W25_GATEWAYSET_B9BFD0_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* canonical name -> true when the gateway owns it (GATEWAY_SETTINGS). */
json_t *w25_gatewayset_b9bfd0_is_setting(const json_t *canonical);

/* cmd -> the text after the canonical word, stripped ("" when absent). */
json_t *w25_gatewayset_b9bfd0_split_arg(const json_t *cmd);

/* arg -> the mirrored client busy mode, or null when the arg is not one. */
json_t *w25_gatewayset_b9bfd0_mirror_busy(const json_t *arg);

/* (canonical, arg, cron_read) -> true when the call must have no deadline.
 * cron_read is the seam: null means the owner answered "read" (None). */
json_t *w25_gatewayset_b9bfd0_unbounded(const json_t *canonical, const json_t *arg,
                                        const json_t *cron_read);

/* (canonical, arg) -> the "slash.exec" command string. */
json_t *w25_gatewayset_b9bfd0_call_command(const json_t *canonical, const json_t *arg);

/* (canonical, reply, stopped) -> the lines printed, or null when the command
 * stopped before any reply was rendered. */
json_t *w25_gatewayset_b9bfd0_render(const json_t *canonical, const json_t *reply,
                                     const json_t *stopped);

/* The command tables as an object. */
json_t *w25_gatewayset_b9bfd0_constants(void);

#ifdef __cplusplus
}
#endif
#endif
