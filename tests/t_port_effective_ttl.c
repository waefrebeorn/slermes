/* t_port_effective_ttl.c — harness for prompt_caching_gaps.c effective_cache_ttl.
 * Reads the resolved facts, emits them plus the returned TTL string.
 * Gate: run_one_oracle.sh effective_ttl oracle_effective_ttl.py ""
 *       "src/agent/prompt_caching_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *effective_cache_ttl(const char *ttl, const char *provider, int auto_flag,
                                int measured_1h, int no_1h_tier, int catalog_route,
                                int qwen_model, int alibaba_family, const char *route_ceiling);

static const char *js(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static int jb(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_BOOL && v->bool_val) ? 1 : 0;
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
        const char *out = effective_cache_ttl(js(in,"ttl"), js(in,"provider"), jb(in,"auto"),
                                             jb(in,"measured"), jb(in,"no_1h"), jb(in,"catalog"),
                                             jb(in,"qwen"), jb(in,"alibaba"), js(in,"ceiling"));
        static const char *const keys[] = {"ttl","provider","auto","measured","no_1h",
                                            "catalog","qwen","alibaba","ceiling"};
        putchar('{');
        for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
            printf("\"%s\":", keys[i]); em(in, keys[i]); putchar(',');
        }
        printf("\"out\":");
        json_t *oj = json_string(out ? out : "");
        char *s = json_dumps(oj, 0);
        if (s) { fputs(s, stdout); free(s); } else fputs("\"\"", stdout);
        json_free(oj);
        printf("}\n");
        json_free(in);
    }
    return 0;
}
