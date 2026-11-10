/*
 * terminal_cmd.h — terminal command helpers.
 *
 * Module-internal header (terminal.c <-> terminal_cmd.c).
 */
#ifndef TERMINAL_CMD_H
#define TERMINAL_CMD_H

#include <stdbool.h>
#include <stddef.h>
#include "libjson/json.h"   /* json_t */

char *run_command(const char *command, int timeout_sec);
int ssh_sync_upload(const char *ssh_host, const char *ssh_user, int ssh_port, const char *ssh_key, const char *remote_home);
int ssh_sync_download(const char *ssh_host, const char *ssh_user, int ssh_port, const char *ssh_key, const char *remote_home);
void _inject_sudo_failure(json_t *rj, const char *command);
bool looks_like_env_assignment(const char *token);

#endif /* TERMINAL_CMD_H */
