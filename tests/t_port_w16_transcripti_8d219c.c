#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_transcripti_8d219c.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static int getnum(const json_t *o, const char *k, double *v) {
    json_t *n = json_obj_get(o, k);
    if (n && n->type == JSON_NUMBER) { *v = n->num_val; return 1; }
    return 0;
}
static const char *sget(const json_t *o, const char *k) { return json_get_str(o, k, ""); }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *a = json_obj_get(in, "in");
        double x = 0, y = 0, z = 0, mb = 0, mfs = 26214400.0;
        int hy = 0, hz = 0;
        getnum(a, "max_file_size", &mfs);
        getnum(a, "size", &x);
        getnum(a, "max_bytes", &mb);
        hy = getnum(a, "max_seconds", &y);
        hz = getnum(a, "duration", &z);
        json_t *out = NULL;
        if (!strcmp(func, "upload_limit"))
            out = w16_transcripti_8d219c_upload_limit(sget(a,"provider"), sget(a,"model"), mfs);
        else if (!strcmp(func, "fits"))
            out = w16_transcripti_8d219c_fits(x, mb, hy, y, hz, z);
        else if (!strcmp(func, "exceeds")) {
            getnum(a, "max_bytes", &y);
            hy = getnum(a, "max_seconds", &z);
            double probe = 0; int hp = getnum(a, "probe_duration", &probe);
            out = w16_transcripti_8d219c_exceeds_upload_limit(x, y, hy, z, hp, probe);
        } else if (!strcmp(func, "cut_points")) {
            getnum(a, "duration", &x); getnum(a, "target", &y);
            out = w16_transcripti_8d219c_cut_points(x, y, json_obj_get(a, "silences"));
        } else if (!strcmp(func, "too_large")) {
            getnum(a, "max_bytes", &y);
            out = w16_transcripti_8d219c_too_large(x, y, sget(a, "reason"));
        } else out = json_string("UNKNOWN_FUNC");
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
