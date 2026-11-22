/* t_port_compress_fast_lane.c — harness for auxiliary_client_gaps.c
 * resolve_compression_fast_lane. Reads the resolved facts + inputs, emits
 * them plus out. Gate: run_one_oracle.sh compress_fast_lane
 * oracle_compress_fast_lane.py "" "src/agent/auxiliary_client_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int resolve_compression_fast_lane(const char *actual_norm, const char *actual_model,
                                  const char *provider_norm, const char *cfg_provider,
                                  const char *cfg_model, int non_reasoning,
                                  const char *requested_provider, const char *requested_model);

static const char *js(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static void em(json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    if (v) { char *s = json_dumps(v, 0); if (s) { fputs(s, stdout); free(s); } }
    else fputs("null", stdout);
}
int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *nr = json_obj_get(in, "non_reasoning");
        int flag = (nr && nr->type == JSON_BOOL) ? nr->bool_val : 0;
        int out = resolve_compression_fast_lane(
            js(in, "actual_norm"), js(in, "actual_model"), js(in, "provider_norm"),
            js(in, "cfg_provider"), js(in, "cfg_model"), flag,
            js(in, "requested_provider"), js(in, "requested_model"));
        static const char *const keys[] = {"actual_norm", "actual_model", "provider_norm",
            "cfg_provider", "cfg_model", "non_reasoning", "requested_provider", "requested_model"};
        putchar('{');
        for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
            printf("\"%s\":", keys[i]); em(in, keys[i]); putchar(',');
        }
        printf("\"out\":%d}\n", out);
        json_free(in);
    }
    return 0;
}
