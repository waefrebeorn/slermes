/*
 * acp_gateway_server_flow.c — FLOW C11 port of 20 UNCLAIMED defs of
 * acp_adapter/gateway_server.py (the admission -> attach -> ack ->
 * clarify/permission -> answer lifecycle and its decision helpers).
 *
 * Loaded via #include by tests/t_port_acp_gateway_server_flow.c. Every function
 * is static agsf_*; the harness dispatches by fixture "func" name. For instance
 * methods argv[0] is the self-state object and the remaining argv entries are
 * the call's real arguments (module/static funcs take their args in argv).
 *
 * Collaborators crossing the C/Python boundary are UNIVERSAL SEAMS: one scripted
 * queue per seam name (first result per call, last repeats); an absent/exhausted
 * script yields a RuntimeError raise spec recorded as the event ret before it
 * propagates. Raise specs are {"__raise__":kind,"message":...}. `except` sites
 * walk the FULL builtin exception hierarchy (AGSF_EXC) so BaseException-only
 * kinds (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError) pass through
 * `except Exception` unchanged — real BaseException passthrough. Fail-closed:
 * unmodelled/missing state yields the exact Python raise spec, never a guess.
 *
 * Representation contract (shared with the oracle):
 * - None == JSON null; identity gates are exact (`is True` is JSON_BOOL true).
 * - tuples/fences/frames travel as JSON arrays; set-valued state as arrays.
 * - logger events project as {"msg","args":[...],"exc_info":bool?}.
 * - all rpc (client/gateway) calls share the single "rpc" seam.
 * - _permissions keys are the "|"-joined (session,prompt,generation) triple.
 *
 * PoP annotations: each agsf_<name> is annotated with its defining Python def.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>

/* ---------------- dynamic string builder ---------------- */
typedef struct { char *p; size_t n, cap; } agsf_sb;
static void agsf_sb_init(agsf_sb *b){ b->cap=128; b->n=0; b->p=(char*)malloc(b->cap); if(b->p)b->p[0]=0; }
static void agsf_sb_grow(agsf_sb *b, size_t need){
    if(!b->p){ b->cap=128; b->n=0; b->p=(char*)malloc(b->cap); }
    if(b->n+need+1>b->cap){ while(b->n+need+1>b->cap) b->cap*=2; b->p=(char*)realloc(b->p,b->cap); }
}
static void agsf_sb_addn(agsf_sb*b,const char*s,size_t n){ if(!s||!n)return; agsf_sb_grow(b,n); memcpy(b->p+b->n,s,n); b->n+=n; b->p[b->n]=0; }
static void agsf_sb_add(agsf_sb*b,const char*s){ if(s) agsf_sb_addn(b,s,strlen(s)); }
static void agsf_sb_addf(agsf_sb*b,const char*f,...){ va_list ap; va_start(ap,f); int n=vsnprintf(NULL,0,f,ap); va_end(ap); if(n<0)n=0; char*t=(char*)malloc((size_t)n+1); va_start(ap,f); vsnprintf(t,(size_t)n+1,f,ap); va_end(ap); agsf_sb_addn(b,t,(size_t)n); free(t); }
static void agsf_sb_ch(agsf_sb*b,char c){ agsf_sb_grow(b,1); b->p[b->n++]=c; b->p[b->n]=0; }
static char *agsf_sb_take(agsf_sb*b){ char*r=b->p?b->p:(char*)""; b->p=NULL; b->n=b->cap=0; return r; }
static char *agsf_dup(const char*s){ return strdup(s?s:""); }
static char *agsf_fmts(const char*fmt,...){ va_list ap; va_start(ap,fmt); int n=vsnprintf(NULL,0,fmt,ap); va_end(ap); if(n<0)n=0; char*b=(char*)malloc((size_t)n+1); va_start(ap,fmt); vsnprintf(b,(size_t)n+1,fmt,ap); va_end(ap); return b; }

