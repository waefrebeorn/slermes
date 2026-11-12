/* t_port_microsoft_graph_auth.c — oracle harness for format_graph_error and
 * _fresh_cached. Emits {"tag":"fge",...} and {"tag":"fc",...} lines. */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

char *tools_microsoft_graph_auth_format_graph_error(const json_t *error);
void tools_microsoft_graph_auth_set_cached_token(const char *token, time_t expires_at);
int tools_microsoft_graph_auth_fresh_cached(int skew_seconds);

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    static const char *const inputs[] = {
        "null", "\"plain error\"",
        "{\"code\":\"invalid_grant\",\"message\":\"bad thing\"}",
        "{\"code\":\"x\"}",
        "{\"message\":\"only message\"}",
        "{\"code\":\"\",\"message\":\"m\"}",
        "{\"code\":\"c\",\"message\":\"\"}",
        "{\"code\":123,\"message\":\"m\"}",
        "{\"code\":1.5,\"message\":\"m\"}",
        "{\"code\":true,\"message\":\"m\"}",
        "{\"code\":null,\"message\":\"m\"}",
        "{\"code\":0,\"message\":\"m\"}",
        "{}",
        "[]",
        "42",
        "true",
    };
    size_t n = sizeof(inputs) / sizeof(inputs[0]);
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_parse(inputs[i], NULL);
        char *out = tools_microsoft_graph_auth_format_graph_error(v);
        printf("{\"tag\":\"fge\",\"case\":\"m_%zu\",\"in\":%s,\"out\":", i, inputs[i]);
        jstr(out);
        printf("}\n");
        free(out);
        if (v) json_free(v);
    }

    /* _fresh_cached: cached token present/absent, expiry near/far, skew 0/120.
     * Offsets are far enough from now that time.time() and time(NULL) agree. */
    time_t now = time(NULL);
    static const long long offsets[] = {3600, 3600, -3600, 60, 300, 3600};
    static const int skews[] =         {0,    120,   0,    120, 120, 0};
    static const int cached[] =        {1,    1,     1,    1,   1,   0};
    size_t m = sizeof(offsets) / sizeof(offsets[0]);
    for (size_t i = 0; i < m; i++) {
        time_t exp = now + (time_t)offsets[i];
        tools_microsoft_graph_auth_set_cached_token(
            cached[i] ? "tok" : NULL, exp);
        int fresh = tools_microsoft_graph_auth_fresh_cached(skews[i]);
        printf("{\"tag\":\"fc\",\"case\":\"f_%zu\",\"cached\":%d,"
               "\"expires_at\":%lld,\"skew\":%d,\"fresh\":%d}\n",
               i, cached[i], (long long)exp, skews[i], fresh);
    }
    return 0;
}
