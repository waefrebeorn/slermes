/*
 * skills_hub_official.c — C home for upstream Hermes tools/skills_hub_official.py.
 * PoP: w12_skillshub_b005d3_strip_prefix @ tools/skills_hub_official.py:_strip_prefix
 * PoP: w12_skillshub_b005d3_clean_rel_parts @ tools/skills_hub_official.py:_clean_rel_parts
 * PoP: w12_skillshub_b005d3_entry_provider @ tools/skills_hub_official.py:_entry_provider
 * PoP: w12_skillshub_b005d3_optional_rel @ tools/skills_hub_official.py:OptionalSkillSource._rel
 * PoP: w12_skillshub_b005d3_upstream_pointer @ tools/skills_hub_official.py:OptionalSkillSource._upstream_pointer_from_frontmatter
 * PoP: w12_skillshub_b005d3_to_meta @ tools/skills_hub_official.py:HermesIndexSource._to_meta
 *
 * Pure identifier/path/frontmatter helpers from the two official sources.
 * GitHub/network fetches and filesystem scans stay in Python.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_skillshub_b005d3.h"

static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
static int ws(unsigned char c){ return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f'; }
static int truthy(json_t *v){
    if(!v||v->type==JSON_NULL) return 0;
    if(v->type==JSON_BOOL) return v->bool_val?1:0;
    if(v->type==JSON_NUMBER) return v->num_val!=0.0;
    if(v->type==JSON_STRING) return v->str_val&&v->str_val[0];
    return v->c.count>0;
}
static char *fmt(const char *f, ...) {
    va_list ap; va_start(ap,f); va_list ap2; va_copy(ap2,ap);
    int n=vsnprintf(NULL,0,f,ap); va_end(ap);
    char *o=(char*)malloc((size_t)(n>0?n:0)+1);
    vsnprintf(o,(size_t)n+1,f,ap2); va_end(ap2); return o;
}
static char *py_str_alloc(json_t *v){
    if(!v||v->type==JSON_NULL) return (char*)strdup("None");
    if(v->type==JSON_STRING) return (char*)strdup(v->str_val?v->str_val:"");
    if(v->type==JSON_BOOL) return (char*)strdup(v->bool_val?"True":"False");
    if(v->type==JSON_NUMBER){ double d=v->num_val;
        if(d==(double)(long long)d) return fmt("%lld",(long long)d); return fmt("%g",d); }
    char *s=json_dumps(v,0); return s?s:(char*)strdup("");
}
static char *strip_dup(const char *s){
    s=s?s:""; size_t n=strlen(s);
    while(n&&ws((unsigned char)*s)){s++;n--;}
    while(n&&ws((unsigned char)s[n-1]))n--;
    char *o=(char*)malloc(n+1); memcpy(o,s,n); o[n]=0; return o;
}
static char *strip_slashes_dup(const char *s){
    s=s?s:""; size_t n=strlen(s);
    while(n&&s[0]=='/'){s++;n--;}
    while(n&&s[n-1]=='/') n--;
    char *o=(char*)malloc(n+1); memcpy(o,s,n); o[n]=0; return o;
}
static char *lower_dup(const char *s){
    size_t n=strlen(s); char *o=(char*)malloc(n+1);
    for(size_t i=0;i<n;i++) o[i]=(char)tolower((unsigned char)s[i]);
    o[n]=0; return o;
}
static int startswith(const char *s,const char *p){ return strncmp(s,p,strlen(p))==0; }
static int count_char(const char *s,char ch){ int n=0; for(;*s;s++) if(*s==ch)n++; return n; }

/* str.join over an array of strings */
static char *join_array(json_t *parts,const char *sep){
    size_t cap=1, slen=strlen(sep);
    for(size_t i=0;i<json_array_size(parts);i++){ json_t *p=json_get(parts,i);
        cap+=strlen(jstr(p)?jstr(p):"")+slen; }
    char *o=(char*)malloc(cap); o[0]=0; size_t k=0;
    for(size_t i=0;i<json_array_size(parts);i++){
        if(i){ memcpy(o+k,sep,slen); k+=slen; }
        const char *p=jstr(json_get(parts,i))?jstr(json_get(parts,i)):"";
        size_t n=strlen(p); memcpy(o+k,p,n); k+=n;
    }
    o[k]=0; return o;
}

json_t *w12_skillshub_b005d3_strip_prefix(json_t *value, json_t *prefixes){
    const char *v=jstr(value)?jstr(value):"";
    if(prefixes&&prefixes->type==JSON_ARRAY)
        for(size_t i=0;i<json_array_size(prefixes);i++){
            const char *p=jstr(json_get(prefixes,i));
            if(p&&startswith(v,p)) return json_string(v+strlen(p));
        }
    return json_string(v);
}

