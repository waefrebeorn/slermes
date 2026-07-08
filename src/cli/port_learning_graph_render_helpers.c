/*
 * port_learning_graph_render_helpers.c
 *
 * Pure, portable color/time-label helpers ported from
 * agent/learning_graph_render.py. These are self-contained numeric/string
 * transforms (no graphviz/networkx/IO): recency fade curves, hex<->rgb
 * conversion, rgb mixing, HSL round-trips, complementary ink, palette
 * derivation, and node-label truncation. decode/encode outputs that are
 * dicts (derive_palette) return malloc'd JSON strings via libjson.
 *
 * Module prefix used by the scanner for agent/learning_graph_render.py is
 * "learning_graph_render_".
 *
 * C name <- python name (learning_graph_render_ prefix):
 *   recency_ink, hex_to_rgb, rgb_to_hex, mix_rgb, derive_palette, node_label
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "hermes_json.h"

/* constants from the python module */
#define AGE_OLD_INK 0.42
#define AGE_MID_INK 0.74
#define AGE_NEW_INK 0.95
#define AGE_MID     0.52
#define LEAD_IN     0.06

/* ---- private helpers (re-used by the PoP public wrappers below) ---- */
static double _clamp_impl(double v, double lo, double hi)
{ return v < lo ? lo : (v > hi ? hi : v); }

static double _smoothstep_impl(double p)
{ p = _clamp_impl(p, 0.0, 1.0); return p * p * (3.0 - 2.0 * p); }

/* ---- rgb triple helper ---- */
typedef struct { int r, g, b; } rgb_t;

static rgb_t hex_to_rgb_impl(const char *s)
{
    char buf[16];
    /* strip leading # and whitespace */
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '#') s++;
    size_t n = 0;
    while (*s && n < 6) { buf[n++] = *s++; }
    buf[n] = 0;
    if (n == 3) {
        char e[7];
        e[0]=buf[0]; e[1]=buf[0]; e[2]=buf[1]; e[3]=buf[1];
        e[4]=buf[2]; e[5]=buf[2]; e[6]=0;
        memcpy(buf, e, 7);
    }
    if (n < 6) return (rgb_t){255,215,0};
    char rh[3]={buf[0],buf[1],0}, gh[3]={buf[2],buf[3],0}, bh[3]={buf[4],buf[5],0};
    unsigned int r=255,g=215,b=0;
    if (sscanf(rh,"%x",&r)!=1) r=255;
    if (sscanf(gh,"%x",&g)!=1) g=215;
    if (sscanf(bh,"%x",&b)!=1) b=0;
    return (rgb_t){(int)r,(int)g,(int)b};
}

/* ---------------------------------------------------------------------- */
/* PoP: recency_ink @ agent/learning_graph_render.py:recency_ink */
double learning_graph_render_recency_ink(double rec)
{
    double t = _clamp_impl(rec, 0.0, 1.0);
    if (t <= AGE_MID)
        return AGE_OLD_INK + (AGE_MID_INK - AGE_OLD_INK) * _smoothstep_impl(t / AGE_MID);
    return AGE_MID_INK + (AGE_NEW_INK - AGE_MID_INK) * _smoothstep_impl((t - AGE_MID) / (1.0 - AGE_MID));
}

