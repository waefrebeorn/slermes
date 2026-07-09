/*
 * t_port_learning_graph_render_helpers.c — faithful verification harness for
 * port_learning_graph_render_helpers.c (v544 extension).
 *
 * Compiled SEPARATELY and linked against lib/libjson/json.o (NOT the slermes
 * binary). Includes the helper directly so it exercises the exact compiled
 * object that ships in the binary.
 *
 * Each check asserts the C port produces output byte-equivalent to the
 * Python source on the same inputs. The harness prints one JSON object per
 * check: {"fn":<name>, "in":<args>, "out":<c_result>}. sta_oracle_lgr.py
 * then recomputes the same function from the LIVE agent/learning_graph_render.py
 * and compares exactly. Exits 0 only if ALL checks pass (and the oracle agrees).
 */

#include "port_learning_graph_render_helpers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int failures = 0;

/* emit a line for the oracle: function name, JSON input, C output (string) */
static void emit_str(const char *fn, const char *in_json, const char *out)
{
    /* JSON-encode the string output (quote + escape) */
    printf("{\"fn\":\"%s\",\"in\":%s,\"out\":", fn, in_json);
    const char *p = out;
    putchar('"');
    while (*p) {
        if (*p == '"' || *p == '\\') putchar('\\');
        putchar(*p);
        p++;
    }
    printf("\"}\n");
}

/* emit a line where the C output is a JSON number */
static void emit_num(const char *fn, const char *in_json, const char *numstr)
{
    printf("{\"fn\":\"%s\",\"in\":%s,\"out\":%s}\n", fn, in_json, numstr);
}

static int apx(double a, double b, double eps)
{ return fabs(a - b) <= eps; }

static void check_d(const char *fn, const char *in_json, double got, double exp, double eps)
{
    char bout[64];
    snprintf(bout, sizeof(bout), "%.10g", got);
    if (apx(got, exp, eps)) {
        printf("  PASS  %s = %.6f (in %s)\n", fn, got, in_json);
        emit_num(fn, in_json, bout);
    } else {
        printf("  FAIL  %s got %.6f exp %.6f (in %s)\n", fn, got, exp, in_json);
        emit_num(fn, in_json, bout);
        failures++;
    }
}

