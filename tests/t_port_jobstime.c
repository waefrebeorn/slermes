#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_jobstime.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int kind_code(const char *k) {
    if (!strcmp(k, "null")) return 0;
    if (!strcmp(k, "bool")) return 1;
    if (!strcmp(k, "int")) return 2;
    if (!strcmp(k, "float")) return 3;
    return 4;
}

static json_t *status_value(int st, char *s, long long n) {
    json_t *o = json_object();
    if (st == 2) {
        json_set(o, "raises", json_string("ValueError"));
    } else if (st == 1) {
        if (s) { json_set(o, "ok", json_string(s)); free(s); }
        else json_set(o, "ok", json_number((double)n));
    } else {
        json_set(o, "ok", json_null());
    }
    return o;
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

        if (!strcmp(func, "grace") || !strcmp(func, "sec") || !strcmp(func, "tz")) {
            int kind = kind_code(json_get_str(in, "kind", "null"));
            long long ival = 0, n = 0;
            double dval = 0;
            const char *sval = json_get_str(in, "value", "");
            if (kind == 2) ival = strtoll(sval, NULL, 10);
            else if (kind == 3) dval = strtod(sval, NULL);
            if (!strcmp(func, "grace")) {
                int st = jobstime_normalize_grace_minutes(kind, ival, dval, sval, &n);
                out = status_value(st, NULL, n);
            } else if (!strcmp(func, "sec")) {
                int st = jobstime_grace_seconds_override(kind, ival, dval, sval, &n);
                out = json_object();
                json_set(out, "seconds", st == 1 ? json_number((double)n) : json_null());
            } else {
                char *s = NULL;
                int st = jobstime_normalize_timezone(kind, sval, &s);
                out = status_value(st, s, 0);
            }
        } else if (!strcmp(func, "zone")) {
            json_t *v = json_obj_get(in, "value");
            const char *sv = (v && v->type == JSON_STRING) ? v->str_val : NULL;
            char *s = NULL;
            int st = jobstime_zone_for_job(
                !strcmp(json_get_str(in, "mode", "bare"), "dict") ? 1 : 0, sv, &s);
            out = json_object();
            json_set(out, "zone", st == 1 ? json_string(s ? s : "") : json_null());
            free(s);
        } else if (!strcmp(func, "prune")) {
            json_t *rec = json_obj_get(in, "record");
            out = json_copy(rec ? rec : json_null());
            jobstime_prune_inherited_knobs(out);
        } else if (!strcmp(func, "const")) {
            out = json_object();
            json_set(out, "min", json_number((double)jobstime_min_grace_minutes()));
            json_set(out, "max", json_number((double)jobstime_max_grace_minutes()));
            json_set(out, "tzkey", json_string(jobstime_timezone_key()));
            json_set(out, "gkey", json_string(jobstime_grace_key()));
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
