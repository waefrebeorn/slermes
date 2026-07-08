/*
 * t_port_learning_graph_render.c — faithful verification for
 * port_learning_graph_render.c.
 *
 * Strategy (matches the v543 faithfulness mandate): assert C output equals
 * the LIVE Python source. The C harness computes every value, dumps them as
 * a compact JSON object on stdout, then a single `python3` invocation
 * recomputes the same values from agent/learning_graph_render.py and does
 * an exact byte comparison, printing PYCOMPARE OK / MISMATCH <key>.
 */

#include "port_learning_graph_render.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main(void)
{
    int rgb[3];

    /* ---- scalars / doubles ---- */
    double clamp_lo   = lgr_clamp(-1, 0, 1);
    double clamp_hi   = lgr_clamp(5, 0, 1);
    double clamp_mid  = lgr_clamp(0.4, 0, 1);
    double smooth_0   = lgr_smoothstep(0);
    double smooth_1   = lgr_smoothstep(1);
    double smooth_05  = lgr_smoothstep(0.5);
    double smooth_ov  = lgr_smoothstep(2);
    double rec_0      = lgr_recency_ink(0);
    double rec_mid    = lgr_recency_ink(0.52);
    double rec_1      = lgr_recency_ink(1);
    double rec_ov     = lgr_recency_ink(2);
    int   to_ts_none  = isnan(lgr_to_ts(NULL)) ? 1 : 0;
    int   to_ts_bad   = isnan(lgr_to_ts("abc")) ? 1 : 0;
    double to_ts_int  = lgr_to_ts("1700000000.5");

    lgr_hex_to_rgb("#abc", rgb);
    char hex3[8]; snprintf(hex3, 8, "#%02X%02X%02X", rgb[0], rgb[1], rgb[2]);
    lgr_hex_to_rgb("#1a2b3c", rgb);
    char hex6[8]; snprintf(hex6, 8, "#%02X%02X%02X", rgb[0], rgb[1], rgb[2]);
    lgr_hex_to_rgb("zz", rgb);
    char hexbad[8]; snprintf(hexbad, 8, "#%02X%02X%02X", rgb[0], rgb[1], rgb[2]);

    int in[3] = {26, 43, 60}; char rgb2hex[8]; lgr_rgb_to_hex(in, rgb2hex);

    int a[3] = {0,0,0}, b[3] = {100,200,50}, m[3];
    lgr_mix_rgb(a, b, 0.5, m);
    char mix_half[8]; snprintf(mix_half, 8, "#%02X%02X%02X", m[0], m[1], m[2]);

    int red[3] = {255,0,0}; double hsl[3]; lgr_rgb_to_hsl(red, hsl);
    double hsl_r = hsl[0];
    int grn[3]; lgr_hsl_to_rgb(120, 1.0, 0.5, grn);
    char hsl2rgb_g[8]; snprintf(hsl2rgb_g, 8, "#%02X%02X%02X", grn[0], grn[1], grn[2]);
    int comp[3]; lgr_complementary_ink(red, comp);
    char comp_hex[8]; snprintf(comp_hex, 8, "#%02X%02X%02X", comp[0], comp[1], comp[2]);

    double node_mem   = lgr_node_score("memory", 0.5, 0, 0);
    double node_skill = lgr_node_score("skill", 0.5, 4, 1);

    char lab_short[40], lab_long[40], lab_id[40];
    lgr_node_label("hello", "X", lab_short);
    lgr_node_label("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "X", lab_long);
    lgr_node_label(NULL, "node-9", lab_id);

    char meta_mem[128], meta_skill[128];
    lgr_node_meta("memory", "profile", NULL, 1700000000.0, 0, 0, meta_mem);
    lgr_node_meta("skill", NULL, "python", 1700000000.0, 3, 1, meta_skill);

    char pal[6][8];
    lgr_derive_palette("#00C2A8", 1, pal);

    char per_day[32], per_month[32];
    lgr_period_label(1700000000.0, "day", per_day);
    lgr_period_label(1700000000.0, "month", per_month);
    char fmt_date[32], fmt_none[32];
    lgr_format_date(1700000000.0, fmt_date);
    lgr_format_date(0.0, fmt_none);

    /* ---- emit compact JSON for the python oracle (full-precision doubles) ---- */
    char *json = malloc(8192);
    snprintf(json, 8192,
        "{"
        "\"clamp_lo\":%.17g,\"clamp_hi\":%.17g,\"clamp_mid\":%.17g,"
        "\"smooth_0\":%.17g,\"smooth_1\":%.17g,\"smooth_05\":%.17g,\"smooth_ov\":%.17g,"
        "\"rec_0\":%.17g,\"rec_mid\":%.17g,\"rec_1\":%.17g,\"rec_ov\":%.17g,"
        "\"to_ts_none\":%d,\"to_ts_bad\":%d,\"to_ts_int\":%.17g,"
        "\"hex3\":\"%s\",\"hex6\":\"%s\",\"hexbad\":\"%s\","
        "\"rgb2hex\":\"%s\",\"mix_half\":\"%s\","
        "\"hsl_r\":%.17g,\"hsl2rgb_g\":\"%s\",\"comp\":\"%s\","
        "\"node_mem\":%.17g,\"node_skill\":%.17g,"
        "\"lab_short\":\"%s\",\"lab_long\":\"%s\",\"lab_id\":\"%s\","
        "\"lab_long_bytelen\":%d,"
        "\"meta_mem\":\"%s\",\"meta_skill\":\"%s\","
        "\"pal_primary\":\"%s\",\"pal_memory\":\"%s\",\"pal_skill\":\"%s\","
        "\"pal_label\":\"%s\",\"pal_dim\":\"%s\",\"pal_bg\":\"%s\","
        "\"per_day\":\"%s\",\"per_month\":\"%s\","
        "\"fmt_date\":\"%s\",\"fmt_none\":\"%s\""
        "}",
        clamp_lo, clamp_hi, clamp_mid,
        smooth_0, smooth_1, smooth_05, smooth_ov,
        rec_0, rec_mid, rec_1, rec_ov,
        to_ts_none, to_ts_bad, to_ts_int,
        hex3, hex6, hexbad,
        rgb2hex, mix_half,
        hsl_r, hsl2rgb_g, comp_hex,
        node_mem, node_skill,
        lab_short, lab_long, lab_id, (int)strlen(lab_long),
        meta_mem, meta_skill,
        pal[0], pal[1], pal[2], pal[3], pal[4], pal[5],
        per_day, per_month, fmt_date, fmt_none);

    FILE *out = fopen("/tmp/lgr_c.json", "w");
    if (out) { fputs(json, out); fclose(out); }
    free(json);

    /* ---- python oracle: recompute ground truth, exact-compare ---- */
    int rc = system("python3 tests/lgr_oracle.py");
    return rc ? 1 : 0;
}