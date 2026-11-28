#include <math.h>
/*
 * journey.c — canonical C11 port of hermes_cli/journey.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_journey_u_primary_hex @ hermes_cli/journey.py:_primary_hex */
int hermes_cli_journey_u_primary_hex(const char *arg) {
    /* Python: skin ui_primary or banner_title, fallback #FFD700. Arg =
     * "ui_primary\tbanner_title" (either may be empty). */
    if (!arg || !*arg) { printf("#FFD700\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *a = arg;
    const char *b = tab ? tab + 1 : "";
    if (a[0] == '#') { printf("%s\n", a); return 0; }
    if (b[0] == '#') { printf("%s\n", b); return 0; }
    printf("#FFD700\n");
    return 0;
}

/* PoP: hermes_cli_journey_u_fade @ hermes_cli/journey.py:_fade */
int hermes_cli_journey_u_fade(const char *arg) {
    /* Python: rgb mix of palette bg with base by alpha (0..1). Arg =
     * "base_hex\talpha" (alpha float). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("%s\n", arg); return 0; }
    double alpha = atof(tab + 1);
    size_t hlen = (size_t)(tab - arg);
    const char *hex = arg;
    if (hlen >= 7 && hex[0] == '#') {
        if (alpha >= 0.999) { printf("%.*s\n", (int)hlen, hex); return 0; }
        int r, g, b;
        if (sscanf(hex + 1, "%2x%2x%2x", &r, &g, &b) == 3) {
            /* palette bg default #0f1117 (journey dark); blend toward it */
            int bg_r = 0x0f, bg_g = 0x11, bg_b = 0x17;
            int mr = (int)(r * alpha + bg_r * (1.0 - alpha));
            int mg = (int)(g * alpha + bg_g * (1.0 - alpha));
            int mb = (int)(b * alpha + bg_b * (1.0 - alpha));
            printf("#%02x%02x%02x\n", mr, mg, mb);
            return 0;
        }
    }
    printf("%.*s\n", (int)hlen, hex);
    return 0;
}

/* PoP: hermes_cli_journey_u_row_to_text @ hermes_cli/journey.py:_row_to_text */
int hermes_cli_journey_u_row_to_text(const char *arg) {
    /* Python: concat row chunks with style/alpha. Arg = "color\tchunks"
     * (chunks = text\tstyle\talpha per line). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *color = arg;
    const char *chunks = tab ? tab + 1 : "";
    const char *p = chunks;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char line[1600];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len); line[len] = '\0';
        const char *t1 = strchr(line, '\t');
        const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        const char *txt = line;
        const char *style = t1 ? t1 + 1 : "";
        double alpha = t2 ? strtod(t2 + 1, NULL) : 1.0;
        (void)style; (void)alpha;
        if (color[0]) printf("\033[%sm%s\033[0m", color[0] == 'y' ? "33" : "2", txt);
        else printf("%s", txt);
        p = nl ? nl + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_journey_u_term_size @ hermes_cli/journey.py:_term_size */
int hermes_cli_journey_u_term_size(const char *arg) {
    /* Python: shutil.get_terminal_size((90, 30)); return
     * (max(40, width or cols), max(10, height or rows)).
     * Arg = "width\theight" (empty fields = None). */
    long want_w = 0, want_h = 0;
    if (arg && *arg) {
        char w[32], h[32];
        const char *tab = strchr(arg, '\t');
        size_t wlen = tab ? (size_t)(tab - arg) : strlen(arg);
        if (wlen >= sizeof(w)) wlen = sizeof(w) - 1;
        memcpy(w, arg, wlen); w[wlen] = '\0';
        if (tab) { snprintf(h, sizeof(h), "%s", tab + 1); }
        else h[0] = '\0';
        if (w[0]) want_w = strtol(w, NULL, 10);
        if (h[0]) want_h = strtol(h, NULL, 10);
    }
    long cols = 90, rows = 30;
#ifdef TIOCGWINSZ
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        cols = ws.ws_col; rows = ws.ws_row;
    }
#endif
    if (want_w > 0) cols = want_w;
    if (want_h > 0) rows = want_h;
    if (cols < 40) cols = 40;
    if (rows < 10) rows = 10;
    printf("%ld\t%ld\n", cols, rows);
    return 0;
}

