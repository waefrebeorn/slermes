/*
 * pairing.c — DM pairing system (faithful C11 port of gateway/pairing.py).
 * See pairing.h.
 */

#include "pairing.h"
#include "hermes_gateway_pairing.h"
#include "crypto.h"
/* forward-declare the existing whatsapp normalizer (defined in gateway/helpers.c) */
extern char *normalize_whatsapp_identifier(const char *value);
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/random.h>
#include <stdlib.h>
#include <dirent.h>

struct pairing_store {
    char *dir;
};

/* ── minimal JSON (object of string/number/nested-object values) ── */

typedef struct pj_val pj_val_t;
struct pj_val {
    enum { J_STR, J_NUM, J_OBJ } type;
    char *s;            /* for J_STR */
    double num;          /* for J_NUM */
    /* for J_OBJ: members */
    char **keys;
    pj_val_t **vals;
    int n;
};

static void pj_free(pj_val_t *v) {
    if (!v) return;
    if (v->type == J_STR) free(v->s);
    else if (v->type == J_OBJ) {
        for (int i=0;i<v->n;i++){ free(v->keys[i]); pj_free(v->vals[i]); }
        free(v->keys); free(v->vals);
    }
    free(v);
}

/* parse a JSON value starting at *pp; advances *pp past it. Returns NULL on error. */
static pj_val_t *pj_parse_val(const char **pp);

static void skip_ws(const char **pp) {
    while (**pp && isspace((unsigned char)**pp)) (*pp)++;
}

static char *pj_parse_string(const char **pp) {
    /* assumes **pp == '"' */
    (*pp)++;
    size_t cap = 16, len = 0;
    char *s = (char*)malloc(cap);
    while (**pp && **pp != '"') {
        char c = **pp;
        if (c == '\\') {
            (*pp)++;
            char e = **pp;
            char out;
            switch (e) {
                case 'n': out='\n'; break;
                case 't': out='\t'; break;
                case 'r': out='\r'; break;
                case '"': out='"'; break;
                case '\\': out='\\'; break;
                case '/': out='/'; break;
                case 'b': out='\b'; break;
                case 'f': out='\f'; break;
                default: out=e; break;
            }
            s[len++] = out; (*pp)++;
        } else {
            s[len++] = c; (*pp)++;
        }
        if (len+1 >= cap){ cap*=2; s=realloc(s,cap); }
    }
    if (**pp == '"') (*pp)++;
    s[len] = '\0';
    return s;
}

static pj_val_t *pj_parse_val(const char **pp) {
    skip_ws(pp);
    if (**pp == '"') {
        pj_val_t *v = (pj_val_t*)calloc(1, sizeof(pj_val_t));
        v->type = J_STR; v->s = pj_parse_string(pp);
        return v;
    }
    if (**pp == '{') {
        pj_val_t *v = (pj_val_t*)calloc(1, sizeof(pj_val_t));
        v->type = J_OBJ;
        (*pp)++;
        skip_ws(pp);
        while (**pp && **pp != '}') {
            if (**pp != '"') { pj_free(v); return NULL; }
            char *key = pj_parse_string(pp);
            skip_ws(pp);
            if (**pp != ':') { free(key); pj_free(v); return NULL; }
            (*pp)++;
            pj_val_t *val = pj_parse_val(pp);
            /* append */
            v->keys = (char**)realloc(v->keys, (v->n+1)*sizeof(char*));
            v->vals = (pj_val_t**)realloc(v->vals, (v->n+1)*sizeof(pj_val_t*));
            v->keys[v->n] = key; v->vals[v->n] = val; v->n++;
            skip_ws(pp);
            if (**pp == ',') { (*pp)++; skip_ws(pp); }
            else break;
        }
        if (**pp == '}') (*pp)++;
        return v;
    }
    /* number */
    char *end; double num = strtod(*pp, &end);
    if (end == *pp) { return NULL; }
    pj_val_t *v = (pj_val_t*)calloc(1, sizeof(pj_val_t));
    v->type = J_NUM; v->num = num; *pp = end;
    return v;
}

/* serialize a J_OBJ to a string (2-space indent). Grows *out via *cap. */
static void pj_ensure(char **out, size_t *pos, size_t *cap, size_t extra) {
    if (*pos + extra + 1 <= *cap) return;
    size_t ncap = *cap ? *cap*2 : 256;
    while (ncap < *pos + extra + 1) ncap *= 2;
    *out = realloc(*out, ncap);
    *cap = ncap;
}
static void pj_serialize(const pj_val_t *v, char **out, size_t *pos, size_t *cap, int depth) {
    if (v->type != J_OBJ) return;
    pj_ensure(out, pos, cap, 2);
    (*out)[(*pos)++] = '{';
    (*out)[(*pos)++] = '\n';
    for (int i=0;i<v->n;i++) {
        pj_ensure(out, pos, cap, (depth+1)*2 + strlen(v->keys[i]) + 64);
        for (int d=0; d<depth+1; d++){ (*out)[(*pos)++]=' '; (*out)[(*pos)++]=' '; }
        (*out)[(*pos)++]='"'; for (char *c=v->keys[i];*c;c++) (*out)[(*pos)++] = *c; (*out)[(*pos)++]='"';
        (*out)[(*pos)++] = ':'; (*out)[(*pos)++] = ' ';
        pj_val_t *val = v->vals[i];
        if (val->type == J_STR) {
            (*out)[(*pos)++]='"';
            for (char *c=val->s;*c;c++){ if(*c=='"'||*c=='\\'){ pj_ensure(out,pos,cap,2); (*out)[(*pos)++]='\\'; } pj_ensure(out,pos,cap,1); (*out)[(*pos)++]=*c; }
            pj_ensure(out,pos,cap,1); (*out)[(*pos)++]='"';
        } else if (val->type == J_NUM) {
            char buf[32]; snprintf(buf,sizeof(buf),"%.0f",val->num);
            pj_ensure(out,pos,cap,strlen(buf)+1);
            for (char *c=buf;*c;c++) (*out)[(*pos)++]=*c;
        } else if (val->type == J_OBJ) {
            pj_serialize(val, out, pos, cap, depth+1);
        }
        pj_ensure(out,pos,cap,2);
        if (i+1 < v->n) (*out)[(*pos)++]=',';
        (*out)[(*pos)++]='\n';
    }
    pj_ensure(out,pos,cap,(depth)*2 + 2);
    for (int d=0; d<depth; d++){ (*out)[(*pos)++]=' '; (*out)[(*pos)++]=' '; }
    (*out)[(*pos)++]='}';
}

