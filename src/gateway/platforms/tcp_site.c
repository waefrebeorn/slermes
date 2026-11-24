/*
 * tcp_site.c -- C11 port of gateway/platforms/tcp_site.py.
 *
 * PoP: w20_tcpsite_d7fd64_has_live_listener @ gateway/platforms/tcp_site.py:has_live_listener
 * PoP: w20_tcpsite_d7fd64_is_wildcard_host @ gateway/platforms/shared_ingress.py:is_wildcard_host
 * PoP: w20_tcpsite_d7fd64_start @ gateway/platforms/tcp_site.py:start_tcp_site
 *
 * The aiohttp runner and the kernel bind stay upstream; the exclusivity policy,
 * the TIME_WAIT retry and the wildcard carve-out are pure decisions and are
 * reproduced here against a scripted site.
 */
#include "hermes_json.h"
#include "noc_w20_tcpsite_d7fd64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ts_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

int w20_tcpsite_d7fd64_has_live_listener(const char *probe) {
    if (probe && !strcmp(probe, "refused")) return 0;   /* ConnectionRefusedError */
    if (probe && !strcmp(probe, "ok")) return 1;
    return 1;                                            /* any other OSError: assume live */
}

int w20_tcpsite_d7fd64_is_wildcard_host(const char *host) {
    const char *p;
    size_t n;
    if (!host) return 1;                                 /* host is None */
    p = host;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    n = strlen(p);
    while (n && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\n' || p[n - 1] == '\r')) n--;
    if (n == 0) return 1;
    if (n == 7 && !strncmp(p, "0.0.0.0", 7)) return 1;
    if (n == 2 && !strncmp(p, "::", 2)) return 1;
    if (n == 1 && p[0] == '*') return 1;
    return 0;
}

static void ts_call(json_t *calls, int reuse_known, int reuse, const char *result) {
    json_t *call = json_object();
    if (reuse_known) json_set(call, "reuse", json_bool(reuse ? 1 : 0));
    else json_set(call, "reuse", json_null());
    json_set(call, "result", json_string(result ? result : "ok"));
    json_append(calls, call);
}

json_t *w20_tcpsite_d7fd64_start(const char *platform, const char *host, int port,
                                 const char *log_tag, const char *first,
                                 int listener_live, const char *second) {
    int exclusive = platform && !strcmp(platform, "darwin");
    json_t *calls = json_array();
    json_t *value, *out;
    const char *raised = NULL;
    int stops = 0;
    char *log = NULL;

    if (!first) first = "ok";
    if (!second) second = "ok";

    ts_call(calls, exclusive ? 1 : 0, 0, first);

    if (!strcmp(first, "ok")) {
        /* fall through to success */
    } else if (!exclusive || strcmp(first, "EADDRINUSE") ||
               w20_tcpsite_d7fd64_is_wildcard_host(host)) {
        raised = first;
    } else if (listener_live) {
        raised = first;
    } else {
        char buf[512];
        stops = 1;
        snprintf(buf, sizeof(buf),
                 "[%s] %s:%d busy without a live listener (TIME_WAIT from the previous "
                 "gateway); rebinding with SO_REUSEADDR",
                 log_tag ? log_tag : "", host ? host : "", port);
        log = ts_dup(buf);
        ts_call(calls, 1, 1, second);
        if (strcmp(second, "ok")) raised = second;
    }

    if (raised) {
        out = json_object();
        json_set(out, "status", json_string("raised"));
        json_set(out, "type", json_string("OSError"));
        json_set(out, "errno", json_string(raised));
        json_free(calls);
        free(log);
        return out;
    }
    value = json_object();
    json_set(value, "calls", calls);
    json_set(value, "stops", json_number((double)stops));
    json_set(value, "log", log ? json_string(log) : json_null());
    json_set(value, "outcome", json_string("site"));
    out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value);
    free(log);
    return out;
}
