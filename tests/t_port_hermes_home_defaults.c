/* t_port_hermes_home_defaults.c — platform default home gate (expand v3). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void hc_expand_hermes_home(const char *path, const char *home, char *out, size_t n);
extern void hc_platform_default_hermes_home(const char *suffix, const char *local_appdata,
                                            const char *user_home, int win32,
                                            char *out, size_t n);
static const char *js(const json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static int ji(const json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    return (v && v->type == JSON_BOOL) ? (v->bool_val ? 1 : 0) :
           (v && v->type == JSON_NUMBER) ? (v->num_val != 0) : 0;
}
int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        char out[2048];
        hc_platform_default_hermes_home(js(in, "suffix"), js(in, "local_appdata"),
                                        js(in, "user_home"), ji(in, "win32"),
                                        out, sizeof(out));
        fputs("{\"suffix\":", stdout); { char *s = json_dumps(json_string(js(in, "suffix")), 0); fputs(s?s:"\"\"",stdout); free(s); }
        fputs(",\"local_appdata\":", stdout); { char *s = json_dumps(json_string(js(in, "local_appdata")), 0); fputs(s?s:"\"\"",stdout); free(s); }
        fputs(",\"user_home\":", stdout); { char *s = json_dumps(json_string(js(in, "user_home")), 0); fputs(s?s:"\"\"",stdout); free(s); }
        printf(",\"win32\":%d,\"out\":", ji(in, "win32"));
        { char *s = json_dumps(json_string(out), 0); fputs(s?s:"\"\"", stdout); free(s); }
        const char *mv = js(in, "myvar");
        if (mv && *mv) setenv("MYVAR", mv, 1); else unsetenv("MYVAR");
        char expanded[2048];
        hc_expand_hermes_home(js(in, "expand"), js(in, "home"), expanded, sizeof(expanded));
        fputs(",\"home\":", stdout); { char *s=json_dumps(json_string(js(in,"home")),0); fputs(s?s:"\"\"",stdout); free(s); }
        fputs(",\"myvar\":", stdout); { char *s=json_dumps(json_string(js(in,"myvar")),0); fputs(s?s:"\"\"",stdout); free(s); }
        fputs(",\"expand\":", stdout); { char *s=json_dumps(json_string(js(in,"expand")),0); fputs(s?s:"\"\"",stdout); free(s); }
        fputs(",\"expanded\":", stdout); { char *s=json_dumps(json_string(expanded),0); fputs(s?s:"\"\"",stdout); free(s); }
        fputs("}\n", stdout);
        json_free(in);
    }
    return 0;
}
