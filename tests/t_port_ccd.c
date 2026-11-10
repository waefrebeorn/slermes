/* t_port_ccd.c — oracle harness for _cli_config_defaults.
 * Emits {"tag":"ccd","case":0,"out":<json string>}. */
#include "port_cli_config_load.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    json_t *d = ccl_cli_config_defaults();
    char *s = json_serialize(d);
    printf("{\"tag\":\"ccd\",\"case\":0,\"out\":");
    if (!s) { fputs("null", stdout); }
    else {
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
    printf("}\n");
    free(s); json_free(d);
    return 0;
}
