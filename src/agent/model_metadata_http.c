/*
 * model_metadata_http.c - C home for upstream Hermes agent/model_metadata_http.py.
 * PoP: w11_modelmetad_06ab9b_resolve_verify @ agent/model_metadata_http.py:resolve_verify
 * PoP: w11_modelmetad_06ab9b_total_deadline_s @ agent/model_metadata_http.py:TOTAL_DEADLINE_S
 * PoP: w11_modelmetad_06ab9b_scope_headers @ agent/model_metadata_http.py:stream.scope_credentials
 *
 * Module-level literal constants mirrored from the Python source (provenance):
 *   TOTAL_DEADLINE_S = 15.0
 *   safe cross-origin headers = {"accept", "user-agent"}
 *   always-private headers = {"authorization", "cookie", "proxy-authorization"}
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_modelmetad_06ab9b.h"

double w11_modelmetad_06ab9b_total_deadline_s(void) { return 15.0; }
static void w11mh_lower(char *s) { for (; s && *s; s++) if (*s>='A' && *s<='Z') *s=(char)(*s+32); }

json_t *w11_modelmetad_06ab9b_resolve_verify(const char *base_url, json_t *settings, int settings_raise) {
    json_t *o=json_object();
    json_t *ca=NULL, *sv=NULL;
    if (base_url && *base_url && !settings_raise && settings && settings->type==JSON_OBJECT) {
        ca=json_obj_get(settings,"ssl_ca_cert");
        sv=json_obj_get(settings,"ssl_verify");
    }
    json_set(o,"ca_bundle", ca ? json_copy(ca) : json_null());
    json_set(o,"ssl_verify", sv ? json_copy(sv) : json_null());
    json_set(o,"base_url", json_string(base_url ? base_url : ""));
    return o;
}
/* url_origin() from hermes_cli/urllib_security.py, reduced to (scheme, host, port).
 * port -1 represents Python None (absent and no scheme default). */
static void w11mh_origin(const char *url, char *scheme, size_t sc, char *host, size_t hc, int *port) {
    scheme[0]=0; host[0]=0; *port=-1;
    if (!url) return;
    const char *s=url, *sep=strstr(url,"://");
    if (sep) {
        size_t n=(size_t)(sep-url); if (n>=sc) n=sc-1;
        memcpy(scheme,url,n); scheme[n]=0; w11mh_lower(scheme); s=sep+3;
    }
    const char *e=s; while (*e && *e!='/' && *e!='?' && *e!='#') e++;
    size_t alen=(size_t)(e-s);
    char *auth=(char *)malloc(alen+1); memcpy(auth,s,alen); auth[alen]=0;
    const char *hp=auth, *at=strrchr(auth,'@'); if (at) hp=at+1;
    const char *hend=hp+strlen(hp);
    if (*hp=='[') {
        const char *rb=strchr(hp,']'); if (rb) hend=rb+1;
    } else {
        const char *col=strchr(hp,':');
        if (col) {
            hend=col;
            if (col[1]) *port=(int)strtol(col+1,NULL,10);
        }
    }
    size_t hl=(size_t)(hend-hp); if (hl>=hc) hl=hc-1;
    memcpy(host,hp,hl); host[hl]=0; w11mh_lower(host);
    while (hl && host[hl-1]=='.') host[--hl]=0;
    free(auth);
    if (*port<0) {
        if (strcmp(scheme,"https")==0) *port=443;
        else if (strcmp(scheme,"http")==0) *port=80;
    }
}
static int w11mh_same_origin(const char *a, const char *b) {
    char as[64],ah[512],bs[64],bh[512]; int ap,bp;
    w11mh_origin(a,as,sizeof(as),ah,sizeof(ah),&ap);
    w11mh_origin(b,bs,sizeof(bs),bh,sizeof(bh),&bp);
    return strcmp(as,bs)==0 && strcmp(ah,bh)==0 && ap==bp;
}
static int w11mh_in(const char *const *set, size_t n, const char *s) {
    for (size_t i=0;i<n;i++) if (strcmp(set[i],s)==0) return 1;
    return 0;
}
json_t *w11_modelmetad_06ab9b_scope_headers(const char *initial_url, const char *request_url, json_t *headers) {
    static const char *const SAFE[]={"accept","user-agent"};
    static const char *const FORCED[]={"authorization","cookie","proxy-authorization"};
    json_t *out = headers ? json_copy(headers) : json_object();
    size_t n = json_object_size(out);
    char **priv=(char **)calloc(n+3,sizeof(char *)); size_t np=0;
    for (size_t i=0;i<n;i++) {
        const char *k=json_object_get_key_at(out,i);
        if (!k) continue;
        char *lk=strdup(k); w11mh_lower(lk);
        if (!w11mh_in(SAFE,sizeof(SAFE)/sizeof(SAFE[0]),lk)) {
            int seen=0;
            for (size_t j=0;j<np;j++) if (strcmp(priv[j],lk)==0) { seen=1; break; }
            if (!seen) priv[np++]=lk; else free(lk);
        } else free(lk);
    }
    for (size_t i=0;i<sizeof(FORCED)/sizeof(FORCED[0]);i++) {
        const char *f=FORCED[i]; int seen=0;
        for (size_t j=0;j<np;j++) if (strcmp(priv[j],f)==0) { seen=1; break; }
        if (!seen) { priv[np++]=strdup(f); }
    }
    json_t *res=out;
    if (!w11mh_same_origin(initial_url,request_url)) {
        res=json_object();
        for (size_t i=0;i<json_object_size(out);i++) {
            const char *k=json_object_get_key_at(out,i);
            if (!k) continue;
            char *lk=strdup(k); w11mh_lower(lk);
            int drop=w11mh_in((const char *const *)priv,np,lk);
            free(lk);
            if (!drop) json_set(res,k,json_copy(json_object_get_at(out,i)));
        }
        json_free(out);
    }
    for (size_t i=0;i<np;i++) free(priv[i]);
    free(priv);
    return res;
}
