/*
 * tool_search_catalog.c — C home for upstream Hermes tools/tool_search_catalog.py.
 * PoP: w12_toolsearch_6ec2b5_short_desc @ tools/tool_search_catalog.py:_short_desc
 * PoP: w12_toolsearch_6ec2b5_listing_group_label @ tools/tool_search_catalog.py:_listing_group_label
 * PoP: w12_toolsearch_6ec2b5_fn @ tools/tool_search_catalog.py:_fn
 * PoP: w12_toolsearch_6ec2b5_required_term_coverage @ tools/tool_search_catalog.py:_required_term_coverage
 * PoP: w12_toolsearch_6ec2b5_entry_search_text @ tools/tool_search_catalog.py:_entry_search_text
 *
 * Pure text/shape helpers from the deferred-tool catalog module. BM25 scoring,
 * Snowball stemming and the registry-backed catalog build stay in Python.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_toolsearch_6ec2b5.h"

#define CATALOG_SUMMARY_MAX_CHARS 80
#define MIN_ANSWERABLE_TERMS 4

static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
static int ws(unsigned char c){ return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f'; }
static int truthy(json_t *v){
    if(!v||v->type==JSON_NULL) return 0;
    if(v->type==JSON_BOOL) return v->bool_val?1:0;
    if(v->type==JSON_NUMBER) return v->num_val!=0.0;
    if(v->type==JSON_STRING) return v->str_val&&v->str_val[0];
    return v->c.count>0;
}
static int startswith(const char *s,const char *p){ return strncmp(s,p,strlen(p))==0; }

/* " ".join(s.split()) */
static char *collapse_ws_dup(const char *s){
    s=s?s:""; size_t n=strlen(s);
    char *o=(char*)malloc(n+2); size_t k=0; int pending=0;
    for(const unsigned char *p=(const unsigned char*)s;*p;p++){
        if(ws(*p)){ pending=1; continue; }
        if(pending&&k>0) o[k++]=' ';
        pending=0; o[k++]=*p;
    }
    o[k]=0; return o;
}

/* (?<!\be\.g)(?<!\bi\.e)(?<!\betc)[.!?](?=\s|$) -> end offset (i+1) or 0 */
static size_t sentence_end(const char *text){
    size_t n=strlen(text);
    static const char *const pats[3]={"e.g","i.e","etc"};
    for(size_t i=0;i<n;i++){
        char c=text[i];
        if(c!='.'&&c!='!'&&c!='?') continue;
        if(i+1<n && !ws((unsigned char)text[i+1])) continue;  /* needs \s or EOS */
        int blocked=0;
        for(int p=0;p<3;p++){
            size_t L=strlen(pats[p]);
            if(i>=L && !memcmp(text+i-L,pats[p],L)){
                int boundary = (i-L==0) ? 1 :
                    !(isalnum((unsigned char)text[i-L-1])||text[i-L-1]=='_');
                if(boundary){ blocked=1; break; }
            }
        }
        if(!blocked) return i+1;
    }
    return 0;
}
static char *rstrip_set(char *s,const char *set){
    size_t n=strlen(s);
    while(n && strchr(set,s[n-1])) n--;
    s[n]=0; return s;
}

json_t *w12_toolsearch_6ec2b5_short_desc(json_t *description, long long max_chars){
    char *text=collapse_ws_dup(truthy(description)?jstr(description):NULL);
    size_t end=sentence_end(text);
    if(end){ text[end]=0; }            /* NUL at the new length (was text[:m.end()]) */
    size_t len=strlen(text);
    if(len <= (size_t)max_chars){
        json_t *out=json_string(text); free(text); return out;
    }
    char *clipped=(char*)malloc((size_t)max_chars+1);
    memcpy(clipped,text,(size_t)max_chars); clipped[max_chars]=0;
    char *sp=strrchr(clipped,' ');
    if(sp) *sp=0;                       /* NUL at rsplit boundary */
    rstrip_set(clipped,",;: ");
    size_t n=strlen(clipped)+strlen("\xe2\x80\xa6")+1;
    char *res=(char*)malloc(n);
    snprintf(res,n,"%s\xe2\x80\xa6",clipped);
    free(clipped); free(text);
    json_t *out=json_string(res); free(res); return out;
}

