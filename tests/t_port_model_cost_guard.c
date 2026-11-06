/*
 * t_port_model_cost_guard.c — emit-input oracle harness for
 * src/cli/port_model_cost_guard.c (hermes_cli/model_cost_guard.py).
 *
 * Emits one JSON line per case with the INPUT (model-info fixture) plus the
 * C output, so the Python oracle replays LIVE Python on the identical value.
 *
 * Two families, per embedded fixture:
 *   {fn:"pricing", case, in:<model_info>, present, in, out, source}
 *   {fn:"money",   case, in:[cost_in,cost_out|null], out:["$X.YY/M",...]}
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void model_cost_guard_format_money(const double *value, char *out, size_t out_sz);
extern int model_cost_guard_pricing_from_model_info(const json_t *model_info,
                                                     double *in_out, double *out_out,
                                                     char *source, size_t source_sz);

static void run_case(const char *case_name, const char *model_info_raw)
{
    json_t *parsed = json_parse(model_info_raw ? model_info_raw : "null", NULL);

    double in = 0, out = 0;
    char src[64] = "";
    int present = model_cost_guard_pricing_from_model_info(
        parsed ? parsed : NULL, &in, &out, src, sizeof(src));

    /* pricing line */
    json_t *p = json_new_object();
    json_set(p, "fn", json_string("pricing"));
    json_set(p, "case", json_string(case_name));
    json_set(p, "input", parsed ? json_copy(parsed) : json_null());
    json_set(p, "present", json_number(present));
    json_set(p, "in", present ? json_number(in) : json_number(0.0));
    json_set(p, "out", present ? json_number(out) : json_number(0.0));
    json_set(p, "source", json_string(src));
    char *s = json_serialize(p);
    printf("%s\n", s);
    free(s);
    json_free(p);

    /* money line */
    char fm_in[64], fm_out[64];
    model_cost_guard_format_money(present ? &in : NULL, fm_in, sizeof(fm_in));
    model_cost_guard_format_money(present ? &out : NULL, fm_out, sizeof(fm_out));
    json_t *m = json_new_object();
    json_set(m, "fn", json_string("money"));
    json_set(m, "case", json_string(case_name));
    json_t *mi = json_new_array();
    json_array_append(mi, present ? json_number(in) : json_null());
    json_array_append(mi, present ? json_number(out) : json_null());
    json_set(m, "in", mi);
    json_t *mo = json_new_array();
    json_array_append(mo, json_string(fm_in));
    json_array_append(mo, json_string(fm_out));
    json_set(m, "out", mo);
    s = json_serialize(m);
    printf("%s\n", s);
    free(s);
    json_free(m);

    if (parsed) json_free(parsed);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    struct { const char *name; const char *json; } cases[] = {
        { "null",          "null" },
        { "empty_obj",     "{}" },
        { "zero_costs",    "{\"cost_input\":0,\"cost_output\":0}" },
        { "in_only",       "{\"cost_input\":1.5}" },
        { "out_only",      "{\"cost_output\":2.75}" },
        { "both",          "{\"cost_input\":1.5,\"cost_output\":2.75}" },
        { "in_zero_out_hi","{\"cost_input\":0,\"cost_output\":99.0}" },
        { "in_hi_out_zero","{\"cost_input\":20.0,\"cost_output\":0}" },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char name[64];
        snprintf(name, sizeof(name), "mcg_%s", cases[i].name);
        run_case(name, cases[i].json);
    }
    return 0;
}