/* ---------------- Python value semantics over json_t ---------------- */
static int agsf_truth(json_t*v){
    if(!v) return 0;
    switch(v->type){
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val?1:0;
        case JSON_NUMBER: return v->num_val!=0.0;
        case JSON_STRING: return v->str_val&&v->str_val[0]?1:0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count>0?1:0;
    }
    return 0;
}
static int agsf_is_null(json_t*v){ return !v||v->type==JSON_NULL; }
static int agsf_is_str(json_t*v){ return v&&v->type==JSON_STRING; }
static int agsf_is_arr(json_t*v){ return v&&v->type==JSON_ARRAY; }
static int agsf_is_obj(json_t*v){ return v&&v->type==JSON_OBJECT; }
static int agsf_is_true(json_t*v){ return v&&v->type==JSON_BOOL&&v->bool_val; } /* `is True` */
static int agsf_isspace(unsigned char c){ return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f'||c==0x1c||c==0x1d||c==0x1e||c==0x1f; }
static json_t *agsf_get(json_t*o,const char*k){ return agsf_is_obj(o)?json_obj_get(o,k):NULL; }
static const char *agsf_getstr(json_t*o,const char*k,const char*d){ json_t*v=agsf_get(o,k); return (v&&v->type==JSON_STRING)?v->str_val:d; }
static const char *agsf_sarg(json_t*v,const char*d){ return (v&&v->type==JSON_STRING)?v->str_val:d; }
static double agsf_num(json_t*v){ if(!v)return 0.0; if(v->type==JSON_NUMBER)return v->num_val; if(v->type==JSON_BOOL)return v->bool_val?1.0:0.0; return 0.0; }
static char *agsf_strip(const char*s){
    if(!s) return agsf_dup("");
    size_t a=0,b=strlen(s);
    while(a<b&&agsf_isspace((unsigned char)s[a]))a++;
    while(b>a&&agsf_isspace((unsigned char)s[b-1]))b--;
    char*r=(char*)malloc(b-a+1); memcpy(r,s+a,b-a); r[b-a]=0; return r;
}
static char *agsf_lstrip(const char*s){ if(!s)return agsf_dup(""); size_t i=0; while(s[i]&&agsf_isspace((unsigned char)s[i]))i++; return agsf_dup(s+i); }
static char *agsf_pystr(json_t*v){
    if(!v) return agsf_dup("None");
    switch(v->type){
        case JSON_NULL: return agsf_dup("None");
        case JSON_BOOL: return agsf_dup(v->bool_val?"True":"False");
        case JSON_NUMBER: { double d=v->num_val; if(d==floor(d)&&fabs(d)<1e15) return agsf_fmts("%.0f",d); return agsf_fmts("%.15g",d); }
        case JSON_STRING: return agsf_dup(v->str_val?v->str_val:"");
        default: return agsf_dup("");
    }
}
/* Python == deep equality for the JSON domain used by fixtures. */
static int agsf_json_eq(json_t*a,json_t*b){
    if(a==b) return 1;
    if(!a||!b) return 0;
    if(a->type!=b->type){ /* int 1 == float 1 == True; bool/int/number unified */ return 0; }
    switch(a->type){
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val==b->bool_val;
        case JSON_NUMBER: return a->num_val==b->num_val;
        case JSON_STRING: return strcmp(a->str_val?a->str_val:"",b->str_val?b->str_val:"")==0;
        case JSON_ARRAY: {
            if(a->c.count!=b->c.count) return 0;
            for(size_t i=0;i<a->c.count;i++) if(!agsf_json_eq(a->c.items[i],b->c.items[i])) return 0;
            return 1;
        }
        case JSON_OBJECT: {
            if(a->c.count!=b->c.count) return 0;
            for(size_t i=0;i<a->c.count;i++){
                json_t*vv=json_obj_get(b,a->c.keys[i]);
                if(!vv||!agsf_json_eq(a->c.items[i],vv)) return 0;
            }
            return 1;
        }
    }
    return 0;
}
static int agsf_in_strs(json_t*arr,const char*s){
    if(!agsf_is_arr(arr)) return 0;
    for(size_t i=0;i<json_len(arr);i++){ json_t*v=json_get(arr,i); if(agsf_is_str(v)&&strcmp(v->str_val,s)==0) return 1; }
    return 0;
}

/* ---------------- full builtin exception hierarchy ---------------- */
typedef struct { const char *n, *p; } agsf_exc_t;
static const agsf_exc_t AGSF_EXC[] = {
    {"ArithmeticError","Exception"},{"AssertionError","Exception"},{"AttributeError","Exception"},
    {"BaseException","object"},{"BaseExceptionGroup","BaseException"},{"BlockingIOError","OSError"},
    {"BrokenPipeError","ConnectionError"},{"BufferError","Exception"},{"BytesWarning","Warning"},
    {"ChildProcessError","OSError"},{"ConnectionAbortedError","ConnectionError"},{"ConnectionError","OSError"},
    {"ConnectionRefusedError","ConnectionError"},{"ConnectionResetError","ConnectionError"},
    {"DeprecationWarning","Warning"},{"EOFError","Exception"},{"EncodingWarning","Warning"},
    {"EnvironmentError","Exception"},{"Exception","BaseException"},{"ExceptionGroup","BaseExceptionGroup"},
    {"FileExistsError","OSError"},{"FileNotFoundError","OSError"},{"FloatingPointError","ArithmeticError"},
    {"FutureWarning","Warning"},{"GeneratorExit","BaseException"},{"IOError","Exception"},
    {"ImportError","Exception"},{"ImportWarning","Warning"},{"IndentationError","SyntaxError"},
    {"IndexError","LookupError"},{"InterruptedError","OSError"},{"IsADirectoryError","OSError"},
    {"KeyError","LookupError"},{"KeyboardInterrupt","BaseException"},{"LookupError","Exception"},
    {"MemoryError","Exception"},{"ModuleNotFoundError","ImportError"},{"NameError","Exception"},
    {"NotADirectoryError","OSError"},{"NotImplementedError","RuntimeError"},{"OSError","Exception"},
    {"OverflowError","ArithmeticError"},{"PendingDeprecationWarning","Warning"},{"PermissionError","OSError"},
    {"ProcessLookupError","OSError"},{"PythonFinalizationError","RuntimeError"},{"RecursionError","RuntimeError"},
    {"ReferenceError","Exception"},{"ResourceWarning","Warning"},{"RuntimeError","Exception"},
    {"RuntimeWarning","Warning"},{"StopAsyncIteration","Exception"},{"StopIteration","Exception"},
    {"SyntaxError","Exception"},{"SyntaxWarning","Warning"},{"SystemError","Exception"},
    {"SystemExit","BaseException"},{"TabError","IndentationError"},{"TimeoutError","OSError"},
    {"TypeError","Exception"},{"UnboundLocalError","NameError"},{"UnicodeDecodeError","UnicodeError"},
    {"UnicodeEncodeError","UnicodeError"},{"UnicodeError","ValueError"},{"UnicodeTranslateError","UnicodeError"},
    {"UnicodeWarning","Warning"},{"UserWarning","Warning"},{"ValueError","Exception"},{"Warning","Exception"},
    {"WindowsError","Exception"},{"ZeroDivisionError","ArithmeticError"},{"_IncompleteInputError","SyntaxError"},
    {"CancelledError","BaseException"},
    {"GatewayClientError","Exception"},{"GatewayRPCError","GatewayClientError"},{"CommandRefused","Exception"},
    {NULL,NULL}
};
static const char *agsf_exc_parent(const char*n){ for(int i=0;AGSF_EXC[i].n;i++) if(strcmp(AGSF_EXC[i].n,n)==0) return AGSF_EXC[i].p; return "Exception"; }
static int agsf_exc_sub(const char*n,const char*target){ int g=0; const char*c=n; while(c&&*c&&g++<40){ if(strcmp(c,"object")==0||strcmp(c,"type")==0) return 0; if(strcmp(c,target)==0) return 1; c=agsf_exc_parent(c); } return 0; }
static int agsf_is_raise(json_t*v){ return v&&v->type==JSON_OBJECT&&json_obj_get(v,"__raise__")!=NULL; }
static int agsf_caught(json_t*spec,const char*target){ if(!spec)return 0; return agsf_exc_sub(agsf_getstr(spec,"__raise__","Exception"),target); }
static json_t *agsf_mkraise(const char*kind,const char*msg){ json_t*o=json_object(); json_set(o,"__raise__",json_string(kind)); json_set(o,"message",json_string(msg?msg:"")); return o; }
static json_t *agsf_keyerr(const char*key){ char*q=agsf_fmts("'%s'",key); json_t*r=agsf_mkraise("KeyError",q); free(q); return r; }

/* ---------------- universal seam dispatcher ---------------- */
typedef struct { json_t *seams; json_t *cursors; json_t *trace; } agsf_io_t;
static json_t *agsf_seam(agsf_io_t *io,const char *name,json_t *args){
    json_t *ret=NULL; int have=0;
    const json_t *sc=io->seams?json_obj_get(io->seams,name):NULL;
    if(sc){
        if(sc->type==JSON_ARRAY){
            double cur=json_get_num(io->cursors,name,0.0); size_t i=(size_t)cur;
            if(i<json_len(sc)){ ret=json_copy(json_get((json_t*)sc,i)); json_set(io->cursors,name,json_number(cur+1.0)); have=1; }
        } else { ret=json_copy((json_t*)sc); have=1; }
    }
    if(!have){ char*m=agsf_fmts("no script for %s",name); ret=agsf_mkraise("RuntimeError",m); free(m); }
    json_t *ev=json_object();
    json_set(ev,"ev",json_string(name));
    json_set(ev,"args",args?args:json_object());
    json_set(ev,"ret",json_copy(ret));
    json_append(io->trace,ev);
    return ret;
}
static json_t *agsf_A1(const char*k1,json_t*v1){ json_t*o=json_object(); json_set(o,k1,v1?v1:json_null()); return o; }
static json_t *agsf_A2(const char*k1,json_t*v1,const char*k2,json_t*v2){ json_t*o=json_object(); json_set(o,k1,v1?v1:json_null()); json_set(o,k2,v2?v2:json_null()); return o; }
static json_t *agsf_A3(const char*k1,json_t*v1,const char*k2,json_t*v2,const char*k3,json_t*v3){ json_t*o=json_object(); json_set(o,k1,v1?v1:json_null()); json_set(o,k2,v2?v2:json_null()); json_set(o,k3,v3?v3:json_null()); return o; }
static json_t *agsf_A4(const char*k1,json_t*v1,const char*k2,json_t*v2,const char*k3,json_t*v3,const char*k4,json_t*v4){ json_t*o=json_object(); json_set(o,k1,v1?v1:json_null()); json_set(o,k2,v2?v2:json_null()); json_set(o,k3,v3?v3:json_null()); json_set(o,k4,v4?v4:json_null()); return o; }
static json_t *agsf_A5(const char*k1,json_t*v1,const char*k2,json_t*v2,const char*k3,json_t*v3,const char*k4,json_t*v4,const char*k5,json_t*v5){ json_t*o=json_object(); json_set(o,k1,v1?v1:json_null()); json_set(o,k2,v2?v2:json_null()); json_set(o,k3,v3?v3:json_null()); json_set(o,k4,v4?v4:json_null()); json_set(o,k5,v5?v5:json_null()); return o; }
/* logger.<level> seam: {"msg", "args":[...], "exc_info":bool?} */
static json_t *agsf_log(agsf_io_t*io,const char*name,const char*msg,json_t*args_arr,int exc_info){
    json_t*a=json_object(); json_set(a,"msg",json_string(msg)); json_set(a,"args",args_arr?args_arr:json_array());
    if(exc_info) json_set(a,"exc_info",json_bool(true));
    return agsf_seam(io,name,a);
}
/* comma-join a JSON string array; "none" when empty */
static char *agsf_join_strs(json_t*arr,const char*empty){
    agsf_sb b; agsf_sb_init(&b);
    if(agsf_is_arr(arr)) for(size_t i=0;i<json_len(arr);i++){ if(i)agsf_sb_add(&b,", "); char*s=agsf_pystr(json_get(arr,i)); agsf_sb_add(&b,s); free(s); }
    if(!b.n){ agsf_sb_add(&b,empty?empty:""); }
    return agsf_sb_take(&b);
}

/* ==================== 1 _slash_command ==================== */
/* PoP: agsf__slash_command @ acp_adapter/gateway_server.py:_slash_command */
static json_t *agsf__slash_command(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw;
    char *stripped=agsf_lstrip(agsf_sarg(json_get(argv,0),""));
    if(stripped[0]!='/'){ free(stripped); return json_null(); }
    const char*p=stripped+1; size_t tl=0; while(p[tl]&&!agsf_isspace((unsigned char)p[tl]))tl++;
    char*name=(char*)malloc(tl+1); memcpy(name,p,tl); name[tl]=0; free(stripped);
    if(tl==0||strchr(name,'/')){ free(name); return json_null(); }
    json_t*r=agsf_seam(io,"resolve_command",agsf_A1("name",json_string(name)));
    if(agsf_is_raise(r)){ free(name); return r; }
    if(agsf_is_obj(r)){ json_t*n=json_obj_get(r,"name"); if(n){ json_t*o=json_copy(n); free(name); return o; } }
    json_t*rr=agsf_seam(io,"ROUTES",agsf_A1("name",json_string(name)));
    int found=agsf_in_strs(rr,name); free(name);
    if(agsf_is_raise(rr)) return rr;
    return found?json_string(name):json_null();
}

/* ==================== 2 _tools_changed_notice ==================== */
/* PoP: agsf__tools_changed_notice @ acp_adapter/gateway_server.py:_tools_changed_notice */
static json_t *agsf__tools_changed_notice(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)io;(void)kw;
    json_t*payload=json_get(argv,0);
    json_t*srv=agsf_get(payload,"mcp_servers");
    if(!agsf_truth(srv)) srv=NULL;
    char*joined=agsf_join_strs(srv,"none");
    char*out=agsf_fmts("Editor MCP tools changed (servers: %s); the new tool list applies from the next turn.\n",joined);
    free(joined); json_t*o=json_string(out); free(out); return o;
}

