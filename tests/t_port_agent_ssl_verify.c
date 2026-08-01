/* Oracle harness: agent/ssl_verify.py vs LIVE Python. */
#include <stdio.h>
#include <stdlib.h>
#include "agent/port_agent_ssl_verify.h"

static const char *js(const char *s) {
    static char bufs[8][256];
    static int bi = 0; char *b = bufs[bi]; bi = (bi + 1) % 8;
    char *q = b; *q++ = '"';
    for (const char *p = s; p && *p && q - b < 250; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; } else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

int main(void) {
    printf("{\"t\":\"coerce\",\"in\":%s,\"out\":%d}\n", js("false"), agent_ssl_verify_coerce_insecure("false")?1:0);
    printf("{\"t\":\"coerce\",\"in\":%s,\"out\":%d}\n", js("0"), agent_ssl_verify_coerce_insecure("0")?1:0);
    printf("{\"t\":\"coerce\",\"in\":%s,\"out\":%d}\n", js("off"), agent_ssl_verify_coerce_insecure("off")?1:0);
    printf("{\"t\":\"coerce\",\"in\":%s,\"out\":%d}\n", js("true"), agent_ssl_verify_coerce_insecure("true")?1:0);
    printf("{\"t\":\"coerce\",\"in\":%s,\"out\":%d}\n", js(""), agent_ssl_verify_coerce_insecure("")?1:0);

    /* resolve_httpx_verify with no insecure + no CA env -> "true" */
    unsetenv("HERMES_CA_BUNDLE"); unsetenv("SSL_CERT_FILE"); unsetenv("REQUESTS_CA_BUNDLE"); unsetenv("CURL_CA_BUNDLE");
    printf("{\"t\":\"resolve\",\"ca\":%s,\"sv\":%s,\"out\":%s}\n", js(""), js(""),
           js(agent_ssl_verify_resolve_httpx_verify("", "true")));
    printf("{\"t\":\"resolve\",\"ca\":%s,\"sv\":%s,\"out\":%s}\n", js(""), js("false"),
           js(agent_ssl_verify_resolve_httpx_verify("", "false")));
    /* CA env/path pointing at existing file (passed via ca_bundle arg, matching oracle) */
    const char *tmp = "/tmp/_ssl_ca_test.pem";
    FILE *f = fopen(tmp, "w"); if (f) { fputs("x", f); fclose(f); }
    printf("{\"t\":\"resolve\",\"ca\":%s,\"sv\":%s,\"out\":%s}\n", js(tmp), js("true"),
           js(agent_ssl_verify_resolve_httpx_verify(tmp, "true")));
    remove(tmp);
    return 0;
}