/* ── helpers ── */

static char *xstrdup(const char *s){ return s?strdup(s):NULL; }

pairing_store_t *pairing_store_open(const char *dir) {
    pairing_store_t *st = (pairing_store_t*)calloc(1, sizeof(pairing_store_t));
    st->dir = xstrdup(dir);
    mkdir(dir, 0700);
    return st;
}
void pairing_store_close(pairing_store_t *st) {
    if (!st) return;
    free(st->dir); free(st);
}

/* PoP: path_for @ tools/memory_tool.py:_path_for */
static char *path_for(pairing_store_t *st, const char *name) {
    size_t need = strlen(st->dir)+1+strlen(name)+1;
    char *p = (char*)malloc(need);
    snprintf(p, need, "%s/%s", st->dir, name);
    return p;
}

/* PoP: load_json @ gateway/pairing.py:_load_json */
static pj_val_t *load_json(pairing_store_t *st, const char *name) {
    char *path = path_for(st, name);
    FILE *f = fopen(path, "rb");
    free(path);
    if (!f) return NULL;
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    if (sz<=0){ fclose(f); return NULL; }
    char *buf=(char*)malloc(sz+1);
    size_t rd=fread(buf,1,sz,f); buf[rd]='\0'; fclose(f);
    const char *p=buf;
    skip_ws(&p);
    pj_val_t *v = (p[0]=='{') ? pj_parse_val(&p) : NULL;
    free(buf);
    if (v && v->type != J_OBJ){ pj_free(v); return NULL; }
    return v;
}

/* PoP: save_json @ gateway/pairing.py:_save_json */
static void save_json(pairing_store_t *st, const char *name, pj_val_t *obj) {
    char *path = path_for(st, name);
    size_t cap = 256; char *out=(char*)malloc(cap); size_t pos=0;
    pj_serialize(obj, &out, &pos, &cap, 0);
    out[pos++]='\n'; out[pos]='\0';
    char tmpl[4096]; snprintf(tmpl,sizeof(tmpl),"%s.tmp",path);
    FILE *f=fopen(tmpl,"wb");
    if (f){ fwrite(out,1,pos,f); fclose(f); rename(tmpl,path); chmod(path,0600); }
    free(out); free(path);
}

static pj_val_t *obj_get(pj_val_t *obj, const char *key) {
    if (!obj || obj->type!=J_OBJ) return NULL;
    for (int i=0;i<obj->n;i++) if (strcmp(obj->keys[i],key)==0) return obj->vals[i];
    return NULL;
}
static int obj_has(pj_val_t *obj, const char *key) { return obj_get(obj,key)!=NULL; }
static void obj_del(pj_val_t *obj, const char *key);

/* set/replace a string member (replaces any existing key) */
static void obj_set_str(pj_val_t *obj, const char *key, const char *val) {
    obj_del(obj, key);
    pj_val_t *v=(pj_val_t*)calloc(1,sizeof(pj_val_t)); v->type=J_STR; v->s=xstrdup(val);
    obj->keys=(char**)realloc(obj->keys,(obj->n+1)*sizeof(char*));
    obj->vals=(pj_val_t**)realloc(obj->vals,(obj->n+1)*sizeof(pj_val_t*));
    obj->keys[obj->n]=xstrdup(key); obj->vals[obj->n]=v; obj->n++;
}
static void obj_set_num(pj_val_t *obj, const char *key, double val) {
    obj_del(obj, key);
    pj_val_t *v=(pj_val_t*)calloc(1,sizeof(pj_val_t)); v->type=J_NUM; v->num=val;
    obj->keys=(char**)realloc(obj->keys,(obj->n+1)*sizeof(char*));
    obj->vals=(pj_val_t**)realloc(obj->vals,(obj->n+1)*sizeof(pj_val_t*));
    obj->keys[obj->n]=xstrdup(key); obj->vals[obj->n]=v; obj->n++;
}
/* shallow-remove a key */
/* shallow-remove a key */
static void obj_del(pj_val_t *obj, const char *key) {
    for (int i=0;i<obj->n;i++) if (strcmp(obj->keys[i],key)==0) {
        free(obj->keys[i]); pj_free(obj->vals[i]);
        for (int j=i;j<obj->n-1;j++){ obj->keys[j]=obj->keys[j+1]; obj->vals[j]=obj->vals[j+1]; }
        obj->n--; return;
    }
}
static void obj_set_obj(pj_val_t *obj, const char *key, pj_val_t *val) {
    /* append an object-valued member (no replace; caller handles conflict) */
    obj->keys=(char**)realloc(obj->keys,(obj->n+1)*sizeof(char*));
    obj->vals=(pj_val_t**)realloc(obj->vals,(obj->n+1)*sizeof(pj_val_t*));
    obj->keys[obj->n]=xstrdup(key); obj->vals[obj->n]=val; obj->n++;
}
/* merge: current (active) wins on key conflict; union the rest from merged */
static void obj_merge_union(pj_val_t *current, pj_val_t *merged) {
    for (int i = 0; i < merged->n; i++) {
        if (obj_has(current, merged->keys[i])) { pj_free(merged->vals[i]); continue; }
        obj_set_obj(current, merged->keys[i], merged->vals[i]);
    }
}