/* ==================== 3 _bypass_route ==================== */
/* PoP: agsf__bypass_route @ acp_adapter/gateway_server.py:_bypass_route */
static json_t *agsf__bypass_route(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)argv;(void)kw;
    json_t*m=agsf_seam(io,"os.environ.get",agsf_A2("key",json_string("HERMES_MODEL"),"default",json_null()));
    if(agsf_is_raise(m)) return m;
    if(!agsf_truth(m)){ m=agsf_seam(io,"os.environ.get",agsf_A2("key",json_string("HERMES_INFERENCE_MODEL"),"default",json_null())); if(agsf_is_raise(m)) return m; }
    char*model=agsf_strip(agsf_truth(m)?agsf_pystr(m):"");
    if(!model[0]){ free(model); return agsf_mkraise("GatewayClientError","--safe-mode / --ignore-user-config read no profile default model: launch with one, e.g. hermes -m <model> --provider <provider> --safe-mode acp"); }
    json_t*p=agsf_seam(io,"os.environ.get",agsf_A2("key",json_string("HERMES_INFERENCE_PROVIDER"),"default",json_string("")));
    if(agsf_is_raise(p)){ free(model); return p; }
    char*provider=agsf_strip(agsf_truth(p)?agsf_pystr(p):"");
    if(!provider[0]){ free(model); free(provider); return agsf_mkraise("GatewayClientError","--safe-mode / --ignore-user-config read no profile provider: launch with --provider too, e.g. hermes -m <model> --provider <provider> --safe-mode acp"); }
    json_t*o=json_object(); json_set(o,"model",json_string(model)); json_set(o,"provider",json_string(provider));
    free(model); free(provider); return o;
}

