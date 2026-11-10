/*
 * webhook_http.h - webhook HTTP request/response helpers.
 *
 * Module-internal header (webhook.c <-> webhook_http.c).
 */
#ifndef WEBHOOK_HTTP_H
#define WEBHOOK_HTTP_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include "hermes_gateway_webhook.h"   /* webhook_subscription_t */

#define WH_SHA256_LEN 32

char * build_custom_headers_str(const webhook_subscription_t *sub);
char * build_hmac_header_line(const webhook_subscription_t *sub, const char *payload);
char * build_http_response(int status, const char *status_text, const char *content_type, const char *body);
const char * extract_http_body(const char *buf);
int parse_http_request_line(const char *buf, char *method, size_t msize, char *path, size_t psize);

#endif /* WEBHOOK_HTTP_H */
