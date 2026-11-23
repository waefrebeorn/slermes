/*
 * PoP: w9_sessionresu_9c104c_record_unexecuted_failure @ gateway/session_results.py:record_unexecuted_failure
 * PoP: w9_sessionresu_9c104c_record_terminal_failure   @ gateway/session_results.py:record_terminal_failure
 *
 * The Python functions mutate the `execution_result` ContextVar dict in place
 * and return the input unchanged. Here `captured` is passed explicitly; each
 * export returns {"ret": <unchanged input>, "captured": <mutated dict|null>}.
 */
#include "hermes_json.h"
#include "noc_w9_sessionresu_9c104c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int has_key(const json_t *obj, const char *key){
    if(!obj || obj->type!=JSON_OBJECT) return 0;
    for(size_t i=0;i<json_object_size(obj);i++)
        if(json_object_get_key_at(obj,i) && !strcmp(json_object_get_key_at(obj,i),key)) return 1;
    return 0;
}
static int truthy(const json_t *v){
    if(!v || v->type==JSON_NULL) return 0;
    if(v->type==JSON_BOOL) return v->bool_val?1:0;
    if(v->type==JSON_NUMBER) return v->num_val!=0.0;
    if(v->type==JSON_STRING) return v->str_val && v->str_val[0];
    if(v->type==JSON_ARRAY || v->type==JSON_OBJECT) return v->c.count>0;
    return 0;
}
static char *py_str(const json_t *v){
    if(!v) return NULL;
    if(v->type==JSON_STRING) return v->str_val?strdup(v->str_val):strdup("");
    if(v->type==JSON_NULL) return strdup("None");
    if(v->type==JSON_BOOL) return strdup(v->bool_val?"True":"False");
    if(v->type==JSON_NUMBER){
        char b[64];
        if(v->num_val==(double)(long long)v->num_val)
            snprintf(b,sizeof(b),"%lld",(long long)v->num_val);
        else snprintf(b,sizeof(b),"%g",v->num_val);
        return strdup(b);
    }
    return json_dumps(v,0);
}
static json_t *result_obj(const char *errstr, const char *reason){
    json_t *r=json_object();
    json_set(r,"final_response",json_string(""));
    json_set(r,"messages",json_array());
    json_set(r,"failed",json_bool(1));
    json_set(r,"completed",json_bool(0));
    json_set(r,"error",json_string(errstr));
    if(reason && reason[0]){
        json_set(r,"failure_reason",json_string(reason));
        json_set(r,"failure_retryable",json_bool(0));
    }
    return r;
}

json_t *w9_sessionresu_9c104c_record_unexecuted_failure(json_t *reply,
        const char *reason, json_t *error, json_t *captured){
    json_t *capt=captured?json_copy(captured):NULL;
    if(capt && capt->type==JSON_OBJECT && !has_key(capt,"result")){
        const char *pick = truthy(error)?"error":(truthy(reply)?"reply":"default");
        char *es=NULL;
        if(!strcmp(pick,"error")) es=py_str(error);
        else if(!strcmp(pick,"reply")) es=py_str(reply);
        else es=strdup("The admitted turn failed.");
        if(!es) es=strdup("The admitted turn failed.");
        json_set(capt,"result",result_obj(es,reason));
        free(es);
    }
    json_t *out=json_object();
    json_set(out,"ret",reply?json_copy(reply):json_null());
    json_set(out,"captured",capt?capt:json_null());
    return out;
}

json_t *w9_sessionresu_9c104c_record_terminal_failure(json_t *result, json_t *captured){
    json_t *capt=captured?json_copy(captured):NULL;
    if(capt && capt->type==JSON_OBJECT)
        json_set(capt,"terminal_result",result?json_copy(result):json_null());
    json_t *out=json_object();
    json_set(out,"ret",result?json_copy(result):json_null());
    json_set(out,"captured",capt?capt:json_null());
    return out;
}
