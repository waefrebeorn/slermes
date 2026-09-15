/* gate: hermes_cli_doctor_render_state_db_stats — traced copy of
 * src/hermes_cli/doctor_helpers.c::doctor_render_state_db_stats.
 * LIVE twin: hermes_cli/doctor.py::_render_state_db_stats.
 *
 * Shared branch ladder (twin mirrors EXACTLY):
 *   B0 stats not an object -> []
 *   size bits: B1 logical_size_bytes / B2 page_count /
 *              B3 freelist_count / B4 wal_size_bytes -> info line
 *   row bits:  B5 messages / B6 sessions / B7 journal_mode /
 *              B8 holders > 0                             -> info line
 *   (the C port ends here; LIVE's FTS + oversize-warn rungs are excluded by
 *    input choice — see notes)
 * V-event: comma-joined emitted line kinds ("info", ...).
 *
 * Input encoding: {"stats": {...}, "holders": <int>} as one argv JSON.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* ---- tiny JSON parser (objects/strings/numbers/null, nested ok) ---- */
typedef struct JV JV;
struct JV { int type; double num; char *str; char **keys; JV **vals; int n; };
static void skipws(const char **p){while(**p==' '||**p=='\t'||**p=='\n'||**p=='\r')(*p)++;}
static char *parse_str(const char **p){
    skipws(p); if(**p!='"')return NULL; (*p)++;
    const char*s=*p; while(**p&&**p!='"')(*p)++;
    size_t n=(size_t)(*p-s); if(**p)(*p)++;
    char*o=malloc(n+1); memcpy(o,s,n); o[n]='\0'; return o;
}
static JV *parse_val(const char **p){
    skipws(p);
    if(**p=='{'){
        JV*o=calloc(1,sizeof*o); o->type=2;
        o->keys=malloc(sizeof(char*)*32); o->vals=malloc(sizeof(JV*)*32);
        skipws(p); (*p)++;
        while(1){ skipws(p); if(**p=='}'){(*p)++;break;}
            char*k=parse_str(p); skipws(p); (*p)++;
            JV*v=parse_val(p);
            o->keys[o->n]=k;o->vals[o->n]=v;o->n++;
            skipws(p); if(**p==','){(*p)++;continue;} }
        return o;
    }
    if(**p=='"') { JV*o=calloc(1,sizeof*o); o->type=1; o->str=parse_str(p); return o; }
    if(**p=='n'){*p+=4; JV*o=calloc(1,sizeof*o); o->type=3; return o;}
    JV*o=calloc(1,sizeof*o); o->type=0; char*e; o->num=strtod(*p,&e); *p=e; return o;
}
static JV *obj_get(JV *o, const char *k){
    if(!o||o->type!=2)return NULL;
    for(int i=0;i<o->n;i++) if(strcmp(o->keys[i],k)==0) return o->vals[i];
    return NULL;
}
static char kinds[64]; static int kn;

static void note(const char *k){
    if(kn) strcat(kinds, ",");
    strcat(kinds, k);
    kn++;
}

static long getnum(JV *o, const char *k, int *ok){
    JV *v = obj_get(o,k);
    *ok = v && v->type==0;
    return *ok ? (long)v->num : 0;
}

static void wc_render(JV *root){
    WCUP_BEGIN("render_state_db_stats");
    kn = 0; kinds[0] = '\0';
    JV *stats = root ? obj_get(root, "stats") : NULL;
    JV *hj = root ? obj_get(root, "holders") : NULL;
    long holders = (hj && hj->type == 0) ? (long)hj->num : -1;
    if (!stats || stats->type != 2) { WCUP_BRANCH(1); goto out; }   /* B0 */
    WCUP_BRANCH(0);
    {
        int any = 0;
        JV *v;
        v=obj_get(stats,"logical_size_bytes");
        if (v && v->type==0) { WCUP_BRANCH(1); any=1; } else WCUP_BRANCH(0);   /* B1 */
        v=obj_get(stats,"page_count");
        if (v && v->type==0) { WCUP_BRANCH(1); any=1; } else WCUP_BRANCH(0);   /* B2 */
        v=obj_get(stats,"freelist_count");
        if (v && v->type==0) { WCUP_BRANCH(1); any=1; } else WCUP_BRANCH(0);   /* B3 */
        v=obj_get(stats,"wal_size_bytes");
        if (v && v->type==0) { WCUP_BRANCH(1); any=1; } else WCUP_BRANCH(0);   /* B4 */
        if (any) note("info");
    }
    {
        JV *v; int any=0;
        v=obj_get(stats,"messages");
        if (v && v->type==0) { WCUP_BRANCH(1); any=1; } else WCUP_BRANCH(0);   /* B5 */
        v=obj_get(stats,"sessions");
        if (v && v->type==0) { WCUP_BRANCH(1); any=1; } else WCUP_BRANCH(0);   /* B6 */
        v=obj_get(stats,"journal_mode");
        if (v && v->type==1 && v->str && v->str[0]) { WCUP_BRANCH(1); any=1; } else WCUP_BRANCH(0); /* B7 */
        if (holders > 0)     { WCUP_BRANCH(1); any=1; } else WCUP_BRANCH(0);   /* B8 */
        if (any) note("info");
    }
out:;
    WCUP_VALSTR(kinds);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("render_state_db_stats");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s render_state_db_stats <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *p = argv[i];
        JV *v = parse_val(&p);
        wc_render(v);
    }
    return 0;
}
