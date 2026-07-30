/**
 * @file hermes_gateway_forensics.h
 * @brief Shutdown forensics API (port of Python gateway/shutdown_forensics.py).
 */
#ifndef HERMES_GATEWAY_FORENSICS_H
#define HERMES_GATEWAY_FORENSICS_H

#include "hermes_gateway_types.h"
#include "hermes_json.h"

/* ================================================================
 *  Shutdown Forensics
 * ================================================================ */

/* Fast (<10ms) snapshot of shutdown context: signal, proc info, systemd state.
 * Returns a json_node_t (caller must json_free), or NULL on error. */
json_node_t *forensics_snapshot_context(int received_signal);

/* Fire-and-forget ps-style diagnostic snapshot written to log_path.
 * Returns child PID on success, -1 on failure. */
int forensics_spawn_diagnostic(const char *log_path,
                                const char *signal_name,
                                int timeout_seconds);

/* Render shutdown context dict as a single scannable log line.
 * Returns malloc'd string (caller must free). */
char *forensics_format_context(json_node_t *ctx);

/* JSON-serialise context dict for structured ingestion.
 * Returns malloc'd string (caller must free). Never returns NULL. */
char *forensics_context_to_json(json_node_t *ctx);

/* Parse systemd duration string to microseconds (e.g. "1min 30s" -> 90000000).
 * Returns 0 on unparseable input. */
long forensics_parse_systemd_duration_us(const char *raw);

/* At startup, sanity-check systemd TimeoutStopSec >= drain_timeout.
 * Returns json_node_t with mismatch info, or NULL if not applicable.
 * Caller must json_free the result. */
json_node_t *forensics_check_systemd_timing(double drain_timeout);

#endif /* HERMES_GATEWAY_FORENSICS_H */