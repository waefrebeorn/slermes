/* t_port_hermes_cli_misc.c — oracle harness for the three thin pure helpers. */
#include "port_hermes_cli_misc.h"
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

    static const char *const srcs[] = { NULL, "", "manual:pool:foo", "env",
                                        "manual:", "manual:a:b", "Manual:x" };
    for (size_t i = 0; i < sizeof(srcs) / sizeof(srcs[0]); i++) {
        printf("{\"family\":\"displaysrc\",\"case\":\"ds_%zu\",\"in\":", i);
        jstr(srcs[i]);
        printf(",\"out\":");
        jstr(misc_display_source(srcs[i]));
        printf("}\n");
    }

    static const char *const cfgs[] = {
        "{}", "{\"backend\":\"default\"}", "{\"backend\":\"Codex \"}",
        "{\"backend\":123}", "{\"other\":1}", "null", "[]", "{\"backend\":\"\"}",
        "{\"backend\":\"  \"}", "{\"backend\":true}",
    };
    for (size_t i = 0; i < sizeof(cfgs) / sizeof(cfgs[0]); i++) {
        printf("{\"family\":\"backend\",\"case\":\"bk_%zu\",\"in\":%s,\"out\":", i, cfgs[i]);
        jstr(misc_configured_backend(cfgs[i]));
        printf("}\n");
    }

    static const char *const vals[] = { "null", "1.5", "7", "-3", "true", "false",
                                        "\"15\"", "\"+15\"", "\"-7\"", "\"\"",
                                        "\"3.5\"", "\"abc\"", "\"  12  \"", "[]" };
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        json_t *v = json_parse(vals[i], NULL);
        printf("{\"family\":\"safeint\",\"case\":\"si_%zu\",\"in\":%s,\"ret\":%lld}\n",
               i, vals[i], (long long)misc_safe_int(v));
        json_free(v);
    }
    return 0;
}
