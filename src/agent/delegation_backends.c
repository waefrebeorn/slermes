/*
 * delegation_backends.c - C home for upstream Hermes agent/delegation_backends.py.
 * PoP: w11_delegation_74fc9b_route_keys @ agent/delegation_backends.py:_route_keys
 * PoP: w11_delegation_74fc9b_delegate_def @ agent/delegation_backends.py:_delegate_def
 * PoP: w11_delegation_74fc9b_pinned_backend @ agent/delegation_backends.py:pinned_backend
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_delegation_74fc9b.h"

static int w11db_ws(unsigned char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';
}
static char *w11db_strip(const char *s) {
    if (!s) return (char *)calloc(1,1);
    size_t n=strlen(s), a=0, b=n;
    while (a<b && w11db_ws((unsigned char)s[a])) a++;
    while (b>a && w11db_ws((unsigned char)s[b-1])) b--;
    char *o=(char *)malloc(b-a+1); memcpy(o,s+a,b-a); o[b-a]=0; return o;
}
/* _route_keys: tuple(dict.fromkeys(k.strip() for k in raw if isinstance(k,str) and k.strip())) */
json_t *w11_delegation_74fc9b_route_keys(json_t *provider) {
    json_t *out=json_array();
    json_t *raw = provider ? json_obj_get(provider,"route_keys") : NULL;
    if (!raw || raw->type != JSON_ARRAY) return out;
    for (size_t i=0;i<raw->c.count;i++) {
        json_t *it=json_get(raw,i);
        if (!it || it->type!=JSON_STRING) continue;
        char *t=w11db_strip(it->str_val);
        if (!*t) { free(t); continue; }
        int seen=0;
        for (size_t j=0;j<json_len(out);j++) {
            const char *e=json_string_value(json_get(out,j));
            if (e && strcmp(e,t)==0) { seen=1; break; }
        }
        if (!seen) json_append(out, json_string(t));
        free(t);
    }
    return out;
}
/* _delegate_def: first dict whose (function or {}).name == "delegate_task" */
json_t *w11_delegation_74fc9b_delegate_def(json_t *tools) {
    if (!tools || tools->type!=JSON_ARRAY) return json_null();
    for (size_t i=0;i<tools->c.count;i++) {
        json_t *t=json_get(tools,i);
        if (!t || t->type!=JSON_OBJECT) continue;
        json_t *fn=json_obj_get(t,"function");
        if (!fn || fn->type!=JSON_OBJECT) continue;  /* null/{} handled; non-dict strings avoided in fixtures */
        json_t *name=json_obj_get(fn,"name");
        if (name && name->type==JSON_STRING && strcmp(name->str_val,"delegate_task")==0)
            return json_copy(t);
    }
    return json_null();
}
/* pinned_backend: getattr(agent, "_delegation_backend", None) if str else "" */
char *w11_delegation_74fc9b_pinned_backend(json_t *agent) {
    json_t *p = agent ? json_obj_get(agent,"_delegation_backend") : NULL;
    if (p && p->type==JSON_STRING) return strdup(p->str_val);
    return strdup("");
}
