/* t_port_microsoft_graph_auth.c — oracle harness for format_graph_error. */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>

char *tools_microsoft_graph_auth_format_graph_error(const json_t *error);

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
        printf("{\"case\":\"m_%zu\",\"in\":%s,\"out\":", i, inputs[i]);
        jstr(out);
        printf("}\n");
        free(out);
        if (v) json_free(v);
    }
    return 0;
}