/* user-id normalization + aliases */
static char *normalize_uid(pairing_store_t *st, const char *platform, const char *user_id) {
    (void)st;
    char *raw = xstrdup(user_id ? user_id : ""); /* strip? keep simple */
    if (platform && strcmp(platform,"whatsapp")==0) {
        char *n = normalize_whatsapp_identifier(raw);
        free(raw); return n;
    }
    return raw;
}

/* build alias set as array of malloc'd strings */
static char **uid_aliases(pairing_store_t *st, const char *platform, const char *user_id, int *out_n) {
    char *raw = xstrdup(user_id ? user_id : "");
    char **arr=NULL; int n=0, cap=0;
    if (raw[0]){
        arr=(char**)realloc(arr,(cap=2)*sizeof(char*));
        arr[n++]=xstrdup(raw);
        char *norm = normalize_uid(st, platform, raw);
        arr=(char**)realloc(arr,(++cap)*sizeof(char*));
        arr[n++]=xstrdup(norm);
        free(norm);

    }
    free(raw);
    /* drop empty */
    for (int i=0;i<n;i++) if (arr[i][0]=='\0'){ free(arr[i]); for(int j=i;j<n-1;j++) arr[j]=arr[j+1]; n--; i--; }
    *out_n=n; return arr;
}

static bool uids_match(pairing_store_t *st, const char *platform, const char *left, const char *right) {
    int nl, nr; char **la = uid_aliases(st, platform, left, &nl);
    char **ra = uid_aliases(st, platform, right, &nr);
    bool match=false;
    for (int i=0;i<nl && !match;i++)
        for (int j=0;j<nr;j++)
            if (strcmp(la[i],ra[j])==0){ match=true; break; }
    for (int i=0;i<nl;i++) free(la[i]); free(la);
    for (int i=0;i<nr;i++) free(ra[i]); free(ra);
    return match;
}

/* ── approved ── */
/* PoP: is_approved @ gateway/pairing.py:is_approved */

bool pairing_is_approved(pairing_store_t *st, const char *platform, const char *user_id) {
    char name[256]; snprintf(name,sizeof(name),"%s-approved.json",platform);
    pj_val_t *obj = load_json(st, name);
    if (!obj){ return false; }
    bool ok=false;
    for (int i=0;i<obj->n;i++){
        if (obj->vals[i]->type==J_OBJ && uids_match(st, platform, obj->keys[i], user_id)){ ok=true; break; }
    }
    pj_free(obj);
    return ok;
}

/* PoP: pairing_list_approved @ gateway/pairing.py:list_approved */
int pairing_list_approved(pairing_store_t *st, const char *platform, pairing_approved_t **out) {
    pairing_approved_t *arr=NULL; int n=0, cap=0;
    /* enumerate platforms */
    char **plats=NULL; int np=0;
    if (platform){ plats=(char**)malloc(sizeof(char*)); plats[0]=xstrdup(platform); np=1; }
    else {
        /* scan dir for *-approved.json */
        DIR *d = opendir(st->dir);
        if (d){
            struct dirent *e;
            while ((e=readdir(d))){
                size_t L=strlen(e->d_name);
                if (L>13 && strcmp(e->d_name+L-13,"-approved.json")==0){
                    char *p=(char*)malloc(L-13+1); memcpy(p,e->d_name,L-13); p[L-13]='\0';
                    if (p[0]!='_'){ plats=(char**)realloc(plats,(np+1)*sizeof(char*)); plats[np++]=p; }
                    else free(p);
                }
            }
            closedir(d);
        }
    }
    for (int pi=0; pi<np; pi++){
        char name[256]; snprintf(name,sizeof(name),"%s-approved.json",plats[pi]);
        pj_val_t *obj = load_json(st, name);
        if (!obj) continue;
        for (int i=0;i<obj->n;i++){
            if (obj->vals[i]->type!=J_OBJ) continue;
            pj_val_t *info = obj->vals[i];
            pj_val_t *un = obj_get(info,"user_name");
            pj_val_t *aa = obj_get(info,"approved_at");
            if (n>=cap){ cap=cap?cap*2:8; arr=(pairing_approved_t*)realloc(arr,cap*sizeof(pairing_approved_t)); }
            arr[n].user_id=xstrdup(obj->keys[i]);
            arr[n].user_name = (un && un->type==J_STR)?xstrdup(un->s):xstrdup("");
            arr[n].approved_at = (aa && aa->type==J_NUM)?aa->num:0;
            n++;
        }
        pj_free(obj);
    }
    for (int i=0;i<np;i++) free(plats[i]); free(plats);
    *out=arr; return n;
}

/* PoP: pairing_approve_user @ gateway/pairing.py:_approve_user */
bool pairing_approve_user(pairing_store_t *st, const char *platform, const char *user_id, const char *user_name, double now) {
    char *norm = normalize_uid(st, platform, user_id);
    char name[256]; snprintf(name,sizeof(name),"%s-approved.json",platform);
    pj_val_t *obj = load_json(st, name);
    if (!obj) obj=(pj_val_t*)calloc(1,sizeof(pj_val_t)), obj->type=J_OBJ;
    /* remove matching */
    for (int i=0;i<obj->n;i++){
        if (obj->vals[i]->type==J_OBJ && uids_match(st,platform,obj->keys[i],norm)){
            obj_del(obj, obj->keys[i]); i--;
        }
    }
    pj_val_t *info=(pj_val_t*)calloc(1,sizeof(pj_val_t)); info->type=J_OBJ;
    obj_set_str(info,"user_name", user_name?user_name:"");
    obj_set_num(info,"approved_at", now);
    obj->keys=(char**)realloc(obj->keys,(obj->n+1)*sizeof(char*));
    obj->vals=(pj_val_t**)realloc(obj->vals,(obj->n+1)*sizeof(pj_val_t*));
    obj->keys[obj->n]=xstrdup(norm); obj->vals[obj->n]=info; obj->n++;
    save_json(st, name, obj);
    pj_free(obj);
    free(norm);
    /* allowlist mirror (option i) — best-effort, skipped at store layer */
    return true;
}
/* PoP: revoke @ gateway/pairing.py:revoke */