/* ==================== 4 _editor_id (helper) ==================== */
static json_t *agsf_editor_id_body(json_t*st,json_t*sid){
    json_t*aliases=agsf_get(st,"_aliases");
    /* Python: next((held for held,sid in self._aliases.items() if sid==session_id), session_id).
       _aliases is a JSON OBJECT: iterate c.items/c.keys, NOT json_get/json_len (array-only). */
    if(agsf_is_obj(aliases)) for(size_t i=0;i<aliases->c.count;i++) if(agsf_json_eq(aliases->c.items[i],sid)) return json_string(aliases->c.keys[i]?aliases->c.keys[i]:"");
    return json_copy(sid);
}
/* PoP: agsf__editor_id @ acp_adapter/gateway_server.py:_editor_id */
static json_t *agsf__editor_id(agsf_io_t *io,const json_t *argv,const json_t *kw){ (void)io;(void)kw; return agsf_editor_id_body(json_get(argv,0),json_get(argv,1)); }

/* ==================== 5 _moved ==================== */
/* PoP: agsf__moved @ acp_adapter/gateway_server.py:_moved */
static json_t *agsf__moved(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*info=json_get(argv,0); json_t*cwd=json_get(argv,1);
    if(json_obj_get(info,"cwd")==NULL) return json_bool(false);
    json_t*r1=agsf_seam(io,"_normalize_cwd_for_compare",agsf_A1("cwd",json_copy(agsf_get(info,"cwd"))));
    if(agsf_is_raise(r1)) return r1;
    json_t*t=agsf_seam(io,"_translate_acp_cwd",agsf_A1("cwd",json_copy(cwd)));
    if(agsf_is_raise(t)) return t;
    json_t*r2=agsf_seam(io,"_normalize_cwd_for_compare",agsf_A1("cwd",t));
    if(agsf_is_raise(r2)) return r2;
    return json_bool(!agsf_json_eq(r1,r2)?true:false);
}

/* ==================== 6 _transport_closed ==================== */
/* PoP: agsf__transport_closed @ acp_adapter/gateway_server.py:_transport_closed */
static json_t *agsf__transport_closed(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*gw=agsf_get(st,"_gateway");
    if(!agsf_truth(gw)) return json_bool(false);
    json_t*r=agsf_seam(io,"gateway.reader",json_object());
    if(agsf_is_raise(r)) return r;
    if(agsf_is_obj(r)&&agsf_truth(agsf_get(r,"is_future"))) return json_bool(agsf_truth(agsf_get(r,"done"))?true:false);
    return json_bool(false);
}

/* ==================== 7 _held_session ==================== */
/* PoP: agsf__held_session @ acp_adapter/gateway_server.py:_held_session */
static json_t *agsf__held_session(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)io;(void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1);
    json_t*effective=json_copy(sid);
    json_t*aliases=agsf_get(st,"_aliases");
    if(agsf_is_obj(aliases)){ const char*k=agsf_sarg(sid,""); json_t*v=json_obj_get(aliases,k); if(v) effective=json_copy(v); }
    json_t*snaps=agsf_get(st,"_snapshots");
    const char*eks=agsf_sarg(effective,"");
    if(!agsf_is_obj(snaps)||json_obj_get(snaps,eks)==NULL) return agsf_mkraise("GatewayClientError","not_found");
    return effective;
}

