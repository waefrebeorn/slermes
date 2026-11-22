/* t_port_managed_local_endpoint.c — harness for auxiliary_client_gaps.c
 * is_managed_local_endpoint. Reads {"url":<str|null>,"managed":<str|null>},
 * emits input + out. Gate: run_one_oracle.sh managed_local_endpoint
 * oracle_managed_local_endpoint.py "" "src/agent/auxiliary_client_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_managed_local_endpoint(const char *base_url, const char *managed_netloc);

static const char *jstr(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static void emit_str(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    if (v && v->type == JSON_STRING) { char *s=json_dumps(v,0); if(s){fputs(s,stdout);free(s);} }
    else fputs("null", stdout);
}
int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        int out = is_managed_local_endpoint(jstr(in,"url"), jstr(in,"managed"));
        printf("{\"url\":"); emit_str(in,"url");
        printf(",\"managed\":"); emit_str(in,"managed");
        printf(",\"out\":%d}\n", out);
        json_free(in);
    }
    return 0;
}