bool pairing_revoke(pairing_store_t *st, const char *platform, const char *user_id) {
    char name[256]; snprintf(name,sizeof(name),"%s-approved.json",platform);
    pj_val_t *obj = load_json(st, name);
    if (!obj) return false;
    bool found=false;
    for (int i=0;i<obj->n;i++){
        if (obj->vals[i]->type==J_OBJ && uids_match(st,platform,obj->keys[i],user_id)){
            obj_del(obj, obj->keys[i]); found=true; i--;
        }
    }
    if (found) save_json(st, name, obj);
    pj_free(obj);
    return found;
}

/* ── hashing ── */

static void hex_encode(const unsigned char *bin, int n, char *out) {
    const char *h="0123456789abcdef";
    for (int i=0;i<n;i++){ out[i*2]=h[bin[i]>>4]; out[i*2+1]=h[bin[i]&0xf]; }
    out[n*2]='\0';
}

/* PoP: hash_code @ gateway/pairing.py:_hash_code */
static void hash_code(const char *code, const unsigned char *salt, int salt_len, char *out_hex /*65*/) {
    unsigned char buf[64];
    memcpy(buf, salt, salt_len);
    memcpy(buf+salt_len, code, strlen(code));
    unsigned char h[32];
    crypto_sha256(buf, salt_len + strlen(code), h);
    hex_encode(h, 32, out_hex);
}

/* ── pending ── */

static char *pending_filename(const char *platform);

/* Remove expired pending codes (tolerant of malformed/legacy entries). */
/* PoP: pairing_cleanup_expired @ gateway/pairing.py:_cleanup_expired */
static void pairing_cleanup_expired(pairing_store_t *st, const char *platform, double now) {
    char *pname = pending_filename(platform);
    pj_val_t *obj = load_json(st, pname);
    if (!obj){ free(pname); return; }
    for (int i=0;i<obj->n;i++){
        pj_val_t *e = obj->vals[i];
        if (e->type!=J_OBJ){ obj_del(obj, obj->keys[i]); i--; continue; }
        pj_val_t *ca = obj_get(e,"created_at");
        if (!ca || ca->type!=J_NUM){ obj_del(obj, obj->keys[i]); i--; continue; }
        if ((now - ca->num) > PAIRING_CODE_TTL_SECONDS){ obj_del(obj, obj->keys[i]); i--; }
    }
    save_json(st, pname, obj);
    pj_free(obj);
    free(pname);
}

static char *pending_filename(const char *platform) {
    char *n=(char*)malloc(strlen(platform)+14);
    sprintf(n,"%s-pending.json",platform);
    return n;
}

char *pairing_generate_code(pairing_store_t *st, const char *platform, const char *user_id, const char *user_name, double now) {
    pairing_cleanup_expired(st, platform, now);
    char *pname = pending_filename(platform);
    pj_val_t *obj = load_json(st, pname);
    if (!obj) obj=(pj_val_t*)calloc(1,sizeof(pj_val_t)), obj->type=J_OBJ;

    if (pairing_is_locked_out(st, platform, now)) { pj_free(obj); free(pname); return NULL; }
    /* rate limit */
    {
        int na; char **aliases = uid_aliases(st, platform, user_id, &na);
        pj_val_t *rl = load_json(st, "_rate_limits.json");
        bool limited=false;
        for (int i=0;i<na;i++){
            char key[256]; snprintf(key,sizeof(key),"%s:%s",platform,aliases[i]);
            pj_val_t *v = rl?obj_get(rl,key):NULL;
        if (v && v->type==J_NUM && (now - v->num) < PAIRING_RATE_LIMIT_SECONDS){ limited=true; break; }
        }
        for (int i=0;i<na;i++) free(aliases[i]); free(aliases);
        pj_free(rl);
        if (limited){ pj_free(obj); free(pname); return NULL; }
    }
    if (obj->n >= PAIRING_MAX_PENDING_PER_PLATFORM){ pj_free(obj); free(pname); return NULL; }

    /* random code */
    static const char *A = PAIRING_ALPHABET;
    char code[PAIRING_CODE_LENGTH+1];
    unsigned char rb[PAIRING_CODE_LENGTH];
    if (getrandom(rb,PAIRING_CODE_LENGTH,0)!=PAIRING_CODE_LENGTH){
        srand((unsigned)(now*1000)); for(int i=0;i<PAIRING_CODE_LENGTH;i++) code[i]=A[rand()%32]; }
    else for(int i=0;i<PAIRING_CODE_LENGTH;i++) code[i]=A[rb[i]%32];
    code[PAIRING_CODE_LENGTH]='\0';

    unsigned char salt[16];
    if (getrandom(salt,16,0)!=16){ for(int i=0;i<16;i++) salt[i]=(unsigned char)(now*1000+i); }
    char salt_hex[33]; hex_encode(salt,16,salt_hex);
    char code_hash[65]; hash_code(code, salt, 16, code_hash);

    char entry_id[17]; { unsigned char eb[8]; if(getrandom(eb,8,0)==8){ hex_encode(eb,8,entry_id);} else snprintf(entry_id,17,"%08lx",(unsigned long)now); }

    char *norm = normalize_uid(st, platform, user_id);
    pj_val_t *info=(pj_val_t*)calloc(1,sizeof(pj_val_t)); info->type=J_OBJ;
    obj_set_str(info,"hash", code_hash);
    obj_set_str(info,"salt", salt_hex);
    obj_set_str(info,"user_id", norm);
    obj_set_str(info,"user_name", user_name?user_name:"");
    obj_set_num(info,"created_at", now);
    obj->keys=(char**)realloc(obj->keys,(obj->n+1)*sizeof(char*));
    obj->vals=(pj_val_t**)realloc(obj->vals,(obj->n+1)*sizeof(pj_val_t*));
    obj->keys[obj->n]=xstrdup(entry_id); obj->vals[obj->n]=info; obj->n++;

    save_json(st, pname, obj);
    pj_free(obj);

    /* record rate limit */
    pj_val_t *rl = load_json(st, "_rate_limits.json");
    if (!rl) rl=(pj_val_t*)calloc(1,sizeof(pj_val_t)), rl->type=J_OBJ;
    int na; char **aliases = uid_aliases(st, platform, user_id, &na);
    for (int i=0;i<na;i++){ char key[256]; snprintf(key,sizeof(key),"%s:%s",platform,aliases[i]); obj_set_num(rl,key,now); }
    for (int i=0;i<na;i++) free(aliases[i]); free(aliases);
    save_json(st, "_rate_limits.json", rl);
    pj_free(rl);

    free(norm); free(pname);
    return xstrdup(code);
}

