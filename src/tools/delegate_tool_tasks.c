/*
 * delegate_tool_tasks.c — C home for upstream Hermes tools/delegate_tool_tasks.py.
 * PoP: w12_delegateto_66d55c_normalize_task_images @ tools/delegate_tool_tasks.py:_normalize_task_images
 * PoP: w12_delegateto_66d55c_coerce_task_images @ tools/delegate_tool_tasks.py:_coerce_task_images
 * PoP: w12_delegateto_66d55c_validate_batch_tasks @ tools/delegate_tool_tasks.py:_validate_batch_tasks
 *
 * Pure validation/normalisation helpers from the delegate_task input layer.
 * No filesystem, registry or network access — inputs arrive as JSON.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_delegateto_66d55c.h"

#define MAX_TASK_IMAGES 8
#define MIN_BATCH_GOAL_LEN 10

static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
static int ws(unsigned char c){ return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f'; }
/* build a fresh 2-element JSON array (takes ownership of both args) */
static json_t *pack2(json_t *a, json_t *b){ json_t *r=json_array(); json_append(r,a); json_append(r,b); return r; }

static char *fmt(const char *f, ...) {
    va_list ap; va_start(ap, f);
    va_list ap2; va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, f, ap);
    va_end(ap);
    char *o = (char*)malloc((size_t)(n>0?n:0)+1);
    vsnprintf(o, (size_t)n+1, f, ap2);
    va_end(ap2);
    return o;
}

static char *strip_dup(const char *s){
    s = s ? s : "";
    size_t n = strlen(s);
    while(n && ws((unsigned char)*s)){ s++; n--; }
    while(n && ws((unsigned char)s[n-1])) n--;
    char *o = (char*)malloc(n+1); memcpy(o,s,n); o[n]=0; return o;
}

/* str(value) coercion for the scalar JSON types used by goal fields. */
static char *py_str_alloc(json_t *v){
    if(!v || v->type==JSON_NULL) return (char*)strdup("None");
    if(v->type==JSON_STRING) return (char*)strdup(v->str_val ? v->str_val : "");
    if(v->type==JSON_BOOL) return (char*)strdup(v->bool_val ? "True" : "False");
    if(v->type==JSON_NUMBER){
        double d=v->num_val;
        if(d==(double)(long long)d) return fmt("%lld",(long long)d);
        return fmt("%g",d);
    }
    char *s = json_dumps(v,0); return s ? s : (char*)strdup("");
}

/* Python repr() of a str (quote choice, backslash/quote/control escaping). */
static char *py_repr_alloc(const char *s){
    s = s ? s : "";
    int has_s = strchr(s,'\'')!=NULL, has_d = strchr(s,'"')!=NULL;
    char quote = (has_s && !has_d) ? '"' : '\'';
    size_t cap = strlen(s)*4 + 3;
    char *o = (char*)malloc(cap);
    size_t k=0;
    o[k++]=quote;
    for(const unsigned char *p=(const unsigned char*)s; *p; p++){
        unsigned char c=*p;
        if(c=='\\'){ o[k++]='\\'; o[k++]='\\'; }
        else if(c==(unsigned char)quote){ o[k++]='\\'; o[k++]=c; }
        else if(c=='\n'){ o[k++]='\\'; o[k++]='n'; }
        else if(c=='\r'){ o[k++]='\\'; o[k++]='r'; }
        else if(c=='\t'){ o[k++]='\\'; o[k++]='t'; }
        else if(c<0x20 || c==0x7f){ o[k++]='\\'; o[k++]='x';
            static const char hx[]="0123456789abcdef";
            o[k++]=hx[c>>4]; o[k++]=hx[c&15]; }
        else o[k++]=c;
    }
    o[k++]=quote; o[k]=0;
    return o;
}

/* " ".join(goal.lower().split()) */
static char *collapse_lower(const char *s){
    size_t n=strlen(s);
    char *o=(char*)malloc(n+2);
    size_t k=0; int pending=0;
    for(const unsigned char *p=(const unsigned char*)s; *p; p++){
        if(ws(*p)){ pending=1; continue; }
        if(pending && k>0) o[k++]=' ';
        pending=0;
        o[k++]=(char)tolower(*p);
    }
    o[k]=0; return o;
}

/* ^(todo|task\s*\d+)$ on an already-normalised (lower, collapsed) string. */
static int is_placeholder(const char *norm){
    if(!strcmp(norm,"todo")) return 1;
    if(!strncmp(norm,"task",4)){
        const char *p=norm+4;
        while(*p==' ') p++;
        if(!*p) return 0;
        const char *d=p;
        while(isdigit((unsigned char)*p)) p++;
        if(p>d && !*p) return 1;
    }
    return 0;
}

/* _TEMPLATE_MARKER_RE.search(goal): first <word[sep word]+> or {word[sep word]+}. */
static char *find_template_marker(const char *goal){
    size_t len=strlen(goal);
    for(size_t i=0;i<len;i++){
        char open=goal[i], close=0;
        if(open=='<') close='>'; else if(open=='{') close='}'; else continue;
        size_t j=i+1;
        if(j>=len || !isalpha((unsigned char)goal[j])) continue;
        j++;
        while(j<len && isalnum((unsigned char)goal[j])) j++;
        int groups=0;
        while(j<len && (goal[j]==' '||goal[j]=='_'||goal[j]=='-')){
            size_t sep=j; j++;
            if(j<len && isalnum((unsigned char)goal[j])){
                while(j<len && isalnum((unsigned char)goal[j])) j++;
                groups++;
            } else { j=sep; break; }
        }
        if(groups>=1 && j<len && goal[j]==close){
            char *m=(char*)malloc(j-i+2);
            memcpy(m,goal+i,j-i+1); m[j-i+1]=0;
            return m;
        }
    }
    return NULL;
}

