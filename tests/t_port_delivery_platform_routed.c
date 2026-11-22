/* t_port_delivery_platform_routed.c — harness for scheduler_gaps.c
 * delivery_platform_routed_from_primary_gateway.
 * Reads {"platform":<str>,"routes":<json array>}, emits {"platform":...,"routes":...,"out":0|1}.
 * Gate: run_one_oracle.sh delivery_platform_routed oracle_delivery_platform_routed.py ""
 *       "src/cron/scheduler_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int delivery_platform_routed_from_primary_gateway(const char *platform_name, const json_t *routes);

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *p = json_obj_get(in, "platform");
        json_t *r = json_obj_get(in, "routes");
        const char *pn = (p && p->type == JSON_STRING) ? p->str_val : "";
        int out = delivery_platform_routed_from_primary_gateway(pn, r);
        printf("{\"platform\":\"%s\",\"routes\":", pn);
        if (r) { char *s=json_dumps(r,0); if(s){fputs(s,stdout);free(s);} else fputs("null",stdout); } else fputs("null",stdout);
        printf(",\"out\":%d}\n", out);
        json_free(in);
    }
    return 0;
}