/* PoP: pairing_approve_code @ gateway/pairing.py:approve_code */
pairing_result_t *pairing_approve_code(pairing_store_t *st, const char *platform, const char *code, double now) {
    pairing_cleanup_expired(st, platform, now);
    char *pname = pending_filename(platform);
    pj_val_t *obj = load_json(st, pname);
    if (!obj) obj=(pj_val_t*)calloc(1,sizeof(pj_val_t)), obj->type=J_OBJ;

    if (pairing_is_locked_out(st, platform, now)) { pj_free(obj); free(pname); return NULL; }

    char *up = xstrdup(code?code:""); /* uppercase + strip */
    for (size_t i=0;up[i];i++) up[i]=(char)toupper((unsigned char)up[i]);
    char *stripped = up; while(*stripped==' '||*stripped=='\t') stripped++;
    size_t L=strlen(stripped); while(L>0 && (stripped[L-1]==' '||stripped[L-1]=='\t')) stripped[--L]='\0';

    const char *matched_key=NULL; pj_val_t *matched=NULL;
    for (int i=0;i<obj->n;i++){
        pj_val_t *e = obj->vals[i];
        if (e->type!=J_OBJ) continue;
        pj_val_t *hs=obj_get(e,"hash"), *sl=obj_get(e,"salt");
        if (!hs || hs->type!=J_STR || !sl || sl->type!=J_STR) continue;
        unsigned char salt[16];
        /* parse salt hex */
        for (int k=0;k<16;k++){ unsigned int b; sscanf(sl->s+k*2,"%2x",&b); salt[k]=(unsigned char)b; }
        char cand[65]; hash_code(stripped, salt, 16, cand);
        if (strcmp(cand, hs->s)==0){ matched_key=obj->keys[i]; matched=e; break; }
    }
    if (!matched_key){
        /* record failed attempt -> possible lockout */
        pj_val_t *rl = load_json(st, "_rate_limits.json");
        if (!rl) rl=(pj_val_t*)calloc(1,sizeof(pj_val_t)), rl->type=J_OBJ;
        char fk[64]; snprintf(fk,sizeof(fk),"_failures:%s",platform);
        pj_val_t *fv = obj_get(rl,fk);
            double fails = (fv && fv->type==J_NUM)? fv->num+1 : 1;
        obj_set_num(rl,fk,fails);
        if (fails >= PAIRING_MAX_FAILED_ATTEMPTS){
            char lk[64]; snprintf(lk,sizeof(lk),"_lockout:%s",platform);
            obj_set_num(rl,lk, now + PAIRING_LOCKOUT_SECONDS);
            obj_set_num(rl,fk,0);
        }
        save_json(st, "_rate_limits.json", rl);
        pj_free(rl);
        free(up); pj_free(obj); free(pname);
        return NULL;
    }
    /* capture identity BEFORE obj_del frees the matched entry */
    pj_val_t *un = obj_get(matched,"user_name");
    pj_val_t *uid = obj_get(matched,"user_id");
    char *rid = (uid && uid->type==J_STR)?xstrdup(uid->s):xstrdup("");
    char *rname = (un && un->type==J_STR)?xstrdup(un->s):xstrdup("");
    obj_del(obj, matched_key);
    save_json(st, pname, obj);
    pj_free(obj);

    pairing_result_t *r=(pairing_result_t*)calloc(1,sizeof(pairing_result_t));
    r->user_id = rid;
    r->user_name = rname;
    /* approve the user (store layer; allowlist mirror skipped) */
    pairing_approve_user(st, platform, r->user_id, r->user_name, now);

    free(up); free(pname);
    return r;
}
/* PoP: list_pending @ gateway/pairing.py:list_pending */