/* ==================== 8 _has_unknown (helper) ==================== */
static json_t *agsf_has_unknown_body(json_t*st,json_t*sid){
    json_t*snaps=agsf_get(st,"_snapshots");
    if(!agsf_is_obj(snaps)) return agsf_mkraise("AttributeError","'NoneType' object has no attribute 'get'");
    const char*k=agsf_sarg(sid,""); json_t*snap=json_obj_get(snaps,k);
    if(!snap) return agsf_keyerr(k);
    json_t*pending=agsf_get(snap,"pending");
    if(agsf_is_arr(pending)) for(size_t i=0;i<json_len(pending);i++){ json_t*row=json_get(pending,i); if(agsf_is_obj(row)&&strcmp(agsf_getstr(row,"status",""),"unknown")==0) return json_bool(true); }
    return json_bool(false);
}
/* PoP: agsf__has_unknown @ acp_adapter/gateway_server.py:_has_unknown */
static json_t *agsf__has_unknown(agsf_io_t *io,const json_t *argv,const json_t *kw){ (void)io;(void)kw; return agsf_has_unknown_body(json_get(argv,0),json_get(argv,1)); }

/* ==================== 9 _pending_event_is_new ==================== */
/* PoP: agsf__pending_event_is_new @ acp_adapter/gateway_server.py:_pending_event_is_new */
static json_t *agsf__pending_event_is_new(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)io;(void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1); json_t*ev=json_get(argv,2);
    json_t*snaps=agsf_get(st,"_snapshots");
    json_t*snap=agsf_is_obj(snaps)?json_obj_get(snaps,agsf_sarg(sid,"")):NULL;
    json_t*fence=agsf_get(snap,"_pending_watermark");
    json_t*sequence=agsf_get(ev,"seq");
    if(agsf_is_null(fence)||agsf_is_null(sequence)) return json_bool(true);
    json_t*f0=json_get(fence,0); json_t*f1=json_get(fence,1);
    json_t*epoch=agsf_get(ev,"replay_epoch");
    if(!agsf_json_eq(epoch,f0)) return json_bool(true);
    if(agsf_num(sequence)>agsf_num(f1)) return json_bool(true);
    return json_bool(false);
}

/* ==================== 10 _blocked_admission ==================== */
/* PoP: agsf__blocked_admission @ acp_adapter/gateway_server.py:_blocked_admission */
static json_t *agsf__blocked_admission(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)io;(void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1); json_t*aid=json_get(argv,2);
    json_t*hu=agsf_has_unknown_body(st,sid); if(agsf_is_raise(hu)) return hu;
    if(!agsf_truth(hu)) return json_bool(false);
    json_t*snaps=agsf_get(st,"_snapshots");
    json_t*snap=agsf_is_obj(snaps)?json_obj_get(snaps,agsf_sarg(sid,"")):NULL;
    json_t*pending=agsf_get(snap,"pending");
    if(agsf_is_arr(pending)) for(size_t i=0;i<json_len(pending);i++){
        json_t*row=json_get(pending,i); if(!agsf_is_obj(row))continue;
        if(!agsf_json_eq(agsf_get(row,"admission_id"),aid))continue;
        const char*stt=agsf_getstr(row,"status","");
        if(strcmp(stt,"queued")==0||strcmp(stt,"unknown")==0) return json_bool(true);
    }
    return json_bool(false);
}

/* ==================== 11 _unknown_recovery (helper) ==================== */
static json_t *agsf_unknown_recovery_body(agsf_io_t*io,json_t*st,json_t*sid,json_t*aid,int submitted){
    json_t*snaps=agsf_get(st,"_snapshots");
    json_t*snap=agsf_is_obj(snaps)?json_obj_get(snaps,agsf_sarg(sid,"")):NULL;
    json_t*pending=agsf_get(snap,"pending");
    json_t*lost=json_array();
    if(agsf_is_arr(pending)) for(size_t i=0;i<json_len(pending);i++){ json_t*row=json_get(pending,i); if(agsf_is_obj(row)&&strcmp(agsf_getstr(row,"status",""),"unknown")==0){ json_t*v=json_obj_get(row,"admission_id"); json_append(lost,v?json_copy(v):json_null()); } }
    if(json_len(lost)==0) json_append(lost,aid?json_copy(aid):json_null());
    json_t*pr=agsf_seam(io,"profile_name_for_home",agsf_A1("home",json_copy(agsf_get(st,"_home"))));
    if(agsf_is_raise(pr)) return pr;
    int plain=1; agsf_sb cmdb; agsf_sb_init(&cmdb); agsf_sb_add(&cmdb,"hermes");
    if(!agsf_is_null(pr)){ char*pv=agsf_pystr(pr); if(strcmp(pv,"default")!=0){ plain=0; agsf_sb_addf(&cmdb," -p %s",pv); } free(pv); }
    char*command=agsf_sb_take(&cmdb); (void)plain;
    const char*state=submitted?"your input is accepted and kept":"nothing was submitted";
    agsf_sb b; agsf_sb_init(&b);
    agsf_sb_add(&b,"unknown_execution: do not resend accepted input; a turn in this session was lost when the gateway restarted and its outcome is unknown (");
    agsf_sb_add(&b,state);
    agsf_sb_add(&b,"). Send ");
    for(size_t i=0;i<json_len(lost);i++){ if(i)agsf_sb_add(&b," and "); char*v=agsf_pystr(json_get(lost,i)); agsf_sb_addf(&b,"`/discard %s`",v); free(v); }
    agsf_sb_add(&b," here (or `"); agsf_sb_add(&b,command); agsf_sb_add(&b," sessions discard "); agsf_sb_add(&b,agsf_sarg(sid,"")); agsf_sb_add(&b,"` in a terminal), then continue.");
    free(command); json_free(lost);
    char*out=agsf_sb_take(&b); json_t*res=json_string(out); free(out); return res;
}
/* PoP: agsf__unknown_recovery @ acp_adapter/gateway_server.py:_unknown_recovery */
static json_t *agsf__unknown_recovery(agsf_io_t *io,const json_t *argv,const json_t *kw){
    json_t*sub=json_obj_get((json_t*)kw,"submitted");
    int submitted=sub?agsf_truth(sub):1;
    return agsf_unknown_recovery_body(io,json_get(argv,0),json_get(argv,1),json_get(argv,2),submitted);
}

