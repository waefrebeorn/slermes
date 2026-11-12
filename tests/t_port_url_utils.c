/* t_port_url_utils.c — oracle harness for format_url_host. */
#include "port_url_utils.h"

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

    static const char *const hosts[] = {
        NULL, "", "   ", "example.com", "  example.com  ", "\texample.com\n",
        "::1", "fe80::1%eth0", "[::1]", "[fe80::1%eth0]", "[fe80::1%25eth0]",
        "[example.com]", "[]", "[", "]", "[::1", "::1]",
        "2001:db8::1", "[2001:db8::1]", "host:8080", "[host:8080]",
        "192.168.1.1", "  [ ::1 ]  ", "a:b:c",
    };
    size_t n = sizeof(hosts) / sizeof(hosts[0]);
    for (size_t i = 0; i < n; i++) {
        char *out = format_url_host(hosts[i]);
        printf("{\"case\":\"u_%zu\",\"in\":", i);
        jstr(hosts[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }
    return 0;
}
