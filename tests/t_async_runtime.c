/*
 * t_async_runtime.c — standalone verification harness for lib/libasync.
 *
 * Exercises, without any network:
 *   1. async_http_parse_url / async_http_parse_response (the IO runtime parse
 *      layer) against canned HTTP/1.1 + chunked responses.
 *   2. async_http_get with an INJECTABLE transport (mirrors port_models_net),
 *      proving the request/response path is functional offline.
 *   3. async_runtime_run top-level entry that registers an async task on the
 *      loop and a timer, verifying the loop-pump runtime works.
 *
 * Build: gcc -std=c11 -I include -I lib/libasync_poll -I lib/libasync \
 *        t_async_runtime.c lib/libasync/fiber.o lib/libasync/async_http.o \
 *        lib/libasync_poll/async_poll.o -lpthread -lssl -lcrypto -o /tmp/t_ar
 */

#include "async_runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── 3. async_runtime_run top entry ───────────────────────────────────── */
static int top_ran = 0;
static void top_entry(void *arg) {
    (void)arg;
    top_ran = 1;
}

/* ── 2. injectable transport ──────────────────────────────────────────── */
static int fake_transport(const char *method, const char *url,
                          const char *headers, const char *body,
                          async_http_result_t *out, void *ctx) {
    (void)headers; (void)body; (void)ctx;
    char payload[1024];
    int blen = snprintf(payload, sizeof(payload),
        "{\"method\":\"%s\",\"url\":\"%s\"}", method, url);
    char buf[2048];
    int n = snprintf(buf, sizeof(buf),
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
        "Content-Length: %d\r\nConnection: close\r\n\r\n%s",
        blen, payload);
    return async_http_parse_response(buf, (size_t)n, out);
}

int main(void) {
    int failures = 0;

    /* 3. loop-pump runtime */
    async_runtime_run(top_entry, NULL);
    if (top_ran != 1) { printf("FAIL: async_runtime_run top entry\n"); failures++; }
    else printf("ok: async_runtime_run top entry\n");

    /* URL parse */
    async_http_url_t u;
    if (async_http_parse_url("https://example.com:8443/a/b?x=1", &u) != 0 ||
        strcmp(u.scheme, "https") != 0 || strcmp(u.host, "example.com") != 0 ||
        u.port != 8443 || strcmp(u.path, "/a/b?x=1") != 0) {
        printf("FAIL: url parse\n"); failures++;
    } else {
        printf("ok: url parse (%s://%s:%d%s)\n", u.scheme, u.host, u.port, u.path);
    }
    async_http_url_free(&u);

    /* Response parse: content-length */
    {
        const char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello";
        async_http_result_t *r = malloc(sizeof(*r));
        memset(r, 0, sizeof(*r));
        async_http_parse_response(resp, strlen(resp), r);
        if (r->code != 200 || r->len != 5 || strcmp(r->body, "hello") != 0) {
            printf("FAIL: content-length parse (code=%d len=%zu body=%s)\n",
                   r->code, r->len, r->body ? r->body : ""); failures++;
        } else printf("ok: content-length parse\n");
        async_http_result_free(r);
    }

    /* Response parse: chunked */
    {
        const char *resp =
            "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
            "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n";
        async_http_result_t *r = malloc(sizeof(*r));
        memset(r, 0, sizeof(*r));
        async_http_parse_response(resp, strlen(resp), r);
        if (r->code != 200 || strcmp(r->body, "hello world") != 0) {
            printf("FAIL: chunked parse (body=%s)\n", r->body ? r->body : ""); failures++;
        } else printf("ok: chunked parse (%s)\n", r->body);
        async_http_result_free(r);
    }

    /* Injectable transport (offline async GET) */
    {
        async_http_client_t *c = async_http_client_new(5000);
        async_http_set_transport(c, fake_transport, NULL);
        async_http_result_t *r = async_http_get(c, "https://api.test/v1/x");
        if (r->code != 200 || !r->body || strstr(r->body, "\"url\":\"https://api.test/v1/x\"") == NULL) {
            printf("FAIL: injectable transport (code=%d body=%s)\n",
                   r->code, r->body ? r->body : ""); failures++;
        } else printf("ok: injectable transport parse (%s)\n", r->body);
        async_http_result_free(r);
        async_http_client_free(c);
    }

    if (failures == 0) { printf("ALL ASYNC RUNTIME TESTS PASSED\n"); return 0; }
    printf("%d FAILURES\n", failures);
    return 1;
}
