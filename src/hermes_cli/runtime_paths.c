/*
 * runtime_paths.c — faithful C11 port of the pure path/layout helpers
 * re-exported by upstream Hermes hermes_cli/runtime_paths.py
 * (gate w10_runtimepat_34adf5; implementation lives in pm.environments.py).
 *
 * PoP: w10_runtimepat_34adf5_install_key        @ pm/environments.py:install_key
 * PoP: w10_runtimepat_34adf5_venv_bin_dir       @ pm/environments.py:venv_bin_dir
 * PoP: w10_runtimepat_34adf5_venv_python        @ pm/environments.py:venv_python
 * PoP: w10_runtimepat_34adf5_site_packages      @ pm/environments.py:site_packages
 * PoP: w10_runtimepat_34adf5_install_state_dir  @ pm/environments.py:install_state_dir
 * PoP: w10_runtimepat_34adf5_runtime_facts_path @ pm/environments.py:runtime_facts_path
 * PoP: w10_runtimepat_34adf5_cli_command_name   @ pm/environments.py:cli_command_name
 * PoP: w10_runtimepat_34adf5_shell_exports      @ pm/environments.py:shell_exports
 *
 * Filesystem/env probes are injected as explicit seams so the deterministic
 * logic can be compared against the live Python. Self-contained SHA-256.
 */
#include "noc_w10_runtimepat_34adf5.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/* ---- POSIX-style path helpers (pathlib semantics on the target platform) ---- */
static void pjoin(char *dst, size_t cap, const char *a, const char *b) {
    if (!a || !a[0]) { snprintf(dst, cap, "%s", b ? b : ""); return; }
    if (!b || !b[0]) { snprintf(dst, cap, "%s", a); return; }
    size_t n = strlen(a);
    while (n > 1 && a[n-1] == '/') n--;
    snprintf(dst, cap, "%.*s/%s", (int)n, a, b);
}
static const char *pleaf(const char *p) {
    if (!p) return "";
    size_t n = strlen(p);
    while (n > 1 && p[n-1] == '/') n--;
    size_t i = n;
    while (i > 0 && p[i-1] != '/') i--;
    return p + i;
}
static int is_exe_suffix(const char *s) {
    size_t n = strlen(s);
    return n >= 4 && !strcmp(s + n - 4, ".exe");
}

