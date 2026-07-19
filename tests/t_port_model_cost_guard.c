/*
 * t_port_model_cost_guard.c — faithful verification harness for
 * src/cli/port_model_cost_guard.c (hermes_cli/model_cost_guard.py).
 *
 * Reads a model-info JSON fixture from argv[1], runs the two ported C helpers
 * (pricing_from_model_info + format_money), and emits one JSON object per op.
 * The Python oracle (tests/sta_oracle_model_cost_guard.py) recomputes the SAME
 * helpers from the LIVE hermes_cli/model_cost_guard.py; the runner diffs them.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* These helpers live in src/cli/port_model_cost_guard.c (no public header). */
extern void model_cost_guard_format_money(const double *value, char *out, size_t out_sz);
extern int model_cost_guard_pricing_from_model_info(const json_t *model_info,
                                                     double *in_out, double *out_out,
                                                     char *source, size_t source_sz);

static const char *js(const char *s)
{
    static char bufs[4][4096];
    static int bi = 0;
    char *b = bufs[bi];
    bi = (bi + 1) % 4;
    char *q = b;
    *q++ = '"';
    for (const char *p = s; p && *p && q - b < 4000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else *q++ = c;
    }
    *q++ = '"';
    *q = '\0';
    return b;
}

static char *read_all(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <model_info.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char *err = NULL;
    json_t *mi = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }

    double in = 0, out = 0;
    char src[64] = "";
    int present = model_cost_guard_pricing_from_model_info(mi, &in, &out, src, sizeof(src));

    /* Emit via a json_t node so the numeric serialization matches Python
     * (json.dumps) byte-for-byte — avoids %.2f trailing-zero divergence. */
    json_t *pj = json_new_object();
    json_set(pj, "fn", json_string("pricing"));
    json_set(pj, "present", json_number(present));
    if (present) {
        json_set(pj, "in", json_number(in));
        json_set(pj, "out", json_number(out));
    } else {
        json_set(pj, "in", json_number(0.0));
        json_set(pj, "out", json_number(0.0));
    }
    json_set(pj, "source", json_string(src));
    char *pj_s = json_serialize(pj);
    printf("%s\n", pj_s);
    free(pj_s);
    json_free(pj);

    /* format_money on the extracted (or NULL) costs */
    char fm_in[64], fm_out[64];
    model_cost_guard_format_money(present ? &in : NULL, fm_in, sizeof(fm_in));
    model_cost_guard_format_money(present ? &out : NULL, fm_out, sizeof(fm_out));
    printf("{\"fn\":\"money\",\"in\":%s,\"out\":%s}\n", js(fm_in), js(fm_out));

    json_free(mi);
    free(input);
    return 0;
}
