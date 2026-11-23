/*
 * skills_hub_sources.c — C home for upstream Hermes tools/skills_hub_sources.py.
 * PoP: w11_skillshub_121926_query_to_index_url @ tools/skills_hub_sources.py:WellKnownSkillSource._query_to_index_url
 * PoP: w11_skillshub_121926_wrap_identifier @ tools/skills_hub_sources.py:WellKnownSkillSource._wrap_identifier
 * PoP: w11_skillshub_121926_parse_wellknown_identifier @ tools/skills_hub_sources.py:WellKnownSkillSource._parse_identifier
 * PoP: w11_skillshub_121926_is_valid_skill_name @ tools/skills_hub_sources.py:UrlSource._is_valid_skill_name
 * PoP: w11_skillshub_121926_resolve_skill_name @ tools/skills_hub_sources.py:UrlSource._resolve_skill_name
 * PoP: w11_skillshub_121926_url_matches @ tools/skills_hub_sources.py:UrlSource._matches
 * PoP: w11_skillshub_121926_agent_id @ tools/skills_hub_sources.py:LobeHubSource._agent_id
 * PoP: w11_skillshub_121926_slug_from_identifier @ tools/skills_hub_sources.py:BrowseShSource._slug_from_identifier
 *
 * All network fetches stay in Python. These are the pure URL/identifier/name
 * helpers used by the four source adapters.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_skillshub_121926.h"

#define BASE_PATH "/.well-known/skills"
static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
static int is_space(unsigned char c){ return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f'; }
static char *strip_dup(const char *s, int lower) {
    s=s?s:""; size_t n=strlen(s);
    while(n&&is_space((unsigned char)*s)){s++;n--;}
    while(n&&is_space((unsigned char)s[n-1]))n--;
    char *o=(char*)malloc(n+1);
    for(size_t i=0;i<n;i++) o[i]=lower?(char)tolower((unsigned char)s[i]):s[i];
    o[n]='\0'; return o;
}
static int startswith(const char *s,const char *p){ return strncmp(s,p,strlen(p))==0; }
static int endswith(const char *s,const char *p){
    size_t n=strlen(s),m=strlen(p); return n>=m && !strcmp(s+n-m,p);
}
static char *rstrip_slashes_dup(const char *s){
    size_t n=strlen(s); while(n&&s[n-1]=='/')n--;
    char *o=(char*)malloc(n+1); memcpy(o,s,n); o[n]='\0'; return o;
}
static char *concat3(const char *a,const char *b,const char *c){
    size_t na=strlen(a),nb=strlen(b),nc=strlen(c);
    char *o=(char*)malloc(na+nb+nc+1);
    memcpy(o,a,na); memcpy(o+na,b,nb); memcpy(o+na+nb,c,nc); o[na+nb+nc]='\0';
    return o;
}
/* urlparse(url).path: slice between the netloc and ?/#. */
static char *url_path(const char *url){
    url=url?url:"";
    const char *p=strstr(url,"://");
    const char *start;
    if(p){ const char *auth=p+3; start=strchr(auth,'/'); if(!start) return (char*)calloc(1,1); }
    else start=url;
    const char *end=start;
    while(*end && *end!='?' && *end!='#') end++;
    size_t n=(size_t)(end-start);
    char *o=(char*)malloc(n+1); memcpy(o,start,n); o[n]='\0'; return o;
}

char *w11_skillshub_121926_query_to_index_url(const char *query){
    char *q=strip_dup(query,0);
    if(!startswith(q,"http://")&&!startswith(q,"https://")){ free(q); return NULL; }
    if(endswith(q,"/index.json")) return q;
    const char *marker=BASE_PATH "/";
    const char *at=strstr(q,marker);
    char *out;
    if(at){ size_t n=(size_t)(at-q); char *head=(char*)malloc(n+1);
            memcpy(head,q,n); head[n]='\0';
            out=concat3(head,marker,"index.json"); free(head); }
    else { char *base=rstrip_slashes_dup(q);
           out=concat3(base,marker,"index.json"); free(base); }
    free(q); return out;
}
char *w11_skillshub_121926_wrap_identifier(const char *base_url,const char *skill_name){
    char *base=rstrip_slashes_dup(base_url);
    char *out=concat3("well-known:",base,"/");
    char *out2=concat3(out,skill_name?skill_name:"","");
    free(out); free(base); return out2;
}
json_t *w11_skillshub_121926_parse_wellknown_identifier(const char *identifier){
    const char *raw=identifier?identifier:"";
    if(startswith(raw,"well-known:")) raw+=11;
    if(!startswith(raw,"http://")&&!startswith(raw,"https://")) return json_null();
    const char *hash=strchr(raw,'#');
    size_t clean_n=hash?(size_t)(hash-raw):strlen(raw);
    char *clean=(char*)malloc(clean_n+1); memcpy(clean,raw,clean_n); clean[clean_n]='\0';
    char *base=NULL,*skill=NULL,*skill_url=NULL;
    if(endswith(clean,"/index.json")){
        if(!hash || hash[1]=='\0'){ free(clean); return json_null(); }
        base=(char*)malloc(clean_n-11+1); memcpy(base,clean,clean_n-11); base[clean_n-11]='\0';
        skill=(char*)calloc(strlen(hash+1)+1,1); strcpy(skill,hash+1);
        skill_url=concat3(base,"/",skill);
    } else {
        if(endswith(clean,"/SKILL.md")){
            skill_url=(char*)malloc(clean_n-9+1); memcpy(skill_url,clean,clean_n-9); skill_url[clean_n-9]='\0';
        } else skill_url=rstrip_slashes_dup(clean);
        const char *marker=BASE_PATH "/";
        if(!strstr(skill_url,marker)){ free(skill_url); free(clean); return json_null(); }
        char *slash=strrchr(skill_url,'/');
        if(!slash){ free(skill_url); free(clean); return json_null(); }
        skill=(char*)calloc(strlen(slash+1)+1,1); strcpy(skill,slash+1);
        base=(char*)malloc((size_t)(slash-skill_url)+1);
        memcpy(base,skill_url,(size_t)(slash-skill_url)); base[slash-skill_url]='\0';
    }
    char *index_url=concat3(base,"/","index.json");
    json_t *res=json_object();
    json_set(res,"index_url",json_string(index_url));
    json_set(res,"base_url",json_string(base));
    json_set(res,"skill_name",json_string(skill));
    json_set(res,"skill_url",json_string(skill_url));
    free(index_url); free(base); free(skill); free(skill_url); free(clean);
    return res;
}

