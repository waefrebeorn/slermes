/*
 * gateway_status_paths.h - runtime status/pid/lock/marker file paths.
 *
 * Module-internal header (status.c <-> gateway_status_paths.c).
 */
#ifndef GATEWAY_STATUS_PATHS_H
#define GATEWAY_STATUS_PATHS_H

#include <stddef.h>
#include <stdbool.h>

void ensure_parent_dir(const char *file_path);
char * get_gateway_lock_path(const char *pid_path, char *buf, size_t n);
char * get_lock_dir(char *buf, size_t n);
char * get_pid_path(char *buf, size_t n);
char * get_planned_stop_marker_path(char *buf, size_t n);
char * get_runtime_status_path(char *buf, size_t n);
char * get_scope_lock_path(const char *scope, const char *identity, char *buf, size_t n);
char * get_takeover_marker_path(char *buf, size_t n);
void scope_hash(const char *identity, char out[17]);
void utc_now_iso_buf(char *buf, size_t n);

#endif /* GATEWAY_STATUS_PATHS_H */
