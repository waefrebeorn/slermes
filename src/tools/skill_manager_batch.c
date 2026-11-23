/*
 * skill_manager_batch.c — C home for upstream Hermes tools/skill_manager_batch.py.
 * PoP: w12_skillmanag_069e36_misplaced_text_hint @ tools/skill_manager_batch.py:_misplaced_text_hint
 * PoP: w12_skillmanag_069e36_op_shape_error @ tools/skill_manager_batch.py:_op_shape_error
 *
 * Pure per-op argument-shape checks from the atomic skill_manage batch path.
 * The filesystem/network application layer stays in Python.
 */
#include "hermes_json.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_skillmanag_069e36.h"

static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
static int is_none(json_t *v){ return !v || v->type==JSON_NULL; }
/* Python truthiness for JSON scalars/containers. */
static int truthy(json_t *v){
    if(!v || v->type==JSON_NULL) return 0;
    if(v->type==JSON_BOOL) return v->bool_val ? 1 : 0;
    if(v->type==JSON_NUMBER) return v->num_val != 0.0;
    if(v->type==JSON_STRING) return v->str_val && v->str_val[0];
    if(v->type==JSON_ARRAY || v->type==JSON_OBJECT) return v->c.count > 0;
    return 0;
}
static char *cat2(const char *a,const char *b){
    size_t na=strlen(a),nb=strlen(b);
    char *o=(char*)malloc(na+nb+1);
    memcpy(o,a,na); memcpy(o+na,b,nb); o[na+nb]=0; return o;
}
static char *fmt(const char *f, ...) {
    va_list ap; va_start(ap,f); va_list ap2; va_copy(ap2,ap);
    int n=vsnprintf(NULL,0,f,ap); va_end(ap);
    char *o=(char*)malloc((size_t)(n>0?n:0)+1);
    vsnprintf(o,(size_t)n+1,f,ap2); va_end(ap2);
    return o;
}

/* owner keys, in Python dict insertion order, plus their sentence fragments */
static const char *const OWNER_KEYS[] = {"content","new_string","file_content"};
static const char *const OWNER_DESC[] = {
    "create (and a full-rewrite patch)",
    "a targeted patch (with old_string)",
    "write_file",
};
static const char *reads_owner(const char *action, const char *k){
    /* returns non-NULL when `action` reads text slot `k` */
    if(!strcmp(action,"create")||!strcmp(action,"edit"))
        return !strcmp(k,"content") ? "1" : NULL;
    if(!strcmp(action,"patch"))
        return (!strcmp(k,"content")||!strcmp(k,"new_string")) ? "1" : NULL;
    if(!strcmp(action,"write_file"))
        return !strcmp(k,"file_content") ? "1" : NULL;
    return NULL;
}
static const char *hint_destination(const char *action){
    if(!strcmp(action,"create")||!strcmp(action,"edit")) return "'content'";
    if(!strcmp(action,"patch")) return "old_string/new_string (targeted) or 'content' (full rewrite, last resort)";
    if(!strcmp(action,"write_file")) return "'file_content'";
    return NULL;
}

json_t *w12_skillmanag_069e36_misplaced_text_hint(json_t *action, json_t *args){
    const char *act = jstr(action) ? jstr(action) : "";
    const char *dest = hint_destination(act);
    json_t *empty = json_string("");
    if(!dest) return empty;
    /* stray = [k for k in _TEXT_SLOT_OWNER if k not in reads and args.get(k) is not None] */
    const char *strays[3]; size_t ns=0;
    for(size_t i=0;i<3;i++){
        const char *k=OWNER_KEYS[i];
        json_t *v = (args&&args->type==JSON_OBJECT) ? json_obj_get(args,k) : NULL;
        if(!reads_owner(act,k) && !is_none(v)) strays[ns++]=k;
    }
    if(ns==0) return empty;
    char *carried=(char*)strdup("");
    for(size_t i=0;i<ns;i++){
        int idx=0;
        for(size_t d=0;d<3;d++) if(!strcmp(strays[i],OWNER_KEYS[d])) idx=(int)d;
        char *piece=fmt("'%s' (that key is for %s)",strays[i],OWNER_DESC[idx]);
        char *tmp;
        if(i==0) tmp=(char*)strdup(piece);
        else { tmp=cat2(carried," and "); char *t2=cat2(tmp,piece); free(tmp); tmp=t2; }
        free(piece); free(carried); carried=tmp;
    }
    char *hint=fmt(" Note: this op carries %s \xe2\x80\x94 move that text to %s.",carried,dest);
    free(carried);
    json_t *out=json_string(hint); free(hint);
    return out;
}

