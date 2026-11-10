/*
 * gateway_status_proc.h - gateway process inspection helpers.
 *
 * Module-internal header (status.c <-> gateway_status_proc.c).
 */
#ifndef GATEWAY_STATUS_PROC_H
#define GATEWAY_STATUS_PROC_H

#include <stddef.h>
#include <stdbool.h>
#include <signal.h>
#include "hermes_json.h"   /* json_t */

#define GATEWAY_KIND               "hermes-gateway"

void free_tokens(char **argv, int argc);
bool gwstatus_pid_exists(pid_t pid);
bool looks_like_gateway_process(pid_t pid);
char * read_process_cmdline(pid_t pid);
bool record_looks_like_gateway(json_t *record);
bool record_matches_live_gateway_pid(json_t *record, pid_t pid, const char *expected_home);
int split_command(const char *command, char **argv, int max);

#endif /* GATEWAY_STATUS_PROC_H */
