/*
 * api_server_sse.h - SSE response helpers.
 *
 * Module-internal header (api_server.c <-> api_server_sse.c).
 */
#ifndef API_SERVER_SSE_H
#define API_SERVER_SSE_H

#include <stddef.h>
#include <stdbool.h>

void send_sse_event(int fd, const char *data);
void sse_send_chunk(int fd, const char *content, int index);

#endif /* API_SERVER_SSE_H */
