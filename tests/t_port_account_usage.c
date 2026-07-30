/*
 * t_port_account_usage.c — faithful verification harness for
 * src/tools/account_usage.c (agent/account_usage.py).
 *
 * Reads a JSON array fixture from argv[1]; each element is one case with an
 * "op" discriminator covering the three pure helper ports:
 *   title_case_slug  -> "foo_bar-baz" -> "Foo Bar Baz"
 *   fmt_usd          -> f"${d:,.2f}" with thousands separators
 *   is_finite_num    -> finite real-number test
 * The Python oracle (tests/sta_oracle_account_usage.py) recomputes the SAME
 * helpers from the LIVE agent/account_usage.py; the runner diffs them.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_account_usage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static json_t *emit_title_case(const json_t *c)
{
    const char *value = json_get_str(c, "value", "");
    const char *out = account_usage_title_case_slug(value);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("title_case_slug"));
    json_set(o, "out", json_string(out ? out : ""));
    return o;
}

static json_t *emit_fmt_usd(const json_t *c)
{
    double d = json_get_num(c, "value", 0.0);
    char buf[64];
    account_usage_fmt_usd(d, buf, sizeof(buf));
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("fmt_usd"));
    json_set(o, "out", json_string(buf));
    return o;
}

static json_t *emit_is_finite(const json_t *c)
{
    double v = json_get_num(c, "value", 0.0);
    bool fin = account_usage_is_finite_num(v);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("is_finite_num"));
    json_set(o, "out", json_bool(fin));
    return o;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char *err = NULL;
    json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }

    int n = json_array_size(root);
    for (int i = 0; i < n; i++) {
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (strcmp(op, "title_case_slug") == 0)      o = emit_title_case(c);
        else if (strcmp(op, "fmt_usd") == 0)         o = emit_fmt_usd(c);
        else if (strcmp(op, "is_finite_num") == 0)   o = emit_is_finite(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }

        char *ser = json_serialize(o);
        printf("%s\n", ser);
        free(ser);
        json_free(o);
    }
    free(input);
    return 0;
}
