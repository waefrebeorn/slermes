/*
 * t_port_browser_redact.c — oracle harness for v555 browser_redact extraction.
 * Emits JSON lines consumed by sta_oracle_browser_redact.py.
 *
 *   gcc -O2 -g -I include -I src/tools -I lib/libregex \
 *     tests/t_port_browser_redact.c src/tools/browser_redact.o -o /tmp/t_br
 */
#include "browser_redact.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* JSON-escape a string into out (caller must size out >= 4*len+1). */
static void emit_escaped(const char *s, char *out)
{
    if (!s) { strcpy(out, "null"); return; }
    char *p = out;
    *p++ = '"';
    for (const char *q = s; *q; q++) {
        unsigned char c = (unsigned char)*q;
        if (c == '"' || c == '\\') { *p++ = '\\'; *p++ = c; }
        else if (c == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else if (c == '\t') { *p++ = '\\'; *p++ = 't'; }
        else if (c == '\r') { *p++ = '\\'; *p++ = 'r'; }
        else if (c < 0x20) { sprintf(p, "\\u%04x", c); p += 6; }
        else *p++ = c;
    }
    *p++ = '"';
    *p = '\0';
}

static void emit(const char *fn, const char *in)
{
    char in_esc[8192], out_esc[8192];
    char *res = (strcmp(fn, "sensitive") == 0)
        ? browser_redact_sensitive_text(in)
        : browser_redact_cdp_url(in);
    emit_escaped(in, in_esc);
    emit_escaped(res, out_esc);
    printf("{\"fn\":\"%s\",\"in\":%s,\"out\":%s}\n", fn, in_esc, out_esc);
    free(res);
}

int main(void)
{
    /* 1) known vendor secret prefixes — FAKE fixtures (scanner-safe shapes that
     * still exercise the C regex branches; not real credentials). */
    emit("sensitive", "key=sk-ZZZZZZZZZZzzzzzzzzzz");
    emit("sensitive", "token ghp_ZZZZZZZZZZzzzzzzzzzz inside");
    emit("sensitive", "AKIAfakefakefake aws key");
    emit("sensitive", "slackplaceholder-ZZZZZZZZZZ token");
    emit("sensitive", "AIzaZZZZZZZZZZzzzzzzzzzzzzzzzzzzzz");
    emit("sensitive", "sk_live_ZZZZZZZZZZ session");

    /* 2) Authorization / Bearer headers */
    emit("sensitive", "Authorization: Bearer eyJabc123def456");
    emit("sensitive", "proxy-authorization: Basic dXNlcjpwYXNz");
    emit("sensitive", "X-Api-Key: abcdefghijklmnopqrstuvwxyz012345");

    /* 3) private key block */
    emit("sensitive",
        "-----BEGIN RSA PRIVATE KEY-----\nMIIEowIBAAKCAQEA\n-----END RSA PRIVATE KEY-----");

    /* 4) DB connection strings */
    emit("sensitive", "postgresql://user:hunter2pass@db.example.com:5432/app");
    emit("sensitive", "redis://admin:s3cr3t@cache.local:6379");

    /* 5) JWT */
    emit("sensitive", "header eyJhbGciOiJIUzI1NiJ9.payload.sig here");

    /* 6) Telegram bot token */
    emit("sensitive", "bot1234567890:ABCDEFGHIJKLMNOPQRSTUVWXYZ012345");

    /* 7) bare-token URL userinfo */
    emit("sensitive", "https://abcdefghijklmnopqrstuvwxyz012345@github.com/u/r");

    /* 8) generic user:pass@ in http URL */
    emit("sensitive", "https://bob:secretpass@api.example.com/v1/foo");

    /* 9) CDP URL with token query param + userinfo */
    emit("cdp", "ws://localhost:9222/devtools/browser/abc?token=supersecrettoken123");
    emit("cdp", "wss://user:passw0rd@cdp.browserbase.com/connect");
    emit("cdp", "http://localhost:9222/json?access_token=abcdef123456");
    emit("cdp", "ws://127.0.0.1:9222/devtools/page/xyz?client_secret=shhh");

    /* 10) passthrough: non-secret text unchanged */
    emit("sensitive", "just a normal log line with no secrets");
    emit("cdp", "ws://localhost:9222/devtools/page/abc");

    return 0;
}
