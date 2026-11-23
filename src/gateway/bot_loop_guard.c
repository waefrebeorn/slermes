/*
 * bot_loop_guard.c — C home for upstream Hermes gateway/bot_loop_guard.py.
 *
 * PoP: botloop_as_bool @ gateway/bot_loop_guard.py:_as_bool
 * PoP: botloop_as_positive @ gateway/bot_loop_guard.py:_as_positive
 * PoP: botloop_as_positive_int @ gateway/bot_loop_guard.py:_as_positive_int
 * PoP: botloop_configured_bot_user @ gateway/bot_loop_guard.py:configured_bot_user
 * PoP: botloop_settings_from_config @ gateway/bot_loop_guard.py:settings_from_config
 * PoP: botloop_guard @ gateway/bot_loop_guard.py:BotLoopGuard.admit/chain_admit/note_reply/blocked
 */
#include "noc_botloop.h"

#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- minimal SHA-256 (for chain ask hashes) ---------- */
typedef struct { uint32_t h[8]; uint64_t bitlen; unsigned char buf[64]; size_t buflen; } bl_sha_ctx;
static const uint32_t BL_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
#define BL_ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
static void bl_sha_block(bl_sha_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    for (int i=0;i<16;++i) w[i]=((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|(uint32_t)p[i*4+3];
    for (int i=16;i<64;++i) {
        uint32_t s0=BL_ROR(w[i-15],7)^BL_ROR(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=BL_ROR(w[i-2],17)^BL_ROR(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],hh=c->h[7];
    for (int i=0;i<64;++i) {
        uint32_t S1=BL_ROR(e,6)^BL_ROR(e,11)^BL_ROR(e,25);
        uint32_t ch=(e&f)^((~e)&g);
        uint32_t t1=hh+S1+ch+BL_K[i]+w[i];
        uint32_t S0=BL_ROR(a,2)^BL_ROR(a,13)^BL_ROR(a,22);
        uint32_t maj=(a&b)^(a&cc)^(b&cc);
        uint32_t t2=S0+maj;
        hh=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;
    }
    c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=hh;
}
static void bl_sha_init(bl_sha_ctx *c){c->h[0]=0x6a09e667u;c->h[1]=0xbb67ae85u;c->h[2]=0x3c6ef372u;c->h[3]=0xa54ff53au;c->h[4]=0x510e527fu;c->h[5]=0x9b05688cu;c->h[6]=0x1f83d9abu;c->h[7]=0x5be0cd19u;c->bitlen=0;c->buflen=0;}
static void bl_sha_update(bl_sha_ctx *c,const unsigned char*d,size_t n){c->bitlen+=(uint64_t)n*8;while(n){size_t t=64-c->buflen;if(t>n)t=n;memcpy(c->buf+c->buflen,d,t);c->buflen+=t;d+=t;n-=t;if(c->buflen==64){bl_sha_block(c,c->buf);c->buflen=0;}}}
static void bl_sha_final(bl_sha_ctx *c,unsigned char out[32]){
    uint64_t bits=c->bitlen;unsigned char p=0x80;bl_sha_update(c,&p,1);unsigned char z=0;
    while(c->buflen!=56) bl_sha_update(c,&z,1);
    unsigned char len[8];for(int i=0;i<8;++i)len[i]=(unsigned char)(bits>>(56-8*i));
    bl_sha_update(c,len,8);
    for(int i=0;i<8;++i){out[i*4]=(unsigned char)(c->h[i]>>24);out[i*4+1]=(unsigned char)(c->h[i]>>16);out[i*4+2]=(unsigned char)(c->h[i]>>8);out[i*4+3]=(unsigned char)c->h[i];}
}
static void bl_sha_hex(const unsigned char *d,size_t n,char out[65]){
    bl_sha_ctx c;bl_sha_init(&c);bl_sha_update(&c,d,n);unsigned char dig[32];bl_sha_final(&c,dig);
    static const char hx[]="0123456789abcdef";
    for(int i=0;i<32;++i){out[i*2]=hx[dig[i]>>4];out[i*2+1]=hx[dig[i]&15];}out[64]=0;
}

/* Python str() coercion for JSON scalars (arrays/objects -> ""; not used). */
static void bl_py_str(const json_t *v, char *out, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(out,cap,""); return; }
    if (v->type == JSON_STRING) { snprintf(out,cap,"%s", v->str_val); return; }
    if (v->type == JSON_BOOL) { snprintf(out,cap,"%s", v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 9.0e18) snprintf(out,cap,"%lld",(long long)d);
        else snprintf(out,cap,"%g",d);
        return;
    }
    snprintf(out,cap,"");
}
/* Python str.strip().lower() -- strip only at the edges, preserve internal ws. */
static void bl_strip_lower(const char *s, char *out, size_t cap) {
    const char *a=s; while(*a && isspace((unsigned char)*a)) a++;
    const char *b=a+strlen(a); while(b>a && isspace((unsigned char)b[-1])) b--;
    size_t n=0; size_t lim=cap?cap-1:0;
    for(const char*p=a;p<b && n<lim;++p) out[n++]=(char)tolower((unsigned char)*p);
    out[n]=0;
}
static void bl_strip(const char *s, char *out, size_t cap) {
    const char *a=s; while(*a && isspace((unsigned char)*a)) a++;
    const char *b=a+strlen(a); while(b>a && isspace((unsigned char)b[-1])) b--;
    size_t n=0; size_t lim=cap?cap-1:0;
    for(const char*p=a;p<b && n<lim;++p) out[n++]=*p;
    out[n]=0;
}

bool botloop_as_bool(const struct json_t *raw, bool default_value) {
    if (raw && raw->type == JSON_BOOL) return raw->bool_val;
    char s[512], t[512];
    bl_py_str(raw, s, sizeof s);  /* None -> "" */
    bl_strip_lower(s, t, sizeof t);
    if (!strcmp(t,"true") || !strcmp(t,"1") || !strcmp(t,"yes") || !strcmp(t,"on")) return true;
    if (!strcmp(t,"false") || !strcmp(t,"0") || !strcmp(t,"no") || !strcmp(t,"off")) return false;
    return default_value;
}

double botloop_as_positive(const struct json_t *raw, double default_value) {
    if (!raw || raw->type == JSON_BOOL || raw->type == JSON_NULL ||
        raw->type == JSON_ARRAY || raw->type == JSON_OBJECT) return default_value;
    double v;
    if (raw->type == JSON_NUMBER) v = raw->num_val;
    else {
        char s[512]; bl_py_str(raw,s,sizeof s);
        char *end=NULL; v=strtod(s,&end);
        while(end && isspace((unsigned char)*end)) end++;
        if (!end || *end) return default_value;
    }
    return v > 0 ? v : default_value;
}

long long botloop_as_positive_int(const struct json_t *raw, long long default_value) {
    double v = botloop_as_positive(raw, 0.0);
    if (v >= 1 && v == floor(v)) return (long long)v;
    return default_value;
}

bool botloop_configured_bot_user(const struct json_t *extra, const struct json_t *user_id) {
    if (!user_id || user_id->type == JSON_NULL) return false;
    char us[512]; bl_py_str(user_id,us,sizeof us);
    if (!us[0]) return false;
    const json_t *cfg = (extra && extra->type == JSON_OBJECT) ? json_obj_get(extra,"bot_users") : NULL;
    json_t *owned = NULL;
    const json_t *list = NULL;
    if (cfg && cfg->type == JSON_STRING) {
        owned = json_array();
        const char *p = cfg->str_val;
        while (1) {
            const char *q = strchr(p, ',');
            size_t len = q ? (size_t)(q-p) : strlen(p);
            char *item = (char *)malloc(len+1);
            if (item) { memcpy(item,p,len); item[len]=0; json_append(owned,json_string(item)); free(item); }
            if (!q) break;
            p = q+1;
        }
        list = owned;
    } else if (cfg && cfg->type == JSON_ARRAY) list = cfg;
    bool found = false;
    if (list) {
        for (size_t i=0;i<json_len(list);++i) {
            char raw[512], t[512];
            bl_py_str(json_get(list,i),raw,sizeof raw);
            bl_strip(raw,t,sizeof t);
            if (!strcmp(t,us)) { found=true; break; }
        }
    }
    json_free(owned);
    return found;
}

struct json_t *botloop_settings_from_block(const struct json_t *block) {
    const json_t *b = (block && block->type == JSON_OBJECT) ? block : NULL;
    json_t *out = json_object();
    bool enabled = b ? botloop_as_bool(json_obj_get(b,"enabled"), true) : true;
    long long max_events = b ? botloop_as_positive_int(json_obj_get(b,"max_events"), 20) : 20;
    double window = b ? botloop_as_positive(json_obj_get(b,"window_seconds"), 300.0) : 300.0;
    double cooldown = b ? botloop_as_positive(json_obj_get(b,"cooldown_seconds"), 600.0) : 600.0;
    long long max_hops = b ? botloop_as_positive_int(json_obj_get(b,"max_hops"), 4) : 4;
    bool expect = b ? botloop_as_bool(json_obj_get(b,"bot_replies_expect_reply"), false) : false;
    json_set(out,"enabled",json_bool(enabled));
    json_set(out,"max_events",json_number((double)max_events));
    json_set(out,"window_seconds",json_number(window));
    json_set(out,"cooldown_seconds",json_number(cooldown));
    json_set(out,"max_hops",json_number((double)max_hops));
    json_set(out,"bot_replies_expect_reply",json_bool(expect));
    return out;
}

/* ---------- guard state machine ---------- */
#define BL_MAXC 32
#define BL_MAXE 256
#define BL_MAXA 256
typedef struct { double v[BL_MAXE]; size_t n; } bl_events;
typedef struct { double until; int active; } bl_cd;
typedef struct {
    int active; long long hops; char replied_to[256]; double updated;
    char asks[BL_MAXA][640]; size_t nasks;
} bl_chain;
static bl_events g_events[BL_MAXC]; static char *g_evkeys[BL_MAXC]; static size_t g_nev;
static bl_cd g_cd[BL_MAXC]; static char *g_cdkeys[BL_MAXC]; static size_t g_ncd;
static bl_chain g_ch[BL_MAXC]; static char *g_chkeys[BL_MAXC]; static size_t g_nch;
static double g_last_sweep;

static void bl_reset(void) {
    for (size_t i=0;i<g_nev;++i) free(g_evkeys[i]);
    for (size_t i=0;i<g_ncd;++i) free(g_cdkeys[i]);
    for (size_t i=0;i<g_nch;++i) free(g_chkeys[i]);
    g_nev=g_ncd=g_nch=0; g_last_sweep=0.0;
    memset(g_events,0,sizeof g_events); memset(g_cd,0,sizeof g_cd); memset(g_ch,0,sizeof g_ch);
    memset(g_evkeys,0,sizeof g_evkeys); memset(g_cdkeys,0,sizeof g_cdkeys); memset(g_chkeys,0,sizeof g_chkeys);
}
static size_t bl_key_idx(char **keys, size_t *n, const char *k) {
    for (size_t i=0;i<*n;++i) if (!strcmp(keys[i],k)) return i;
    if (*n < BL_MAXC) { keys[*n]=strdup(k); return (*n)++; }
    return (size_t)-1;
}
static void bl_events_idx(const char *conv) { bl_key_idx(g_evkeys,&g_nev,conv); }
static size_t bl_cd_idx(const char *conv) { return bl_key_idx(g_cdkeys,&g_ncd,conv); }
static size_t bl_ch_idx(const char *conv) {
    size_t i = bl_key_idx(g_chkeys,&g_nch,conv);
    if (i != (size_t)-1 && !g_ch[i].active) {
        memset(&g_ch[i],0,sizeof(bl_chain)); g_ch[i].active=1;
    }
    return i;
}
static void bl_sweep(double now, double window, double cooldown) {
    if (now - g_last_sweep < window) return;
    g_last_sweep = now;
    double idle = now - window;
    /* events: drop empty or idle */
    for (size_t i=0;i<g_nev;) {
        bl_events *e=&g_events[i];
        if (e->n==0 || e->v[e->n-1] <= idle) {
            free(g_evkeys[i]);
            for (size_t j=i+1;j<g_nev;++j){g_evkeys[j-1]=g_evkeys[j];g_events[j-1]=g_events[j];}
            g_nev--; memset(&g_events[g_nev],0,sizeof(bl_events)); g_evkeys[g_nev]=NULL;
        } else i++;
    }
    for (size_t i=0;i<g_ncd;) {
        if (g_cd[i].active && g_cd[i].until <= now) {
            free(g_cdkeys[i]);
            for(size_t j=i+1;j<g_ncd;++j){g_cdkeys[j-1]=g_cdkeys[j];g_cd[j-1]=g_cd[j];}
            g_ncd--; memset(&g_cd[g_ncd],0,sizeof(bl_cd)); g_cdkeys[g_ncd]=NULL;
        } else i++;
    }
    for (size_t i=0;i<g_nch;) {
        if (g_ch[i].active && now - g_ch[i].updated > cooldown) {
            free(g_chkeys[i]);
            for(size_t j=i+1;j<g_nch;++j){g_chkeys[j-1]=g_chkeys[j];g_ch[j-1]=g_ch[j];}
            g_nch--; memset(&g_ch[g_nch],0,sizeof(bl_chain)); g_chkeys[g_nch]=NULL;
        } else i++;
    }
}
static json_t *bl_pair(bool ok, const char *state) {
    json_t *a=json_array(); json_append(a,json_bool(ok)); json_append(a,json_string(state)); return a;
}
/* " ".join(str(text or "").split()) */
static void bl_norm_text(const char *text, char *out, size_t cap) {
    char src[2048]; snprintf(src,sizeof src,"%s",text?text:"");
    size_t n=0; size_t lim=cap?cap-1:0;
    const char *p=src;
    while(*p) {
        while(*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (n) { if(n<lim) out[n++]=' '; }
        while(*p && !isspace((unsigned char)*p)) { if(n<lim) out[n++]=*p; p++; }
    }
    out[n]=0;
}

struct json_t *botloop_guard(const struct json_t *settings, const struct json_t *ops) {
    bl_reset();
    json_t *results = json_array();
    int enabled = settings ? json_get_bool(settings,"enabled",true) : true;
    long long max_events = settings ? (long long)json_get_num(settings,"max_events",20.0) : 20;
    double window = settings ? json_get_num(settings,"window_seconds",300.0) : 300.0;
    double cooldown = settings ? json_get_num(settings,"cooldown_seconds",600.0) : 600.0;
    long long max_hops = settings ? (long long)json_get_num(settings,"max_hops",4.0) : 4;
    int expect = settings ? json_get_bool(settings,"bot_replies_expect_reply",false) : false;
    if (!ops || ops->type != JSON_ARRAY) return results;

    for (size_t oi=0; oi<json_len(ops); ++oi) {
        const json_t *op = json_get(ops,oi);
        const char *what = json_get_str(op,"op","");
        const char *conv = json_get_str(op,"conv","");
        if (!strcmp(what,"blocked")) {
            if (!enabled) { json_append(results,json_bool(false)); continue; }
            double now = json_get_num(op,"t",0.0);
            size_t ci=bl_cd_idx(conv);
            bool b = ci!=(size_t)-1 && g_cd[ci].active && g_cd[ci].until > now;
            json_append(results,json_bool(b)); continue;
        }
        if (!strcmp(what,"note")) {
            if (!enabled) { json_append(results,json_null()); continue; }
            size_t ci=bl_ch_idx(conv);
            if (ci!=(size_t)-1 && g_ch[ci].active) {
                const char *rec = json_get_str(op,"recipient","");
                snprintf(g_ch[ci].replied_to,sizeof g_ch[ci].replied_to,"%s",rec?rec:"");
            }
            json_append(results,json_null()); continue;
        }
        if (!strcmp(what,"chain")) {
            if (!enabled) { json_append(results,bl_pair(true,"disabled")); continue; }
            double now = json_get_num(op,"t",0.0);
            bl_sweep(now,window,cooldown);
            int is_bot = json_get_bool(op,"is_bot",false);
            if (!is_bot) {
                for (size_t i=0;i<g_nch;++i) if (!strcmp(g_chkeys[i],conv)) {
                    free(g_chkeys[i]);
                    for(size_t j=i+1;j<g_nch;++j){g_chkeys[j-1]=g_chkeys[j];g_ch[j-1]=g_ch[j];}
                    g_nch--; memset(&g_ch[g_nch],0,sizeof(bl_chain)); g_chkeys[g_nch]=NULL;
                    break;
                }
                json_append(results,bl_pair(true,"human")); continue;
            }
            size_t ci=bl_ch_idx(conv);
            if (ci==(size_t)-1) { json_append(results,bl_pair(true,"ok")); continue; }
            if (!g_ch[ci].active || now - g_ch[ci].updated > cooldown) {
                memset(&g_ch[ci],0,sizeof(bl_chain)); g_ch[ci].active=1;
            }
            g_ch[ci].updated = now;
            char norm[2048], hex[65];
            bl_norm_text(json_get_str(op,"text",""),norm,sizeof norm);
            bl_sha_hex((const unsigned char*)norm,strlen(norm),hex);
            char ask[1024];
            snprintf(ask,sizeof ask,"%s\x01%s",json_get_str(op,"sender",""),hex);
            int dup=0;
            for (size_t i=0;i<g_ch[ci].nasks;++i) if (!strcmp(g_ch[ci].asks[i],ask)) { dup=1; break; }
            if (dup) { json_append(results,bl_pair(false,"duplicate")); continue; }
            if (!expect && !strcmp(g_ch[ci].replied_to,json_get_str(op,"sender",""))) {
                json_append(results,bl_pair(false,"reply_to_reply")); continue;
            }
            if (g_ch[ci].hops >= max_hops) { json_append(results,bl_pair(false,"max_hops")); continue; }
            g_ch[ci].hops++;
            if (g_ch[ci].nasks < BL_MAXA) { snprintf(g_ch[ci].asks[g_ch[ci].nasks],640,"%s",ask); g_ch[ci].nasks++; }
            json_append(results,bl_pair(true,"ok")); continue;
        }
        if (!strcmp(what,"admit")) {
            if (!enabled) { json_append(results,bl_pair(true,"disabled")); continue; }
            double now = json_get_num(op,"t",0.0);
            bl_sweep(now,window,cooldown);
            size_t cdi=bl_cd_idx(conv);
            if (cdi!=(size_t)-1 && g_cd[cdi].active && g_cd[cdi].until > now) {
                json_append(results,bl_pair(false,"cooldown")); continue;
            }
            size_t ei=bl_key_idx(g_evkeys,&g_nev,conv);
            if (ei==(size_t)-1) { json_append(results,bl_pair(false,"cooldown")); continue; }
            bl_events *e=&g_events[ei];
            double cutoff = now - window;
            size_t keep=0;
            while (keep<e->n && e->v[keep] <= cutoff) keep++;
            if (keep) { memmove(e->v,e->v+keep,(e->n-keep)*sizeof(double)); e->n-=keep; }
            if ((long long)e->n >= max_events) {
                if (cdi==(size_t)-1) cdi=bl_cd_idx(conv);
                if (cdi!=(size_t)-1) { g_cd[cdi].until=now+cooldown; g_cd[cdi].active=1; }
                e->n=0;
                json_append(results,bl_pair(false,"tripped")); continue;
            }
            if (e->n < BL_MAXE) e->v[e->n++]=now;
            json_append(results,bl_pair(true,"ok")); continue;
        }
        json_append(results,json_null());
    }
    return results;
}