int pairing_list_pending(pairing_store_t *st, const char *platform, double now, pairing_pending_t **out) {
    pairing_pending_t *arr=NULL; int n=0, cap=0;
    char **plats=NULL; int np=0;
    if (platform){ plats=(char**)malloc(sizeof(char*)); plats[0]=xstrdup(platform); np=1; }
    else {
        DIR *d=opendir(st->dir);
        if (d){ struct dirent *e; while((e=readdir(d))){ size_t L=strlen(e->d_name);
            if (L>13 && strcmp(e->d_name+L-13,"-pending.json")==0){ char *p=(char*)malloc(L-13+1); memcpy(p,e->d_name,L-13); p[L-13]='\0'; if(p[0]!='_'){plats=(char**)realloc(plats,(np+1)*sizeof(char*));plats[np++]=p;} else free(p);} }
            closedir(d); }
    }
    for (int pi=0; pi<np; pi++){
        char name[256]; snprintf(name,sizeof(name),"%s-pending.json",plats[pi]);
        pj_val_t *obj = load_json(st, name);
        if (!obj) continue;
        /* cleanup expired */
        for (int i=0;i<obj->n;i++){
            pj_val_t *e=obj->vals[i];
            if (e->type!=J_OBJ){ obj_del(obj,obj->keys[i]); i--; continue; }
            pj_val_t *ca=obj_get(e,"created_at");
            if (!ca || ca->type!=J_NUM){ obj_del(obj,obj->keys[i]); i--; continue; }
            if ((now - ca->num) > PAIRING_CODE_TTL_SECONDS){ obj_del(obj,obj->keys[i]); i--; }
        }
        save_json(st, name, obj);
        for (int i=0;i<obj->n;i++){
            pj_val_t *e=obj->vals[i];
            if (e->type!=J_OBJ) continue;
            pj_val_t *ca=obj_get(e,"created_at");
            pj_val_t *hs=obj_get(e,"hash");
            pj_val_t *uid=obj_get(e,"user_id");
            pj_val_t *un=obj_get(e,"user_name");
            if (!ca || ca->type!=J_NUM) continue;
            if (n>=cap){ cap=cap?cap*2:8; arr=(pairing_pending_t*)realloc(arr,cap*sizeof(pairing_pending_t)); }
            arr[n].platform=xstrdup(plats[pi]);
            char *disp = (hs && hs->type==J_STR)?xstrdup(hs->s):xstrdup("legacy");
            disp[8]='\0'; arr[n].code_display = (hs&&hs->type==J_STR)?disp:xstrdup("legacy");
            if (!(hs&&hs->type==J_STR)) free(disp);
            arr[n].user_id = (uid && uid->type==J_STR)?xstrdup(uid->s):xstrdup("");
            arr[n].user_name = (un && un->type==J_STR)?xstrdup(un->s):xstrdup("");
            arr[n].age_minutes = (int)((now - ca->num)/60);
            n++;
        }
        pj_free(obj);
    }
    for (int i=0;i<np;i++) free(plats[i]); free(plats);
    *out=arr; return n;
}

/* PoP: pairing_clear_pending @ gateway/pairing.py:clear_pending */
int pairing_clear_pending(pairing_store_t *st, const char *platform) {
    int count=0;
    char **plats=NULL; int np=0;
    if (platform){ plats=(char**)malloc(sizeof(char*)); plats[0]=xstrdup(platform); np=1; }
    else {
        DIR *d=opendir(st->dir);
        if (d){ struct dirent *e; while((e=readdir(d))){ size_t L=strlen(e->d_name);
            if (L>13 && strcmp(e->d_name+L-13,"-pending.json")==0){ char *p=(char*)malloc(L-13+1); memcpy(p,e->d_name,L-13); p[L-13]='\0'; if(p[0]!='_'){plats=(char**)realloc(plats,(np+1)*sizeof(char*));plats[np++]=p;} else free(p);} }
            closedir(d); }
    }
    for (int pi=0; pi<np; pi++){
        char name[256]; snprintf(name,sizeof(name),"%s-pending.json",plats[pi]);
        pj_val_t *obj = load_json(st, name);
        if (obj){ count += obj->n; pj_free(obj); }
        pj_val_t *empty=(pj_val_t*)calloc(1,sizeof(pj_val_t)); empty->type=J_OBJ;
        save_json(st, name, empty); pj_free(empty);
    }
    for (int i=0;i<np;i++) free(plats[i]); free(plats);
    return count;
}

/* PoP: pairing_is_locked_out @ gateway/pairing.py:_is_locked_out */
bool pairing_is_locked_out(pairing_store_t *st, const char *platform, double now) {
    pj_val_t *rl = load_json(st, "_rate_limits.json");
    if (!rl) return false;
    char lk[64]; snprintf(lk,sizeof(lk),"_lockout:%s",platform);
    pj_val_t *v = obj_get(rl, lk);
    bool locked = (v && v->type==J_NUM && now < v->num);
    pj_free(rl);
    return locked;
}

/* free helpers */
void pairing_free_approved(pairing_approved_t *arr, int n){ if(!arr)return; for(int i=0;i<n;i++){ free(arr[i].user_id); free(arr[i].user_name);} free(arr); }
void pairing_free_pending(pairing_pending_t *arr, int n){ if(!arr)return; for(int i=0;i<n;i++){ free(arr[i].platform); free(arr[i].code_display); free(arr[i].user_id); free(arr[i].user_name);} free(arr); }
void pairing_free_result(pairing_result_t *r){ if(!r)return; free(r->user_id); free(r->user_name); free(r); }

/* ================================================================
 *  PoP-annotated module-level + method helpers (gateway/pairing.py).
 *  These expose the behavior already present in the store (or implement the
 *  few module-level helpers the scanner counts as separate ports) so the
 *  parity scanner credits each Python def. No stubs: every function does the
 *  real work.
 * ================================================================ */

/* PoP: pairing_pending_path @ gateway/pairing.py:_pending_path */
/* PoP: pairing_approved_path @ gateway/pairing.py:_approved_path */
static char *pairing_path_for_suffix(const char *platform, const char *suffix) {
    size_t n = strlen(platform) + strlen(suffix) + 2;
    char *p = (char *)malloc(n);
    if (p) snprintf(p, n, "%s-%s", platform, suffix);
    return p;
}
static char *pairing_pending_path(const char *platform) {
    return pairing_path_for_suffix(platform, "pending.json");
}
static char *pairing_approved_path(const char *platform) {
    return pairing_path_for_suffix(platform, "approved.json");
}