json_t *w12_toolsearch_6ec2b5_listing_group_label(json_t *source_name){
    const char *s=(source_name&&source_name->type==JSON_STRING&&source_name->str_val&&source_name->str_val[0])
        ? source_name->str_val : "other";
    if(startswith(s,"mcp-")) s+=4;
    return json_string(s);
}

/* td.get("function") or {} */
json_t *w12_toolsearch_6ec2b5_fn(json_t *td){
    json_t *v=(td&&td->type==JSON_OBJECT)?json_obj_get(td,"function"):NULL;
    if(!truthy(v)) return json_object();
    return json_copy(v);
}

json_t *w12_toolsearch_6ec2b5_required_term_coverage(json_t *n){
    long long count=(n&&n->type==JSON_NUMBER)?(long long)n->num_val:0;
    long long r;
    if(count < MIN_ANSWERABLE_TERMS) r=1;
    else r=(count+1)/2;                 /* ceil(count * 0.5) for integer count */
    return json_number((double)r);
}

json_t *w12_toolsearch_6ec2b5_entry_search_text(json_t *td, json_t *source_label){
    json_t *fn=w12_toolsearch_6ec2b5_fn(td);
    const char *name=jstr(json_obj_get(fn,"name"))?jstr(json_obj_get(fn,"name")):"";
    if(startswith(name,"mcp__")) name+=5;
    /* re.sub(r"[_.:-]", " ", name) */
    size_t nl=strlen(name);
    char *nw=(char*)malloc(nl+1);
    for(size_t i=0;i<nl;i++){
        char c=name[i];
        nw[i]=(c=='_'||c=='.'||c==':'||c=='-')?' ':c;
    }
    nw[nl]=0;
    /* extra = source_label if source_label and source_label not in name_words.split() */
    const char *sl=(source_label&&source_label->type==JSON_STRING)?source_label->str_val:NULL;
    const char *extra="";
    if(sl && *sl){
        int found=0;
        char *work=(char*)strdup(nw);
        char *tok=strtok(work," \t\n\r\f\v");
        while(tok){ if(!strcmp(tok,sl)){found=1;break;} tok=strtok(NULL," \t\n\r\f\v"); }
        free(work);
        if(!found) extra=sl;
    }
    /* param_names = " ".join(((fn.get("parameters") or {}).get("properties") or {}).keys()) */
    char *params=(char*)strdup("");
    json_t *pfn=json_obj_get(fn,"parameters");
    if(truthy(pfn)&&pfn->type==JSON_OBJECT){
        json_t *props=json_obj_get(pfn,"properties");
        if(truthy(props)&&props->type==JSON_OBJECT){
            size_t cap=1;
            for(size_t i=0;i<json_object_size(props);i++)
                cap+=strlen(json_object_get_key_at(props,i)?json_object_get_key_at(props,i):"")+1;
            free(params); params=(char*)malloc(cap); params[0]=0;
            size_t k=0;
            for(size_t i=0;i<json_object_size(props);i++){
                if(k){ params[k++]=' '; }
                const char *key=json_object_get_key_at(props,i)?json_object_get_key_at(props,i):"";
                size_t m=strlen(key); memcpy(params+k,key,m); k+=m;
            }
            params[k]=0;
        }
    }
    const char *desc=jstr(json_obj_get(fn,"description"))?jstr(json_obj_get(fn,"description")):"";
    size_t outn=strlen(nw)+strlen(extra)+strlen(desc)+strlen(params)+5;
    char *out=(char*)malloc(outn);
    snprintf(out,outn,"%s %s %s %s",nw,extra,desc,params);
    free(nw);free(params); json_free(fn);
    json_t *res=json_string(out); free(out); return res;
}
