/*
 * api_server_http.c - HTTP request/response plumbing for the API server.
 *
 * Split from api_server.c (soul.md: self-contained modules). Request-line
 * and body parsing, query-parameter extraction and the response writers.
 * No file-scope state.
 */
#include "api_server_http.h"
#include "hermes_json.h"
#include "hermes_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include "registry.h"
#include "hermes_skills.h"


/* ── HTTP response helpers ──────────────────────────────────────── */

void send_response(int fd, int status, const char *status_text,
                           const char *body, const char *content_type) {
    char header[512];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, status_text, content_type ? content_type : "application/json",
        body ? strlen(body) : 0);

    write(fd, header, (size_t)n);
    if (body) write(fd, body, strlen(body));
}


void api_send_json(int fd, int status, const char *status_text,
                       const char *json_body) {
    send_response(fd, status, status_text, json_body, "application/json");
}


void send_error(int fd, int status, const char *message) {
    char body[4096];
    snprintf(body, sizeof(body),
        "{\"error\":{\"message\":\"%s\",\"type\":\"error\",\"code\":%d}}",
        message, status);
    api_send_json(fd, status, message, body);
}


/* ── HTTP request parser (minimal) ──────────────────────────────── */

/**
 * Parse the HTTP method and path from the first line.
 * Returns 0 on success.
 */
int parse_request_line(const char *buf, char *method, size_t mlen,
                                char *path, size_t plen) {
    if (!buf || !method || !path) return -1;
    method[0] = path[0] = '\0';

    const char *p = buf;

    /* Read method */
    while (*p && *p != ' ' && *p != '\r' && *p != '\n' && mlen > 1) {
        *method++ = *p++;
        mlen--;
    }
    *method = '\0';
    if (*p == ' ') p++;

    /* Read path */
    while (*p && *p != ' ' && *p != '\r' && *p != '\n' && *p != '?' && plen > 1) {
        *path++ = *p++;
        plen--;
    }
    *path = '\0';

    return 0;
}


/**
 * Find the request body (after \r\n\r\n).
 * Returns pointer to body, or NULL if not found.
 */
const char *find_body(const char *buf) {
    const char *p = strstr(buf, "\r\n\r\n");
    if (!p) {
        p = strstr(buf, "\n\n");
        if (!p) return NULL;
        return p + 2;
    }
    return p + 4;
}


/* ── Route handlers ─────────────────────────────────────────────── */

/**
 * Parse query string from request buffer into a simple key=value map.
 * Extracts the portion after '?' in the request line.
 */
void parse_query_params(const char *buf, char *query, size_t qlen) {
    query[0] = '\0';
    const char *q = strchr(buf, '?');
    if (!q) return;
    q++; /* skip '?' */
    const char *space = strchr(q, ' ');
    size_t len = space ? (size_t)(space - q) : strlen(q);
    if (len >= qlen) len = qlen - 1;
    memcpy(query, q, len);
    query[len] = '\0';
}


/**
 * Get a query parameter value by name.
 * Returns pointer to value or NULL if not found.
 */
const char *get_query_param(const char *query, const char *name) {
    if (!query || !query[0] || !name) return NULL;
    size_t nlen = strlen(name);

    const char *p = query;
    while (*p) {
        /* Skip leading '&' if not first */
        if (p != query && *p == '&') p++;

        if (strncmp(p, name, nlen) == 0 && p[nlen] == '=') {
            return p + nlen + 1;
        }

        /* Skip to next param */
        p = strchr(p, '&');
        if (!p) break;
        p++;
    }
    return NULL;
}


/* ── Path helper ────────────────────────────────────────────────── */

/**
 * Check if path starts with a prefix and extract the suffix.
 * E.g. starts_with("/v1/sessions/abc123", "/v1/sessions/", id, 64) → true, id="abc123"
 */
bool starts_with(const char *path, const char *prefix, char *suffix, size_t slen) {
    size_t plen = strlen(prefix);
    if (strncmp(path, prefix, plen) != 0) return false;
    const char *rest = path + plen;
    if (!rest[0]) return false; /* path = prefix, no suffix */
    strncpy(suffix, rest, slen - 1);
    suffix[slen - 1] = '\0';
    return true;
}
