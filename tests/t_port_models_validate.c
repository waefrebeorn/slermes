/* t_port_models_validate.c — oracle harness for _non_public_host.
 * Emits {"case":N,"in":"<host>","out":0|1}. */
#include "port_models_validate.h"

#include <stdio.h>

static void jstr(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        if (*s == '"' || *s == '\\') {
            putchar('\\');
            putchar(*s);
        } else if ((unsigned char)*s < 0x20) {
            printf("\\u%04x", (unsigned char)*s);
        } else {
            putchar(*s);
        }
    }
    putchar('"');
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    static const char *const hosts[] = {
        "",
        "localhost",
        "LOCALHOST.",
        "foo.localhost",
        "127.0.0.1",
        "::1",
        "0.0.0.0",
        "example.local",
        "example.lan",
        "example.internal",
        "example.home",
        "example.localdomain",
        "intranet",
        "myserver",
        "a.b",
        "10.0.0.1",
        "127.5.5.5",
        "192.168.1.1",
        "172.16.0.1",
        "172.31.255.255",
        "172.32.0.1",
        "172.15.0.1",
        "8.8.8.8",
        "1.2.3.4.5",
        "1.2.3",
        "1.2.3.x",
        "10.0.0",
        "999.1.1.1",
        "  Example.COM  ",
        "example.com.",
        "EXAMPLE.COM",
        "192.168.0.1.",
        "10.255.255.255",
        "172.20.10.5",
    };
    size_t n = sizeof(hosts) / sizeof(hosts[0]);
    for (size_t i = 0; i < n; i++) {
        printf("{\"case\":%zu,\"in\":", i);
        jstr(hosts[i]);
        printf(",\"out\":%d}\n", mv_non_public_host(hosts[i]) ? 1 : 0);
    }
    return 0;
}
