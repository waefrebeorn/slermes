/*
 * web_dashboard_http.h - dashboard HTTP request/response helpers.
 *
 * Module-internal header (web_dashboard.c <-> web_dashboard_http.c).
 */
#ifndef WEB_DASHBOARD_HTTP_H
#define WEB_DASHBOARD_HTTP_H

#include <stddef.h>
#include <stdbool.h>
#include "hermes_web_dashboard.h"   /* g_session_token */

extern char g_dash_web_dir[1024];   /* set by dashboard_init() */
#ifndef DASH_BUF
#define DASH_BUF 65536
#endif

typedef struct {
    char method[16], path[1024], body[16384];
    size_t body_len;
    uint32_t client_ip; /* Network byte order from accept() */
    char raw_buf[DASH_BUF]; /* Full request buffer for header extraction */
    size_t raw_len;
} http_req_t;


const char * extract_headers(const char *buf);
void generate_session_token(void);
bool has_valid_token(const char *headers);
bool is_loopback_client(uint32_t client_ip);
bool is_public_path(const char *path);
int parse_req(int fd, http_req_t *req, uint32_t client_ip);
char * read_file_to_buf(const char *path, size_t max_size);
void send_err(int fd, int status, const char *msg);
void send_json(int fd, int status, const char *st, const char *body);
void send_resp(int fd, int status, const char *st, const char *body, const char *ctype);
void serve_static(int fd, const char *p);

#endif /* WEB_DASHBOARD_HTTP_H */