/* ==================== 12 _request_live_reasoning ==================== */
/* PoP: agsf__request_live_reasoning @ acp_adapter/gateway_server.py:_request_live_reasoning */
static json_t *agsf__request_live_reasoning(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,2);
    json_t*r=agsf_seam(io,"rpc",agsf_A5("method",json_string("config.set"),"session_id",json_copy(sid),"key",json_string("show_reasoning"),"value",json_string("1"),"_timeout",json_number(10.0)));
    if(agsf_is_raise(r)){
        if(!agsf_caught(r,"Exception")) return r;  /* BaseException passthrough */
        json_t*a=json_array(); json_append(a,json_copy(sid));
        json_t*l=agsf_log(io,"logger.debug","ACP session %s keeps its reasoning hidden",a,1);
        if(agsf_is_raise(l)) return l;
        return json_null();  /* boundary swallowed Exception */
    }
    return json_copy(agsf_get(st,"_gateway"));
}

/* ==================== 13 _after_attach (composes _project_usage) ==================== */
static json_t *agsf_project_usage_body(agsf_io_t*io,json_t*st,json_t*sid,json_t*editor,json_t*stats);
/* PoP: agsf__after_attach @ acp_adapter/gateway_server.py:_after_attach */
static json_t *agsf__after_attach(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1); json_t*editor=json_get(argv,2);
    json_t*fail=NULL;
    json_t*s=agsf_seam(io,"sleep",agsf_A1("t",json_number(0.0))); if(agsf_is_raise(s)) return s;
    json_t*av=agsf_seam(io,"available_commands_update",json_object()); if(agsf_is_raise(av)){ fail=av; goto except; }
    json_t*su=agsf_seam(io,"conn.session_update",agsf_A2("session_id",json_copy(editor),"update",av)); if(agsf_is_raise(su)){ fail=su; goto except; }
    json_t*cl=agsf_seam(io,"client",json_object()); if(agsf_is_raise(cl)){ fail=cl; goto except; }
    json_t*rp=agsf_seam(io,"rpc",agsf_A2("method",json_string("session.stats"),"session_id",json_copy(sid))); if(agsf_is_raise(rp)){ fail=rp; goto except; }
    json_t*pu=agsf_project_usage_body(io,st,sid,editor,rp); if(agsf_is_raise(pu)){ fail=pu; goto except; }
    return pu;
except: {
        /* `except Exception`: BaseException-only kinds pass straight through. */
        if(fail&&!agsf_caught(fail,"Exception")) return fail;
        json_t*a=json_array(); json_append(a,json_copy(sid));
        json_t*l=agsf_log(io,"logger.debug","ACP post-attach notices for %s not delivered",a,1);
        if(agsf_is_raise(l)) return l;
        return json_null();
    }
}

/* ==================== 14 _admission_from_receipt ==================== */
/* PoP: agsf__admission_from_receipt @ acp_adapter/gateway_server.py:_admission_from_receipt */
static json_t *agsf__admission_from_receipt(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*held=json_get(argv,2); json_t*sid=json_get(argv,3); json_t*receipt=json_get(argv,4);
    json_t*vs=agsf_get(receipt,"voice_stopped");
    if(agsf_is_true(vs)){
        if(agsf_truth(agsf_get(st,"_conn"))){
            json_t*t=agsf_seam(io,"t",agsf_A1("key",json_string("cli.voice.stop_phrase_typed"))); if(agsf_is_raise(t)) return t;
            char*ts=agsf_pystr(t); char*full=agsf_fmts("%s\n",ts); free(ts);
            json_t*u=agsf_seam(io,"update_agent_message_text",agsf_A1("text",json_string(full))); free(full); if(agsf_is_raise(u)) return u;
            json_t*su=agsf_seam(io,"conn.session_update",agsf_A2("session_id",json_copy(held),"update",u)); if(agsf_is_raise(su)) return su;
        }
        return json_null();
    }
    json_t*aid=json_obj_get(receipt,"admission_id"); if(!aid) return agsf_keyerr("admission_id");
    const char*status=agsf_getstr(receipt,"status","");
    if(strcmp(status,"terminal")==0){
        json_t*r=agsf_seam(io,"settled_receipt",agsf_A4("client",json_string("client"),"held_id",json_copy(held),"session_id",json_copy(sid),"receipt",json_copy(receipt)));
        if(agsf_is_raise(r)) return r;
    } else if(strcmp(status,"unknown")==0){
        json_t*msg=agsf_unknown_recovery_body(io,st,sid,aid,1);
        if(agsf_is_raise(msg)) return msg;
        char*m=agsf_pystr(msg); json_t*r=agsf_mkraise("GatewayClientError",m); free(m); return r;
    }
    return json_copy(aid);
}

/* ==================== 15 _after_ack ==================== */
/* PoP: agsf__after_ack @ acp_adapter/gateway_server.py:_after_ack */
static json_t *agsf__after_ack(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1); json_t*aid=json_get(argv,2); json_t*link=json_get(argv,3); json_t*resumes=json_get(argv,4);
    json_t*rs=agsf_get(st,"_resumes");
    if(agsf_is_obj(rs)){ json_t*v=json_obj_get(rs,agsf_sarg(sid,"")); if(agsf_num(v)==agsf_num(resumes)) return json_null(); }
    json_t*w=agsf_seam(io,"changed.wait_for",json_object()); if(agsf_is_raise(w)) return w;
    json_t*ad=agsf_get(st,"_adopted"); json_t*adv=agsf_is_obj(ad)?json_obj_get(ad,agsf_sarg(sid,"")):NULL;
    if(agsf_num(adv)>agsf_num(resumes)){
        json_t*gw=agsf_get(st,"_gateway"); json_t*events=agsf_get(gw,"events");
        json_t*frame=json_array(); json_append(frame,json_copy(sid)); json_append(frame,json_copy(aid));
        json_t*r=agsf_seam(io,"put_behind",agsf_A2("events",events?json_copy(events):json_null(),"frame",frame));
        if(agsf_is_raise(r)) return r;
    }
    return json_null();
}

