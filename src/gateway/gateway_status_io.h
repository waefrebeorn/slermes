/*
 * gateway_status_io.h - status file / record IO helpers.
 *
 * Module-internal header (status.c <-> gateway_status_io.c).
 */
#ifndef GATEWAY_STATUS_IO_H
#define GATEWAY_STATUS_IO_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"   /* json_t */
#include "gateway_status_paths.h"
#include "gateway_status_proc.h"


json_t * build_pid_record(void);
void cleanup_invalid_pid_path(const char *pid_path, bool cleanup_stale);
long gwstatus_get_process_start_time(pid_t pid);
pid_t pid_from_record(json_t *record);
bool proc_is_stopped(pid_t pid);
json_t * read_gateway_lock_record(const char *lock_path);
json_t * read_json_file(const char *path);
json_t * read_pid_record(const char *pid_path);
long record_start_time(json_t *rec);
void write_gateway_lock_record(int fd);
int write_json_file(const char *path, json_t *payload);

#endif /* GATEWAY_STATUS_IO_H */
