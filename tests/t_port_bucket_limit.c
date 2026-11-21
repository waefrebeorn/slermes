/* t_port_bucket_limit.c — harness for bucket_limit in web_result_cache_gaps.c.
 * Reads {"limit":n} lines, emits {"limit":n,"out":m}. Oracle replays the
 * LIVE Python bucket_limit on the emitted limit.
 * Gate: run_one_oracle.sh bucket_limit oracle_bucket_limit.py ""
 *       "src/tools/web_result_cache_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bucket_limit(int limit);

int main(void) {
    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        int limit = (int)json_get_num(in, "limit", 0);
        int out = bucket_limit(limit);
        printf("{\"limit\":%d,\"out\":%d}\n", limit, out);
        json_free(in);
    }
    return 0;
}