/* ==================== 16 _project_title ==================== */
/* PoP: agsf__project_title @ acp_adapter/gateway_server.py:_project_title */
static json_t *agsf__project_title(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1); json_t*editor=json_get(argv,2); json_t*title=json_get(argv,3);
    if(!agsf_is_str(title)) return json_null();
    char*tp=agsf_strip(title->str_val); int nonempty=tp[0]!=0; free(tp);
    if(!nonempty) return json_null();
    json_t*titles=agsf_get(st,"_titles");
    if(agsf_is_obj(titles)){ json_t*prev=json_obj_get(titles,agsf_sarg(sid,"")); if(prev&&agsf_json_eq(prev,title)) return json_null(); }
    if(!agsf_truth(agsf_get(st,"_conn"))) return json_null();
    json_t*upd=agsf_seam(io,"title_update",agsf_A1("title",json_copy(title))); if(agsf_is_raise(upd)) return upd;
    json_t*su=agsf_seam(io,"conn.session_update",agsf_A2("session_id",json_copy(editor),"update",upd)); if(agsf_is_raise(su)) return su;
    return json_null();
}

/* ==================== 17 _project_usage (helper) ==================== */
static json_t *agsf_project_usage_body(agsf_io_t*io,json_t*st,json_t*sid,json_t*editor,json_t*stats){
    if(!agsf_truth(agsf_get(st,"_conn"))) return json_null();
    json_t*u=agsf_seam(io,"usage_update",agsf_A1("stats",json_copy(stats))); if(agsf_is_raise(u)) return u;
    if(agsf_is_null(u)) return json_null();
    json_t*su=agsf_seam(io,"conn.session_update",agsf_A2("session_id",json_copy(editor),"update",u)); if(agsf_is_raise(su)) return su;
    return json_null();
}
/* PoP: agsf__project_usage @ acp_adapter/gateway_server.py:_project_usage */
static json_t *agsf__project_usage(agsf_io_t *io,const json_t *argv,const json_t *kw){ (void)kw; return agsf_project_usage_body(io,json_get(argv,0),json_get(argv,1),json_get(argv,2),json_get(argv,3)); }

/* ==================== 18 _permission ==================== */
/* PoP: agsf__permission @ acp_adapter/gateway_server.py:_permission */
static json_t *agsf__permission(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1); json_t*prompt=json_get(argv,2);
    const char*kind=agsf_getstr(prompt,"kind","");
    const char*answer=NULL;
    if(strcmp(kind,"approval")==0) answer="answer.approval";
    else if(strcmp(kind,"clarify")==0) answer="answer.clarify";
    if(answer==NULL||!agsf_truth(agsf_get(st,"_conn"))) return json_null();
    /* key = (session_id, prompt_id, execution_generation) -> "|"-joined string */
    char*pid=agsf_pystr(agsf_get(prompt,"prompt_id")); char*gen=agsf_pystr(agsf_get(prompt,"execution_generation"));
    char*key=agsf_fmts("%s|%s|%s",agsf_sarg(sid,""),pid,gen); free(pid); free(gen);
    json_t*perms=agsf_get(st,"_permissions");
    int present=agsf_is_obj(perms)&&json_obj_get(perms,key)!=NULL; free(key);
    if(present) return json_null();
    json_t*r=agsf_seam(io,answer,agsf_A2("session_id",json_copy(sid),"prompt",json_copy(prompt)));
    if(agsf_is_raise(r)) return r;
    json_t*c=agsf_seam(io,"create_task",agsf_A1("target",r)); if(agsf_is_raise(c)) return c;
    return json_null();
}

/* ==================== 19 _project_config ==================== */
/* PoP: agsf__project_config @ acp_adapter/gateway_server.py:_project_config */
static json_t *agsf__project_config(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1); json_t*editor=json_get(argv,2); json_t*payload=json_get(argv,3);
    static const char*FIELDS[]={"model","provider","reasoning_effort",NULL};
    int any=0; for(int i=0;FIELDS[i];i++) if(json_obj_get(payload,FIELDS[i])!=NULL) any=1;
    if(json_obj_get(payload,"edit_approval_policy")!=NULL) any=1;
    if(json_obj_get(payload,"service_tier")!=NULL) any=1;
    if(!any) return json_null();
    json_t*configs=agsf_get(st,"_configs");
    json_t*seen=agsf_is_obj(configs)?json_obj_get(configs,agsf_sarg(sid,"")):NULL;
    json_t*modes=agsf_get(st,"_modes"); json_t*modeval=agsf_is_obj(modes)?json_obj_get(modes,agsf_sarg(sid,"")):NULL;
    json_t*mode;
    if(agsf_truth(modeval)) mode=json_copy(modeval);
    else { mode=agsf_seam(io,"current_mode",agsf_A1("payload",json_copy(payload))); if(agsf_is_raise(mode)) return mode; }
    json_t*shown=json_object();
    if(agsf_is_obj(seen)) for(size_t i=0;i<json_len(seen);i++) json_set(shown,seen->c.keys[i],json_copy(json_get(seen,i)));
    for(int i=0;FIELDS[i];i++){ json_t*v=json_obj_get(payload,FIELDS[i]); if(v) json_set(shown,FIELDS[i],json_copy(v)); }
    json_set(shown,"mode",json_copy(mode));
    int eq=agsf_is_obj(seen)&&agsf_json_eq(seen,shown);
    if(eq&&json_obj_get(payload,"service_tier")==NULL) return json_null();
    if(seen==NULL||!agsf_truth(agsf_get(st,"_conn"))) return json_null();
    json_t*models=agsf_seam(io,"model_state",agsf_A1("shown",json_copy(shown))); if(agsf_is_raise(models)) return models;
    json_t*opts=agsf_seam(io,"session_config_options",agsf_A4("sid",json_copy(sid),"models",models,"shown",json_copy(shown),"mode",json_copy(mode)));
    if(agsf_is_raise(opts)) return opts;
    json_t*ca=json_object(); json_set(ca,"session_update",json_string("config_option_update")); json_set(ca,"config_options",json_copy(opts));
    json_t*upd=agsf_seam(io,"ConfigOptionUpdate",ca); if(agsf_is_raise(upd)) return upd;
    json_t*su=agsf_seam(io,"conn.session_update",agsf_A2("session_id",json_copy(editor),"update",upd)); if(agsf_is_raise(su)) return su;
    return json_null();
}

