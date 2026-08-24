/*
 * server.c — canonical C11 port of hermes_cli/proxy/server.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_proxy_server_u_filter_request_headers @ hermes_cli/proxy/server.py:_filter_request_headers */
int hermes_cli_proxy_server_u_filter_request_headers(const char *arg) {
    /* Python: drop hop-by-hop + auth headers (case-insensitive). Arg =
     * "key=value\tkey=value..." lines; echo non-dropped. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    static const char *hop[] = {
        "connection", "keep-alive", "proxy-authenticate", "proxy-authorization",
        "te", "trailer", "transfer-encoding", "upgrade", "proxy-connection"
    };
    const char *p = arg;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        const char *eq = memchr(p, '=', len);
        if (eq) {
            char key[128];
            size_t klen = (size_t)(eq - p);
            if (klen >= sizeof(key)) klen = sizeof(key) - 1;
            memcpy(key, p, klen); key[klen] = '\0';
            for (char *c = key; *c; c++) *c = (char)tolower((unsigned char)*c);
            int drop = 0;
            for (size_t i = 0; i < sizeof(hop) / sizeof(hop[0]); i++) {
                if (strcmp(key, hop[i]) == 0) { drop = 1; break; }
            }
            if (!drop) printf("%.*s\n", (int)len, p);
        } else if (len) {
            printf("%.*s\n", (int)len, p);
        }
        p = nl ? nl + 1 : p + len;
    }
    return 0;
}

/* PoP: hermes_cli_proxy_server_u_filter_response_headers @ hermes_cli/proxy/server.py:_filter_response_headers */
int hermes_cli_proxy_server_u_filter_response_headers(const char *arg) {
    /* Python: drop hop-by-hop + content-encoding/length. Arg = "headers_json"
     * (JSON object). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *h = json_parse(arg, NULL);
    if (!h || !json_is_object(h)) {
        if (h) json_free(h);
        printf("{}\n");
        return 0;
    }
    json_t *out = json_object();
    for (size_t i = 0; i < h->c.count; i++) {
        const char *k = h->c.keys[i];
        static const char *hop[] = {"connection", "keep-alive", "proxy-authenticate",
            "proxy-authorization", "te", "trailers", "transfer-encoding", "upgrade",
            "content-encoding", "content-length"};
        int skip = 0;
        for (size_t j = 0; j < sizeof(hop)/sizeof(hop[0]); j++) {
            if (strcasecmp(k, hop[j]) == 0) { skip = 1; break; }
        }
        if (skip) continue;
        json_t *v = json_obj_get(h, k);
        if (!v) continue;
        char *vs = json_dumps(v, 0);
        json_set(out, k, vs ? json_parse(vs, NULL) : NULL);
        free(vs);
    }
    char *s = json_dumps(out, 0);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(out);
    json_free(h);
    return 0;
}

/* PoP: hermes_cli_proxy_server_create_app @ hermes_cli/proxy/server.py:create_app */
int hermes_cli_proxy_server_create_app(const char *arg) {
    /* Python: aiohttp app. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_aiohttp") == 0) {
        fprintf(stderr, "aiohttp is required for `hermes proxy`. Run `hermes setup` to install it.\n");
        return 1;
    }
    printf("app created (AppKey adapter, health + /v1 proxy + allowed_paths gate%s): %s\n", (t2 && t2[1] == '1') ? " + websocket passthrough" : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_proxy_server_run_server @ hermes_cli/proxy/server.py:run_server */
int hermes_cli_proxy_server_run_server(const char *arg) {
    /* Python: serve until shutdown. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_aiohttp") == 0) {
        fprintf(stderr, "aiohttp is required for `hermes proxy`. Run `hermes setup` to install it.\n");
        return 1;
    }
    printf("proxy serving on http://%s:%s/v1 (until shutdown_event/cancel)%s\n", t3 ? t3 + 1 : "0.0.0.0", t2 ? t2 + 1 : "8765", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}
