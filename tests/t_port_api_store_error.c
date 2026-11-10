/* t_port_api_store_error.c — oracle harness for store_error_response's status
 * decision. */
#include "port_api_store_error.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    struct { const char *r; int rt; } cases[] = {
        {"invalid_params", 1}, {"runtime_draining", 1},
        {"storage_unavailable", 1}, {"chain_moved", 1},
        {"admission_queued", 1}, {"unknown_reason", 1},
        {"", 0}, {"invalid_params", 0}, {"admission_queued", 0},
        {"", 1},
    };
    for (int i = 0; i < 10; i++) {
        char *out = asr_error_status(cases[i].r, cases[i].rt);
        printf("{\"family\":\"asr\",\"case\":%d,\"reason\":", i);
        /* jstr-equivalent inline */
        fputc('"', stdout);
        for (const unsigned char *p = (const unsigned char *)(cases[i].r ? cases[i].r : ""); *p; p++) {
            if (*p == '"') fputs("\\\"", stdout);
            else if (*p == '\\') fputs("\\\\", stdout);
            else fputc((char)*p, stdout);
        }
        fputc('"', stdout);
        printf(",\"rt\":%d,\"out\":%s}\n", cases[i].rt, out);
        free(out);
    }
    return 0;
}
