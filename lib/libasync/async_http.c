/*
 * async_http.c — Async HTTP/HTTPS client for the Slermes async runtime.
 *
 * The socket I/O is driven non-blocking through the poll-based event loop
 * (lib/libasync_poll), mirroring the callback-driven model websocket_async.c
 * uses (no ucontext/fibers). TLS is via OpenSSL. The high-level async_http_get
 * / async_http_post_json register a request and pump the loop until the
 * response arrives (the faithful "await one request" semantic for a single
 * in-flight call). An INJECTABLE transport lets the parse logic be verified
 * offline (mirrors port_models_net.h).
 */

#define _GNU_SOURCE
#include "async_runtime.h"
#include "async_poll.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

/* ════════════════════════════════════════════════════════════════════════
 * result
 * ════════════════════════════════════════════════════════════════════════ */
void async_http_result_free(async_http_result_t *r) {
    if (!r) return;
    free(r->body); free(r->err); free(r);
}

static async_http_result_t *mk_result(void) {
    return calloc(1, sizeof(async_http_result_t));
}

/* ════════════════════════════════════════════════════════════════════════
 * URL parsing
 * ════════════════════════════════════════════════════════════════════════ */
int async_http_parse_url(const char *url, async_http_url_t *out) {
    memset(out, 0, sizeof(*out));
    const char *p = url;
    const char *colon = strchr(p, ':');
    if (!colon || colon[1] != '/' || colon[2] != '/') return -1;
    size_t sl = (size_t)(colon - p);
    out->scheme = malloc(sl + 1);
    memcpy(out->scheme, p, sl);
    out->scheme[sl] = '\0';
    p = colon + 3;
    const char *slash = strchr(p, '/');
    const char *at = strchr(p, '@');
    const char *host_end = slash ? slash : p + strlen(p);
    if (at && at < host_end) host_end = at;
    const char *port_colon = NULL;
    for (const char *q = p; q < host_end; q++) if (*q == ':') { port_colon = q; break; }
    size_t hlen = port_colon ? (size_t)(port_colon - p) : (size_t)(host_end - p);
    out->host = malloc(hlen + 1);
    memcpy(out->host, p, hlen); out->host[hlen] = '\0';
    if (port_colon) out->port = atoi(port_colon + 1);
    if (out->port <= 0) out->port = (strcmp(out->scheme, "https") == 0) ? 443 : 80;
    if (slash) { size_t plen = strlen(slash); out->path = malloc(plen + 1); strcpy(out->path, slash); }
    else { out->path = malloc(2); strcpy(out->path, "/"); }
    return 0;
}

void async_http_url_free(async_http_url_t *u) {
    if (!u) return;
    free(u->scheme); free(u->host); free(u->path);
    memset(u, 0, sizeof(*u));
}

/* ════════════════════════════════════════════════════════════════════════
 * response parsing
 * ════════════════════════════════════════════════════════════════════════ */
