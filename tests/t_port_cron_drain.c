/* t_port_cron_drain.c — harness for gateway_gaps2.c get_cron_drain_timeout.
 * Reads {"raw":<json>}, emits {"raw":<json>,"out":<double>}. Oracle replays
 * LIVE parse_cron_drain_timeout with the emitted raw value.
 * Gate: run_one_oracle.sh cron_drain oracle_cron_drain.py ""
 *       "src/hermes_cli/gateway_gaps2.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

double get_cron_drain_timeout(const json_t *raw);

int main(void) {
    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *raw = json_obj_get(in, "raw");
        double out = get_cron_drain_timeout(raw);
        printf("{\"raw\":");
        if (raw) { char *s=json_dumps(raw,0); if(s){fputs(s,stdout);free(s);} else fputs("null",stdout); }
        else fputs("null",stdout);
        printf(",\"out\":%.17g}\n", out);
        json_free(in);
    }
    return 0;
}