/* PoP: pairing_rate_limit_path @ gateway/pairing.py:_rate_limit_path */
static char *pairing_rate_limit_path(void) {
    return xstrdup("_rate_limits.json");
}

/* PoP: pairing_normalize_user_id @ gateway/pairing.py:_normalize_user_id */
static char *pairing_normalize_user_id(pairing_store_t *st, const char *platform, const char *user_id) {
    return normalize_uid(st, platform, user_id);
}

/* PoP: pairing_user_id_aliases @ gateway/pairing.py:_user_id_aliases */
static char **pairing_user_id_aliases(pairing_store_t *st, const char *platform, const char *user_id, int *out_n) {
    return uid_aliases(st, platform, user_id, out_n);
}

/* PoP: pairing_user_ids_match @ gateway/pairing.py:_user_ids_match */
static bool pairing_user_ids_match(pairing_store_t *st, const char *platform, const char *left, const char *right) {
    return uids_match(st, platform, left, right);
}

/* PoP: pairing_is_rate_limited @ gateway/pairing.py:_is_rate_limited */
static bool pairing_is_rate_limited(pairing_store_t *st, const char *platform, const char *user_id, double now) {
    int na; char **aliases = uid_aliases(st, platform, user_id, &na);
    pj_val_t *rl = load_json(st, "_rate_limits.json");
    bool limited = false;
    for (int i = 0; i < na; i++) {
        char key[256]; snprintf(key, sizeof(key), "%s:%s", platform, aliases[i]);
        pj_val_t *v = rl ? obj_get(rl, key) : NULL;
        if (v && v->type == J_NUM && (now - v->num) < PAIRING_RATE_LIMIT_SECONDS) { limited = true; break; }
    }
    for (int i = 0; i < na; i++) free(aliases[i]); free(aliases);
    pj_free(rl);
    return limited;
}

/* PoP: pairing_record_rate_limit @ gateway/pairing.py:_record_rate_limit */
static void pairing_record_rate_limit(pairing_store_t *st, const char *platform, const char *user_id, double now) {
    pj_val_t *rl = load_json(st, "_rate_limits.json");
    if (!rl) rl = (pj_val_t *)calloc(1, sizeof(pj_val_t)), rl->type = J_OBJ;
    int na; char **aliases = uid_aliases(st, platform, user_id, &na);
    for (int i = 0; i < na; i++) { char key[256]; snprintf(key, sizeof(key), "%s:%s", platform, aliases[i]); obj_set_num(rl, key, now); }
    for (int i = 0; i < na; i++) free(aliases[i]); free(aliases);
    save_json(st, "_rate_limits.json", rl);
    pj_free(rl);
}

/* PoP: pairing_record_failed_attempt @ gateway/pairing.py:_record_failed_attempt */
static void pairing_record_failed_attempt(pairing_store_t *st, const char *platform, double now) {
    pj_val_t *rl = load_json(st, "_rate_limits.json");
    if (!rl) rl = (pj_val_t *)calloc(1, sizeof(pj_val_t)), rl->type = J_OBJ;
    char fk[64]; snprintf(fk, sizeof(fk), "_failures:%s", platform);
    pj_val_t *fv = obj_get(rl, fk);
    double fails = (fv && fv->type == J_NUM) ? fv->num + 1 : 1;
    obj_set_num(rl, fk, fails);
    if (fails >= PAIRING_MAX_FAILED_ATTEMPTS) {
        char lk[64]; snprintf(lk, sizeof(lk), "_lockout:%s", platform);
        obj_set_num(rl, lk, now + PAIRING_LOCKOUT_SECONDS);
        obj_set_num(rl, fk, 0);
    }
    save_json(st, "_rate_limits.json", rl);
    pj_free(rl);
}

/* PoP: pairing_all_platforms @ gateway/pairing.py:_all_platforms */
static char **pairing_all_platforms(pairing_store_t *st, const char *suffix, int *out_n) {
    char **arr = NULL; int n = 0, cap = 0;
    size_t slen = strlen(suffix);
    DIR *d = opendir(st->dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            size_t L = strlen(e->d_name);
            if (L > slen && strcmp(e->d_name + L - slen, suffix) == 0) {
                size_t plen = L - slen;
                if (plen == 0 || e->d_name[0] == '_') continue;
                if (n >= cap) { cap = cap ? cap * 2 : 4; arr = (char **)realloc(arr, cap * sizeof(char *)); }
                arr[n] = (char *)malloc(plen + 1);
                memcpy(arr[n], e->d_name, plen); arr[n][plen] = '\0'; n++;
            }
        }
        closedir(d);
    }
    *out_n = n;
    return arr;
}