int main(void)
{
    /* ---- _clamp ---- */
    check_d("_clamp", "[-5,0,10]", learning_graph_render_clamp(-5,0,10), 0.0, 1e-12);
    check_d("_clamp", "[15,0,10]", learning_graph_render_clamp(15,0,10), 10.0, 1e-12);
    check_d("_clamp", "[5,0,10]",  learning_graph_render_clamp(5,0,10), 5.0, 1e-12);
    check_d("_clamp", "[3.3,1,2]", learning_graph_render_clamp(3.3,1,2), 2.0, 1e-12);

    /* ---- _smoothstep ---- */
    check_d("_smoothstep", "[-0.5]", learning_graph_render_smoothstep(-0.5), 0.0, 1e-12);
    check_d("_smoothstep", "[0.5]",  learning_graph_render_smoothstep(0.5), 0.5, 1e-12);
    check_d("_smoothstep", "[1.5]",  learning_graph_render_smoothstep(1.5), 1.0, 1e-12);
    check_d("_smoothstep", "[0.25]", learning_graph_render_smoothstep(0.25), 0.25*0.25*(3-0.5), 1e-12);

    /* ---- format_date ---- */
    {
        struct { double ts; const char *name; } fd[] = {
            {0.0, "[0.0]"}, {1.0, "[1.0]"}, {-1.0, "[-1.0]"},
            {1700000000.0, "[1700000000.0]"}, {4102444800.0, "[4102444800.0]"},
            {1e12, "[1e12]"}, {1e15, "[1e15]"}, {1e18, "[1e18]"},
            {-1e12, "[-1e12]"},
        };
        for (size_t i=0; i<sizeof(fd)/sizeof(fd[0]); i++) {
            char *out = learning_graph_render_format_date(fd[i].ts);
            /* oracle compares; just emit */
            emit_str("format_date", fd[i].name, out);
            printf("  EMIT  format_date %s -> %s\n", fd[i].name, out);
            free(out);
        }
        /* NaN / inf: cannot pass as JSON number; emit sentinel handled by oracle */
        char *nano = learning_graph_render_format_date(NAN);
        emit_str("format_date", "[\"nan\"]", nano);
        free(nano);
        char *info = learning_graph_render_format_date(INFINITY);
        emit_str("format_date", "[\"inf\"]", info);
        free(info);
    }

    /* ---- _rgb_to_hsl / _hsl_to_rgb roundtrip + spot checks ---- */
    {
        int cases[][3] = {{255,0,0},{0,255,0},{0,0,255},{255,255,0},{128,64,200},{0,0,0},{255,255,255}};
        for (size_t i=0; i<sizeof(cases)/sizeof(cases[0]); i++) {
            int r=cases[i][0], g=cases[i][1], b=cases[i][2];
            char *hsl = learning_graph_render_rgb_to_hsl(r,g,b);
            char inbuf[64]; snprintf(inbuf,sizeof(inbuf),"[%d,%d,%d]",r,g,b);
            emit_str("_rgb_to_hsl", inbuf, hsl);
            printf("  EMIT  _rgb_to_hsl %s -> %s\n", inbuf, hsl);
            free(hsl);
        }
        /* hsl->rgb spot checks */
        struct { double h,s,l; } hr[] = {{0,1,0.5},{120,1,0.5},{240,1,0.5},{0,0,0.5},{137.508,0.55,0.62}};
        for (size_t i=0; i<sizeof(hr)/sizeof(hr[0]); i++) {
            char *rgb = learning_graph_render_hsl_to_rgb(hr[i].h, hr[i].s, hr[i].l);
            char inbuf[64]; snprintf(inbuf,sizeof(inbuf),"[%.3f,%.3f,%.3f]",hr[i].h,hr[i].s,hr[i].l);
            emit_str("_hsl_to_rgb", inbuf, rgb);
            printf("  EMIT  _hsl_to_rgb %s -> %s\n", inbuf, rgb);
            free(rgb);
        }
    }

    /* ---- _complementary_ink ---- */
    {
        int cases[][3] = {{255,0,0},{0,255,0},{0,0,255},{255,128,64}};
        for (size_t i=0; i<sizeof(cases)/sizeof(cases[0]); i++) {
            int r=cases[i][0], g=cases[i][1], b=cases[i][2];
            char *out = learning_graph_render_complementary_ink(r,g,b);
            char inbuf[64]; snprintf(inbuf,sizeof(inbuf),"[%d,%d,%d]",r,g,b);
            emit_str("_complementary_ink", inbuf, out);
            printf("  EMIT  _complementary_ink %s -> %s\n", inbuf, out);
            free(out);
        }
    }

    /* ---- _to_ts ---- */
    {
        double vals[] = {1700000000.0, 0.0, -1.0, 123456.789, 4102444800.0};
        for (size_t i=0; i<sizeof(vals)/sizeof(vals[0]); i++) {
            double out = 0; int ok = learning_graph_render_to_ts(&vals[i], &out);
            char inbuf[40]; snprintf(inbuf,sizeof(inbuf),"[%g]",vals[i]);
            char bout[16]; snprintf(bout,sizeof(bout),"%d,%.6g",ok,ok?out:0.0);
            emit_str("_to_ts", inbuf, bout);
            printf("  EMIT  _to_ts %s -> ok=%d out=%.6g\n", inbuf, ok, out);
        }
    }

    /* ---- _period_key / _period_label ---- */
    {
        double ts[] = {1700000000.0, 1609459200.0, 4102444800.0, 0.0};
        const char *gran[] = {"day","month","year"};
        for (size_t i=0; i<sizeof(ts)/sizeof(ts[0]); i++) {
            for (size_t j=0; j<3; j++) {
                char *k = learning_graph_render_period_key(ts[i], gran[j]);
                char *l = learning_graph_render_period_label(ts[i], gran[j]);
                char inbuf[64]; snprintf(inbuf,sizeof(inbuf),"[%.6f,\"%s\"]",ts[i],gran[j]);
                emit_str("_period_key", inbuf, k);
                emit_str("_period_label", inbuf, l);
                printf("  EMIT  _period_key/_period_label %s -> key=%s label=%s\n", inbuf, k, l);
                free(k); free(l);
            }
        }
    }

    /* ---- _node_score / _node_meta ---- */
    {
        /* build small json nodes */
        const char *nodes[] = {
            "{\"kind\":\"memory\",\"timestamp\":1700000000.0,\"memorySource\":\"profile\",\"id\":\"a\"}",
            "{\"kind\":\"memory\",\"timestamp\":1700000000.0,\"id\":\"b\"}",
            "{\"category\":\"skill\",\"useCount\":5,\"pinned\":true,\"timestamp\":1700000000.0,\"id\":\"c\"}",
            "{\"category\":\"tool\",\"useCount\":0,\"timestamp\":1700000000.0,\"id\":\"d\"}",
            "{\"category\":\"reasoning\",\"timestamp\":1700000000.0,\"id\":\"e\"}",
        };
        for (size_t i=0; i<sizeof(nodes)/sizeof(nodes[0]); i++) {
            json_t *node = json_parse(nodes[i], NULL);
            double score = learning_graph_render_node_score(node, 0.7);
            char *meta = learning_graph_render_node_meta(node);
            char inbuf[200]; snprintf(inbuf,sizeof(inbuf),"[%s,0.7]",nodes[i]);
            char sbout[32]; snprintf(sbout,sizeof(sbout),"%.10g",score);
            emit_num("_node_score", inbuf, sbout);
            emit_str("_node_meta", inbuf, meta);
            printf("  EMIT  _node_score/_node_meta %s -> score=%.6f meta=%s\n", inbuf, score, meta);
            free(meta); json_free(node);
        }
    }

    printf("\nC HARNESS (%d failures)\n", failures);
    return failures ? 1 : 0;
}