static int valid_name_str(const char *s){
    if(!s) return 0;
    char *cand=strip_dup(s,1);
    if(!*cand){ free(cand); return 0; }
    if(!strcmp(cand,"skill")||!strcmp(cand,"readme")||!strcmp(cand,"index")||!strcmp(cand,"unnamed-skill")){
        free(cand); return 0;
    }
    if(cand[0]<'a'||cand[0]>'z'){ free(cand); return 0; }
    for(const char *p=cand+1;*p;p++)
        if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-')){ free(cand); return 0; }
    free(cand); return 1;
}
int w11_skillshub_121926_is_valid_skill_name(json_t *name){
    return valid_name_str(jstr(name));
}
/* r"\.md$" with IGNORECASE, applied to the last path segment. */
static char *strip_md_suffix(const char *s){
    size_t n=strlen(s);
    if(n>=3 && (s[n-3]=='.') && tolower((unsigned char)s[n-2])=='m' && tolower((unsigned char)s[n-1])=='d'){
        char *o=(char*)malloc(n-3+1); memcpy(o,s,n-3); o[n-3]='\0'; return o;
    }
    char *o=(char*)malloc(n+1); strcpy(o,s); return o;
}
char *w11_skillshub_121926_resolve_skill_name(json_t *fm,const char *url){
    const char *fn=(fm&&fm->type==JSON_OBJECT)?jstr(json_obj_get(fm,"name")):NULL;
    if(fn && valid_name_str(fn)) return strip_dup(fn,0);
    char *path=url_path(url);
    const char *parts[128]; size_t np=0;
    char *work=(char*)malloc(strlen(path)+1); strcpy(work,path);
    char *tok=strtok(work,"/");
    while(tok){ if(*tok){ if(np<128) parts[np]=tok; np++; } tok=strtok(NULL,"/"); }
    char *result=NULL;
    if(np>=2){
        char *lastlower=strip_dup(parts[np-1],1);
        if(!strcmp(lastlower,"skill.md") && valid_name_str(parts[np-2]))
            result=strip_dup(parts[np-2],0);
        free(lastlower);
    }
    if(!result){
        const char *last=np?parts[np-1]:"";
        char *cand=strip_md_suffix(last);
        if(valid_name_str(cand)) result=strip_dup(cand,0);
        free(cand);
    }
    free(work); free(path);
    return result;
}
int w11_skillshub_121926_url_matches(json_t *identifier){
    const char *s=jstr(identifier);
    if(!s) return 0;
    char *ident=strip_dup(s,0);
    char *lower=strip_dup(ident,1);
    int ok=1;
    if(!startswith(lower,"http://")&&!startswith(lower,"https://")) ok=0;
    if(strstr(ident,BASE_PATH "/")) ok=0;
    char *r=rstrip_slashes_dup(lower);
    if(endswith(r,"/index.json")) ok=0;
    free(r);
    if(ok){
        char *path=url_path(ident);
        char *pl=strip_dup(path,1);
        if(!endswith(pl,".md")) ok=0;
        free(pl); free(path);
    }
    free(lower); free(ident);
    return ok;
}
char *w11_skillshub_121926_agent_id(const char *identifier){
    const char *s=identifier?identifier:"";
    if(startswith(s,"lobehub/")){
        const char *p=s+8;
        char *o=(char*)malloc(strlen(p)+1); strcpy(o,p); return o;
    }
    char *o=(char*)malloc(strlen(s)+1); strcpy(o,s); return o;
}
char *w11_skillshub_121926_slug_from_identifier(const char *identifier){
    const char *s=identifier?identifier:"";
    if(startswith(s,"browse-sh/")){
        const char *p=s+10;
        char *o=(char*)malloc(strlen(p)+1); strcpy(o,p); return o;
    }
    char *o=(char*)malloc(strlen(s)+1); strcpy(o,s); return o;
}
