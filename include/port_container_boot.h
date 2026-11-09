/*
 * port_container_boot.h -- pure argv helpers ported from
 * hermes_cli/container_boot.py.
 *
 * The filesystem / /proc / s6 service-slot half stays in Python.
 */
#ifndef PORT_CONTAINER_BOOT_H
#define PORT_CONTAINER_BOOT_H

#include <stddef.h>

/* container_boot.py:_strip_container_argv_prefix -- drop everything through
 * the main-wrapper.sh token (then an optional entrypoint-dispatch.sh), leaving
 * the hermes args. Returns a newly allocated NULL-terminated array of BORROWED
 * pointers; free the ARRAY only. *out_count receives the length. */
const char **cb_strip_argv_prefix(const char *const *argv, int *out_count);

/* container_boot.py:_is_legacy_gateway_run_request -- the stripped argv is
 * exactly `gateway run` and does not carry --no-supervise. */
int cb_is_legacy_gateway_run(const char *const *argv);

/* container_boot.py:_is_dashboard_container -- the stripped argv starts with
 * `dashboard` (a dashboard container never supervises gateways). */
int cb_is_dashboard_container(const char *const *argv);

/* container_boot.py:boot_notice -- the one-line notice naming the folded
 * profiles, sorted and comma-joined. Caller frees. */
char *cb_boot_notice(const char *const *folded);

#endif /* PORT_CONTAINER_BOOT_H */
