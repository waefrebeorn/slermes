/* t_port_gateway_profile_routing.c — Oracle harness for the pure discriminators. */
#include "port_gateway_profile_routing.h"
#include "hermes_json.h"

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

    for (int g = 0; g < 2; g++)
        for (int c = 0; c < 2; c++)
            for (int t = 0; t < 2; t++)
                for (int u = 0; u < 2; u++)
                    printf("{\"family\":\"spec\",\"case\":\"s_%d%d%d%d\",\"g\":%d,\"c\":%d,\"t\":%d,\"u\":%d,\"ret\":%d}\n",
                           g, c, t, u, g, c, t, u, pr_specificity(g, c, t, u));

    static const char *const names[] = { NULL, "", "   ", "bot-alpha", "default",
                                         " Default ", "profile_x" };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        printf("{\"family\":\"botkey\",\"case\":\"b_%zu\",\"in\":", i);
        jstr(names[i]);
        printf(",\"out\":");
        jstr(pr_bot_profile_key(names[i]));
        printf("}\n");
    }

    static const char *const ids[] = { "null", "\"abc\"", "123", "4567890", "true",
                                       "false", "1.5", "-7", "\"\"", "null" };
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        json_t *v = json_parse(ids[i], NULL);
        printf("{\"family\":\"coerce\",\"case\":\"c_%zu\",\"in\":%s,\"out\":", i, ids[i]);
        jstr(pr_coerce_route_id(v));
        printf("}\n");
        json_free(v);
    }

    static const char *const wc[] = { NULL, "", "123@g.us", "group123@g.us",
                                      "abc@broadcast", "x@newsletter", " 987@G.US ",
                                      "user@c.us", "plain", NULL };
    for (size_t i = 0; i < sizeof(wc) / sizeof(wc[0]); i++) {
        printf("{\"family\":\"whatsapp\",\"case\":\"w_%zu\",\"in\":", i);
        jstr(wc[i]);
        printf(",\"ret\":%d}\n", pr_is_whatsapp_non_user_chat(wc[i]));
    }
    return 0;
}