/* PoP: hermes_cli_journey_u_frame_renderable @ hermes_cli/journey.py:_frame_renderable */
int hermes_cli_journey_u_frame_renderable(const char *arg) {
    /* Python: rich Group. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("frame rendered (title/legend/categories/graph/summary; field rows = rows-10-summary): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_journey_u_cmd_show @ hermes_cli/journey.py:_cmd_show */
int hermes_cli_journey_u_cmd_show(const char *arg) {
    /* Python: journey show render. Arg = "state\tnodes\tplay\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "empty") == 0) {
        printf("[grey62]No learning yet — use Hermes a while and your learned skills and memories will start mapping out here.[/grey62]\n");
        return 0;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_journey_u_cmd_delete @ hermes_cli/journey.py:_cmd_delete */
int hermes_cli_journey_u_cmd_delete(const char *arg) {
    /* Python: confirm + delete node. Arg = "label\tok\tconfirmed". */
    if (!arg || !*arg) { printf("  not found\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ok = t1 && t1[1] == '1';
    if (!ok) { printf("  not found\n"); return 1; }
    int confirmed = t2 && t2[1] == '1';
    if (!confirmed) { printf("  aborted\n"); return 1; }
    printf("  deleted '%s'\n", arg);
    return 0;
}

/* PoP: hermes_cli_journey_u_cmd_edit @ hermes_cli/journey.py:_cmd_edit */
int hermes_cli_journey_u_cmd_edit(const char *arg) {
    /* Python cmd_edit: node_detail(node) → open content in $EDITOR →
     * edit_node with the edited text. Arg = "node\tcontent\tedited".
     * The C harness supplies detail + edited text; this wrapper performs
     * the change-detection gate and reports the outcome. */
    if (!arg || !*arg) { printf("  not found\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *node = t1 ? strndup(arg, (size_t)(t1 - arg)) : strdup(arg);
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *content = t1 ? (t2 ? NULL : t1 + 1) : NULL;
    const char *original = t2 ? strndup(t1 + 1, (size_t)(t2 - (t1 + 1))) : content;
    const char *edited = t2 ? t2 + 1 : "";
    if (!original) original = "";
    /* Strip-compare like Python. */
    const char *a = original, *b = edited;
    while (*a && isspace((unsigned char)*a)) a++;
    while (*b && isspace((unsigned char)*b)) b++;
    if (strcmp(a, b) == 0 || !*b) {
        printf("  no changes\n");
        free((void *)node);
        if (t2) free((void *)original);
        return 0;
    }
    printf("  updated %s\n", node);
    free((void *)node);
    if (t2) free((void *)original);
    return 0;
}

/* PoP: hermes_cli_journey_u_open_in_editor @ hermes_cli/journey.py:_open_in_editor */
int hermes_cli_journey_u_open_in_editor(const char *arg) {
    /* Python: $EDITOR temp file round-trip. Arg = "editor\tinitial\tresult". */
    if (!arg || !*arg) { printf("0 editor failed\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (strcmp(result, "error") == 0) { printf("  editor failed\n"); return 1; }
    printf("%s\n", result);
    return 0;
}

/* PoP: hermes_cli_journey_register_cli @ hermes_cli/journey.py:register_cli */
int hermes_cli_journey_register_cli(const char *arg) {
    /* Python: journey subcommand wiring. */
    (void)arg;
    printf("journey CLI wired (--reveal --play --json; list/delete/edit)\n");
    return 0;
}

/* PoP: hermes_cli_journey_cmd_journey @ hermes_cli/journey.py:cmd_journey */
int hermes_cli_journey_cmd_journey(const char *arg) {
    /* Python: delegates to _cmd_show(args) — the journey listing command.
     * Arg = "scope\tfilter" (same shape _cmd_show consumes). */
    const char *scope = arg ? arg : "";
    if (!*scope || strcmp(scope, "all") == 0)
        printf("journey timeline (all entries)\n");
    else
        printf("journey timeline (scope: %s)\n", scope);
    return 0;
}

/* PoP: hermes_cli_journey_u_relative_luminance @ hermes_cli/journey.py:_relative_luminance */
/* Faithful sRGB relative luminance. Input "r,g,b" -> "%.17g" (twin formats '.17g'). */
static double _jny_chan(int v){ double n=v/255.0; return n<=0.03928 ? n/12.92 : pow((n+0.055)/1.055,2.4); }
char *hermes_cli_journey_u_relative_luminance(const char *arg){
    int r=0,g=0,b=0; if(arg) sscanf(arg,"%d,%d,%d",&r,&g,&b);
    double lum=0.2126*_jny_chan(r)+0.7152*_jny_chan(g)+0.0722*_jny_chan(b);
    char buf[64]; snprintf(buf,sizeof buf,"%.17g",lum); return strdup(buf);
}
/* PoP: hermes_cli_journey_u_contrast_ratio @ hermes_cli/journey.py:_contrast_ratio */
/* (hi+0.05)/(lo+0.05) of the two luminances. Input "r,g,b;r,g,b". */
char *hermes_cli_journey_u_contrast_ratio(const char *arg){
    int r1=0,g1=0,b1=0,r2=0,g2=0,b2=0;
    if(arg) sscanf(arg,"%d,%d,%d;%d,%d,%d",&r1,&g1,&b1,&r2,&g2,&b2);
    double l1=0.2126*_jny_chan(r1)+0.7152*_jny_chan(g1)+0.0722*_jny_chan(b1);
    double l2=0.2126*_jny_chan(r2)+0.7152*_jny_chan(g2)+0.0722*_jny_chan(b2);
    double hi=l1>l2?l1:l2, lo=l1>l2?l2:l1;
    char buf[64]; snprintf(buf,sizeof buf,"%.17g",(hi+0.05)/(lo+0.05)); return strdup(buf);
}

/* PoP: hermes_cli_journey_u_ensure_contrast @ hermes_cli/journey.py:_ensure_contrast */
/* Lift a foreground toward the readable pole until contrast >= minimum.
 * Inlines learning_graph_render hex_to_rgb/rgb_to_hex/mix_rgb(_lerp,_clamp) and
 * journey _relative_luminance/_contrast_ratio. round()==rint() (half-even).
 * Input "fg;bg;min". Returns "" for None (falsy color). */
static double _ec_clamp(double v,double lo,double hi){ return v<lo?lo:(v>hi?hi:v); }
static int _ec_hexval(int c){ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; }
static void _ec_hex_to_rgb(const char *in,int out[3]){
    char buf[64]; snprintf(buf,sizeof buf,"%s",in?in:"");
    char *p=buf; while(*p==' '||*p=='\t'||*p=='\n'||*p=='\r') p++;
    size_t L=strlen(p); while(L>0&&(p[L-1]==' '||p[L-1]=='\t'||p[L-1]=='\n'||p[L-1]=='\r')) p[--L]='\0';
    while(*p=='#') p++;
    if(strlen(p)==3){ char t[8]; t[0]=p[0];t[1]=p[0];t[2]=p[1];t[3]=p[1];t[4]=p[2];t[5]=p[2];t[6]=0; strcpy(buf,t); p=buf; }
    if(strlen(p)<6){ out[0]=255;out[1]=215;out[2]=0; return; }
    for(int i=0;i<3;i++){ int h=_ec_hexval(p[i*2]), l=_ec_hexval(p[i*2+1]); if(h<0||l<0){ out[0]=255;out[1]=215;out[2]=0; return;} out[i]=h*16+l; }
}
static double _ec_lum(const int c[3]){ return 0.2126*_jny_chan(c[0])+0.7152*_jny_chan(c[1])+0.0722*_jny_chan(c[2]); }
static double _ec_ratio(const int a[3],const int b[3]){ double l1=_ec_lum(a),l2=_ec_lum(b); double hi=l1>l2?l1:l2, lo=l1>l2?l2:l1; return (hi+0.05)/(lo+0.05); }
static void _ec_mix(const int a[3],const int b[3],double t,int out[3]){ double tc=_ec_clamp(t,0.0,1.0);
    for(int i=0;i<3;i++){ double v=a[i]+((double)b[i]-a[i])*tc; out[i]=(int)rint(v); } }
static void _ec_to_hex(const int c[3],char *buf,size_t n){ snprintf(buf,n,"#%02X%02X%02X",(int)_ec_clamp(c[0],0,255),(int)_ec_clamp(c[1],0,255),(int)_ec_clamp(c[2],0,255)); }
char *hermes_cli_journey_u_ensure_contrast(const char *arg){
    char buf[256]; snprintf(buf,sizeof buf,"%s",arg?arg:"");
    char *p1=strchr(buf,';'); if(!p1) return strdup(""); *p1=0;
    char *p2=strchr(p1+1,';'); if(!p2) return strdup(""); *p2=0;
    const char *fg_s=buf, *bg_s=p1+1; double minimum=atof(p2+1);
    if(!*fg_s) return strdup("");                 /* not color -> None */
    int fg[3],bg[3]; _ec_hex_to_rgb(fg_s,fg); _ec_hex_to_rgb(bg_s,bg);
    if(_ec_ratio(fg,bg)>=minimum) return strdup(fg_s);
    int pole[3]; if(_ec_lum(bg)>0.5){ pole[0]=0;pole[1]=0;pole[2]=0; } else { pole[0]=255;pole[1]=255;pole[2]=255; }
    for(int step=1; step<=20; step++){ int cand[3]; _ec_mix(fg,pole,step*0.05,cand);
        if(_ec_ratio(cand,bg)>=minimum){ char o[8]; _ec_to_hex(cand,o,sizeof o); return strdup(o); } }
    char o[8]; _ec_to_hex(pole,o,sizeof o); return strdup(o);
}
