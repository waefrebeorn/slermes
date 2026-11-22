/* t_port_hermes_time_env.c — _env_timezone gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const char *hc_env_timezone(int multiplex_active);
extern void hc_timezone_cache_identity(int multiplex_active, const char *config_path,
                                       char *out, size_t n);

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *tz = json_obj_get(in, "tz");
        json_t *mx = json_obj_get(in, "multiplex");
        if (tz && tz->type == JSON_STRING) setenv("HERMES_TIMEZONE", tz->str_val, 1);
        else unsetenv("HERMES_TIMEZONE");
        int multiplex = (mx && (mx->type == JSON_BOOL ? mx->bool_val :
                                mx->type == JSON_NUMBER ? mx->num_val != 0 : 0)) ? 1 : 0;
        const char *out = hc_env_timezone(multiplex);
        json_t *cp = json_obj_get(in, "config_path");
        const char *config_path = (cp && cp->type == JSON_STRING) ? cp->str_val : "";
        char ident[2048];
        hc_timezone_cache_identity(multiplex, config_path, ident, sizeof(ident));
        fputs("{\"tz\":", stdout);
        if (tz) { char *s = json_dumps(tz, 0); fputs(s ? s : "null", stdout); free(s); }
        else fputs("null", stdout);
        printf(",\"multiplex\":%d,\"out\":", multiplex);
        { char *s = json_dumps(json_string(out), 0); fputs(s ? s : "\"\"", stdout); free(s); }
        fputs(",\"config_path\":", stdout); { char *s=json_dumps(json_string(config_path),0); fputs(s?s:"\"\"",stdout); free(s); }
        fputs(",\"identity\":", stdout); fputs(ident, stdout);
        fputs("}\n", stdout);
        json_free(in);
    }
    return 0;
}
