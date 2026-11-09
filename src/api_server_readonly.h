/*
 * api_server_readonly.h - read-only HTTP handlers.
 *
 * Module-internal header (api_server.c <-> api_server_readonly.c).
 */
#ifndef API_SERVER_READONLY_H
#define API_SERVER_READONLY_H

#include <stddef.h>
#include <stdbool.h>

void handle_capabilities(int fd);
void handle_delete_response(int fd, const char *response_id);
void handle_options(int fd);
void handle_skills(int fd);

#endif /* API_SERVER_READONLY_H */