/* ==================== 20 _answer_permission ==================== */
static const char *agsf_opt2hermes(const char*oid){
    if(!oid) return "deny";
    if(strcmp(oid,"allow_once")==0) return "once";
    if(strcmp(oid,"allow_session")==0) return "session";
    if(strcmp(oid,"allow_always")==0) return "always";
    if(strcmp(oid,"deny")==0) return "deny";
    if(strcmp(oid,"deny_always")==0) return "deny";
    return "deny";
}
/* PoP: agsf__answer_permission @ acp_adapter/gateway_server.py:_answer_permission */
static json_t *agsf__answer_permission(agsf_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1); json_t*prompt=json_get(argv,2);
    json_t*choices=agsf_get(prompt,"choices");
    /* build_permission_options is OUTSIDE the try -> BaseException propagates */
    json_t*raw=agsf_seam(io,"build_permission_options",agsf_A2("allow_permanent",json_bool(agsf_in_strs(choices,"always")?true:false),"allow_session",json_bool(agsf_in_strs(choices,"session")?true:false)));
    if(agsf_is_raise(raw)) return raw;
    json_t*opts=json_array();
    if(agsf_is_arr(raw)) for(size_t i=0;i<json_len(raw);i++){
        json_t*o=json_get(raw,i);
        const char*oid=agsf_getstr(o,"option_id","");
        if(strcmp(oid,"allow_once")!=0 && strcmp(oid,"allow_session")!=0 &&
           strcmp(oid,"allow_always")!=0 && strcmp(oid,"deny")!=0 &&
           strcmp(oid,"deny_always")!=0){
            char msg[512]; snprintf(msg,sizeof(msg),"'%s'",oid);
            json_free(opts);
            return agsf_mkraise("KeyError",msg);
        }
        const char*h=agsf_opt2hermes(oid);
        if(agsf_in_strs(choices,h)) json_append(opts,json_copy(o));
    }
    json_t*r; json_t*fail=NULL;
    json_t*edit=json_obj_get(prompt,"edit");
    if(edit){
        r=agsf_seam(io,"build_acp_edit_tool_call",agsf_A1("proposal",json_copy(edit))); if(agsf_is_raise(r)){ fail=r; goto handled; }
    } else {
        r=agsf_seam(io,"build_permission_tool_call",agsf_A2("command",json_string(agsf_getstr(prompt,"command","")),"description",json_string(agsf_getstr(prompt,"description","")))); if(agsf_is_raise(r)){ fail=r; goto handled; }
    }
    json_t*eid=agsf_editor_id_body(st,sid);
    json_t*rp=agsf_seam(io,"request_permission",agsf_A3("session_id",eid,"tool_call",r,"options",json_copy(opts))); if(agsf_is_raise(rp)){ fail=rp; goto handled; }
    /* close_card receives the response with the "allowed_outcome" marker stripped. */
    json_t*clean=json_copy(rp); json_t*oc0=agsf_get(clean,"outcome");
    /* ConnDouble folds "allowed_outcome" into the AllowedOutcome instance; every
       other outcome key survives. Object => c.items/c.count, not json_get/json_len. */
    if(agsf_is_obj(oc0)){ json_t*no=json_object(); for(size_t i=0;i<oc0->c.count;i++){ if(strcmp(oc0->c.keys[i],"allowed_outcome")==0) continue; json_set(no,oc0->c.keys[i],json_copy(oc0->c.items[i])); } json_set(clean,"outcome",no); }
    json_t*eid2=agsf_editor_id_body(st,sid);
    json_t*cc=agsf_seam(io,"close_card",agsf_A4("editor_id",eid2,"tool_call",json_copy(r),"options",json_copy(opts),"response",clean)); if(agsf_is_raise(cc)){ fail=cc; goto handled; }
    json_t*outcome=agsf_get(rp,"outcome");
    if(!(agsf_is_obj(outcome)&&agsf_truth(agsf_get(outcome,"allowed_outcome")))) return json_bool(false);
    const char*oid=agsf_getstr(outcome,"option_id","");
    int inopts=0; for(size_t i=0;i<json_len(opts);i++) if(strcmp(agsf_getstr(json_get(opts,i),"option_id",""),oid)==0){ inopts=1; break; }
    if(!inopts) return json_bool(false);
    json_t*ra=agsf_A5("method",json_string("approval.respond"),"session_id",json_copy(sid),"prompt_id",json_copy(agsf_get(prompt,"prompt_id")),"execution_generation",json_copy(agsf_get(prompt,"execution_generation")),"choice",json_string(agsf_opt2hermes(oid)));
    json_t*fr=agsf_seam(io,"rpc",ra); if(agsf_is_raise(fr)){ fail=fr; goto handled; }
    return json_bool(true);
handled: {
        /* `except Exception` in the live body: BaseException-only kinds
           (SystemExit/KeyboardInterrupt/...) raised by ANY in-try seam propagate
           untouched; only Exception subclasses hit the logger/return-False path. */
        if(fail&&!agsf_caught(fail,"Exception")) return fail;
        json_t*l=agsf_log(io,"logger.info","ACP permission viewer detached or control expired",json_array(),1);
        if(agsf_is_raise(l)) return l;
        return json_bool(false);
    }
}
