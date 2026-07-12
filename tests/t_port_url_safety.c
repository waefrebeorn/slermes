/* t_port_url_safety.c — oracle harness for tools/url_safety.py query/redirect
 * helpers. Emits one JSON object per line: {"fn":..,"in":..,"out":..}. */
#include <stdio.h>
#include <string.h>
#include "hermes_url_safety.h"

static const char *js(const char *s) {
    static char bufs[8][4096];
    static int bi = 0;
    char *b = bufs[bi]; bi = (bi + 1) % 8;
    char *q = b; *q++ = '"';
    for (const char *p = s; p && *p && q - b < 4000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

static void emit_qp(const char *url) {
    char *r = url_safety_sensitive_query_param_name(url);
    printf("{\"fn\":\"qp\",\"in\":%s,\"out\":%s}\n", js(url ? url : ""), r ? js(r) : "null");
    free(r);
}
static void emit_has(const char *url) {
    int h = url_safety_has_sensitive_query_params(url);
    printf("{\"fn\":\"has\",\"in\":%s,\"out\":%s}\n", js(url ? url : ""), h ? "true" : "false");
}
static void emit_redir(bool is_redirect, const char *cur, const char *loc, const char *nxt) {
    char *r = url_safety_redirect_target_from_response(is_redirect, cur, loc, nxt);
    printf("{\"fn\":\"redir\",\"in\":%s,\"is_redirect\":%s,\"loc\":%s,\"nxt\":%s,\"out\":%s}\n",
           js(cur ? cur : ""), is_redirect ? "true" : "false",
           js(loc ? loc : ""), js(nxt ? nxt : ""), r ? js(r) : "null");
    free(r);
}

int main(void) {
    emit_qp("https://example.com/path?token=abc123&x=1");
    emit_qp("https://example.com/?password=secret&user=bob");
    emit_qp("https://example.com/?x=1");
    emit_qp("https://example.com/?Token=abc");
    emit_qp("ftp://example.com/?token=1");
    emit_qp("https://example.com/noquery");
    emit_qp("not a url");
    emit_qp(NULL);
    emit_qp("https://example.com/?secret="); /* value empty -> not sensitive */
    emit_has("https://example.com/?apikey=zzz");
    emit_has("https://example.com/?foo=bar");
    emit_redir(true, "https://a.com/page", "/next", NULL);
    emit_redir(true, "https://a.com/page", "https://b.com/abs", NULL);
    emit_redir(false, "https://a.com/page", "/next", NULL);
    emit_redir(true, "https://a.com/page", NULL, "https://c.com/final");
    return 0;
}