json_t *w12_delegateto_66d55c_normalize_task_images(json_t *task, long long i){
    json_t *raw = (task && task->type==JSON_OBJECT) ? json_obj_get(task,"images") : NULL;
    if(!raw || raw->type==JSON_NULL) return pack2(json_null(), json_null());
    json_t *list = raw;
    int own_list = 0;
    if(raw->type==JSON_STRING){
        list = json_array();
        json_append(list, json_copy(raw));
        own_list = 1;
    }
    if(list->type != JSON_ARRAY){
        char *e = fmt("Task %lld 'images' must be an array of local file paths or http(s) URLs.", i);
        json_t *r = pack2(json_null(), json_string(e));
        free(e); if(own_list) json_free(list); return r;
    }
    json_t *cleaned = json_array();
    for(size_t k=0;k<json_array_size(list);k++){
        json_t *item = json_get(list,k);
        const char *sv = jstr(item);
        char *t = sv ? strip_dup(sv) : NULL;
        if(!sv || !t || !*t){
            free(t);
            char *e = fmt("Task %lld 'images' entries must be non-empty strings (local file paths or http(s) URLs).", i);
            json_t *r = pack2(json_null(), json_string(e));
            free(e); json_free(cleaned); if(own_list) json_free(list); return r;
        }
        json_append(cleaned, json_string(t));
        free(t);
    }
    if(own_list) json_free(list);
    if(json_array_size(cleaned) > MAX_TASK_IMAGES){
        char *e = fmt("Task %lld has %zu images; the per-task limit is %d. Trim to the images the child actually needs to see.",
                      i, json_array_size(cleaned), MAX_TASK_IMAGES);
        json_t *r = pack2(json_null(), json_string(e));
        free(e); json_free(cleaned); return r;
    }
    if(json_array_size(cleaned)==0){ json_free(cleaned); return pack2(json_null(), json_null()); }
    return pack2(cleaned, json_null());
}

json_t *w12_delegateto_66d55c_coerce_task_images(json_t *task_list, json_t *images){
    json_t *out = json_array();
    if(!task_list || task_list->type!=JSON_ARRAY) return pack2(out, json_null());
    for(size_t i=0;i<json_array_size(task_list);i++){
        json_t *task = json_get(task_list,i);
        json_t *raw = (task && task->type==JSON_OBJECT) ? json_obj_get(task,"images") : NULL;
        int use_top = (!raw || raw->type==JSON_NULL) && json_array_size(task_list)==1 && images;
        json_t *eff_task = task;
        if(use_top){
            eff_task = json_object();
            if(task && task->type==JSON_OBJECT)
                for(size_t q=0;q<json_object_size(task);q++){
                    const char *k=json_object_get_key_at(task,q);
                    if(k) json_set(eff_task,k,json_copy(json_object_get_at(task,q)));
                }
            json_set(eff_task,"images",json_copy(images));
        }
        json_t *pair = w12_delegateto_66d55c_normalize_task_images(eff_task,(long long)i);
        json_t *err = json_get(pair,1);
        if(err && err->type==JSON_STRING){
            json_t *r = pack2(json_array(), json_copy(err));
            json_free(pair); json_free(eff_task==task?NULL:eff_task); json_free(out); return r;
        }
        json_append(out, json_copy(json_get(pair,0)));
        json_free(pair);
        if(eff_task!=task) json_free(eff_task);
    }
    return pack2(out, json_null());
}

json_t *w12_delegateto_66d55c_validate_batch_tasks(json_t *task_list){
    if(!task_list || task_list->type!=JSON_ARRAY) return json_null();
    size_t n = json_array_size(task_list);
    for(size_t i=0;i<n;i++){
        json_t *task = json_get(task_list,i);
        json_t *gv = (task && task->type==JSON_OBJECT) ? json_obj_get(task,"goal") : NULL;
        char *gs = gv ? py_str_alloc(gv) : (char*)strdup("");
        char *goal = strip_dup(gs);
        char *norm = collapse_lower(goal);
        if(is_placeholder(norm)){
            char *r = py_repr_alloc(goal);
            char *e = fmt("Task %zu has a placeholder goal (%s). Replace it with a specific, self-contained description of what the subagent should accomplish.", i, r);
            free(r); free(norm); free(goal); free(gs);
            json_t *out = json_string(e); free(e); return out;
        }
        char *marker = find_template_marker(goal);
        if(marker){
            char *r = py_repr_alloc(marker);
            char *e = fmt("Task %zu goal contains an unexpanded template marker (%s). Substitute the real value before calling delegate_task \xe2\x80\x94 subagents cannot resolve placeholders.", i, r);
            free(r); free(marker); free(norm); free(goal); free(gs);
            json_t *out = json_string(e); free(e); return out;
        }
        if(strlen(goal) < MIN_BATCH_GOAL_LEN && n>=2){
            char *r = py_repr_alloc(goal);
            char *e = fmt("Task %zu goal is too short (%s). Write a specific, self-contained goal of at least %d characters so the subagent knows exactly what to do.", i, r, MIN_BATCH_GOAL_LEN);
            free(r); free(norm); free(goal); free(gs);
            json_t *out = json_string(e); free(e); return out;
        }
        free(norm); free(goal); free(gs);
    }
    return json_null();
}