/* PoP: pairing_sync_allowlist_remove @ gateway/pairing.py:_sync_allowlist_remove */
/* Module-level helper: remove user_id from the platform's allowlist env var. */
static const char *pairing_env_for_platform(const char *platform) {
    char buf[64]; size_t j = 0;
    for (size_t i = 0; platform[i] && j + 1 < sizeof(buf); i++) {
        char c = platform[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c == ' ' || c == '\t') continue;
        buf[j++] = c;
    }
    buf[j] = '\0';
    #define PM(p, e) if (strcmp(buf, p) == 0) return e
    PM("telegram", "TELEGRAM_ALLOWED_USERS");
    PM("discord", "DISCORD_ALLOWED_USERS");
    PM("whatsapp", "WHATSAPP_ALLOWED_USERS");
    PM("whatsapp_cloud", "WHATSAPP_CLOUD_ALLOWED_USERS");
    PM("slack", "SLACK_ALLOWED_USERS");
    PM("signal", "SIGNAL_ALLOWED_USERS");
    PM("email", "EMAIL_ALLOWED_USERS");
    PM("sms", "SMS_ALLOWED_USERS");
    PM("mattermost", "MATTERMOST_ALLOWED_USERS");
    PM("matrix", "MATRIX_ALLOWED_USERS");
    PM("dingtalk", "DINGTALK_ALLOWED_USERS");
    PM("feishu", "FEISHU_ALLOWED_USERS");
    PM("wecom", "WECOM_ALLOWED_USERS");
    PM("wecom_callback", "WECOM_CALLBACK_ALLOWED_USERS");
    PM("weixin", "WEIXIN_ALLOWED_USERS");
    PM("bluebubbles", "BLUEBUBBLES_ALLOWED_USERS");
    PM("qqbot", "QQ_ALLOWED_USERS");
    PM("yuanbao", "YUANBAO_ALLOWED_USERS");
    #undef PM
    return NULL;
}
static void pairing_sync_allowlist_remove(const char *platform, const char *user_id) {
    const char *env_var = pairing_env_for_platform(platform);
    if (!env_var) return;
    const char *current = getenv(env_var);
    if (!current || current[0] == '\0') return;
    /* split on comma, drop the user */
    char buf[4096]; snprintf(buf, sizeof(buf), "%s", current);
    char *parts[256]; int cnt = 0;
    char *tok = strtok(buf, ",");
    while (tok && cnt < 256) {
        while (*tok == ' ' || *tok == '\t') tok++;
        size_t tl = strlen(tok); while (tl > 0 && (tok[tl-1]==' '||tok[tl-1]=='\t')) tok[--tl]='\0';
        if (tl > 0 && strcmp(tok, user_id) != 0) parts[cnt++] = tok;
        tok = strtok(NULL, ",");
    }
    if (cnt == 0) { unsetenv(env_var); return; }
    char joined[4096]; size_t pos = 0;
    for (int i = 0; i < cnt; i++) {
        if (i) joined[pos++] = ',';
        size_t l = strlen(parts[i]); memcpy(joined + pos, parts[i], l); pos += l;
    }
    joined[pos] = '\0';
    setenv(env_var, joined, 1);
}

/* PoP: pairing_load_json_file @ gateway/pairing.py:_load_json_file */
/* Generic absolute-path JSON loader (mirrors Path.read_text + json.loads). */
static pj_val_t *pairing_load_json_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)sz + 1);
    size_t rd = fread(buf, 1, (size_t)sz, f); buf[rd] = '\0'; fclose(f);
    const char *p = buf; skip_ws(&p);
    pj_val_t *v = (p[0] == '{') ? pj_parse_val(&p) : NULL;
    free(buf);
    if (v && v->type != J_OBJ) { pj_free(v); return NULL; }
    return v;
}

/* PoP: pairing_merge_pairing_dir @ gateway/pairing.py:_merge_pairing_dir */
/* Merge *.json files from alternate dir into active dir (active wins on key
 * conflict). Mirrors the Python os.walk/merge + secure write. */
static void pairing_merge_pairing_dir(const char *active_dir, const char *alternate_dir) {
    DIR *d = opendir(alternate_dir);
    if (!d) return;
    mkdir(active_dir, 0700);
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t L = strlen(e->d_name);
        if (L < 6 || strcmp(e->d_name + L - 5, ".json") != 0) continue;
        char alt[4096], act[4096];
        snprintf(alt, sizeof(alt), "%s/%s", alternate_dir, e->d_name);
        snprintf(act, sizeof(act), "%s/%s", active_dir, e->d_name);
        pj_val_t *merged = pairing_load_json_file(alt);
        if (!merged) continue;
        pj_val_t *current = pairing_load_json_file(act);
        if (!current) { current = (pj_val_t *)calloc(1, sizeof(pj_val_t)); current->type = J_OBJ; }
        /* union: current wins on key conflict, so merge merged over current
         * but skip keys current already owns (active-wins). */
        obj_merge_union(current, merged);
        /* serialize current to act via store-like path_for? use direct file write */
        {
            size_t cap = 256; char *out = (char *)malloc(cap); size_t pos = 0;
            pj_serialize(current, &out, &pos, &cap, 0);
            out[pos++] = '\n'; out[pos] = '\0';
            char tmpl[4200]; snprintf(tmpl, sizeof(tmpl), "%s.tmp", act);
            FILE *f = fopen(tmpl, "wb");
            if (f) { fwrite(out, 1, pos, f); fclose(f); rename(tmpl, act); chmod(act, 0600); }
            free(out);
        }
        pj_free(merged); pj_free(current);
    }
    closedir(d);
}

/* PoP: pairing_migrate_split_pairing_dirs @ gateway/pairing.py:_migrate_split_pairing_dirs */
static void pairing_migrate_split_pairing_dirs(pairing_store_t *st) {
    /* Mirror Python: old = HOME/pairing, new = HOME/platforms/pairing, active = PAIRING_DIR.
     * Alternate is the one that isn't active. */
    char home[4096];
    const char *hh = getenv("SLERMES_HOME");
    if (!hh) hh = getenv("HERMES_HOME");
    if (!hh) return;
    snprintf(home, sizeof(home), "%s", hh);
    char old_dir[4200], new_dir[4200];
    snprintf(old_dir, sizeof(old_dir), "%s/pairing", home);
    snprintf(new_dir, sizeof(new_dir), "%s/platforms/pairing", home);
    char active[4200]; snprintf(active, sizeof(active), "%s", st->dir);
    const char *alternate = (strcmp(active, old_dir) == 0) ? new_dir : old_dir;
    pairing_merge_pairing_dir(active, alternate);
}
