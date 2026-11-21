/* t_port_cache_point_allowed.c — harness for bedrock_adapter_gaps.c
 * cache_point_allowed. Reads {"model_id", "placement", "rejections":{...}},
 * emits {"in":{...},"out":0|1}. Oracle seeds the LIVE module's rejection map
 * from the emitted input and replays.
 * Gate: run_one_oracle.sh cache_point_allowed oracle_cache_point_allowed.py ""
 *       "src/agent/bedrock_adapter_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cache_point_allowed(const char *model_id, const char *placement,
                        const json_t *rejections);

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *mid = json_get_str(in, "model_id", "");
        const char *pl = json_get_str(in, "placement", "");
        json_t *rej = json_obj_get(in, "rejections");
        int out = cache_point_allowed(mid, pl, rej);
        printf("{\"in\":");
        char *s = json_dumps(in, 0);
        if (s) { fputs(s, stdout); free(s); } else fputs("{}", stdout);
        printf(",\"out\":%d}\n", out);
        json_free(in);
    }
    return 0;
}
