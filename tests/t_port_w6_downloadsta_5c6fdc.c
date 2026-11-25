#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_downloadsta_5c6fdc.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *stem(const char *name) {
    static char b[512];
    size_t n = strlen(name);
    if (n >= 7 && !strcmp(name + n - 7, ".ranges")) n -= 7;
    else if (n >= 5 && !strcmp(name + n - 5, ".part")) n -= 5;
    if (n >= sizeof b) n = sizeof b - 1;
    memcpy(b, name, n); b[n] = 0;
    return b;
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "collect")) {
            double grace = json_get_num(in, "grace", 0);
            json_t *files = json_obj_get(in, "files");
            size_t nf = (files && files->type == JSON_ARRAY) ? json_len(files) : 0;
            const char *keys[256]; int hp[256], hr[256]; double pa[256], ra[256];
            size_t nk = 0;
            for (size_t i = 0; i < nf && i < 256; i++) {
                json_t *f = json_get(files, i);
                if (!f || f->type != JSON_OBJECT) continue;
                const char *name = json_get_str(f, "name", "");
                json_t *agev = json_obj_get(f, "age");
                double age = (agev && agev->type == JSON_NUMBER) ? agev->num_val : 0.0;
                int is_part = strstr(name, ".part") != NULL;
                const char *s = stem(name);
                size_t j; int found = 0;
                for (j = 0; j < nk; j++) if (!strcmp(keys[j], s)) { found = 1; break; }
                if (!found) {
                    keys[nk] = strdup(s); hp[nk] = 0; hr[nk] = 0; pa[nk] = ra[nk] = 0.0; j = nk++;
                }
                if (is_part) { hp[j] = 1; pa[j] = age; } else { hr[j] = 1; ra[j] = age; }
            }
            const char *surv[256];
            size_t ns = w6_downloadsta_5c6fdc_surviving_keys(keys, hp, pa, hr, ra, nk, grace, surv, 256);
            out = json_array();
            for (size_t i = 0; i < ns; i++) json_append(out, json_string(surv[i]));
            for (size_t i = 0; i < nk; i++) free((void *)keys[i]);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
