/*
 * runtime_state.c — faithful C11 port of the pure decision/path helpers in
 * upstream Hermes hermes_cli/runtime_state.py (gate w10_runtimesta_425e21).
 *
 * PoP: w10_runtimesta_425e21_committed         @ hermes_cli/runtime_state.py:_recover_plugin_publication
 * PoP: w10_runtimesta_425e21_plugin_paths_valid @ hermes_cli/runtime_state.py:_recover_plugin_publication
 * PoP: w10_runtimesta_425e21_config_path_valid  @ hermes_cli/runtime_state.py:recover_publication
 * PoP: w10_runtimesta_425e21_path_relative_to   @ hermes_cli/runtime_state.py:_recover_plugin_publication
 * PoP: w10_runtimesta_425e21_path_leaf          @ hermes_cli/runtime_state.py:recover_publication
 * PoP: w10_runtimesta_425e21_path_parent        @ hermes_cli/runtime_state.py:_recover_plugin_publication
 *
 * Locks, base64 restores and filesystem writes are effectful and out of scope;
 * resolution/digest probes are injected seams, as the module's own tests do.
 */
#include "noc_w10_runtimesta_425e21.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void normp(char *dst,size_t cap,const char *p){
    if(!p){dst[0]='\0';return;}
    size_t n=strlen(p); while(n>1&&p[n-1]=='/')n--;
    if(n>=cap)n=cap-1; memcpy(dst,p,n); dst[n]='\0';
}
static const char *leafp(const char *p){
    if(!p)return "";
    size_t n=strlen(p); while(n>1&&p[n-1]=='/')n--;
    size_t i=n; while(i>0&&p[i-1]!='/')i--;
    return p+i;
}
/* leafp() points into the original string; copy it so a trailing '/' is dropped. */
static void leaf_copy(char *dst,size_t cap,const char *p){
    const char *s=leafp(p); size_t n=strlen(s);
    while(n>1&&s[n-1]=='/')n--;
    if(n>=cap)n=cap-1; memcpy(dst,s,n); dst[n]='\0';
}
static void parentp(char *dst,size_t cap,const char *p){
    char np[2048]; normp(np,sizeof(np),p);
    size_t n=strlen(np);
    if(n==0){snprintf(dst,cap,".");return;}
    size_t i=n; while(i>0&&np[i-1]!='/')i--;
    if(i==0){snprintf(dst,cap,"%s",(n>=1&&np[0]=='/')?"/":".");return;}
    if(i==1){snprintf(dst,cap,"/");return;}
    size_t e=i-1; while(e>1&&np[e-1]=='/')e--;
    snprintf(dst,cap,"%.*s",(int)e,np);
}
static int relto(const char *path,const char *parent){
    char a[2048],b[2048]; normp(a,sizeof(a),path); normp(b,sizeof(b),parent);
    if(!strcmp(a,b))return 1;
    size_t bl=strlen(b);
    if(bl==1&&b[0]=='/')return a[0]=='/';
    char pre[2052]; snprintf(pre,sizeof(pre),"%s/",b);
    return strncmp(a,pre,bl+1)==0;
}
static int truthy(const json_t *v){
    if(!v||v->type==JSON_NULL)return 0;
    if(v->type==JSON_BOOL)return v->bool_val;
    if(v->type==JSON_NUMBER)return v->num_val!=0.0;
    if(v->type==JSON_STRING)return v->str_val&&v->str_val[0];
    if(v->type==JSON_ARRAY||v->type==JSON_OBJECT)return v->c.count>0;
    return 0;
}

json_t *w10_runtimesta_425e21_committed(const json_t *row,const char *facts_digest){
    int c = row&&row->type==JSON_OBJECT ? truthy(json_obj_get(row,"committed")) : 0;
    const char *before="";
    if(row&&row->type==JSON_OBJECT){json_t*b=json_obj_get(row,"facts_before"); if(b&&b->type==JSON_STRING&&b->str_val)before=b->str_val;}
    int diff = strcmp(facts_digest?facts_digest:"",before)!=0;
    return json_bool(c||diff);
}

json_t *w10_runtimesta_425e21_plugin_paths_valid(const char *home,const char *target,
                                                 const char *backup,const char *metadata){
    char tdir[2048],bdir[2048],expect[2048];
    parentp(tdir,sizeof(tdir),target);
    parentp(bdir,sizeof(bdir),backup);
    const char *leaf=leafp(leafp(target)?target:target);
    /* target.parent.name */
    const char *tdirleaf=leafp(tdir);
    /* backup.parent != target.parent */
    if(!relto(target,home))return json_bool(0);
    if(strcmp(tdirleaf,"plugins"))return json_bool(0);
    if(strcmp(bdir,tdir))return json_bool(0);
    const char *bname=leafp(backup);
    if(strncmp(bname,".previous-",10))return json_bool(0);
    snprintf(expect,sizeof(expect),"%s/.install-metadata.json",tdir);
    char nm[2048]; normp(nm,sizeof(nm),metadata);
    if(strcmp(nm,expect))return json_bool(0);
    (void)leaf;
    return json_bool(1);
}
json_t *w10_runtimesta_425e21_config_path_valid(const char *config,const char *home){
    if(strcmp(leafp(config),"config.yaml"))return json_bool(0);
    return json_bool(relto(config,home));
}
json_t *w10_runtimesta_425e21_path_relative_to(const char *path,const char *parent){
    return json_bool(relto(path,parent));
}
json_t *w10_runtimesta_425e21_path_leaf(const char *path){
    char lf[2048]; leaf_copy(lf,sizeof(lf),path); return json_string(lf);
}
json_t *w10_runtimesta_425e21_path_parent(const char *path){
    char out[2048]; parentp(out,sizeof(out),path); return json_string(out);
}
