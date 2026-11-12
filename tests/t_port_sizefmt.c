/* t_port_sizefmt.c — oracle harness for format_bytes. */
#include "port_sizefmt.h"

#include <stdio.h>
#include <stdlib.h>

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
        NULL, "", "   ", "0", "1", "512", "1023", "1023.9", "1024", "1536",
        "1048576", "1073741824", "1099511627776", "1.5", "-5", "abc", "12abc",
        " 1234 ", "1234567", "1e6", "inf", "nan", "0.0", "-1024",
    };
    size_t n = sizeof(inputs) / sizeof(inputs[0]);
    for (size_t i = 0; i < n; i++) {
        char *out = format_bytes(inputs[i]);
        printf("{\"case\":\"fb_%zu\",\"in\":", i);
        jstr(inputs[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }
    return 0;
}
