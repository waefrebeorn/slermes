#include <stdarg.h>
/*
 * PoP: w9_sessionlist_96be7b_check_page_params @ gateway/session_listing.py:check_page_params
 * PoP: w9_sessionlist_96be7b_binding_digest    @ gateway/session_listing.py:_binding_digest
 * PoP: w9_sessionlist_96be7b_encode_cursor     @ gateway/session_listing.py:_encode_cursor
 * PoP: w9_sessionlist_96be7b_decode_cursor     @ gateway/session_listing.py:_decode_cursor
 *
 * Faithful C11 port of the pure cursor/page-param layer of gateway/session_listing.py.
 * Python json.dumps defaults (ensure_ascii=True, ", " separators), SHA-256 and
 * urlsafe-base64 are reproduced locally so the module is self-contained.
 */
#include "hermes_json.h"
#include "noc_w9_sessionlist_96be7b.h"
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CURSOR_TAG "l1"
#define CURSOR_MAX_CHARS 1024
#define QUERY_MAX_CHARS 200

/* ---------- small dynamic string builder ---------- */
typedef struct { char *p; size_t n, cap; } sbuf;
static void sb_init(sbuf *b){ b->cap=64; b->n=0; b->p=(char*)malloc(b->cap); if(b->p)b->p[0]=0; }
static void sb_putc(sbuf *b, char c){
    if(!b->p) return;
    if(b->n+2>b->cap){ b->cap*=2; char *q=(char*)realloc(b->p,b->cap); if(!q)return; b->p=q; }
    b->p[b->n++]=c; b->p[b->n]=0;
}
static void sb_puts(sbuf *b, const char *s){ while(s&&*s) sb_putc(b,*s++); }
static void sb_printf(sbuf *b, const char *fmt, ...){
    char tmp[64]; va_list ap; va_start(ap,fmt);
    int k=vsnprintf(tmp,sizeof(tmp),fmt,ap); va_end(ap);
    if(k<0) return;
    if((size_t)k>=sizeof(tmp)){ /* fall back for long numbers: not used here */ return; }
    sb_puts(b,tmp);
}
static void sb_free(sbuf *b){ free(b->p); b->p=NULL; b->n=b->cap=0; }

/* ---------- UTF-8 decode ---------- */
/* returns bytes consumed, cp = codepoint; invalid byte -> cp=byte, consumed=1 */
static size_t u8_next(const char *s, size_t len, size_t i, unsigned *cp){
    unsigned char c=(unsigned char)s[i];
    if(c<0x80){ *cp=c; return 1; }
    size_t need=0; unsigned v=0;
    if((c&0xE0)==0xC0){ need=2; v=c&0x1F; }
    else if((c&0xF0)==0xE0){ need=3; v=c&0x0F; }
    else if((c&0xF8)==0xF0){ need=4; v=c&0x07; }
    else { *cp=c; return 1; }
    if(i+need>len){ *cp=c; return 1; }
    for(size_t j=1;j<need;j++){
        unsigned char d=(unsigned char)s[i+j];
        if((d&0xC0)!=0x80){ *cp=c; return 1; }
        v=(v<<6)|(d&0x3F);
    }
    *cp=v; return need;
}
static size_t utf8_count(const char *s){
    size_t n=0,i=strlen(s);
    for(size_t k=0;k<i;){ unsigned cp; k+=u8_next(s,i,k,&cp); n++; }
    return n;
}

/* ---------- Python json.dumps string body (ensure_ascii=True) ---------- */
static void py_json_string_body(sbuf *b, const char *s){
    sb_putc(b,'"');
    size_t len=strlen(s);
    for(size_t i=0;i<len;){
        unsigned cp; i+=u8_next(s,len,i,&cp);
        switch(cp){
            case '"':  sb_puts(b,"\\\""); break;
            case '\\': sb_puts(b,"\\\\"); break;
            case '\b': sb_puts(b,"\\b"); break;
            case '\f': sb_puts(b,"\\f"); break;
            case '\n': sb_puts(b,"\\n"); break;
            case '\r': sb_puts(b,"\\r"); break;
            case '\t': sb_puts(b,"\\t"); break;
            default:
                if(cp>=0x20 && cp<0x7f) sb_putc(b,(char)cp);
                else if(cp<0x20) sb_printf(b,"\\u%04x",cp);
                else if(cp<=0xffff) sb_printf(b,"\\u%04x",cp);
                else { unsigned x=cp-0x10000; sb_printf(b,"\\u%04x\\u%04x",0xd800+(x>>10),0xdc00+(x&0x3ff)); }
        }
    }
    sb_putc(b,'"');
}

