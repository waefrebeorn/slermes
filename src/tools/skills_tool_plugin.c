/*
 * skills_tool_plugin.c — C home for upstream Hermes tools/skills_tool_plugin.py.
 * PoP: w11_skillstool_c957bf_json @ tools/skills_tool_plugin.py:_json
 * PoP: w11_skillstool_c957bf_fail @ tools/skills_tool_plugin.py:_fail
 * PoP: w11_skillstool_c957bf_truncate_description @ tools/skills_tool_plugin.py:_truncate_description
 *
 * The filesystem-serving half stays in Python. These three are the pure
 * response-serialization and description-limiting helpers.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_skillstool_c957bf.h"

#define MAX_DESCRIPTION_LENGTH 1024

/* ---------- growable string ---------- */
typedef struct { char *p; size_t n, cap; } SB;
static void sb_init(SB *b){ b->cap=256; b->p=(char*)malloc(b->cap); b->n=0; b->p[0]='\0'; }
static void sb_add(SB *b,const char *s,size_t n){
    if(b->n+n+1>b->cap){
        while(b->n+n+1>b->cap) b->cap*=2;
        b->p=(char*)realloc(b->p,b->cap);
    }
    memcpy(b->p+b->n,s,n); b->n+=n; b->p[b->n]='\0';
}
static void sb_puts(SB *b,const char *s){ sb_add(b,s,strlen(s)); }

/* json.dumps(..., ensure_ascii=False): separators ", " / ": ". */
static void dumps_into(SB *b,json_t *v){
    if(!v){ sb_puts(b,"null"); return; }
    switch(v->type){
    case JSON_NULL: sb_puts(b,"null"); break;
    case JSON_BOOL: sb_puts(b,v->bool_val?"true":"false"); break;
    case JSON_NUMBER:
        if(v->num_val==(double)(long long)v->num_val){
            char t[64]; int n=snprintf(t,sizeof(t),"%lld",(long long)v->num_val);
            sb_add(b,t,(size_t)n);
        } else {
            char t[64]; int n=snprintf(t,sizeof(t),"%g",v->num_val);
            sb_add(b,t,(size_t)n);
        }
        break;
    case JSON_STRING: {
        sb_add(b,"\"",1);
        const char *s=v->str_val?v->str_val:"";
        for(const unsigned char *p=(const unsigned char*)s;*p;p++){
            unsigned char c=*p;
            if(c=='"'||c=='\\'){ char t[2]={'\\',(char)c}; sb_add(b,t,2); }
            else if(c=='\b') sb_puts(b,"\\b");
            else if(c=='\f') sb_puts(b,"\\f");
            else if(c=='\n') sb_puts(b,"\\n");
            else if(c=='\r') sb_puts(b,"\\r");
            else if(c=='\t') sb_puts(b,"\\t");
            else if(c<0x20){ char t[7]; snprintf(t,sizeof(t),"\\u%04x",c); sb_add(b,t,6); }
            else sb_add(b,(const char*)p,1);
        }
        sb_add(b,"\"",1);
        break;
    }
    case JSON_ARRAY:
        sb_add(b,"[",1);
        for(size_t i=0;i<json_array_size(v);i++){
            if(i) sb_puts(b,", ");
            dumps_into(b,json_get(v,i));
        }
        sb_add(b,"]",1);
        break;
    case JSON_OBJECT:
        sb_add(b,"{",1);
        for(size_t i=0;i<json_object_size(v);i++){
            if(i) sb_puts(b,", ");
            const char *k=json_object_get_key_at(v,i);
            SB kb; sb_init(&kb);
            /* escape the key with the same string rules */
            sb_add(&kb,"\"",1);
            const unsigned char *p=(const unsigned char*)(k?k:"");
            for(;*p;p++){
                unsigned char c=*p;
                if(c=='"'||c=='\\'){ char t[2]={'\\',(char)c}; sb_add(&kb,t,2); }
                else if(c=='\b') sb_puts(&kb,"\\b");
                else if(c=='\f') sb_puts(&kb,"\\f");
                else if(c=='\n') sb_puts(&kb,"\\n");
                else if(c=='\r') sb_puts(&kb,"\\r");
                else if(c=='\t') sb_puts(&kb,"\\t");
                else if(c<0x20){ char t[7]; snprintf(t,sizeof(t),"\\u%04x",c); sb_add(&kb,t,6); }
                else sb_add(&kb,(const char*)p,1);
            }
            sb_add(&kb,"\"",1);
            sb_add(b,kb.p,kb.n); free(kb.p);
            sb_puts(b,": ");
            dumps_into(b,json_object_get_at(v,i));
        }
        sb_add(b,"}",1);
        break;
    }
}
char *w11_skillstool_c957bf_json(json_t *payload){
    SB b; sb_init(&b);
    dumps_into(&b,payload);
    return b.p;
}
char *w11_skillstool_c957bf_fail(const char *error,json_t *extra){
    SB b; sb_init(&b);
    sb_puts(&b,"{\"success\": false, \"error\": ");
    json_t *estr=json_string(error?error:"");
    dumps_into(&b,estr); json_free(estr);
    if(extra && extra->type==JSON_OBJECT){
        for(size_t i=0;i<json_object_size(extra);i++){
            const char *k=json_object_get_key_at(extra,i);
            sb_puts(&b,", ");
            json_t *kv=json_string(k?k:"");
            dumps_into(&b,kv); json_free(kv);
            sb_puts(&b,": ");
            dumps_into(&b,json_object_get_at(extra,i));
        }
    }
    sb_puts(&b,"}");
    return b.p;
}
/* Python len() counts Unicode code points; slice at a code-point boundary. */
static size_t byte_at_codepoint(const char *s,size_t want){
    size_t cp=0;
    for(size_t i=0;s[i];){
        unsigned char c=(unsigned char)s[i];
        size_t step = c<0x80 ? 1 :
                      ((c&0xE0)==0xC0 ? 2 : ((c&0xF0)==0xE0 ? 3 : ((c&0xF8)==0xF0 ? 4 : 1)));
        cp++;
        if(cp==want) return i+step;  /* byte offset AFTER the wanted codepoint */
        i+=step;
    }
    return strlen(s);
}
char *w11_skillstool_c957bf_truncate_description(const char *description){
    const char *s=description?description:"";
    size_t n=strlen(s), cp=0;
    for(size_t i=0;i<n;i++) if((unsigned char)s[i]<0x80 || ((unsigned char)s[i]&0xC0)!=0x80) cp++;
    if(cp<=MAX_DESCRIPTION_LENGTH){ char *o=(char*)malloc(n+1); strcpy(o,s); return o; }
    size_t keep=byte_at_codepoint(s,MAX_DESCRIPTION_LENGTH-3);
    char *o=(char*)malloc(keep+4);
    memcpy(o,s,keep); memcpy(o+keep,"...",3); o[keep+3]='\0';
    return o;
}
