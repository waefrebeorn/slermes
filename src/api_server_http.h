/*
 * api_server_http.h - HTTP request/response plumbing for the API server.
 *
 * Module-internal header (api_server.c <-> api_server_http.c).
 */
#ifndef API_SERVER_HTTP_H
#define API_SERVER_HTTP_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

const char * find_body(const char *buf);
const char * get_query_param(const char *query, const char *name);
void parse_query_params(const char *buf, char *query, size_t qlen);
int parse_request_line(const char *buf, char *method, size_t mlen, char *path, size_t plen);
void send_error(int fd, int status, const char *message);
void api_send_json(int fd, int status, const char *status_text, const char *json_body);
void send_response(int fd, int status, const char *status_text, const char *body, const char *content_type);
bool starts_with(const char *path, const char *prefix, char *suffix, size_t slen);

#endif /* API_SERVER_HTTP_H */
