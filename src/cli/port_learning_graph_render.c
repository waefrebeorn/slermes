/*
 * port_learning_graph_render.c
 *
 * Faithful C11 ports of the PURE, stdlib-only helpers from
 * agent/learning_graph_render.py (the learning-timeline terminal renderer).
 * No file I/O, no network, no os/env/catalog calls — only math, color-space
 * conversion, and UTC date formatting. Each function mirrors its Python
 * counterpart exactly (same constants, same rounding).
 *
 * Ported functions (each carries a PoP annotation below its signature):
 *   _clamp                     -> lgr_clamp
 *   _smoothstep                -> lgr_smoothstep
 *   recency_ink                -> lgr_recency_ink
 *   _to_ts                     -> lgr_to_ts
 *   format_date                -> lgr_format_date
 *   hex_to_rgb                 -> lgr_hex_to_rgb
 *   rgb_to_hex                 -> lgr_rgb_to_hex
 *   mix_rgb                    -> lgr_mix_rgb
 *   _rgb_to_hsl                -> lgr_rgb_to_hsl
 *   _hsl_to_rgb                -> lgr_hsl_to_rgb
 *   _complementary_ink         -> lgr_complementary_ink
 *   _node_score                -> lgr_node_score
 *   _node_label                -> lgr_node_label
 *   _node_meta                 -> lgr_node_meta
 *   _period_label              -> lgr_period_label
 *   derive_palette             -> lgr_derive_palette
 */

#include <math.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* ---- constants mirrored from learning_graph_render.py (color.ts / time-axis.ts) */
#define LGR_AGE_OLD_INK   0.42
#define LGR_AGE_MID_INK   0.74
#define LGR_AGE_NEW_INK   0.95
#define LGR_AGE_MID       0.52
#define LGR_LEAD_IN       0.06

