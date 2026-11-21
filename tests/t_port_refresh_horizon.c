/* t_port_refresh_horizon.c — harness for nous_auth_keepalive_gaps.c
 * refresh_horizon_seconds. Reads {"tick":i,"floor":i}, emits echoed + out.
 * Gate: run_one_oracle.sh refresh_horizon oracle_refresh_horizon.py ""
 *       "src/hermes_cli/nous_auth_keepalive_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int refresh_horizon_seconds(int tick_seconds, int floor_seconds);

int main(void) {
    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        int tick = (int)json_get_num(in, "tick", 0);
        int floor = (int)json_get_num(in, "floor", 0);
        int out = refresh_horizon_seconds(tick, floor);
        printf("{\"tick\":%d,\"floor\":%d,\"out\":%d}\n", tick, floor, out);
        json_free(in);
    }
    return 0;
}