json_t *w12_skillshub_b005d3_clean_rel_parts(json_t *path){
    const char *s=jstr(path)?jstr(path):"";
    json_t *res=json_array();
    char *work=(char*)malloc(strlen(s)+1); strcpy(work,s);
    char *tok=strtok(work,"/");
    while(tok){
        if(strcmp(tok,"")&&strcmp(tok,".")){
            if(!strcmp(tok,"..")){ free(work); json_free(res); return json_null(); }
            json_append(res,json_string(tok));
        }
        tok=strtok(NULL,"/");
    }
    free(work);
    if(json_array_size(res)==0){ json_free(res); return json_null(); }
    return res;
}

json_t *w12_skillshub_b005d3_entry_provider(json_t *entry){
    json_t *extra=(entry&&entry->type==JSON_OBJECT)?json_obj_get(entry,"extra"):NULL;
    json_t *prov=NULL;
    if(truthy(extra)&&extra->type==JSON_OBJECT) prov=json_obj_get(extra,"provider");
    char *raw=prov?py_str_alloc(prov):(char*)strdup("");
    char *low=lower_dup(raw);
    free(raw);
    json_t *out=json_string(low); free(low); return out;
}

json_t *w12_skillshub_b005d3_optional_rel(json_t *identifier){
    const char *id=jstr(identifier)?jstr(identifier):"";
    const char *p=startswith(id,"official/")?(id+9):id;
    return json_string(p);
}

json_t *w12_skillshub_b005d3_upstream_pointer(json_t *fm){
    if(!fm||fm->type!=JSON_OBJECT) return json_null();
    json_t *mb=json_obj_get(fm,"metadata");
    if(!mb||mb->type!=JSON_OBJECT) return json_null();
    json_t *hm=json_obj_get(mb,"hermes");
    if(!hm||hm->type!=JSON_OBJECT) return json_null();
    json_t *up=json_obj_get(hm,"upstream");
    if(!up||up->type!=JSON_OBJECT) return json_null();
    json_t *rv=json_obj_get(up,"repo");
    json_t *pv=json_obj_get(up,"path");
    char *r0=rv?py_str_alloc(rv):(char*)strdup("");
    char *p0=pv?py_str_alloc(pv):(char*)strdup("");
    char *r1=strip_dup(r0), *r=strip_slashes_dup(r1);
    char *p1=strip_dup(p0), *p=strip_slashes_dup(p1);
    free(r0);free(r1);free(p0);free(p1);
    if(!*r||count_char(r,'/')!=1||!*p){ free(r);free(p); return json_null(); }
    /* _clean_rel_parts(path) ; join with "/" */
    json_t *ptmp=json_string(p);
    json_t *parts=w12_skillshub_b005d3_clean_rel_parts(ptmp);
    json_free(ptmp);
    if(!parts||parts->type!=JSON_ARRAY){ free(r);free(p); if(parts)json_free(parts); return json_null(); }
    char *joined=join_array(parts,"/");
    json_t *out=json_object();
    json_set(out,"repo",json_string(r));
    json_set(out,"path",json_string(joined));
    free(joined); json_free(parts); free(r); free(p);
    return out;
}

/* entry.get(key, default): copy value when present (even null), else default. */
static void set_default(json_t *out,const char *key,json_t *entry,const char *def){
    json_t *v=(entry&&entry->type==JSON_OBJECT)?json_obj_get(entry,key):NULL;
    if(v) json_set(out,key,json_copy(v));
    else json_set(out,key,json_string(def));
}
json_t *w12_skillshub_b005d3_to_meta(json_t *entry){
    json_t *out=json_object();
    set_default(out,"name",entry,"");
    set_default(out,"description",entry,"");
    set_default(out,"source",entry,"hermes-index");
    set_default(out,"identifier",entry,"");
    set_default(out,"trust_level",entry,"community");
    {
        json_t *v=(entry&&entry->type==JSON_OBJECT)?json_obj_get(entry,"repo"):NULL;
        json_set(out,"repo",v?json_copy(v):json_null());
    }
    {
        json_t *v=(entry&&entry->type==JSON_OBJECT)?json_obj_get(entry,"path"):NULL;
        json_set(out,"path",v?json_copy(v):json_null());
    }
    {
        json_t *v=(entry&&entry->type==JSON_OBJECT)?json_obj_get(entry,"tags"):NULL;
        json_set(out,"tags",v?json_copy(v):json_array());
    }
    {
        json_t *v=(entry&&entry->type==JSON_OBJECT)?json_obj_get(entry,"extra"):NULL;
        json_set(out,"extra",v?json_copy(v):json_object());
    }
    return out;
}
