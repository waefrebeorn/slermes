/*
 * skills_hub_skillssh.c — C home for upstream Hermes tools/skills_hub_skillssh.py.
 * PoP: w12_skillshub_a4dffc_strip_html @ tools/skills_hub_skillssh.py:_strip_html
 * PoP: w12_skillshub_a4dffc_extract_repo_slug @ tools/skills_hub_skillssh.py:SkillsShSource._extract_repo_slug
 * PoP: w12_skillshub_a4dffc_normalize_identifier @ tools/skills_hub_skillssh.py:SkillsShSource._normalize_identifier
 * PoP: w12_skillshub_a4dffc_wrap_identifier @ tools/skills_hub_skillssh.py:SkillsShSource._wrap_identifier
 * PoP: w12_skillshub_a4dffc_detail_to_metadata @ tools/skills_hub_skillssh.py:SkillsShSource._detail_to_metadata
 * PoP: w12_skillshub_a4dffc_token_variants @ tools/skills_hub_skillssh.py:SkillsShSource._token_variants
 *
 * Pure HTML/identifier/token helpers from the skills.sh adapter. All HTTP
 * discovery/detail fetches remain in Python.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_skillshub_a4dffc.h"

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
static char *strip_dup(const char *s){
    s=s?s:""; size_t n=strlen(s);
    while(n&&ws((unsigned char)*s)){s++;n--;}
    while(n&&ws((unsigned char)s[n-1]))n--;
    char *o=(char*)malloc(n+1); memcpy(o,s,n); o[n]=0; return o;
}
static char *lower_dup(const char *s){
    size_t n=strlen(s); char *o=(char*)malloc(n+1);
    for(size_t i=0;i<n;i++) o[i]=(char)tolower((unsigned char)s[i]);
    o[n]=0; return o;
}
static char *strip_char_dup(const char *s,char ch){
    s=s?s:""; size_t n=strlen(s);
    while(n&&s[0]==ch){s++;n--;}
    while(n&&s[n-1]==ch) n--;
    char *o=(char*)malloc(n+1); memcpy(o,s,n); o[n]=0; return o;
}
static char *lstrip_char_dup(const char *s,char ch){
    s=s?s:""; while(*s==ch) s++;
    return (char*)strdup(s);
}
static char *replace_char_dup(const char *s,char from,char to){
    size_t n=strlen(s); char *o=(char*)malloc(n+1);
    for(size_t i=0;i<n;i++) o[i]=s[i]==from?to:s[i];
    o[n]=0; return o;
}
static const char *last_segment(const char *s){
    const char *p=s;
    for(const char *q=s;*q;q++) if(*q=='/') p=q+1;
    return p;
}

/* re.sub(r'<[^>]+>', '', value) */
json_t *w12_skillshub_a4dffc_strip_html(json_t *value){
    const char *s=jstr(value)?jstr(value):"";
    size_t n=strlen(s);
    char *o=(char*)malloc(n+1); size_t k=0;
    for(size_t i=0;i<n;){
        if(s[i]=='<'){
            size_t j=i+1;
            while(j<n && s[j]!='>') j++;
            if(j<n && j>i+1){ i=j+1; continue; }  /* matched <...> */
        }
        o[k++]=s[i++];
    }
    o[k]=0;
    json_t *out=json_string(o); free(o); return out;
}

json_t *w12_skillshub_a4dffc_extract_repo_slug(json_t *repo_value){
    const char *raw=jstr(repo_value)?jstr(repo_value):"";
    char *a=strip_dup(raw);
    const char *b=a;
    const char *pfx="https://github.com/";
    if(startswith(b,pfx)) b+=strlen(pfx);
    char *c=strip_char_dup(b,'/');
    /* split("/") */
    const char *parts[64]; size_t np=0;
    char *work=(char*)strdup(c);
    char *tok=strtok(work,"/");
    while(tok){ if(np<64) parts[np]=tok; np++; tok=strtok(NULL,"/"); }
    json_t *out;
    if(np>=2){
        size_t len=strlen(parts[0])+strlen(parts[1])+2;
        char *r=(char*)malloc(len);
        snprintf(r,len,"%s/%s",parts[0],parts[1]);
        out=json_string(r); free(r);
    } else out=json_null();
    free(work); free(c); free(a); return out;
}

json_t *w12_skillshub_a4dffc_normalize_identifier(json_t *identifier){
    const char *id=jstr(identifier)?jstr(identifier):"";
    static const char *const aliases[]={"skills-sh/","skills.sh/","skils-sh/","skils.sh/"};
    size_t cut=0;
    for(int i=0;i<4;i++) if(startswith(id,aliases[i])){ cut=strlen(aliases[i]); break; }
    return json_string(id+cut);
}

