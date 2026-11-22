/* t_port_memory_available.c — harness for memory_tool.c memory_tool_available.
 * Reads {"memory_enabled":0|1,"user_profile_enabled":0|1}, emits input + out.
 * Gate: run_one_oracle.sh memory_available oracle_memory_available.py ""
 *       "src/tools/memory_tool.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int memory_tool_available(int memory_enabled, int user_profile_enabled);

static int jbool(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_BOOL) ? (v->bool_val ? 1 : 0)
         : (v && v->type == JSON_NUMBER) ? (v->num_val != 0) : 0;
}
int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        int me = jbool(in, "memory_enabled"), up = jbool(in, "user_profile_enabled");
        printf("{\"memory_enabled\":%d,\"user_profile_enabled\":%d,\"out\":%d}\n",
               me, up, memory_tool_available(me, up));
        json_free(in);
    }
    return 0;
}