/* binding -> "[workspace, query, include_hidden]" exactly as json.dumps(list(binding)) */
static char *binding_json(const char *workspace, const char *query, int include_hidden){
    sbuf b; sb_init(&b);
    sb_putc(&b,'[');
    if(workspace) py_json_string_body(&b,workspace); else sb_puts(&b,"null");
    sb_puts(&b,", ");
    if(query) py_json_string_body(&b,query); else sb_puts(&b,"null");
    sb_puts(&b,", ");
    sb_puts(&b,include_hidden?"true":"false");
    sb_putc(&b,']');
    return b.p;
}

/* ---------- SHA-256 (compact, self-contained) ---------- */
static const uint32_t K256[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static uint32_t rr(uint32_t x,int n){ return (x>>n)|(x<<(32-n)); }
static void sha256(const unsigned char *msg, size_t len, unsigned char out[32]){
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    size_t total=((len+9+63)/64)*64;
    unsigned char *m=(unsigned char*)calloc(total,1);
    if(!m){ memset(out,0,32); return; }
    memcpy(m,msg,len); m[len]=0x80;
    uint64_t bits=(uint64_t)len*8;
    for(int i=0;i<8;i++) m[total-1-i]=(unsigned char)(bits>>(8*i));
    for(size_t off=0; off<total; off+=64){
        uint32_t w[64];
        for(int i=0;i<16;i++) w[i]=((uint32_t)m[off+4*i]<<24)|((uint32_t)m[off+4*i+1]<<16)|((uint32_t)m[off+4*i+2]<<8)|m[off+4*i+3];
        for(int i=16;i<64;i++){
            uint32_t s0=rr(w[i-15],7)^rr(w[i-15],18)^(w[i-15]>>3);
            uint32_t s1=rr(w[i-2],17)^rr(w[i-2],19)^(w[i-2]>>10);
            w[i]=w[i-16]+s0+w[i-7]+s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for(int i=0;i<64;i++){
            uint32_t S1=rr(e,6)^rr(e,11)^rr(e,25);
            uint32_t ch=(e&f)^((~e)&g);
            uint32_t t1=hh+S1+ch+K256[i]+w[i];
            uint32_t S0=rr(a,2)^rr(a,13)^rr(a,22);
            uint32_t maj=(a&b)^(a&c)^(b&c);
            uint32_t t2=S0+maj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
    }
    for(int i=0;i<8;i++) for(int j=0;j<4;j++) out[4*i+j]=(unsigned char)(h[i]>>(24-8*j));
    free(m);
}
static char *hex32(const unsigned char d[32]){
    char *s=(char*)malloc(33); if(!s)return NULL;
    static const char *H="0123456789abcdef";
    for(int i=0;i<16;i++){ s[2*i]=H[d[i]>>4]; s[2*i+1]=H[d[i]&15]; }
    s[32]=0; return s;
}

char *w9_sessionlist_96be7b_binding_digest(const char *workspace, const char *query, int include_hidden){
    char *js=binding_json(workspace,query,include_hidden);
    if(!js) return NULL;
    unsigned char d[32]; sha256((const unsigned char*)js,strlen(js),d);
    free(js);
    return hex32(d);
}

/* ---------- urlsafe base64, no padding ---------- */
static const char *B64="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
static char *b64url_encode(const unsigned char *data, size_t len){
    size_t olen=((len+2)/3)*4;
    char *out=(char*)malloc(olen+1); if(!out)return NULL;
    size_t k=0;
    for(size_t i=0;i<len;i+=3){
        uint32_t v=(uint32_t)data[i]<<16;
        if(i+1<len) v|=(uint32_t)data[i+1]<<8;
        if(i+2<len) v|=data[i+2];
        out[k++]=B64[(v>>18)&63]; out[k++]=B64[(v>>12)&63];
        if(i+1<len) out[k++]=B64[(v>>6)&63];
        if(i+2<len) out[k++]=B64[v&63];
    }
    out[k]=0; return out;
}
static int b64val(char c){
    if(c>='A'&&c<='Z') return c-'A';
    if(c>='a'&&c<='z') return c-'a'+26;
    if(c>='0'&&c<='9') return c-'0'+52;
    if(c=='-') return 62; if(c=='_') return 63;
    return -1;
}
static unsigned char *b64url_decode(const char *s, size_t *out_len){
    size_t len=strlen(s);
    unsigned char *out=(unsigned char*)malloc(len*3/4+4); if(!out)return NULL;
    size_t k=0; uint32_t buf=0; int bits=0;
    for(size_t i=0;i<len;i++){
        if(s[i]=='=') break;
        int v=b64val(s[i]); if(v<0){ free(out); return NULL; }
        buf=(buf<<6)|(unsigned)v; bits+=6;
        if(bits>=8){ bits-=8; out[k++]=(unsigned char)((buf>>bits)&0xff); }
    }
    *out_len=k; return out;
}

char *w9_sessionlist_96be7b_encode_cursor(long long offset, const char *workspace, const char *query, int include_hidden){
    char *digest=w9_sessionlist_96be7b_binding_digest(workspace,query,include_hidden);
    if(!digest) return NULL;
    sbuf b; sb_init(&b);
    sb_puts(&b,"["); py_json_string_body(&b,CURSOR_TAG);
    sb_printf(&b,", %lld, ",offset);
    py_json_string_body(&b,digest); sb_puts(&b,"]");
    free(digest);
    char *res=b64url_encode((const unsigned char*)b.p,strlen(b.p));
    sb_free(&b);
    return res;
}

static json_t *refusal_cursor(void){
    json_t *o=json_object();
    json_set(o,"error",json_string("invalid_params"));
    json_set(o,"field",json_string("cursor"));
    return o;
}

json_t *w9_sessionlist_96be7b_decode_cursor(const char *cursor, const char *workspace, const char *query, int include_hidden){
    if(!cursor || strlen(cursor)>CURSOR_MAX_CHARS) return refusal_cursor();
    size_t dlen=0;
    unsigned char *data=b64url_decode(cursor,&dlen);
    if(!data) return refusal_cursor();
    char *text=(char*)malloc(dlen+1); if(!text){ free(data); return refusal_cursor(); }
    memcpy(text,data,dlen); text[dlen]=0; free(data);
    json_t *arr=json_parse(text,NULL); free(text);
    if(!arr || arr->type!=JSON_ARRAY || arr->c.count!=3){ if(arr)json_free(arr); return refusal_cursor(); }
    json_t *tag=json_get(arr,0), *off=json_get(arr,1), *held=json_get(arr,2);
    if(!tag || tag->type!=JSON_STRING || strcmp(tag->str_val,CURSOR_TAG)){ json_free(arr); return refusal_cursor(); }
    if(!off || off->type!=JSON_NUMBER){ json_free(arr); return refusal_cursor(); }
    double dv=off->num_val;
    if(dv<0 || dv>=9223372036854775808.0 || dv!=(double)(long long)dv){ json_free(arr); return refusal_cursor(); }
    if(!held || held->type!=JSON_STRING){ json_free(arr); return refusal_cursor(); }
    char *digest=w9_sessionlist_96be7b_binding_digest(workspace,query,include_hidden);
    int ok=digest && !strcmp(held->str_val,digest);
    free(digest); json_free(arr);
    if(!ok) return refusal_cursor();
    return json_number(dv);
}

/* ---------- check_page_params ---------- */
static int truthy(const json_t *v){
    if(!v || v->type==JSON_NULL) return 0;
    if(v->type==JSON_BOOL) return v->bool_val?1:0;
    if(v->type==JSON_NUMBER) return v->num_val!=0.0;
    if(v->type==JSON_STRING) return v->str_val && v->str_val[0];
    if(v->type==JSON_ARRAY) return v->c.count>0;
    if(v->type==JSON_OBJECT) return v->c.count>0;
    return 0;
}
static json_t *refusal_params(const char *field){
    json_t *o=json_object();
    json_set(o,"error",json_string("invalid_params"));
    json_set(o,"field",json_string(field));
    return o;
}
static int is_blank_ws(const char *s){
    if(!s) return 1;
    for(const char *p=s;*p;p++) if(!isspace((unsigned char)*p)) return 0;
    return 1;
}
json_t *w9_sessionlist_96be7b_check_page_params(json_t *params){
    json_t *query=params?json_obj_get(params,"query"):NULL;
    json_t *cursor=params?json_obj_get(params,"cursor"):NULL;
    int qnull=!query||query->type==JSON_NULL;
    int cnull=!cursor||cursor->type==JSON_NULL;
    if(qnull && cnull){ json_t *o=json_object(); json_set(o,"ok",json_bool(1)); return o; }
    json_t *scope=params?json_obj_get(params,"scope"):NULL;
    int scope_ok=scope&&scope->type==JSON_STRING&&!strcmp(scope->str_val,"stored");
    int has_title=params&&json_obj_get(params,"title")!=NULL;
    if(!scope_ok || has_title) return refusal_params(qnull?"cursor":"query");
    if(!qnull){
        if(query->type!=JSON_STRING || is_blank_ws(query->str_val) || utf8_count(query->str_val)>QUERY_MAX_CHARS)
            return refusal_params("query");
    }
    if(!cnull){
        if(cursor->type!=JSON_STRING) return refusal_params("cursor");
    }
    json_t *o=json_object(); json_set(o,"ok",json_bool(1)); return o;
}
