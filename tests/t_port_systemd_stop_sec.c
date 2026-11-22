/* t_port_systemd_stop_sec.c — harness for restart_gaps.c resolve_systemd_timeout_stop_sec.
 * Reads {"drain":<json>,"cron":<json>,"cleanup":<json>,"headroom":<json>,"floor":<json>},
 * emits input + out. Gate: run_one_oracle.sh systemd_stop_sec
 * oracle_systemd_stop_sec.py "" "src/gateway/restart_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int resolve_systemd_timeout_stop_sec(const json_t *drain, const json_t *cron,
                                     const json_t *cleanup, const json_t *headroom,
                                     const json_t *floor_s);

static void emit(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    if (v) { char *s = json_dumps(v, 0); if (s) { fputs(s, stdout); free(s); } }
    else fputs("null", stdout);
}
int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *d=json_obj_get(in,"drain"), *c=json_obj_get(in,"cron"),
               *cl=json_obj_get(in,"cleanup"), *h=json_obj_get(in,"headroom"),
               *f=json_obj_get(in,"floor");
        int out = resolve_systemd_timeout_stop_sec(d,c,cl,h,f);
        printf("{\"drain\":"); emit(in,"drain");
        printf(",\"cron\":"); emit(in,"cron");
        printf(",\"cleanup\":"); emit(in,"cleanup");
        printf(",\"headroom\":"); emit(in,"headroom");
        printf(",\"floor\":"); emit(in,"floor");
        printf(",\"out\":%d}\n", out);
        json_free(in);
    }
    return 0;
}
