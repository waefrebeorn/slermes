/*
 * port_checkpoints_format.h — public API for the pure hermes_cli/checkpoints.py
 * format helpers. Opaque, minimal includes.
 */

#ifndef PORT_CHECKPOINTS_FORMAT_H
#define PORT_CHECKPOINTS_FORMAT_H

#include <stddef.h>

/* Human-readable byte count ("512 B", "1.5 MB", ...). (PoP: _fmt_bytes) */
void hermes_cli_checkpoints_fmt_bytes(long n, char *out, size_t outsz);

/* Relative age ("5s ago", "3m ago", "2h ago", "4d ago", "now", "—").
 * now is passed in for testability. (PoP: _fmt_age) */
void hermes_cli_checkpoints_fmt_age(double ts, double now, char *out, size_t outsz);

/* Absolute timestamp "YYYY-MM-DD HH:MM" (UTC); "—" on invalid/zero/NaN.
 * (PoP: _fmt_ts) */
void hermes_cli_checkpoints_fmt_ts(double ts, char *out, size_t outsz);

#endif /* PORT_CHECKPOINTS_FORMAT_H */
