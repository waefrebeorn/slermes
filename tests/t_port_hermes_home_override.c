/* t_port_hermes_home_override.c — context-local home override gate (set/reset v2). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern unsigned hc_set_hermes_home_override(const char *home);
extern void hc_reset_hermes_home_override(unsigned token);
extern const char *hc_get_hermes_home_override(void);
extern unsigned hc_set_hermes_home_override(const char *home);
extern void hc_reset_hermes_home_override(unsigned token);

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *h = json_obj_get(in, "home");
        const char *home = (h && h->type == JSON_STRING) ? h->str_val : NULL;
        unsigned token = hc_set_hermes_home_override(home);
        const char *got = hc_get_hermes_home_override();
        fputs("{\"home\":", stdout);
        if (h) { char *s = json_dumps(h, 0); fputs(s ? s : "null", stdout); free(s); }
        else fputs("null", stdout);
        fputs(",\"out\":", stdout);
        { char *s = json_dumps(json_string(got ? got : ""), 0); fputs(s ? s : "\"\"", stdout); free(s); }
        hc_reset_hermes_home_override(token);
        const char *after_reset = hc_get_hermes_home_override();
        fputs(",\"after_reset\":", stdout);
        { char *s = json_dumps(json_string(after_reset ? after_reset : ""), 0); fputs(s ? s : "\"\"", stdout); free(s); }
        fputs("}\n", stdout);
        hc_reset_hermes_home_override(token);
        json_free(in);
    }
    return 0;
}