static const char *LGR_MON[12] =
    {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

void lgr_complementary_ink(const int c[3], int out[3]);  /* forward decl (defined below) */

/* ---------------------------------------------------------------------------
 * _clamp
 * ------------------------------------------------------------------------- */
/* PoP: lgr_clamp @ agent/learning_graph_render.py:_clamp */
double lgr_clamp(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ---------------------------------------------------------------------------
 * _smoothstep
 * ------------------------------------------------------------------------- */
/* PoP: lgr_smoothstep @ agent/learning_graph_render.py:_smoothstep */
double lgr_smoothstep(double p)
{
    p = lgr_clamp(p, 0.0, 1.0);
    return p * p * (3.0 - 2.0 * p);
}

/* ---------------------------------------------------------------------------
 * recency_ink  (geometry.ts recencyInk)
 * ------------------------------------------------------------------------- */
/* PoP: lgr_recency_ink @ agent/learning_graph_render.py:recency_ink */
double lgr_recency_ink(double rec)
{
    double t = lgr_clamp(rec, 0.0, 1.0);
    if (t <= LGR_AGE_MID)
        return LGR_AGE_OLD_INK +
               (LGR_AGE_MID_INK - LGR_AGE_OLD_INK) * lgr_smoothstep(t / LGR_AGE_MID);
    return LGR_AGE_MID_INK +
           (LGR_AGE_NEW_INK - LGR_AGE_MID_INK) *
               lgr_smoothstep((t - LGR_AGE_MID) / (1.0 - LGR_AGE_MID));
}

/* ---------------------------------------------------------------------------
 * _to_ts  — None on missing/unparseable, else float(value)
 * Returns NAN to represent Python None.
 * ------------------------------------------------------------------------- */
/* PoP: lgr_to_ts @ agent/learning_graph_render.py:_to_ts */
double lgr_to_ts(const char *value)
{
    if (!value) return NAN;
    if (!*value) return NAN;
    char *end = NULL;
    double d = strtod(value, &end);
    if (*end != '\0') return NAN;   /* trailing junk -> ValueError -> None */
    return d;
}

/* ---------------------------------------------------------------------------
 * format_date  — "unknown" for falsy ts, else "%-d %b %Y" (UTC, English)
 * ------------------------------------------------------------------------- */
/* PoP: lgr_format_date @ agent/learning_graph_render.py:format_date */
void lgr_format_date(double ts, char out[32])
{
    if (ts == 0.0 || isnan(ts) || ts < 0.0) {
        strcpy(out, "unknown");
        return;
    }
    time_t t = (time_t)ts;
    struct tm tm;
    gmtime_r(&t, &tm);
    char day[4];
    snprintf(day, sizeof(day), "%d", tm.tm_mday);   /* no leading zero (%-d) */
    snprintf(out, 32, "%s %s %d", day, LGR_MON[tm.tm_mon], tm.tm_year + 1900);
}

/* ---------------------------------------------------------------------------
 * _period_label  — day -> "%-d %b", month -> "%b %Y", else ""
 * ------------------------------------------------------------------------- */
/* PoP: lgr_period_label @ agent/learning_graph_render.py:_period_label */
void lgr_period_label(double ts, const char *granularity, char out[32])
{
    if (isnan(ts)) { out[0] = '\0'; return; }
    time_t t = (time_t)ts;
    struct tm tm;
    gmtime_r(&t, &tm);
    if (granularity && strcmp(granularity, "day") == 0) {
        char day[4];
        snprintf(day, sizeof(day), "%d", tm.tm_mday);
        snprintf(out, 32, "%s %s", day, LGR_MON[tm.tm_mon]);
    } else if (granularity && strcmp(granularity, "month") == 0) {
        snprintf(out, 32, "%s %d", LGR_MON[tm.tm_mon], tm.tm_year + 1900);
    } else {
        out[0] = '\0';
    }
}

/* ---------------------------------------------------------------------------
 * hex_to_rgb  — strips leading #, expands #rgb, falls back to (255,215,0)
 * Returns 0 on success, -1 on unparseable (fallback applied).
 * ------------------------------------------------------------------------- */
/* PoP: lgr_hex_to_rgb @ agent/learning_graph_render.py:hex_to_rgb */
int lgr_hex_to_rgb(const char *s, int out[3])
{
    while (s && (*s == ' ' || *s == '\t')) s++;
    if (s && *s == '#') s++;
    char buf[7];
    size_t n = 0;
    for (; s && *s && n < 6; s++) {
        if (isxdigit((unsigned char)*s)) buf[n++] = (char)tolower((unsigned char)*s);
    }
    if (n == 3) {  /* #rgb -> #rrggbb */
        char e[7];
        e[0] = buf[0]; e[1] = buf[0]; e[2] = buf[1];
        e[3] = buf[1]; e[4] = buf[2]; e[5] = buf[2]; e[6] = '\0';
        memcpy(buf, e, 6);
        n = 6;
    }
    if (n < 6) { out[0] = 255; out[1] = 215; out[2] = 0; return -1; }
    buf[6] = '\0';
    for (int i = 0; i < 3; i++) {
        char hx[3] = { buf[i * 2], buf[i * 2 + 1], '\0' };
        out[i] = (int)strtol(hx, NULL, 16);
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * rgb_to_hex  — "#%02X%02X%02X" with each channel clamped to [0,255]
 * ------------------------------------------------------------------------- */
/* PoP: lgr_rgb_to_hex @ agent/learning_graph_render.py:rgb_to_hex */
void lgr_rgb_to_hex(const int c[3], char out[8])
{
    int r = (int)lgr_clamp(c[0], 0, 255);
    int g = (int)lgr_clamp(c[1], 0, 255);
    int b = (int)lgr_clamp(c[2], 0, 255);
    snprintf(out, 8, "#%02X%02X%02X", r, g, b);
}

/* ---------------------------------------------------------------------------
 * mix_rgb  — linear interpolate a->b by t in [0,1], rounded
 * ------------------------------------------------------------------------- */
/* PoP: lgr_mix_rgb @ agent/learning_graph_render.py:mix_rgb */
void lgr_mix_rgb(const int a[3], const int b[3], double t, int out[3])
{
    double p = lgr_clamp(t, 0.0, 1.0);
    for (int i = 0; i < 3; i++)
        out[i] = (int)lround(a[i] + (b[i] - a[i]) * p);
}

/* ---------------------------------------------------------------------------
 * _rgb_to_hsl  — (h in degrees, s, light in [0,1])
 * ------------------------------------------------------------------------- */
/* PoP: lgr_rgb_to_hsl @ agent/learning_graph_render.py:_rgb_to_hsl */
void lgr_rgb_to_hsl(const int c[3], double hsl[3])
{
    double r = c[0] / 255.0, g = c[1] / 255.0, b = c[2] / 255.0;
    double mx = fmax(r, fmax(g, b));
    double mn = fmin(r, fmin(g, b));
    double light = (mx + mn) / 2.0;
    double d = mx - mn;
    if (d == 0.0) { hsl[0] = 0.0; hsl[1] = 0.0; hsl[2] = light; return; }
    double s = light > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);
    double h;
    if (mx == r)      h = (g - b) / d + (g < b ? 6.0 : 0.0);
    else if (mx == g) h = (b - r) / d + 2.0;
    else              h = (r - g) / d + 4.0;
    hsl[0] = h * 60.0; hsl[1] = s; hsl[2] = light;
}

/* ---------------------------------------------------------------------------
 * _hsl_to_rgb  — h in degrees, s/light in [0,1]; rounds to 0..255
 * ------------------------------------------------------------------------- */
/* PoP: lgr_hsl_to_rgb @ agent/learning_graph_render.py:_hsl_to_rgb */
void lgr_hsl_to_rgb(double h, double s, double light, int out[3])
{
    double hue = fmod(h, 360.0);
    if (hue < 0.0) hue += 360.0;
    double c = (1.0 - fabs(2.0 * light - 1.0)) * s;
    double x = c * (1.0 - fabs(fmod(hue / 60.0, 2.0) - 1.0));
    double m = light - c / 2.0;
    double r, g, b;
    if      (hue < 60.0)  { r = c; g = x; b = 0.0; }
    else if (hue < 120.0) { r = x; g = c; b = 0.0; }
    else if (hue < 180.0) { r = 0.0; g = c; b = x; }
    else if (hue < 240.0) { r = 0.0; g = x; b = c; }
    else if (hue < 300.0) { r = x; g = 0.0; b = c; }
    else                   { r = c; g = 0.0; b = x; }
    out[0] = (int)lround((r + m) * 255.0);
    out[1] = (int)lround((g + m) * 255.0);
    out[2] = (int)lround((b + m) * 255.0);
}

/* ---------------------------------------------------------------------------
 * _complementary_ink  — rotate hue +165, force s>=0.5, light in [0.5,0.7]
 * ------------------------------------------------------------------------- */
/* PoP: lgr_complementary_ink @ agent/learning_graph_render.py:_complementary_ink */
void lgr_complementary_ink(const int c[3], int out[3])
{
    double hsl[3];
    lgr_rgb_to_hsl(c, hsl);
    double s = fmax(hsl[1], 0.5);
    double l = lgr_clamp(hsl[2], 0.5, 0.7);
    lgr_hsl_to_rgb(hsl[0] + 165.0, s, l, out);
}

/* ---------------------------------------------------------------------------
 * _node_score  — ranking weight for map markers / label rows
 * node is a dict-like struct with fields: kind, useCount, pinned, rec.
 * We accept the needed scalar fields directly.
 * ------------------------------------------------------------------------- */
/* PoP: lgr_node_score @ agent/learning_graph_render.py:_node_score */
double lgr_node_score(const char *kind, double rec,
                      double use_count, int pinned)
{
    if (kind && strcmp(kind, "memory") == 0)
        return 3.5 + rec;
    double use = use_count > 0.0 ? use_count : 0.0;
    double pin = pinned ? 2.0 : 0.0;
    return rec * 2.0 + sqrt(fmax(0.0, use)) + pin;
}

/* ---------------------------------------------------------------------------
 * _node_label  — <=26 chars, else text[:23] + "…"
 * ------------------------------------------------------------------------- */
/* PoP: lgr_node_label @ agent/learning_graph_render.py:_node_label */
void lgr_node_label(const char *label, const char *id, char out[40])
{
    const char *text = (label && *label) ? label : (id ? id : "unknown");
    size_t len = strlen(text);
    if (len <= 26) {
        snprintf(out, 40, "%s", text);
    } else {
        char head[24];
        memcpy(head, text, 23);
        head[23] = '\0';
        /* rstrip trailing spaces before the ellipsis */
        size_t e = 23;
        while (e > 0 && head[e - 1] == ' ') e--;
        head[e] = '\0';
        snprintf(out, 40, "%s…", head);
    }
}

/* ---------------------------------------------------------------------------
 * _node_meta  — one-line descriptor of a node
 * ------------------------------------------------------------------------- */
/* PoP: lgr_node_meta @ agent/learning_graph_render.py:_node_meta */
void lgr_node_meta(const char *kind, const char *memory_source,
                   const char *category, double ts, int use_count, int pinned,
                   char out[128])
{
    out[0] = '\0';
    if (kind && strcmp(kind, "memory") == 0) {
        const char *source = (memory_source && strcmp(memory_source, "profile") == 0)
                               ? "profile memory" : "memory";
        char date[32];
        lgr_format_date(ts, date);
        snprintf(out, 128, "%s · %s", source, date);
        return;
    }
    const char *cat = category ? category : "skill";
    char date[32];
    lgr_format_date(ts, date);
    snprintf(out, 128, "%s · %s", cat, date);
    size_t cur = strlen(out);
    if (use_count > 0) {
        char x[16];
        snprintf(x, sizeof(x), "x%d", use_count);
        if (cur + strlen(x) + 1 < 128) { strcat(out, " · "); strcat(out, x); cur += strlen(x) + 3; }
    }
    if (pinned) {
        if (cur + strlen(" · pinned") + 1 < 128) strcat(out, " · pinned");
    }
}

/* ---------------------------------------------------------------------------
 * derive_palette  — computePalette for a terminal (primary hex in).
 * Writes the 5 hex strings into the supplied 5x8 buffer array.
 *   idx 0 = primary, 1 = memory, 2 = skill, 3 = label, 4 = dim, 5 = bg
 * ------------------------------------------------------------------------- */
/* PoP: lgr_derive_palette @ agent/learning_graph_render.py:derive_palette */
void lgr_derive_palette(const char *primary_hex, int dark,
                         char out[6][8])
{
    int primary[3];
    lgr_hex_to_rgb(primary_hex, primary);
    int base[3]  = { 255, 255, 255 };
    int bg[3]    = { 8, 8, 12 };
    if (!dark) { base[0] = base[1] = base[2] = 0; bg[0] = bg[1] = bg[2] = 250; }

    int mix[3];
    /* primary */
    snprintf(out[0], 8, "%s", primary_hex);
    /* memory = mix(primary, base, 0.12 dark / 0.18 light) */
    lgr_mix_rgb(primary, base, dark ? 0.12 : 0.18, mix);
    lgr_rgb_to_hex(mix, out[1]);
    /* skill = mix(complementary_ink(primary), bg, 0.45) */
    int comp[3];
    lgr_complementary_ink(primary, comp);
    lgr_mix_rgb(comp, bg, 0.45, mix);
    lgr_rgb_to_hex(mix, out[2]);
    /* label = mix(base, bg, 0.35) */
    lgr_mix_rgb(base, bg, 0.35, mix);
    lgr_rgb_to_hex(mix, out[3]);
    /* dim = mix(base, bg, 0.7) */
    lgr_mix_rgb(base, bg, 0.7, mix);
    lgr_rgb_to_hex(mix, out[4]);
    /* bg */
    lgr_rgb_to_hex(bg, out[5]);
}