json_t *w12_skillmanag_069e36_op_shape_error(json_t *action, json_t *args){
    const char *act = jstr(action) ? jstr(action) : "";
    /* required-arg checks (in insertion order) */
    if(!strcmp(act,"create")){
        json_t *v=json_obj_get(args,"content");
        if(!truthy(v)){
            json_t *h=w12_skillmanag_069e36_misplaced_text_hint(action,args);
            char *m=cat2("content is required for 'create'. Provide the full SKILL.md text (frontmatter + body).", jstr(h)?jstr(h):"");
            json_free(h); json_t *o=json_string(m); free(m); return o;
        }
    } else if(!strcmp(act,"edit")){
        json_t *v=json_obj_get(args,"content");
        if(!truthy(v)){
            json_t *h=w12_skillmanag_069e36_misplaced_text_hint(action,args);
            char *m=cat2("content is required for a full rewrite. Provide the full updated SKILL.md text.", jstr(h)?jstr(h):"");
            json_free(h); json_t *o=json_string(m); free(m); return o;
        }
    } else if(!strcmp(act,"write_file")){
        json_t *fp=json_obj_get(args,"file_path");
        if(!truthy(fp)){
            json_t *h=w12_skillmanag_069e36_misplaced_text_hint(action,args);
            char *m=cat2("file_path is required for 'write_file'. Example: 'references/api-guide.md'", jstr(h)?jstr(h):"");
            json_free(h); json_t *o=json_string(m); free(m); return o;
        }
        json_t *fc=json_obj_get(args,"file_content");
        if(is_none(fc)){
            json_t *h=w12_skillmanag_069e36_misplaced_text_hint(action,args);
            char *m=cat2("file_content is required for 'write_file'.", jstr(h)?jstr(h):"");
            json_free(h); json_t *o=json_string(m); free(m); return o;
        }
    } else if(!strcmp(act,"remove_file")){
        json_t *fp=json_obj_get(args,"file_path");
        if(!truthy(fp)){
            json_t *h=w12_skillmanag_069e36_misplaced_text_hint(action,args);
            char *m=cat2("file_path is required for 'remove_file'.", jstr(h)?jstr(h):"");
            json_free(h); json_t *o=json_string(m); free(m); return o;
        }
    }
    if(!strcmp(act,"patch")){
        json_t *content_v=json_obj_get(args,"content");
        json_t *old_v=json_obj_get(args,"old_string");
        json_t *new_v=json_obj_get(args,"new_string");
        int c=truthy(content_v), o=truthy(old_v), n_nnone=!is_none(new_v);
        if(c && (o || n_nnone))
            return json_string("Pass EITHER content (full SKILL.md rewrite) OR old_string/new_string (targeted replacement), not both.");
        if(!o && !c){
            json_t *h=w12_skillmanag_069e36_misplaced_text_hint(action,args);
            char *m=cat2("old_string is required for 'patch' and must be the EXACT text currently in the file. Read the target file first (read_file on the skill's SKILL.md, or the file named by file_path) and copy the snippet verbatim, then retry 'patch'. Do NOT fall back to action='write_file' \xe2\x80\x94 that rewrites the entire file and destroys unrelated content.", jstr(h)?jstr(h):"");
            json_free(h); json_t *out=json_string(m); free(m); return out;
        }
        if(!c && is_none(new_v))
            return json_string("new_string is required for 'patch'. Use an empty string to delete matched text.");
    }
    return json_null();
}
