/* t_port_cli_config_load.c — oracle harness for hermes_cli/cli_config_load.py
 * pure helpers: _parse_service_tier_config and _merge_file_config. */
#include "port_cli_config_load.h"

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

    /* _parse_service_tier_config: raw -> tier or None. */
    static const char *const raws[] = {
        NULL, "", "fast", "ultra", "auto", "cold", "normal", "bogus",
        "  PRIORITY  ", "off",
    };
    for (size_t i = 0; i < sizeof(raws) / sizeof(raws[0]); i++) {
        char *out = ccl_parse_service_tier_config(raws[i]);
        printf("{\"family\":\"pst\",\"case\":%zu,\"in\":", i);
        jstr(raws[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }

    /* _merge_file_config: defaults + file -> merged defaults. */
    static const char *const defaults = "{\"model\":{\"default\":\"main\"},\"agent\":{},\"terminal\":{\"env_type\":\"local\",\"cwd\":\"/home\"},\"nested\":{\"b\":2}}";
    static const char *const files[] = {
        "{\"model\":\"gpt-4o\"}",
        "{\"model\":{\"model\":\"legacy\"},\"agent\":{}}",
        "{\"model\":{\"default\":\"y\",\"base_url\":\"u\"},\"terminal\":{\"backend\":\"openai\"}}",
        "{\"terminal\":\"weird\"}",
        "{\"terminal\":null}",
        "{\"nested\":{\"a\":1}}",
        "{\"platform_toolsets\":[\"x\"]}",
        "{\"max_turns\":12}",
        "{\"max_turns\":12,\"agent\":{\"max_turns\":5}}",
        "{\"max_turns\":12,\"agent\":{\"max_turns\":null}}",
    };
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        char *out = ccl_merge_file_config(defaults, files[i]);
        printf("{\"family\":\"merge\",\"case\":%zu,\"defaults\":%s,\"file\":",
               i, defaults);
        jstr(files[i]);
        printf(",\"out\":%s}\n", out ? out : "null");
        free(out);
    }
    return 0;
}
