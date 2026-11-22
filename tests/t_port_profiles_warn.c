/* t_port_profiles_warn.c — harness for profiles_warn_port.c profiles_warn_read_error.
 * Reads {"profile":<str>,"error":<str>}, emits profile/error + out (1=warned,0=deduped).
 * The static key set persists across lines in this process (one oracle run).
 * Gate: run_one_oracle.sh profiles_warn oracle_profiles_warn.py ""
 *       "src/hermes_cli/profiles_warn_port.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int profiles_warn_read_error(const char *profile, const char *error_msg);

static const char *jstr(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *p = jstr(in, "profile"), *e = jstr(in, "error");
        int out = profiles_warn_read_error(p, e);
        json_t *pj = json_string(p), *ej = json_string(e);
        printf("{\"profile\":");
        if (pj) { char *s=json_dumps(pj,0); if(s){fputs(s,stdout);free(s);} } printf(",\"error\":");
        if (ej) { char *s=json_dumps(ej,0); if(s){fputs(s,stdout);free(s);} }
        printf(",\"out\":%d}\n", out);
        json_free(pj); json_free(ej); json_free(in);
    }
    return 0;
}
