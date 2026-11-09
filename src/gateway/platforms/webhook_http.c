/*
 * webhook_http.c - webhook HTTP request/response helpers.
 *
 * Split from webhook.c (soul.md: self-contained modules). Request-line
 * parsing, body extraction, header/HMAC building and response writing.
 * No file-scope state.
 */
#include "webhook_http.h"
#include "libcrypto/crypto.h"
#include "hermes_encode.h"
#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_gateway_core.h"
#include "hermes_gateway_whatsapp.h"
#include "hermes_gateway_sms.h"
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



/* ================================================================
 *  HTTP response builder
 * ================================================================ */ char *build_http_response(int status, const char *status_text,
                                  const char *content_type,
                                  const char *body) {
    int blen = strlen(body);
    /* header: ~256 bytes + body */
    int total = 256 + blen + 1;
    char *resp = malloc(total);
    if (!resp) return NULL;

    int n = snprintf(resp, total,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, X-Hub-Signature-256\r\n"
        "\r\n"
        "%s",
        status, status_text,
        content_type,
        blen,
        body);
    if (n < 0) { free(resp); return NULL; }
    return resp;
}


/* ================================================================
 *  HTTP request body extractor
 * ================================================================
 * Returns pointer to body within buf (after \r\n\r\n), or NULL.
 */ const char *extract_http_body(const char *buf) {
    const char *body = strstr(buf, "\r\n\r\n");
    return body ? body + 4 : NULL;
}


/* ================================================================
 *  Extract HTTP method + path
 * ================================================================
 * Parses "POST /webhook HTTP/1.1" from buf.
 * Returns -1 on error.
 */ int parse_http_request_line(const char *buf, char *method, size_t msize,
                                    char *path, size_t psize) {
    /* Skip leading whitespace */
    while (*buf == ' ' || *buf == '\t' || *buf == '\r' || *buf == '\n') buf++;
    if (!*buf) return -1;

    /* Read method up to space */
    size_t i = 0;
    while (*buf && *buf != ' ' && *buf != '\r' && i < msize - 1) {
        method[i++] = *buf++;
    }
    method[i] = '\0';
    if (*buf != ' ') return -1;
    buf++; /* skip space */

    /* Read path up to space */
    i = 0;
    while (*buf && *buf != ' ' && *buf != '\r' && i < psize - 1) {
        path[i++] = *buf++;
    }
    path[i] = '\0';
    return 0;
}


/* ================================================================
 *  P112: Build custom headers string from subscription
 * ================================================================
 * Returns a malloc'd string like "Key1: Val1\r\nKey2: Val2\r\n"
 * or NULL if no custom headers. Caller must free().
 */ char *build_custom_headers_str(const webhook_subscription_t *sub) {
    if (!sub || sub->header_count == 0) return NULL;

    /* Estimate: each header ~ key(128) + ": " + value(512) + "\r\n" = ~650 */
    size_t cap = sub->header_count * 650 + 1;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    size_t pos = 0;

    for (int i = 0; i < sub->header_count; i++) {
        int n = snprintf(out + pos, cap - pos, "%s: %s\r\n",
                         sub->headers[i].key, sub->headers[i].value);
        if (n < 0 || (size_t)n >= cap - pos) {
            free(out);
            return NULL;
        }
        pos += (size_t)n;
    }
    return out;
}


/* ================================================================
 *  P112: Build HMAC signature header for outgoing webhook
 * ================================================================
 * Returns a malloc'd header line like "X-Hub-Signature-256: sha256=<hex>\r\n"
 * or NULL if no secret configured. Caller must free().
 */ char *build_hmac_header_line(const webhook_subscription_t *sub,
                                     const char *payload) {
    if (!sub || !sub->hmac_secret[0] || !payload) return NULL;

    unsigned char sig[WH_SHA256_LEN];
    size_t secret_len = strlen(sub->hmac_secret);
    size_t pay_len = strlen(payload);

    crypto_hmac_sha256((const unsigned char *)sub->hmac_secret, secret_len,
                       (const unsigned char *)payload, pay_len, sig);

    char *hex = hex_encode_alloc(sig, WH_SHA256_LEN);
    if (!hex) return NULL;

    /* "X-Hub-Signature-256: sha256=" + 64 hex chars + "\r\n" = ~95 */
    char *line = (char *)malloc(128);
    if (!line) { free(hex); return NULL; }
    snprintf(line, 128, "X-Hub-Signature-256: sha256=%s\r\n", hex);
    free(hex);
    return line;
}