/* ---------------------------------------------------------------------- */
/* PoP: hex_to_rgb @ agent/learning_graph_render.py:hex_to_rgb */
/* returns malloc'd "r,g,b" string for easy consumption by callers */
char *learning_graph_render_hex_to_rgb(const char *s)
{
    rgb_t c = hex_to_rgb_impl(s);
    char *out = malloc(16);
    snprintf(out, 16, "%d,%d,%d", c.r, c.g, c.b);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: rgb_to_hex @ agent/learning_graph_render.py:rgb_to_hex */
char *learning_graph_render_rgb_to_hex(int r, int g, int b)
{
    int cr = (int)_clamp_impl(r,0,255), cg=(int)_clamp_impl(g,0,255), cb=(int)_clamp_impl(b,0,255);
    char *out = malloc(8);
    snprintf(out, 8, "#%02X%02X%02X", cr, cg, cb);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: mix_rgb @ agent/learning_graph_render.py:mix_rgb */
/* returns malloc'd "r,g,b" string */
char *learning_graph_render_mix_rgb(int ar, int ag, int ab, int br, int bg, int bb, double t)
{
    double p = _clamp_impl(t, 0.0, 1.0);
    int r = (int)round(ar + (br - ar) * p);
    int g = (int)round(ag + (bg - ag) * p);
    int b = (int)round(ab + (bb - ab) * p);
    char *out = malloc(16);
    snprintf(out, 16, "%d,%d,%d", r, g, b);
    return out;
}

/* ---- HSL round-trip (private, used by complementary_ink / derive_palette) ---- */
static void _rgb_to_hsl_impl(rgb_t c, double *h, double *s, double *l)
{
    double r=c.r/255.0, g=c.g/255.0, b=c.b/255.0;
    double mx=fmax(r,fmax(g,b)), mn=fmin(r,fmin(g,b));
    *l=(mx+mn)/2.0;
    double d=mx-mn;
    if (d==0.0){ *h=0.0; *s=0.0; return; }
    *s = (*l>0.5) ? d/(2.0-mx-mn) : d/(mx+mn);
    if (mx==r) *h=(g-b)/d + (g<b?6.0:0.0);
    else if (mx==g) *h=(b-r)/d + 2.0;
    else *h=(r-g)/d + 4.0;
    *h *= 60.0;
}
static rgb_t _hsl_to_rgb_impl(double h, double s, double l)
{
    double hue = fmod(h,360.0); if (hue<0) hue+=360.0;
    double c=(1.0-fabs(2.0*l-1.0))*s;
    double x=c*(1.0-fabs(fmod(hue/60.0,2.0)-1.0));
    double m=l-c/2.0;
    double r,g,b;
    if (hue<60){ r=c; g=x; b=0; }
    else if (hue<120){ r=x; g=c; b=0; }
    else if (hue<180){ r=0; g=c; b=x; }
    else if (hue<240){ r=0; g=x; b=c; }
    else if (hue<300){ r=x; g=0; b=c; }
    else { r=c; g=0; b=x; }
    return (rgb_t){(int)round((r+m)*255),(int)round((g+m)*255),(int)round((b+m)*255)};
}
static rgb_t _complementary_ink_impl(rgb_t c)
{
    double h,s,l; _rgb_to_hsl_impl(c,&h,&s,&l);
    return _hsl_to_rgb_impl(h+165.0, fmax(s,0.5), _clamp_impl(l,0.5,0.7));
}

/* ---------------------------------------------------------------------- */
/* PoP: derive_palette @ agent/learning_graph_render.py:derive_palette */
/* returns malloc'd JSON string: {primary, memory, skill, label, dim, bg} */
char *learning_graph_render_derive_palette(const char *primary_hex, int dark)
{
    rgb_t primary = hex_to_rgb_impl(primary_hex);
    rgb_t base = dark ? (rgb_t){255,255,255} : (rgb_t){0,0,0};
    rgb_t bg = dark ? (rgb_t){8,8,12} : (rgb_t){250,250,250};
    /* memory = mix(primary, base, 0.12|0.18) */
    double mp = dark ? 0.12 : 0.18;
    int mr = (int)round(primary.r+(base.r-primary.r)*mp);
    int mg = (int)round(primary.g+(base.g-primary.g)*mp);
    int mb = (int)round(primary.b+(base.b-primary.b)*mp);
    /* skill = mix(complement, bg, 0.45) */
    rgb_t comp = _complementary_ink_impl(primary);
    int sr=(int)round(comp.r+(bg.r-comp.r)*0.45);
    int sg=(int)round(comp.g+(bg.g-comp.g)*0.45);
    int sb=(int)round(comp.b+(bg.b-comp.b)*0.45);
    /* label = mix(base, bg, 0.35) */
    int lr=(int)round(base.r+(bg.r-base.r)*0.35);
    int lg=(int)round(base.g+(bg.g-base.g)*0.35);
    int lb=(int)round(base.b+(bg.b-base.b)*0.35);
    /* dim = mix(base, bg, 0.7) */
    int dr=(int)round(base.r+(bg.r-base.r)*0.7);
    int dg=(int)round(base.g+(bg.g-base.g)*0.7);
    int db=(int)round(base.b+(bg.b-base.b)*0.7);

    char mem[8],skl[8],lbl[8],dm[8],bgc[8];
    snprintf(mem,8,"#%02X%02X%02X",mr,mg,mb);
    snprintf(skl,8,"#%02X%02X%02X",sr,sg,sb);
    snprintf(lbl,8,"#%02X%02X%02X",lr,lg,lb);
    snprintf(dm,8,"#%02X%02X%02X",dr,dg,db);
    snprintf(bgc,8,"#%02X%02X%02X",bg.r,bg.g,bg.b);

    json_t *root = json_new_object();
    json_object_set(root,"primary",json_string(primary_hex));
    json_object_set(root,"memory",json_string(mem));
    json_object_set(root,"skill",json_string(skl));
    json_object_set(root,"label",json_string(lbl));
    json_object_set(root,"dim",json_string(dm));
    json_object_set(root,"bg",json_string(bgc));
    char *out = json_dumps(root, 0);
    json_free(root);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: _node_label @ agent/learning_graph_render.py:_node_label */
char *learning_graph_render_node_label(const char *label, const char *id)
{
    const char *src = (label && *label) ? label : (id ? id : "unknown");
    size_t len = strlen(src);
    char *out;
    if (len <= 26) {
        out = malloc(len + 1);
        strcpy(out, src);
    } else {
        /* text[:23].rstrip() + "…" */
        char tmp[24];
        memcpy(tmp, src, 23); tmp[23]=0;
        /* rstrip */
        int i=22; while (i>=0 && (tmp[i]==' '||tmp[i]=='\t')) tmp[i--]=0;
        size_t n = strlen(tmp);
        out = malloc(n + 4);
        snprintf(out, n+4, "%s…", tmp);
    }
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: _clamp @ agent/learning_graph_render.py:_clamp */
double learning_graph_render_clamp(double v, double lo, double hi)
{
    return _clamp_impl(v, lo, hi);
}

/* ---------------------------------------------------------------------- */
/* PoP: _smoothstep @ agent/learning_graph_render.py:_smoothstep */
double learning_graph_render_smoothstep(double p)
{
    return _smoothstep_impl(p);
}

/* ---------------------------------------------------------------------- */
/* PoP: _rgb_to_hsl @ agent/learning_graph_render.py:_rgb_to_hsl */
/* returns malloc'd "h,s,l" string (h rounded to 2dp, s/l to 4dp) */
char *learning_graph_render_rgb_to_hsl(int r, int g, int b)
{
    rgb_t c = {(int)_clamp_impl(r,0,255), (int)_clamp_impl(g,0,255), (int)_clamp_impl(b,0,255)};
    double h, s, l;
    _rgb_to_hsl_impl(c, &h, &s, &l);
    char *out = malloc(48);
    snprintf(out, 48, "%.2f,%.4f,%.4f", h, s, l);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: _hsl_to_rgb @ agent/learning_graph_render.py:_hsl_to_rgb */
/* returns malloc'd "r,g,b" string */
char *learning_graph_render_hsl_to_rgb(double h, double s, double l)
{
    rgb_t c = _hsl_to_rgb_impl(h, s, l);
    char *out = malloc(16);
    snprintf(out, 16, "%d,%d,%d", c.r, c.g, c.b);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: _complementary_ink @ agent/learning_graph_render.py:_complementary_ink */
/* returns malloc'd "r,g,b" string */
char *learning_graph_render_complementary_ink(int r, int g, int b)
{
    rgb_t c = {(int)_clamp_impl(r,0,255), (int)_clamp_impl(g,0,255), (int)_clamp_impl(b,0,255)};
    rgb_t o = _complementary_ink_impl(c);
    char *out = malloc(16);
    snprintf(out, 16, "%d,%d,%d", o.r, o.g, o.b);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: format_date @ agent/learning_graph_render.py:format_date */
/* UTC "%-d %b %Y" (glibc supports the BSD '-' flag; on platforms that don't
 * we strip the leading zero by hand). Returns malloc'd string, caller frees.
 * Python: None/falsy/unrepresentable -> "unknown". */
char *learning_graph_render_format_date(double ts)
{
    if (!ts || !isfinite(ts)) return strdup("unknown");
    /* guard against overflow that would make gmtime return NULL */
    if (ts > 253402300799.0 || ts < -62167219200.0) return strdup("unknown");
    time_t t = (time_t)ts;
    struct tm tm;
    if (gmtime_r(&t, &tm) == NULL) return strdup("unknown");
    char buf[32];
    size_t n = strftime(buf, sizeof(buf), "%d %b %Y", &tm);
    if (n == 0) return strdup("unknown");
    /* strip a possible leading zero on the day (handles non-BSD strftime) */
    if (buf[0] == '0' && buf[1] != ' ') {
        memmove(buf, buf + 1, strlen(buf) + 1);
    }
    char *out = malloc(strlen(buf) + 1);
    strcpy(out, buf);
    return out;
}