/* ---- SHA-256 (FIPS 180-4), self-contained ---- */
typedef struct { uint32_t h[8]; uint64_t len; unsigned char buf[64]; size_t used; } shactx;
static const uint32_t K[64] = {
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static uint32_t rr(uint32_t x,int n){return (x>>n)|(x<<(32-n));}
static void shablock(shactx *c, const unsigned char *p) {
    uint32_t w[64], a,b,cc,d,e,f,g,h,t1,t2,s0,s1;
    for (int i=0;i<16;i++) w[i]=((uint32_t)p[4*i]<<24)|((uint32_t)p[4*i+1]<<16)|((uint32_t)p[4*i+2]<<8)|p[4*i+3];
    for (int i=16;i<64;i++){ s0=rr(w[i-15],7)^rr(w[i-15],18)^(w[i-15]>>3); s1=rr(w[i-2],17)^rr(w[i-2],19)^(w[i-2]>>10); w[i]=w[i-16]+s0+w[i-7]+s1; }
    a=c->h[0];b=c->h[1];cc=c->h[2];d=c->h[3];e=c->h[4];f=c->h[5];g=c->h[6];h=c->h[7];
    for (int i=0;i<64;i++){
        s1=rr(e,6)^rr(e,11)^rr(e,25); t1=h+s1+((e&f)^(~e&g))+K[i]+w[i];
        s0=rr(a,2)^rr(a,13)^rr(a,22); t2=s0+((a&b)^(a&cc)^(b&cc));
        h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;
    }
    c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;
}
static void sha_init(shactx *c){ c->h[0]=0x6a09e667;c->h[1]=0xbb67ae85;c->h[2]=0x3c6ef372;c->h[3]=0xa54ff53a;c->h[4]=0x510e527f;c->h[5]=0x9b05688c;c->h[6]=0x1f83d9ab;c->h[7]=0x5be0cd19;c->len=0;c->used=0; }
static void sha_update(shactx *c,const unsigned char *p,size_t n){
    c->len += n;
    while(n){ size_t k=64-c->used; if(k>n)k=n; memcpy(c->buf+c->used,p,k); c->used+=k; p+=k;n-=k; if(c->used==64){shablock(c,c->buf);c->used=0;} }
}
static void sha_final(shactx *c, unsigned char out[32]){
    uint64_t bits=c->len*8; unsigned char pad[64]={0}; size_t k=(c->used<56)?(56-c->used):(120-c->used);
    pad[0]=0x80; sha_update(c,pad,k);
    unsigned char tail[8]; for(int i=0;i<8;i++)tail[i]=(unsigned char)(bits>>(56-8*i));
    sha_update(c,tail,8);
    for(int i=0;i<8;i++){out[4*i]=(unsigned char)(c->h[i]>>24);out[4*i+1]=(unsigned char)(c->h[i]>>16);out[4*i+2]=(unsigned char)(c->h[i]>>8);out[4*i+3]=(unsigned char)c->h[i];}
}

json_t *w10_runtimepat_34adf5_install_key(const char *canonical){
    shactx c; unsigned char d[32]; sha_init(&c); sha_update(&c,(const unsigned char*)(canonical?canonical:""),canonical?strlen(canonical):0); sha_final(&c,d);
    char hex[17]; static const char *H="0123456789abcdef";
    for(int i=0;i<8;i++){hex[2*i]=H[d[i]>>4];hex[2*i+1]=H[d[i]&15];} hex[16]='\0';
    return json_string(hex);
}

json_t *w10_runtimepat_34adf5_venv_bin_dir(const char *venv,int windows){
    char b[2048]; pjoin(b,sizeof(b),venv,windows?"Scripts":"bin"); return json_string(b);
}
json_t *w10_runtimepat_34adf5_venv_python(const char *venv,int windows){
    char bin[2048], out[2048]; pjoin(bin,sizeof(bin),venv,windows?"Scripts":"bin");
    const char *leaf = !strcmp(pleaf(bin),"Scripts") ? "python.exe" : "python";
    pjoin(out,sizeof(out),bin,leaf); return json_string(out);
}
json_t *w10_runtimepat_34adf5_site_packages(const char *venv,int windows,int major,int minor){
    char sub[128], out[2048];
    if(windows) snprintf(sub,sizeof(sub),"Lib/site-packages");
    else snprintf(sub,sizeof(sub),"lib/python%d.%d/site-packages",major,minor);
    pjoin(out,sizeof(out),venv,sub); return json_string(out);
}
json_t *w10_runtimepat_34adf5_install_state_dir(const char *home_root,const char *key){
    char a[2048], out[2048]; pjoin(a,sizeof(a),home_root,"installs"); pjoin(out,sizeof(out),a,key); return json_string(out);
}
json_t *w10_runtimepat_34adf5_runtime_facts_path(const char *home_root,const char *key){
    char a[2048], out[2048]; pjoin(a,sizeof(a),home_root,"installs"); pjoin(out,sizeof(out),a,key);
    char b[2048]; pjoin(b,sizeof(b),out,"facts.json"); return json_string(b);
}
json_t *w10_runtimepat_34adf5_cli_command_name(const char *command){
    const char *name = (command && command[0]) ? pleaf(command) : "";
    if(!name[0]) return json_string("hermes");
    char out[512]; snprintf(out,sizeof(out),"%s",name);
    if(is_exe_suffix(out)) out[strlen(out)-4]='\0';
    return json_string(out);
}

/* ---- shell_exports ---- */
static int ident_ok(const char *s){
    if(!s||!s[0])return 0;
    if(!(isalpha((unsigned char)s[0])||s[0]=='_'))return 0;
    for(const char*p=s+1;*p;p++) if(!(isalnum((unsigned char)*p)||*p=='_'))return 0;
    return 1;
}
static int quote_safe_char(unsigned char c){
    if(isalnum(c)||c=='_')return 1;
    return strchr("@%+=:,./-",c)!=NULL;
}
static void jvalstr(const json_t *v,char *buf,size_t cap){
    if(v&&v->type==JSON_STRING&&v->str_val){snprintf(buf,cap,"%s",v->str_val);return;}
    if(v&&v->type==JSON_BOOL){snprintf(buf,cap,"%s",v->bool_val?"true":"false");return;}
    if(v&&v->type==JSON_NUMBER){ if(v->num_val==(double)(long long)v->num_val)snprintf(buf,cap,"%.0f",v->num_val); else snprintf(buf,cap,"%g",v->num_val); return;}
    buf[0]='\0';
}
static void shlex_quote(const char *s,char *buf,size_t cap){
    if(!s||!s[0]){snprintf(buf,cap,"''");return;}
    int safe=1; for(const char*p=s;*p;p++) if(!quote_safe_char((unsigned char)*p)){safe=0;break;}
    if(safe){snprintf(buf,cap,"%s",s);return;}
    size_t used=0; buf[used++]='\'';
    for(const char*p=s;*p;p++){
        if(*p=='\''){ if(used+5<cap){memcpy(buf+used,"'\"'\"'",5);used+=5;} }
        else { if(used+1<cap)buf[used++]=*p; }
    }
    if(used<cap)buf[used++]='\''; buf[used]='\0';
}
static void fish_quote(const char *s,char *buf,size_t cap){
    size_t used=0; if(used<cap)buf[used++]='\'';
    for(const char*p=s;p&&*p;p++){
        if(*p=='\\'){ if(used+2<cap){buf[used++]='\\';buf[used++]='\\';} }
        else if(*p=='\''){ if(used+2<cap){buf[used++]='\\';buf[used++]='\'';} }
        else { if(used+1<cap)buf[used++]=*p; }
    }
    if(used<cap)buf[used++]='\''; buf[used]='\0';
}
static int fish_ro(const char *n){
    static const char*const ro[]={"PWD","SHLVL","_","status","version","hostname","fish_pid","history","pipestatus","umask","FISH_VERSION"};
    for(int i=0;i<11;i++) if(!strcmp(n,ro[i]))return 1; return 0;
}
json_t *w10_runtimepat_34adf5_shell_exports(const json_t *env,const char *dialect){
    int fish = dialect && !strcmp(dialect,"fish");
    json_t *out=json_array();
    size_t total=0; char *all=NULL;
    if(env&&env->type==JSON_OBJECT){
        for(size_t i=0;i<json_object_size(env);i++){
            const char*k=json_object_get_key_at(env,i); if(!k||!ident_ok(k))continue;
            if(fish&&fish_ro(k))continue;
            json_t*v=json_object_get_at(env,i); char valbuf[4096],qbuf[8192]; jvalstr(v,valbuf,sizeof(valbuf));
            if(fish)fish_quote(valbuf,qbuf,sizeof(qbuf)); else shlex_quote(valbuf,qbuf,sizeof(qbuf));
            size_t klen=strlen(k), qlen=strlen(qbuf);
            size_t add=klen+qlen+(fish?9:8)+2; /* "set -gx " / "export " + "=" + "\n" */
            all=(char*)realloc(all,total+add+1);
            if(fish) total+=sprintf(all+total,"set -gx %s %s\n",k,qbuf);
            else total+=sprintf(all+total,"export %s=%s\n",k,qbuf);
        }
    }
    if(total&&all[total-1]=='\n')all[--total]='\0';
    if(!all)all=(char*)calloc(1,1);
    json_t *r=json_string(all); free(all); return r;
}