int async_http_parse_response(const char *buf, size_t len, async_http_result_t *out) {
    const char *hdr_end = NULL;
    for (size_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') { hdr_end = buf + i + 4; break; }
        if (buf[i] == '\n' && buf[i+1] == '\n') { hdr_end = buf + i + 2; break; }
    }
    if (!hdr_end) { out->transport_err = -1; out->err = strdup("incomplete response"); return -1; }
    const char *eol = strchr(buf, '\n');
    if (eol) {
        size_t l = (size_t)(eol - buf);
        char *line = malloc(l + 1); memcpy(line, buf, l); line[l] = '\0';
        int v = 0; sscanf(line, "HTTP/%*s %d", &v); out->code = v; free(line);
    }
    bool chunked = false;
    long clen = -1;
    const char *h = buf;
    while (h < hdr_end) {
        const char *he = strchr(h, '\n');
        size_t hl = he ? (size_t)(he - h) : strlen(h);
        if (hl > 0 && h[hl-1] == '\r') hl--;
        char tmp[1024]; if (hl >= sizeof(tmp)) hl = sizeof(tmp) - 1;
        memcpy(tmp, h, hl); tmp[hl] = '\0';
        if (strncasecmp(tmp, "content-length:", 15) == 0) clen = atol(tmp + 15);
        else if (strncasecmp(tmp, "transfer-encoding:", 18) == 0 && strcasestr(tmp, "chunked")) chunked = true;
        if (!he) break;
        h = he + 1;
    }
    size_t body_off = (size_t)(hdr_end - buf);
    size_t body_len = len - body_off;
    if (chunked) {
        const char *p = hdr_end;
        size_t cap = body_len + 1, used = 0;
        char *dec = malloc(cap);
        while (p < buf + len) {
            char *nl = strchr((char *)p, '\n');
            if (!nl) break;
            long chunk = strtol(p, NULL, 16);
            p = nl + 1;
            if (chunk <= 0) break;
            if (used + (size_t)chunk + 1 > cap) { cap = used + chunk + 256; dec = realloc(dec, cap); }
            memcpy(dec + used, p, (size_t)chunk);
            used += (size_t)chunk;
            p += chunk;
            if (p < buf + len && *p == '\r') p++;
            if (p < buf + len && *p == '\n') p++;
        }
        dec[used] = '\0';
        out->body = dec; out->len = used;
    } else if (clen >= 0 && (size_t)clen <= body_len) {
        out->body = malloc((size_t)clen + 1);
        memcpy(out->body, hdr_end, (size_t)clen);
        out->body[clen] = '\0'; out->len = (size_t)clen;
    } else {
        out->body = malloc(body_len + 1);
        memcpy(out->body, hdr_end, body_len);
        out->body[body_len] = '\0'; out->len = body_len;
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════
 * real transport: callback-driven non-blocking request over async_poll
 * ════════════════════════════════════════════════════════════════════════ */
typedef enum { ST_CONNECT, ST_HANDSHAKE, ST_WRITE, ST_READ, ST_DONE, ST_ERR } req_state;
typedef struct {
    async_poll_t *loop;
    async_http_client_t *client;
    async_http_result_t *res;
    async_http_url_t url;
    char reqbuf[8192];
    int reqlen, reqsent;
    int fd;
    SSL *ssl;
    char *rbuf; size_t rcap, rlen;
    req_state state;
    bool done;
    int err;
} req_t;

static SSL_CTX *g_ssl_ctx = NULL;
static SSL_CTX *ssl_ctx_get(void) {
    if (!g_ssl_ctx) {
        SSL_load_error_strings();
        SSL_library_init();
        g_ssl_ctx = SSL_CTX_new(TLS_client_method());
        SSL_CTX_set_verify(g_ssl_ctx, SSL_VERIFY_NONE, NULL);
    }
    return g_ssl_ctx;
}

/* fd readiness callbacks (defined below; forward-declared for req_pump). */
static void req_on_read(int fd, void *u);
static void req_on_write(int fd, void *u);

/* Drive the request state machine according to fd readiness. */
static void req_fail(req_t *r, const char *msg) {
    if (!r->res->err) r->res->err = strdup(msg);
    r->res->transport_err = -1;
    r->state = ST_ERR; r->done = true;
    if (r->fd >= 0) async_poll_remove_fd(r->loop, r->fd);
}

/* Drive the request state machine according to fd readiness. */
static void req_pump(req_t *r) {
    while (!r->done) {
        if (r->state == ST_CONNECT) {
            int e = 0; socklen_t el = sizeof(e);
            getsockopt(r->fd, SOL_SOCKET, SO_ERROR, &e, &el);
            if (e != 0) { req_fail(r, "connect failed"); return; }
            if (strcmp(r->url.scheme, "https") == 0) {
                r->ssl = SSL_new(ssl_ctx_get());
                SSL_set_fd(r->ssl, r->fd);
                SSL_set_connect_state(r->ssl);
                r->state = ST_HANDSHAKE;
            } else {
                r->state = ST_WRITE;
            }
            continue;
        }
        if (r->state == ST_HANDSHAKE) {
            int rc = SSL_connect(r->ssl);
            if (rc == 1) { r->state = ST_WRITE; continue; }
            int e = SSL_get_error(r->ssl, rc);
            if (e == SSL_ERROR_WANT_READ) { async_poll_add_reader(r->loop, r->fd, (async_poll_read_cb_t)req_on_read, r); return; }
            else if (e == SSL_ERROR_WANT_WRITE) { async_poll_add_writer(r->loop, r->fd, (async_poll_write_cb_t)req_on_write, r); return; }
            else { req_fail(r, "tls handshake failed"); return; }
        }
        if (r->state == ST_WRITE) {
            int n = r->ssl
                ? SSL_write(r->ssl, r->reqbuf + r->reqsent, r->reqlen - r->reqsent)
                : (int)write(r->fd, r->reqbuf + r->reqsent, r->reqlen - r->reqsent);
            if (n > 0) { r->reqsent += n; if (r->reqsent >= r->reqlen) r->state = ST_READ; continue; }
            int e = r->ssl ? SSL_get_error(r->ssl, n) : 0;
            if ((r->ssl && e == SSL_ERROR_WANT_WRITE) || (!r->ssl && n < 0 && errno == EAGAIN)) {
                async_poll_add_writer(r->loop, r->fd, (async_poll_write_cb_t)req_on_write, r); return;
            }
            if ((r->ssl && e == SSL_ERROR_WANT_READ) || (!r->ssl && n < 0 && errno == EINTR)) {
                async_poll_add_reader(r->loop, r->fd, (async_poll_read_cb_t)req_on_read, r); return;
            }
            req_fail(r, "write failed"); return;
        }
        if (r->state == ST_READ) {
            if (r->rlen + 65536 > r->rcap) { r->rcap *= 2; r->rbuf = realloc(r->rbuf, r->rcap); }
            int n = r->ssl ? SSL_read(r->ssl, r->rbuf + r->rlen, 65536)
                            : (int)read(r->fd, r->rbuf + r->rlen, 65536);
            if (n > 0) { r->rlen += n; continue; }
            if (n == 0) {
                async_http_parse_response(r->rbuf, r->rlen, r->res);
                r->state = ST_DONE; r->done = true;
                async_poll_remove_fd(r->loop, r->fd);
                return;
            }
            int e = r->ssl ? SSL_get_error(r->ssl, n) : 0;
            if ((r->ssl && e == SSL_ERROR_WANT_READ) || (!r->ssl && n < 0 && errno == EAGAIN)) {
                async_poll_add_reader(r->loop, r->fd, (async_poll_read_cb_t)req_on_read, r); return;
            }
            if ((r->ssl && e == SSL_ERROR_WANT_WRITE) || (!r->ssl && n < 0 && errno == EINTR)) {
                async_poll_add_writer(r->loop, r->fd, (async_poll_write_cb_t)req_on_write, r); return;
            }
            req_fail(r, "read failed"); return;
        }
        return;
    }
}

/* fd readiness callbacks (forward-declared; async_poll stores them as void*). */
static void req_on_read(int fd, void *u) {
    (void)fd; req_pump((req_t *)u);
}
static void req_on_write(int fd, void *u) {
    (void)fd; req_pump((req_t *)u);
}

struct async_http_client_t {
    int timeout_ms;
    async_http_transport_t transport;
    void *transport_ctx;
    async_http_ssrf_guard_t ssrf_guard;
    void *ssrf_guard_ctx;
};

static async_http_result_t *real_request(async_http_client_t *c, const char *method,
                                         const char *url, const char *body) {
    async_http_result_t *res = mk_result();
    req_t r;
    memset(&r, 0, sizeof(r));
    r.client = c; r.res = res;
    r.fd = -1; r.rcap = 1 << 16; r.rbuf = malloc(r.rcap);
    if (async_http_parse_url(url, &r.url) != 0) {
        res->transport_err = -1; res->err = strdup("bad url"); free(r.rbuf); return res;
    }

    /* Resolve + connect. If an SSRF guard is installed, it resolves and
     * validates the host and returns the vetted IP strings we may dial;
     * we then connect BY IP (host preserved for Host header / SNI), which
     * closes the DNS-rebind gap between pre-flight validation and connect.
     * Faithful analogue of url_safety._resolved_http_connect_ips + the
     * httpx guarded network backend. */
    struct addrinfo hints, *res0 = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    char portstr[16]; snprintf(portstr, sizeof(portstr), "%d", r.url.port);

    char **vetted = NULL;
    if (c->ssrf_guard) {
        vetted = c->ssrf_guard(r.url.host, r.url.port, r.url.scheme, c->ssrf_guard_ctx);
        if (!vetted) {
            res->transport_err = -1; res->err = strdup("blocked by ssrf guard");
            async_http_url_free(&r.url); free(r.rbuf); return res;
        }
    }

    int fd = -1;
    if (vetted) {
        /* Dial each vetted IP directly (AI_NUMERICHOST — no re-resolution). */
        struct addrinfo nhints;
        for (int i = 0; vetted[i] && fd < 0; i++) {
            memset(&nhints, 0, sizeof(nhints));
            nhints.ai_family = AF_UNSPEC; nhints.ai_socktype = SOCK_STREAM;
            nhints.ai_flags = AI_NUMERICHOST;
            struct addrinfo *ipres = NULL;
            if (getaddrinfo(vetted[i], portstr, &nhints, &ipres) != 0 || !ipres) {
                if (ipres) freeaddrinfo(ipres);
                continue;
            }
            for (struct addrinfo *ai = ipres; ai; ai = ai->ai_next) {
                fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
                if (fd < 0) continue;
                fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
                if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0 || errno == EINPROGRESS) break;
                close(fd); fd = -1;
            }
            freeaddrinfo(ipres);
        }
        for (int i = 0; vetted[i]; i++) free(vetted[i]);
        free(vetted);
    } else {
        if (getaddrinfo(r.url.host, portstr, &hints, &res0) != 0 || !res0) {
            res->transport_err = -1; res->err = strdup("dns failed"); async_http_url_free(&r.url); free(r.rbuf); return res;
        }
        for (struct addrinfo *ai = res0; ai; ai = ai->ai_next) {
            fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (fd < 0) continue;
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
            if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0 || errno == EINPROGRESS) break;
            close(fd); fd = -1;
        }
        freeaddrinfo(res0);
    }
    if (fd < 0) { res->transport_err = -1; res->err = strdup("connect failed"); async_http_url_free(&r.url); free(r.rbuf); return res; }
    r.fd = fd;

    int hl = snprintf(r.reqbuf, sizeof(r.reqbuf),
        "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: slermes-async/1.0\r\n"
        "Accept: */*\r\nConnection: close\r\n", method, r.url.path, r.url.host);
    if (body) hl += snprintf(r.reqbuf + hl, sizeof(r.reqbuf) - hl,
        "Content-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s", strlen(body), body);
    else hl += snprintf(r.reqbuf + hl, sizeof(r.reqbuf) - hl, "\r\n");
    r.reqlen = hl; r.reqsent = 0;
    r.state = ST_CONNECT;

    /* Run the request on its own event loop (await-one-request semantic). */
    async_poll_t *loop = async_poll_create(16);
    r.loop = loop;
    async_poll_add_writer(loop, fd, (async_poll_write_cb_t)req_on_write, &r);
    while (!r.done) async_poll_run_once(loop, 200);
    async_poll_destroy(loop);

    if (r.ssl) SSL_free(r.ssl);
    close(fd);
    async_http_url_free(&r.url);
    free(r.rbuf);
    return res;
}

