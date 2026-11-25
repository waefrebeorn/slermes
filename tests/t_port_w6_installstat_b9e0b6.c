#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_installstat_b9e0b6.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static int py_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static const char *py_trim(const char *s) {
    static char buf[4100]; size_t n = 0;
    const char *a = s ? s : "";
    while (*a && py_space((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && py_space((unsigned char)b[-1])) b--;
    while (a < b && n < sizeof(buf) - 1) buf[n++] = *a++;
    buf[n] = '\0'; return buf;
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
        if (!strcmp(func, "pd")) {
            out = json_bool(w6_installstat_b9e0b6_provably_deleted(
                json_get_bool(in, "root_exists", 0) ? 1 : 0,
                json_get_bool(in, "root_rel", 0) ? 1 : 0,
                json_get_bool(in, "parent_worktrees", 0) ? 1 : 0,
                json_get_bool(in, "parent_isdir", 0) ? 1 : 0) ? 1 : 0);
        } else if (!strcmp(func, "orph")) {
            int state_isdir = json_get_bool(in, "state_isdir", 0) ? 1 : 0;
            int rec_isfile = json_get_bool(in, "record_isfile", 0) ? 1 : 0;
            const char *rr = py_trim(json_get_str(in, "record_root", ""));
            int root_exists = json_get_bool(in, "root_exists", 0) ? 1 : 0;
            int prov = json_get_bool(in, "provably", 0) ? 1 : 0;
            json_t *rv = json_obj_get(in, "retention");
            int rset = (rv && rv->type == JSON_NUMBER) ? 1 : 0;
            double retention = rset ? rv->num_val : -1.0;
            double age = json_get_num(in, "age", 0);
            out = json_bool(w6_installstat_b9e0b6_is_orphan(
                state_isdir, rec_isfile, rr, root_exists, prov, rset, age, retention) ? 1 : 0);
        } else if (!strcmp(func, "retention")) {
            json_t *cfg = json_obj_get(in, "config");
            json_t *wt = cfg ? json_obj_get(cfg, "worktrees") : NULL;
            json_t *v = wt ? json_obj_get(wt, "install_state_retention_days") : NULL;
            int kind = 1; char valbuf[128] = "14";
            if (v) {
                if (v->type == JSON_NUMBER) { kind = 1; snprintf(valbuf, sizeof valbuf, "%g", v->num_val); }
                else if (v->type == JSON_STRING) { kind = 2; snprintf(valbuf, sizeof valbuf, "%s", v->str_val ? v->str_val : ""); }
                else if (v->type == JSON_BOOL) { kind = 3; strcpy(valbuf, v->bool_val ? "1" : "0"); }
                else kind = 0;
            }
            double secs = w6_installstat_b9e0b6_retention_seconds(kind, valbuf);
            out = secs < 0 ? json_null() : json_number(secs);
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