json_t *w12_skillshub_a4dffc_wrap_identifier(json_t *identifier){
    const char *id=jstr(identifier)?jstr(identifier):"";
    size_t n=strlen(id)+10+1;
    char *o=(char*)malloc(n);
    snprintf(o,n,"skills-sh/%s",id);
    json_t *out=json_string(o); free(o); return out;
}

json_t *w12_skillshub_a4dffc_detail_to_metadata(json_t *canonical, json_t *detail){
    const char *can=jstr(canonical)?jstr(canonical):"";
    /* canonical.split("/", 2) -> need first two segments */
    char *work=(char*)strdup(can);
    const char *parts[3]; size_t np=0;
    char *save=work;
    for(char *p=work; ; ){
        char *slash=strchr(p,'/');
        if(slash && np<2){ *slash=0; if(np<3) parts[np]=p; np++; p=slash+1; }
        else { if(np<3) parts[np]=p; np++; break; }
    }
    (void)save;
    json_t *out=json_object();
    {
        size_t n=strlen(can)+strlen("https://skills.sh/")+1;
        char *u=(char*)malloc(n); snprintf(u,n,"https://skills.sh/%s",can);
        json_set(out,"detail_url",json_string(u)); free(u);
    }
    if(np>=2){
        size_t n=strlen(parts[0])+strlen(parts[1])+strlen("https://github.com//")+1;
        char *u=(char*)malloc(n); snprintf(u,n,"https://github.com/%s/%s",parts[0],parts[1]);
        json_set(out,"repo_url",json_string(u)); free(u);
    }
    if(detail && detail->type==JSON_OBJECT){
        static const char *const keys[]={"weekly_installs","install_command","repo_url","detail_url","security_audits"};
        for(int i=0;i<5;i++){
            json_t *v=json_obj_get(detail,keys[i]);
            if(truthy(v)) json_set(out,keys[i],json_copy(v));
        }
    }
    free(work); return out;
}

/* re.sub(r'[^a-z0-9/_-]+', '-', plain).strip('-') */
static char *sanitize_dup(const char *plain){
    size_t n=strlen(plain);
    char *tmp=(char*)malloc(n+1); size_t k=0;
    for(size_t i=0;i<n;i++){
        unsigned char c=(unsigned char)plain[i];
        int ok=(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='/'||c=='_'||c=='-';
        if(ok) tmp[k++]=plain[i];
        else if(k==0 || tmp[k-1]!='-') tmp[k++]='-';
    }
    tmp[k]=0;
    char *o=strip_char_dup(tmp,'-'); free(tmp); return o;
}
static int cmpstr(const void *a,const void *b){
    const char *x=*(const char*const*)a, *y=*(const char*const*)b;
    return strcmp(x,y);
}

json_t *w12_skillshub_a4dffc_token_variants(json_t *value){
    json_t *res=json_array();
    if(!value||value->type==JSON_NULL) return res;
    char *h=strip_dup(jstr(value)?jstr(value):"");
    char *t=strip_char_dup(h,'/');
    char *plain=lower_dup(t);
    free(h);free(t);
    if(!*plain){ free(plain); return res; }
    char *base=(char*)strdup(last_segment(plain));
    char *san=sanitize_dup(plain);
    char *tail=lstrip_char_dup(base,'@');
    const char *pool[12]; size_t np=0;
    char *b_hyphen=replace_char_dup(base,'_','-');
    char *s_slash=replace_char_dup(san,'/','-');
    char *s_last=(char*)strdup(last_segment(san));
    char *tail_hyphen=replace_char_dup(tail,'_','-');
    pool[np++]=plain;
    pool[np++]=replace_char_dup(plain,'_','-');
    pool[np++]=replace_char_dup(plain,'/','-');
    pool[np++]=base;
    pool[np++]=b_hyphen;
    pool[np++]=san;
    pool[np++]=s_slash;
    pool[np++]=s_last;
    pool[np++]=tail;
    pool[np++]=tail_hyphen;
    /* filter empties + dedupe, then sort */
    const char *uniq[12]; size_t nu=0;
    for(size_t i=0;i<np;i++){
        if(!pool[i]||!*pool[i]) continue;
        int seen=0;
        for(size_t j=0;j<nu;j++) if(!strcmp(uniq[j],pool[i])){seen=1;break;}
        if(!seen) uniq[nu++]=pool[i];
    }
    qsort(uniq,nu,sizeof(char*),cmpstr);
    for(size_t i=0;i<nu;i++) json_append(res,json_string(uniq[i]));
    free(plain);free(base);free(san);free(tail);
    free(b_hyphen);free(s_slash);free(s_last);free(tail_hyphen);
    return res;
}
