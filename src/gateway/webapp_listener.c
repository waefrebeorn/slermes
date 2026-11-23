/*
 * PoP: w9_webappliste_3df92f_bare_host   @ gateway/webapp_listener.py:_bare_host
 * PoP: w9_webappliste_3df92f_is_wildcard @ gateway/webapp_listener.py:_is_wildcard
 * PoP: w9_webappliste_3df92f_is_loopback @ gateway/webapp_listener.py:_is_loopback
 * PoP: w9_webappliste_3df92f_dial_host   @ gateway/webapp_listener.py:dial_host
 * PoP: w9_webappliste_3df92f_dial_origin @ gateway/webapp_listener.py:dial_origin
 * PoP: w9_webappliste_3df92f_public_url  @ gateway/webapp_listener.py:_public_url
 */
#include "hermes_json.h"
#include "noc_w9_webappliste_3df92f.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_ws(char c){
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';
}
char *w9_webappliste_3df92f_bare_host(const char *host){
    if(!host) host="";
    size_t n=strlen(host);
    size_t a=0,b=n;
    while(a<b && is_ws(host[a])) a++;
    while(b>a && is_ws(host[b-1])) b--;
    char *s=(char*)malloc(b-a+1); if(!s) return NULL;
    memcpy(s,host+a,b-a); s[b-a]=0;
    size_t l=strlen(s);
    if(l>=2 && s[0]=='[' && s[l-1]==']'){
        int colon=0, bracket=0;
        for(size_t i=1;i+1<l;i++){ if(s[i]==':') colon=1; if(s[i]=='[') bracket=1; }
        if(colon && !bracket){
            char *inner=(char*)malloc(l-1); if(inner){ memcpy(inner,s+1,l-2); inner[l-2]=0; }
            free(s); return inner;
        }
    }
    return s;
}
int w9_webappliste_3df92f_is_wildcard(const char *host){
    return host && (!strcmp(host,"0.0.0.0") || !strcmp(host,"::"));
}
static int parse_ipv4(const char *s, unsigned *oct){
    int n=0; const char *p=s;
    while(n<4){
        if(!isdigit((unsigned char)*p)) return 0;
        unsigned v=0; while(isdigit((unsigned char)*p)){ v=v*10+(unsigned)(*p-'0'); if(v>255)return 0; p++; }
        oct[n++]=v;
        if(n<4){ if(*p!='.') return 0; p++; }
    }
    return *p=='\0';
}
static int parse_ipv6(const char *s, unsigned g[8]){
    if(!s||!*s) return 0;
    const char *dc=NULL;
    for(const char *p=s;*(p+1);p++) if(p[0]==':'&&p[1]==':'){ dc=p; break; }
    unsigned head[8], tail[8]; int hn=0,tn=0;
    char buf[64]; if(strlen(s)>=sizeof(buf)) return 0;
    strcpy(buf,s);
    char *hc=dc?(char*)(dc-s+buf):NULL;
    char *save=NULL;
    const char *hstart=buf;
    const char *hend=hc?hc:buf+strlen(buf);
    char hbuf[64]; size_t hl=(size_t)(hend-hstart);
    if(hl>=sizeof(hbuf)) return 0;
    memcpy(hbuf,hstart,hl); hbuf[hl]=0;
    if(hl){
        char *tok=strtok_r(hbuf,":",&save);
        while(tok){
            if(strchr(tok,'.')){ unsigned o[4]; if(!parse_ipv4(tok,o)) return 0;
                if(hn+2>8) return 0; g[hn++]=(o[0]<<8)|o[1]; g[hn++]=(o[2]<<8)|o[3]; }
            else { char *e=NULL; unsigned long v=strtoul(tok,&e,16);
                if(*e||e==tok||v>0xffff) return 0; if(hn>=8) return 0; g[hn++]=(unsigned)v; }
            tok=strtok_r(NULL,":",&save);
        }
    }
    (void)head;
    if(hc){
        const char *tstart=hc+2;
        if(*tstart!='\0'){
            char tbuf[64]; size_t tl=strlen(tstart);
            if(tl>=sizeof(tbuf)) return 0;
            strcpy(tbuf,tstart);
            char *save2=NULL; char *tok=strtok_r(tbuf,":",&save2);
            while(tok){ if(strchr(tok,'.')){ unsigned o[4]; if(!parse_ipv4(tok,o)) return 0;
                        if(tn+2>8) return 0; tail[tn++]=(o[0]<<8)|o[1]; tail[tn++]=(o[2]<<8)|o[3]; }
                    else { char *e=NULL; unsigned long v=strtoul(tok,&e,16);
                        if(*e||e==tok||v>0xffff) return 0; if(tn>=8) return 0; tail[tn++]=(unsigned)v; }
                    tok=strtok_r(NULL,":",&save2); }
        }
        int zeros=8-hn-tn;
        if(zeros<0) return 0;
        for(int i=0;i<zeros;i++) g[hn+i]=0;
        for(int i=0;i<tn;i++) g[hn+zeros+i]=tail[i];
    } else if(hn!=8) return 0;
    return 1;
}
int w9_webappliste_3df92f_is_loopback(const char *host){
    if(!host) return 0;
    if(!strcmp(host,"localhost")) return 1;
    unsigned o[4];
    if(parse_ipv4(host,o)) return o[0]==127;
    unsigned g[8];
    if(parse_ipv6(host,g)){
        int allzero=1; for(int i=0;i<7;i++) if(g[i]) allzero=0;
        if(allzero && g[7]==1) return 1;
        int mz=1; for(int i=0;i<5;i++) if(g[i]) mz=0;
        if(mz && g[5]==0xffff && ((g[6]>>8)==127)) return 1;
    }
    return 0;
}
char *w9_webappliste_3df92f_dial_host(const char *host){
    if(w9_webappliste_3df92f_is_loopback(host)) return strdup(host?host:"");
    if(host && !strcmp(host,"::")) return strdup("::1");
    return strdup("127.0.0.1");
}
char *w9_webappliste_3df92f_dial_origin(const char *host, const char *port){
    char *h=w9_webappliste_3df92f_dial_host(host);
    if(!h) return NULL;
    char *out=(char*)malloc(strlen(h)+64);
    if(!out){ free(h); return NULL; }
    if(strchr(h,':')) snprintf(out,strlen(h)+64,"http://[%s]:%s",h,port?port:"0");
    else snprintf(out,strlen(h)+64,"http://%s:%s",h,port?port:"0");
    free(h); return out;
}
static char *rstrip_slash(const char *s){
    size_t n=strlen(s); while(n>0&&s[n-1]=='/') n--;
    char *o=(char*)malloc(n+1); if(o){ memcpy(o,s,n); o[n]=0; } return o;
}
json_t *w9_webappliste_3df92f_public_url(json_t *value){
    if(!value || value->type==JSON_NULL) return json_string("");
    if(value->type!=JSON_STRING) return json_null();
    const char *v=value->str_val;
    if(!v||!*v) return json_string("");
    for(const char *p=v;*p;p++)
        if(*p=='"'||*p=='\''||*p=='<'||*p=='>'||*p==' '||*p=='\t'||*p=='\r'||*p=='\n'
           ||*p=='?'||*p=='#'||*p=='@'||*p=='\\') return json_null();
    char *url=rstrip_slash(v);
    if(!url) return json_null();
    const char *sep=strstr(url,"://");
    const char *colon=strchr(url,':');
    if(!sep||sep!=colon){ free(url); return json_null(); }
    size_t sl=(size_t)(sep-url);
    if((sl!=4&&sl!=5)||strncmp(url,"http",4)){ free(url); return json_null(); }
    if(sl==5 && url[4]!='s'){ free(url); return json_null(); }
    const char *auth=sep+3;
    const char *end=auth; while(*end && *end!='/') end++;
    size_t alen=(size_t)(end-auth);
    if(alen==0){ free(url); return json_null(); }
    char *authority=(char*)malloc(alen+1);
    if(!authority){ free(url); return json_null(); }
    memcpy(authority,auth,alen); authority[alen]=0;
    int port=0, has_port=0;
    char *hostpart=authority;
    if(*authority=='['){
        char *rb=strchr(authority,']');
        if(!rb){ free(authority); free(url); return json_null(); }
        *rb=0; hostpart=authority+1;
        char *rest=rb+1;
        if(*rest==':'){ has_port=1; rest++; }
        else if(*rest){ free(authority); free(url); return json_null(); }
        if(has_port){
            if(!*rest){ free(authority); free(url); return json_null(); }
            for(char *q=rest;*q;q++){ if(!isdigit((unsigned char)*q)){ free(authority);free(url);return json_null(); } }
            long pv=strtol(rest,NULL,10); if(pv<0||pv>65535){ free(authority);free(url);return json_null(); } port=(int)pv;
        }
    } else {
        char *c=strrchr(authority,':');
        if(c){ has_port=1; *c=0; char *rest=c+1;
            if(!*rest){ free(authority); free(url); return json_null(); }
            for(char *q=rest;*q;q++){ if(!isdigit((unsigned char)*q)){ free(authority);free(url);return json_null(); } }
            long pv=strtol(rest,NULL,10); if(pv<0||pv>65535){ free(authority);free(url);return json_null(); } port=(int)pv;
        }
    }
    int host_ok = hostpart && *hostpart;
    free(authority);
    json_t *out;
    if(!host_ok || (has_port && port==0)) out=json_null();
    else out=json_string(url);
    free(url);
    return out;
}
