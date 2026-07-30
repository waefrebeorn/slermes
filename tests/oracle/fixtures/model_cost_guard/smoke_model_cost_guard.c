/* Smoke test for model_cost_guard port vs Python reference. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"

extern void model_cost_guard_format_money(const double *value, char *out, size_t out_sz);
extern int model_cost_guard_pricing_from_model_info(const json_t *mi,
        double *in_out, double *out_out, char *source, size_t source_sz);

static int failures = 0;

static void check_fmt(const char *label, const double *val, const char *expected) {
    char got[64];
    model_cost_guard_format_money(val, got, sizeof(got));
    if (strcmp(got, expected) != 0) {
        fprintf(stderr, "FAIL fmt[%s]: got '%s' want '%s'\n", label, got, expected);
        failures++;
    } else {
        printf("ok fmt[%s] = %s\n", label, got);
    }
}

static void check_pricing(const char *label, const char *json_str,
                          int exp_present, double exp_in, double exp_out, const char *exp_src) {
    char *err = NULL;
    json_t *mi = json_parse(json_str, &err);
    if (err) free(err);
    double in=0, out=0; char src[64]="";
    int present = model_cost_guard_pricing_from_model_info(mi, &in, &out, src, sizeof(src));
    int ok = 1;
    if (present != exp_present) ok = 0;
    if (exp_present) {
        if (in != exp_in || out != exp_out) ok = 0;
        if (strcmp(src, exp_src) != 0) ok = 0;
    }
    if (!ok) {
        fprintf(stderr, "FAIL pricing[%s]: present=%d(want %d) in=%.2f(want %.2f) out=%.2f(want %.2f) src='%s'(want '%s')\n",
                label, present, exp_present, in, exp_in, out, exp_out, src, exp_src);
        failures++;
    } else {
        printf("ok pricing[%s] present=%d in=%.2f out=%.2f src=%s\n", label, present, in, out, src);
    }
    if (mi) json_free(mi);
    if (err) free(err);
}

int main(void) {
    /* format_money */
    check_fmt("null", NULL, "unknown");
    double z=0, a=20, b=20.5, c=100, d=1234.567;
    check_fmt("0", &z, "$0.00/M");
    check_fmt("20", &a, "$20.00/M");
    check_fmt("20.5", &b, "$20.50/M");
    check_fmt("100", &c, "$100.00/M");
    check_fmt("1234.567", &d, "$1234.57/M");

    /* pricing: NULL model -> absent */
    check_pricing("none", "null", 0, 0, 0, "");
    check_pricing("empty", "{}", 0, 0, 0, "");
    check_pricing("no_cost", "{\"name\":\"x\"}", 0, 0, 0, "");
    check_pricing("zero_cost", "{\"cost_input\":0.0,\"cost_output\":0.0}", 0, 0, 0, "");
    check_pricing("input_only", "{\"cost_input\":20.0,\"cost_output\":0.0}", 1, 20.0, 0.0, "models.dev");
    check_pricing("both", "{\"cost_input\":20.0,\"cost_output\":100.0}", 1, 20.0, 100.0, "models.dev");
    if (failures) { printf("\n%d FAILURES\n", failures); return 1; }
    printf("\nALL model_cost_guard checks passed\n");
    return 0;
}
