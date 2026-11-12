/* t_port_dashboard_auth_audit.c — oracle harness for _bounded_value.
 * Emits {"case":N,"in":<json>,"out":<json>}. */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>

json_t *dab_bounded_value(const json_t *value, int max_len, const char *marker);

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    static const char *const inputs[] = {
        "null",
        "true",
        "42",
        "\"short\"",
        "\"this string is definitely longer than the twenty-six character cap\"",
        "[]",
        "{}",
        "[1,2,3]",
        "{\"a\":\"short\",\"b\":\"this value is quite long and should be truncated by the cap\"}",
        "{\"nested\":{\"deep\":\"another fairly long value that will be truncated here\"}}",
        "[\"one\",\"two\",\"three\"]",
        "{\"long\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}",
    };
    size_t n = sizeof(inputs) / sizeof(inputs[0]);
    int max_len = 26;
    const char *marker = "...[truncated]";
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_parse(inputs[i], NULL);
        json_t *out = dab_bounded_value(v, max_len, marker);
        char *serialized = json_dumps(out, 0);
        printf("{\"case\":%zu,\"max_len\":%d,\"marker\":\"...[truncated]\","
               "\"in\":%s,\"out\":%s}\n",
               i, max_len, inputs[i], serialized ? serialized : "null");
        free(serialized);
        if (out) json_free(out);
        if (v) json_free(v);
    }
    return 0;
}
