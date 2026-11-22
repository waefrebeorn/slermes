/* t_port_hermes_time_env.c — _env_timezone gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const char *hc_env_timezone(int multiplex_active);

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
        fputs("{\"tz\":", stdout);
        if (tz) { char *s = json_dumps(tz, 0); fputs(s ? s : "null", stdout); free(s); }
        else fputs("null", stdout);
        printf(",\"multiplex\":%d,\"out\":", multiplex);
        { char *s = json_dumps(json_string(out), 0); fputs(s ? s : "\"\"", stdout); free(s); }
        fputs("}\n", stdout);
        json_free(in);
    }
    return 0;
}
