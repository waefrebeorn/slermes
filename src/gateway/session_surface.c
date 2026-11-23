/*
 * PoP: w9_sessionsurf_b02f14_submit_surface_fields @ gateway/session_surface.py:submit_surface_fields
 * PoP: w9_sessionsurf_b02f14_admit_surface        @ gateway/session_surface.py:admit_surface
 * PoP: w9_sessionsurf_b02f14_restore_surface      @ gateway/session_surface.py:restore_surface
 *
 * A refusal is represented as {"__raise__":"AuthorityRefusal","reason":"invalid_params"}.
 */
#include "hermes_json.h"
#include "noc_w9_sessionsurf_b02f14.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOICE_CONTEXT_LIMIT 6000
static const char *SUBMIT_FIELDS[4]={"surface","voice_context","interrupted","voice_turn"};

static int truthy(const json_t *v){
    if(!v || v->type==JSON_NULL) return 0;
    if(v->type==JSON_BOOL) return v->bool_val?1:0;
    if(v->type==JSON_NUMBER) return v->num_val!=0.0;
    if(v->type==JSON_STRING) return v->str_val && v->str_val[0];
    if(v->type==JSON_ARRAY || v->type==JSON_OBJECT) return v->c.count>0;
    return 0;
}
static json_t *refusal(void){
    json_t *o=json_object();
    json_set(o,"__raise__",json_string("AuthorityRefusal"));
    json_set(o,"reason",json_string("invalid_params"));
    return o;
}
static int valid_surface(const json_t *v){
    return v && v->type==JSON_STRING &&
           (!strcmp(v->str_val,"hud") || !strcmp(v->str_val,"voice-live"));
}
static int field_allowed(const char *k){
    for(int i=0;i<4;i++) if(!strcmp(k,SUBMIT_FIELDS[i])) return 1;
    return 0;
}
/* first n unicode codepoints of s (UTF-8), malloc'd */
static char *utf8_prefix(const char *s, size_t n){
    size_t len=strlen(s), k=0, count=0;
    while(k<len && count<n){
        unsigned char c=(unsigned char)s[k];
        size_t step=1;
        if(c>=0xF0) step=4; else if(c>=0xE0) step=3; else if(c>=0xC0) step=2;
        if(k+step>len) step=1;
        k+=step; count++;
    }
    char *out=(char*)malloc(k+1); if(!out)return NULL;
    memcpy(out,s,k); out[k]=0; return out;
}
/* recursive JSON equality, object key order-insensitive */
static int jeq(const json_t *a, const json_t *b){
    if(a==b) return 1;
    if(!a||!b||a->type!=b->type) return 0;
    switch(a->type){
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val==b->bool_val;
        case JSON_NUMBER: return a->num_val==b->num_val;
        case JSON_STRING: return (!a->str_val&&!b->str_val)||(a->str_val&&b->str_val&&!strcmp(a->str_val,b->str_val));
        case JSON_ARRAY:
            if(a->c.count!=b->c.count) return 0;
            for(size_t i=0;i<a->c.count;i++) if(!jeq(json_get(a,i),json_get(b,i))) return 0;
            return 1;
        case JSON_OBJECT:{
            if(a->c.count!=b->c.count) return 0;
            for(size_t i=0;i<a->c.count;i++){
                const char *k=json_object_get_key_at(a,i);
                if(!k||!jeq(json_object_get_at(a,i),json_obj_get(b,k))) return 0;
            }
            return 1;
        }
    }
    return 0;
}

json_t *w9_sessionsurf_b02f14_submit_surface_fields(json_t *params){
    json_t *o=json_object();
    if(!params||params->type!=JSON_OBJECT) return o;
    for(int i=0;i<4;i++){
        json_t *v=json_obj_get(params,SUBMIT_FIELDS[i]);
        if(v) json_set(o,SUBMIT_FIELDS[i],json_copy(v));
    }
    return o;
}

json_t *w9_sessionsurf_b02f14_admit_surface(json_t *params){
    json_t *surface=params?json_obj_get(params,"surface"):NULL;
    json_t *context=params?json_obj_get(params,"voice_context"):NULL;
    json_t *interrupted=params?json_obj_get(params,"interrupted"):NULL;
    json_t *voice_turn=params?json_obj_get(params,"voice_turn"):NULL;
    int surface_present = surface && surface->type!=JSON_NULL;
    int context_present = context && context->type!=JSON_NULL;
    if(surface_present && !valid_surface(surface)) return refusal();
    if(context_present){
        int is_vl = surface_present && surface->type==JSON_STRING && !strcmp(surface->str_val,"voice-live");
        if(!is_vl || context->type!=JSON_STRING) return refusal();
    }
    if(interrupted && interrupted->type!=JSON_NULL && interrupted->type!=JSON_BOOL) return refusal();
    if(voice_turn && voice_turn->type!=JSON_NULL && voice_turn->type!=JSON_BOOL) return refusal();
    json_t *committed=json_object();
    if(truthy(surface)) json_set(committed,"surface",json_string(surface->str_val));
    if(truthy(context)){
        char *p=utf8_prefix(context->str_val,VOICE_CONTEXT_LIMIT);
        json_set(committed,"voice_context",json_string(p?p:""));
        free(p);
    }
    if(truthy(interrupted)) json_set(committed,"interrupted",json_bool(1));
    if(truthy(voice_turn)) json_set(committed,"voice_turn",json_bool(1));
    if(json_object_size(committed)==0){ json_free(committed); return json_object(); }
    json_t *o=json_object(); json_set(o,"surface_v1",committed); return o;
}

json_t *w9_sessionsurf_b02f14_restore_surface(json_t *committed){
    if(!committed || committed->type!=JSON_OBJECT || json_object_size(committed)==0) return refusal();
    for(size_t i=0;i<json_object_size(committed);i++){
        const char *k=json_object_get_key_at(committed,i);
        if(!k||!field_allowed(k)) return refusal();
    }
    json_t *admitted=w9_sessionsurf_b02f14_admit_surface(committed);
    if(admitted && json_obj_get(admitted,"__raise__")){ json_free(admitted); return refusal(); }
    json_t *v1=json_obj_get(admitted,"surface_v1");
    int ok = v1 && jeq(v1,committed);
    json_free(admitted);
    if(!ok) return refusal();
    return json_copy(committed);
}
