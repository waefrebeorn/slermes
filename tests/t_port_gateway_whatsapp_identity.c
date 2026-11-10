/* t_port_gateway_whatsapp_identity.c — oracle harness for the JID helpers. */
#include "port_gateway_whatsapp_identity.h"

#include <stdio.h>

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

    static const char *const norm[] = {
        "6012:47@s.whatsapp.net", "6012@lid", "+6012", "", "+6012345678",
        "AB+12@x", "  +6012  ", "6012.us", "+", "::", "@"
    };
    for (size_t i = 0; i < sizeof(norm) / sizeof(norm[0]); i++) {
        printf("{\"family\":\"normalize\",\"case\":\"n_%zu\",\"in\":", i);
        jstr(norm[i]);
        printf(",\"out\":");
        jstr(wi_normalize_identifier(norm[i]));
        printf("}\n");
    }

    static const char *const jid[] = {
        "6012", "+6012", "+6012 34", "6012-01", "6012:47@s.whatsapp.net",
        "6012:47@lid", "user@domain", "user:dev@x.com", "", "abc", "s.whatsapp.net",
        "6012@s.whatsapp.net", " (6012) 555 "
    };
    for (size_t i = 0; i < sizeof(jid) / sizeof(jid[0]); i++) {
        printf("{\"family\":\"tojid\",\"case\":\"j_%zu\",\"in\":", i);
        jstr(jid[i]);
        printf(",\"out\":");
        jstr(wi_to_jid(jid[i]));
        printf("}\n");
    }

    static const char *const men[] = {
        "6012@s.whatsapp.net", "6012@lid", "6012@x.com", "user@s.whatsapp.net",
        "6012", "+6012", "user:dev@lid", "", "12345@s.whatsapp.net"
    };
    for (size_t i = 0; i < sizeof(men) / sizeof(men[0]); i++) {
        printf("{\"family\":\"mention\",\"case\":\"m_%zu\",\"in\":", i);
        jstr(men[i]);
        printf(",\"out\":");
        jstr(wi_normalize_mention_jid(men[i]));
        printf("}\n");
    }
    return 0;
}