/* ════════════════════════════════════════════════════════════════════════
 * client + injectable transport
 * ════════════════════════════════════════════════════════════════════════ */

async_http_client_t *async_http_client_new(int timeout_ms) {
    async_http_client_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->timeout_ms = timeout_ms > 0 ? timeout_ms : 30000;
    return c;
}
void async_http_client_free(async_http_client_t *c) { free(c); }
void async_http_set_transport(async_http_client_t *c, async_http_transport_t t, void *ctx) {
    if (c) { c->transport = t; c->transport_ctx = ctx; }
}
void async_http_set_ssrf_guard(async_http_client_t *c, async_http_ssrf_guard_t g, void *ctx) {
    if (c) { c->ssrf_guard = g; c->ssrf_guard_ctx = ctx; }
}

async_http_result_t *async_http_get(async_http_client_t *c, const char *url) {
    if (c->transport) {
        async_http_result_t *r = mk_result();
        int rc = c->transport("GET", url, NULL, NULL, r, c->transport_ctx);
        if (rc != 0 && r->code == 0 && !r->err) r->err = strdup("transport error");
        return r;
    }
    return real_request(c, "GET", url, NULL);
}

async_http_result_t *async_http_post_json(async_http_client_t *c, const char *url, const char *json_body) {
    if (c->transport) {
        async_http_result_t *r = mk_result();
        int rc = c->transport("POST", url, NULL, json_body, r, c->transport_ctx);
        if (rc != 0 && r->code == 0 && !r->err) r->err = strdup("transport error");
        return r;
    }
    return real_request(c, "POST", url, json_body);
}
