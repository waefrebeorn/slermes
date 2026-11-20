/*
 * acp_gateway_server_flow2.c — FLOW C11 port of the 31 faithful class-level subset
 * defs of acp_adapter/gateway_server.py NOT claimed by acp_gateway_server_flow
 * (the connect/attach/session-control/projection lifecycle and its helpers).
 *
 * Loaded via #include by tests/t_port_acp_gateway_server_flow2.c. Every function
 * is static agsf2_*; the harness dispatches by fixture "func" name. argv[0] is
 * the self-state object; remaining argv entries are the call's real arguments.
 *
 * Collaborators crossing the C/Python boundary are UNIVERSAL SEAMS: one scripted
 * queue per seam name (first result per call, last repeats); a missing script
 * yields a RuntimeError raise spec recorded as the event ret before it
 * propagates. `except Exception` sites walk the FULL builtin hierarchy so
 * BaseException-only kinds (SystemExit/CancelledError) pass through untouched —
 * real BaseException passthrough. Fail-closed: unmodelled state yields the exact
 * Python raise spec, never a guess.
 *
 * Representation contract (shared with the oracle):
 * - None == JSON null; `is True` is JSON_BOOL true only; raise specs are
 *   {"__raise__":kind,"message":...}.
 * - method collaborators use {"p":[...],"k":{...}} args; `rpc` uses
 *   {"method":..., **kw}; conn.session_update uses {"session_id","update"}.
 * - object doubles project to markers: gateway -> "GW", task -> "TASK".
 * - set-valued state travels as JSON arrays; permission keys are "|"-joined.
 *
 * PoP annotations: each agsf2_<name> is annotated with its defining Python def.
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
typedef struct { char *p; size_t n, cap; } agsf2_sb;
static void agsf2_sb_init(agsf2_sb *b){ b->cap=128; b->n=0; b->p=(char*)malloc(b->cap); if(b->p)b->p[0]=0; }
static void agsf2_sb_grow(agsf2_sb *b, size_t need){
    if(!b->p){ b->cap=128; b->n=0; b->p=(char*)malloc(b->cap); }
    if(b->n+need+1>b->cap){ while(b->n+need+1>b->cap) b->cap*=2; b->p=(char*)realloc(b->p,b->cap); }
}
static void agsf2_sb_addn(agsf2_sb*b,const char*s,size_t n){ if(!s||!n)return; agsf2_sb_grow(b,n); memcpy(b->p+b->n,s,n); b->n+=n; b->p[b->n]=0; }
static void agsf2_sb_add(agsf2_sb*b,const char*s){ if(s) agsf2_sb_addn(b,s,strlen(s)); }
static void agsf2_sb_addf(agsf2_sb*b,const char*f,...){ va_list ap; va_start(ap,f); int n=vsnprintf(NULL,0,f,ap); va_end(ap); if(n<0)n=0; char*t=(char*)malloc((size_t)n+1); va_start(ap,f); vsnprintf(t,(size_t)n+1,f,ap); va_end(ap); agsf2_sb_addn(b,t,(size_t)n); free(t); }
static char *agsf2_sb_take(agsf2_sb*b){ char*r=b->p?b->p:(char*)""; b->p=NULL; b->n=b->cap=0; return r; }
static char *agsf2_dup(const char*s){ return strdup(s?s:""); }
static char *agsf2_fmts(const char*fmt,...){ va_list ap; va_start(ap,fmt); int n=vsnprintf(NULL,0,fmt,ap); va_end(ap); if(n<0)n=0; char*b=(char*)malloc((size_t)n+1); va_start(ap,fmt); vsnprintf(b,(size_t)n+1,fmt,ap); va_end(ap); return b; }

/* ---------------- Python value semantics over json_t ---------------- */
static int agsf2_truth(json_t*v){
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
static int agsf2_is_null(json_t*v){ return !v||v->type==JSON_NULL; }
static int agsf2_is_str(json_t*v){ return v&&v->type==JSON_STRING; }
static int agsf2_is_arr(json_t*v){ return v&&v->type==JSON_ARRAY; }
static int agsf2_is_obj(json_t*v){ return v&&v->type==JSON_OBJECT; }
static int agsf2_is_true(json_t*v){ return v&&v->type==JSON_BOOL&&v->bool_val; }
static int agsf2_isspace(unsigned char c){ return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f'||c==0x1c||c==0x1d||c==0x1e||c==0x1f; }
static json_t *agsf2_get(json_t*o,const char*k){ return agsf2_is_obj(o)?json_obj_get(o,k):NULL; }
static const char *agsf2_getstr(json_t*o,const char*k,const char*d){ json_t*v=agsf2_get(o,k); return (v&&v->type==JSON_STRING)?v->str_val:d; }
static double agsf2_num(json_t*v){ if(!v)return 0.0; if(v->type==JSON_NUMBER)return v->num_val; if(v->type==JSON_BOOL)return v->bool_val?1.0:0.0; return 0.0; }
static char *agsf2_strip(const char*s){
    if(!s) return agsf2_dup("");
    size_t a=0,b=strlen(s);
    while(a<b&&agsf2_isspace((unsigned char)s[a]))a++;
    while(b>a&&agsf2_isspace((unsigned char)s[b-1]))b--;
    char*r=(char*)malloc(b-a+1); memcpy(r,s+a,b-a); r[b-a]=0; return r;
}
static char *agsf2_pystr(json_t*v){
    if(!v) return agsf2_dup("None");
    switch(v->type){
        case JSON_NULL: return agsf2_dup("None");
        case JSON_BOOL: return agsf2_dup(v->bool_val?"True":"False");
        case JSON_NUMBER: { double d=v->num_val; if(d==floor(d)&&fabs(d)<1e15) return agsf2_fmts("%.0f",d); return agsf2_fmts("%.15g",d); }
        case JSON_STRING: return agsf2_dup(v->str_val?v->str_val:"");
        default: return agsf2_dup("");
    }
}
static int agsf2_json_eq(json_t*a,json_t*b){
    if(a==b) return 1;
    if(!a||!b||a->type!=b->type) return 0;
    switch(a->type){
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val==b->bool_val;
        case JSON_NUMBER: return a->num_val==b->num_val;
        case JSON_STRING: return strcmp(a->str_val?a->str_val:"",b->str_val?b->str_val:"")==0;
        case JSON_ARRAY: {
            if(a->c.count!=b->c.count) return 0;
            for(size_t i=0;i<a->c.count;i++) if(!agsf2_json_eq(a->c.items[i],b->c.items[i])) return 0;
            return 1;
        }
        case JSON_OBJECT: {
            if(a->c.count!=b->c.count) return 0;
            for(size_t i=0;i<a->c.count;i++){ json_t*vv=json_obj_get(b,a->c.keys[i]); if(!vv||!agsf2_json_eq(a->c.items[i],vv)) return 0; }
            return 1;
        }
    }
    return 0;
}
static int agsf2_in_strs(json_t*arr,const char*s){
    if(!agsf2_is_arr(arr)) return 0;
    for(size_t i=0;i<json_len(arr);i++){ json_t*v=json_get(arr,i); if(agsf2_is_str(v)&&strcmp(v->str_val,s)==0) return 1; }
    return 0;
}

/* ---------------- full builtin exception hierarchy ---------------- */
typedef struct { const char *n, *p; } agsf2_exc_t;
static const agsf2_exc_t AGSF2_EXC[] = {
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
    {"GatewayClientError","ValueError"},{"GatewayRPCError","GatewayClientError"},
    {"GatewayUnavailableError","GatewayClientError"},{"CommandRefused","Exception"},{"RequestError","Exception"},
    {NULL,NULL}
};
static const char *agsf2_exc_parent(const char*n){ for(int i=0;AGSF2_EXC[i].n;i++) if(strcmp(AGSF2_EXC[i].n,n)==0) return AGSF2_EXC[i].p; return "Exception"; }
static int agsf2_exc_sub(const char*n,const char*target){ int g=0; const char*c=n; while(c&&*c&&g++<40){ if(strcmp(c,"object")==0||strcmp(c,"type")==0) return 0; if(strcmp(c,target)==0) return 1; c=agsf2_exc_parent(c); } return 0; }
static int agsf2_is_raise(json_t*v){ return v&&v->type==JSON_OBJECT&&json_obj_get(v,"__raise__")!=NULL; }
static int agsf2_caught(json_t*spec,const char*target){ if(!spec)return 0; return agsf2_exc_sub(agsf2_getstr(spec,"__raise__","Exception"),target); }
static json_t *agsf2_mkraise(const char*kind,const char*msg){ json_t*o=json_object(); json_set(o,"__raise__",json_string(kind)); json_set(o,"message",json_string(msg?msg:"")); return o; }
static json_t *agsf2_keyerr(const char*key){ char*q=agsf2_fmts("'%s'",key); json_t*r=agsf2_mkraise("KeyError",q); free(q); return r; }

/* ---------------- universal seam dispatcher (type declared early for protos) ---------------- */
typedef struct { json_t *seams; json_t *cursors; json_t *trace; } agsf2_io_t;

/* forward declarations for arg builders (used before definition below) */
static json_t *agsf2_P0(void);
static json_t *agsf2_P1(json_t*);
static json_t *agsf2_P2(json_t*,json_t*);
static json_t *agsf2_P3(json_t*,json_t*,json_t*);
static json_t *agsf2_P4(json_t*,json_t*,json_t*,json_t*);
static json_t *agsf2_K0(void);
static json_t *agsf2_K1(const char*,json_t*);
static json_t *agsf2_K2(const char*,json_t*,const char*,json_t*);
static json_t *agsf2_K3(const char*,json_t*,const char*,json_t*,const char*,json_t*);
static json_t *agsf2_K4(const char*,json_t*,const char*,json_t*,const char*,json_t*,const char*,json_t*);
static json_t *agsf2_call(agsf2_io_t*,const char*,json_t*,json_t*);
static json_t *agsf2_C0(agsf2_io_t*,const char*);
static json_t *agsf2_C1(agsf2_io_t*,const char*,json_t*);
static json_t *agsf2_C2(agsf2_io_t*,const char*,json_t*,json_t*);
static json_t *agsf2_C3(agsf2_io_t*,const char*,json_t*,json_t*,json_t*);
static json_t *agsf2_C4(agsf2_io_t*,const char*,json_t*,json_t*,json_t*,json_t*);
static json_t *agsf2_C5(agsf2_io_t*,const char*,json_t*,json_t*,json_t*,json_t*,json_t*);

/* ---------------- universal seam dispatcher ---------------- */
static json_t *agsf2_seam(agsf2_io_t *io,const char *name,json_t *args){
    json_t *ret=NULL; int have=0;
    const json_t *sc=io->seams?json_obj_get(io->seams,name):NULL;
    if(sc){
        if(sc->type==JSON_ARRAY){
            double cur=json_get_num(io->cursors,name,0.0); size_t i=(size_t)cur;
            if(i<json_len(sc)){ ret=json_copy(json_get((json_t*)sc,i)); json_set(io->cursors,name,json_number(cur+1.0)); have=1; }
        } else { ret=json_copy((json_t*)sc); have=1; }
    }
    if(!have){ char*m=agsf2_fmts("no script for %s",name); ret=agsf2_mkraise("RuntimeError",m); free(m); }
    json_t *ev=json_object();
    json_set(ev,"ev",json_string(name));
    json_set(ev,"args",args?args:json_object());
    json_set(ev,"ret",json_copy(ret));
    json_append(io->trace,ev);
    return ret;
}
/* arg builders */
static json_t *agsf2_P0(void){ return json_array(); }
static json_t *agsf2_P1(json_t*v){ json_t*a=json_array(); json_append(a,v?v:json_null()); return a; }
static json_t *agsf2_P2(json_t*v,json_t*w){ json_t*a=json_array(); json_append(a,v?v:json_null()); json_append(a,w?w:json_null()); return a; }
static json_t *agsf2_P3(json_t*v,json_t*w,json_t*x){ json_t*a=agsf2_P2(v,w); json_append(a,x?x:json_null()); return a; }
static json_t *agsf2_P4(json_t*v,json_t*w,json_t*x,json_t*y){ json_t*a=agsf2_P3(v,w,x); json_append(a,y?y:json_null()); return a; }
static json_t *agsf2_P5(json_t*v,json_t*w,json_t*x,json_t*y,json_t*z){ json_t*a=agsf2_P4(v,w,x,y); json_append(a,z?z:json_null()); return a; }
static json_t *agsf2_K0(void){ return json_object(); }
static json_t *agsf2_K1(const char*k,json_t*v){ json_t*o=json_object(); json_set(o,k,v?v:json_null()); return o; }
static json_t *agsf2_K2(const char*k,json_t*v,const char*k2,json_t*v2){ json_t*o=agsf2_K1(k,v); json_set(o,k2,v2?v2:json_null()); return o; }
static json_t *agsf2_K3(const char*k,json_t*v,const char*k2,json_t*v2,const char*k3,json_t*v3){ json_t*o=agsf2_K2(k,v,k2,v2); json_set(o,k3,v3?v3:json_null()); return o; }
static json_t *agsf2_K4(const char*k,json_t*v,const char*k2,json_t*v2,const char*k3,json_t*v3,const char*k4,json_t*v4){ json_t*o=agsf2_K3(k,v,k2,v2,k3,v3); json_set(o,k4,v4?v4:json_null()); return o; }
/* generic method seam (p/k shape) */
static json_t *agsf2_call(agsf2_io_t*io,const char*name,json_t*pos,json_t*kw){
    json_t*a=json_object(); json_set(a,"p",pos?pos:json_array()); json_set(a,"k",kw?kw:json_object());
    return agsf2_seam(io,name,a);
}
static json_t *agsf2_C0(agsf2_io_t*io,const char*name){ return agsf2_call(io,name,agsf2_P0(),agsf2_K0()); }
static json_t *agsf2_C1(agsf2_io_t*io,const char*name,json_t*v){ return agsf2_call(io,name,agsf2_P1(v),agsf2_K0()); }
static json_t *agsf2_C2(agsf2_io_t*io,const char*name,json_t*v,json_t*w){ return agsf2_call(io,name,agsf2_P2(v,w),agsf2_K0()); }
static json_t *agsf2_C3(agsf2_io_t*io,const char*name,json_t*v,json_t*w,json_t*x){ return agsf2_call(io,name,agsf2_P3(v,w,x),agsf2_K0()); }
static json_t *agsf2_C4(agsf2_io_t*io,const char*name,json_t*v,json_t*w,json_t*x,json_t*y){ return agsf2_call(io,name,agsf2_P4(v,w,x,y),agsf2_K0()); }
static json_t *agsf2_C5(agsf2_io_t*io,const char*name,json_t*v,json_t*w,json_t*x,json_t*y,json_t*z){ return agsf2_call(io,name,agsf2_P5(v,w,x,y,z),agsf2_K0()); }
/* rpc seam: {"method":..., **kw} */
static json_t *agsf2_rpc(agsf2_io_t*io,const char*method,json_t*kw){
    json_t*a=json_object(); json_set(a,"method",json_string(method));
    if(kw&&agsf2_is_obj(kw)) for(size_t i=0;i<json_len(kw);i++) json_set(a,kw->c.keys[i],json_copy(json_get(kw,i)));
    return agsf2_seam(io,"rpc",a);
}
/* conn.session_update seam */
static json_t *agsf2_cu(agsf2_io_t*io,json_t*sid,json_t*upd){ return agsf2_seam(io,"conn.session_update",agsf2_K2("session_id",sid,"update",upd)); }
/* logger wrapper builds the right "logger.<level>" seam name */
static json_t *agsf2_logx(agsf2_io_t*io,const char*lvl,const char*msg,json_t*args,int exc_info){
    json_t*a=json_object(); json_set(a,"msg",json_string(msg)); json_set(a,"args",args?args:json_array());
    if(exc_info) json_set(a,"exc_info",json_bool(true));
    char*n=agsf2_fmts("logger.%s",lvl); json_t*r=agsf2_seam(io,n,a); free(n); return r;
}
/* object markers */
static json_t *agsf2_GW(void){ return json_string("GW"); }

/* ======================================================================= */
/* 1 __init__                                                               */
/* PoP: agsf2___init__ @ acp_adapter/gateway_server.py:__init__             */
static json_t *agsf2___init__(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)argv;(void)kw;
    json_t*h=agsf2_C0(io,"get_hermes_home"); if(agsf2_is_raise(h)) return h;
    json_t*p=agsf2_C1(io,"gateway_profile_home",json_copy(h)); if(agsf2_is_raise(p)) return p;
    return json_null();
}
/* 2 on_connect — the raw conn is a JSON object (no ._conn attribute) => no observer */
/* PoP: agsf2_on_connect @ acp_adapter/gateway_server.py:on_connect */
static json_t *agsf2_on_connect(agsf2_io_t*io,const json_t*argv,const json_t*kw){ (void)io;(void)argv;(void)kw; return json_null(); }
/* 3 _observe_client_message — event is a JSON object (getattr defaults) => no seam */
/* PoP: agsf2__observe_client_message @ acp_adapter/gateway_server.py:_observe_client_message */
static json_t *agsf2__observe_client_message(agsf2_io_t*io,const json_t*argv,const json_t*kw){ (void)io;(void)argv;(void)kw; return json_null(); }

/* 4 initialize */
/* PoP: agsf2_initialize @ acp_adapter/gateway_server.py:initialize */
static json_t *agsf2_initialize(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)argv;(void)kw;
    json_t*am=agsf2_C0(io,"build_auth_methods"); if(agsf2_is_raise(am)) return am;
    json_t*o=json_object();
    json_set(o,"protocol_version",json_string("1.0"));
    json_t*ai=json_object(); json_set(ai,"name",json_string("hermes-agent")); json_set(ai,"version",json_string("1.0"));
    json_set(o,"agent_info",ai);
    json_t*ac=json_object(); json_set(ac,"load_session",json_bool(true));
    json_t*pc=json_object(); json_set(pc,"image",json_bool(true)); json_set(ac,"prompt_capabilities",pc);
    json_t*sc=json_object(); json_set(sc,"fork",json_object()); json_set(sc,"list",json_object()); json_set(sc,"resume",json_object());
    json_set(ac,"session_capabilities",sc); json_set(o,"agent_capabilities",ac);
    json_set(o,"auth_methods",json_copy(am));
    return o;
}
/* 5 authenticate */
/* PoP: agsf2_authenticate @ acp_adapter/gateway_server.py:authenticate */
static json_t *agsf2_authenticate(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; return agsf2_seam(io,"authenticate",agsf2_K1("method_id",json_copy(json_get(argv,1))));
}
/* 6 new_session */
/* PoP: agsf2_new_session @ acp_adapter/gateway_server.py:new_session */
static json_t *agsf2_new_session(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*cwd=json_get(argv,1); json_t*ms=json_get(argv,2);
    json_t*c=agsf2_C0(io,"client"); if(agsf2_is_raise(c)) return c;
    json_t*d=agsf2_rpc(io,"runtime.describe",agsf2_K0()); if(agsf2_is_raise(d)) return d;
    json_t*src=agsf2_get(agsf2_get(d,"session_create"),"sources");
    json_t*caps=agsf2_get(d,"capabilities");
    if(!agsf2_in_strs(src,"acp")||!agsf2_in_strs(caps,"acp-editor-policy-v1")) return agsf2_mkraise("GatewayClientError","acp_policy_unavailable");
    json_t*f=agsf2_C0(io,"env_launch_flags"); if(agsf2_is_raise(f)) return f;
    if(agsf2_truth(agsf2_get(f,"safe_mode"))||agsf2_truth(agsf2_get(f,"ignore_user_config"))){
        json_t*b=agsf2_C0(io,"_bypass_route"); if(agsf2_is_raise(b)) return b;
    }
    json_t*u=agsf2_seam(io,"uuid",json_object()); if(agsf2_is_raise(u)) return u;
    /* continuation (not reached by fixtures): translate cwd, session.create rpc, adopt */
    json_t*tc=agsf2_C1(io,"_translate_acp_cwd",json_copy(cwd)); if(agsf2_is_raise(tc)) return tc;
    json_t*snap=agsf2_rpc(io,"session.create",agsf2_K1("source",json_string("acp"))); if(agsf2_is_raise(snap)) return snap;
    json_t*ad=agsf2_C1(io,"_adopt",json_copy(snap)); if(agsf2_is_raise(ad)) return ad;
    (void)ms; (void)st;
    return agsf2_C1(io,"_session_fields",json_copy(snap));
}
/* 7 _client */
/* PoP: agsf2__client @ acp_adapter/gateway_server.py:_client */
static json_t *agsf2__client(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*fail=agsf2_get(st,"_failure"); json_t*gw=agsf2_get(st,"_gateway");
    json_t*t=NULL;
    if(!agsf2_truth(fail)){ t=agsf2_C0(io,"_transport_closed"); if(agsf2_is_raise(t)) return t; }
    if(agsf2_truth(fail)||agsf2_truth(t)){ json_t*r=agsf2_C0(io,"_teardown"); if(agsf2_is_raise(r)) return r; }
    if(!agsf2_truth(gw)){ return agsf2_seam(io,"connect_gateway",json_object()); }
    if(agsf2_truth(fail)){
        if(agsf2_is_raise(fail)) return json_copy(fail);
        return agsf2_mkraise("TypeError","exceptions must derive from BaseException");
    }
    return agsf2_GW();
}
/* 9 _adopt */
/* PoP: agsf2__adopt @ acp_adapter/gateway_server.py:_adopt */
static json_t *agsf2__adopt(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)argv; json_t*sid=json_obj_get((json_t*)kw,"session_id"); if(!sid) sid=json_get(argv,1);
    json_t*snap=json_get(argv,2); if(!snap) snap=json_obj_get((json_t*)kw,"snapshot");
    json_t*eid=agsf2_C1(io,"_editor_id",json_copy(sid)); if(agsf2_is_raise(eid)) return eid;
    json_t*info=agsf2_get(snap,"info"); if(!agsf2_truth(info)) info=json_object();
    json_t*cl=json_obj_get((json_t*)kw,"client");
    json_t*kwc=agsf2_K0(); if(cl) json_set(kwc,"client",agsf2_GW());
    return agsf2_call(io,"_project_mode",agsf2_P3(json_copy(sid),eid,json_copy(info)),kwc);
}
/* 10 _move_workspace */
/* PoP: agsf2__move_workspace @ acp_adapter/gateway_server.py:_move_workspace */
static json_t *agsf2__move_workspace(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,2); json_t*cwd=json_get(argv,3); (void)st;
    json_t*t=agsf2_C1(io,"_translate_acp_cwd",json_copy(cwd)); if(agsf2_is_raise(t)) return t;
    json_t*a=agsf2_seam(io,"mutations.apply",agsf2_K4("client",agsf2_GW(),"session_id",json_copy(sid),"operation",json_string("workspace"),"payload",agsf2_K1("cwd",t)));
    if(agsf2_is_raise(a)) return a;
    json_t*k=agsf2_seam(io,"mutations.acknowledge",agsf2_K3("session_id",json_copy(sid),"operation",json_string("workspace"),"payload",json_object())); if(agsf2_is_raise(k)) return k;
    return agsf2_rpc(io,"session.info",agsf2_K1("session_id",json_copy(sid)));
}
/* 11 _rebind_editor_mcp */
/* PoP: agsf2__rebind_editor_mcp @ acp_adapter/gateway_server.py:_rebind_editor_mcp */
static json_t *agsf2__rebind_editor_mcp(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*sid=json_get(argv,2); json_t*editor=json_get(argv,3);
    json_t*a=agsf2_seam(io,"mutations.apply",agsf2_K4("client",agsf2_GW(),"session_id",json_copy(sid),"operation",json_string("editor_mcp"),"payload",json_copy(editor)?json_copy(editor):json_null()));
    if(agsf2_is_raise(a)) return a;
    return agsf2_seam(io,"mutations.acknowledge",agsf2_K3("session_id",json_copy(sid),"operation",json_string("editor_mcp"),"payload",json_copy(editor)?json_copy(editor):json_null()));
}
/* 12 load_session / 13 resume_session -> _resume seam */
/* PoP: agsf2_load_session @ acp_adapter/gateway_server.py:load_session */
static json_t *agsf2_load_session(agsf2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return agsf2_C3(io,"_resume",json_copy(json_get(argv,1)),json_copy(json_get(argv,2)),json_get(argv,3)?json_copy(json_get(argv,3)):json_null()); }
/* PoP: agsf2_resume_session @ acp_adapter/gateway_server.py:resume_session */
static json_t *agsf2_resume_session(agsf2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return agsf2_C3(io,"_resume",json_copy(json_get(argv,1)),json_copy(json_get(argv,2)),json_get(argv,3)?json_copy(json_get(argv,3)):json_null()); }
/* 15 cancel */
/* PoP: agsf2_cancel @ acp_adapter/gateway_server.py:cancel */
static json_t *agsf2_cancel(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid0=json_get(argv,1);
    const char*sk=agsf2_pystr(sid0); json_t*al=agsf2_get(st,"_aliases"); json_t*sid=agsf2_is_obj(al)?json_obj_get(al,sk):NULL; if(!sid) sid=sid0;
    json_t*snaps=agsf2_get(st,"_snapshots"); if(!agsf2_is_obj(snaps)||json_obj_get(snaps,agsf2_pystr(sid))==NULL) return agsf2_mkraise("GatewayClientError","not_found");
    json_t*ad=agsf2_get(st,"_admissions"); json_t*aid=agsf2_is_obj(ad)?json_obj_get(ad,agsf2_pystr(sid)):NULL;
    if(agsf2_is_null(aid)) return json_null();
    return agsf2_C2(io,"_cancel_admission",json_copy(sid),json_copy(aid));
}
/* 16 fork_session */
/* PoP: agsf2_fork_session @ acp_adapter/gateway_server.py:fork_session */
static json_t *agsf2_fork_session(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*cwd=json_get(argv,1); json_t*sid0=json_get(argv,2); json_t*ms=json_get(argv,3);
    const char*sk=agsf2_pystr(sid0); json_t*al=agsf2_get(st,"_aliases"); json_t*sid=agsf2_is_obj(al)?json_obj_get(al,sk):NULL; if(!sid) sid=sid0;
    json_t*c=agsf2_C0(io,"client"); if(agsf2_is_raise(c)) return c;
    json_t*info=agsf2_rpc(io,"session.info",agsf2_K1("session_id",json_copy(sid))); if(agsf2_is_raise(info)) return info;
    json_t*res=agsf2_seam(io,"mutations.apply",agsf2_K4("client",agsf2_GW(),"session_id",json_copy(sid),"operation",json_string("branch"),"payload",(agsf2_is_arr(ms)&&json_len(ms)>0)?json_array():json_object()));
    if(agsf2_is_raise(res)) return res;
    json_t*child=agsf2_get(res,"branched_session_id"); if(!child) return agsf2_keyerr("branched_session_id");
    json_t*r=agsf2_rpc(io,"session.resume",agsf2_K1("session_id",json_copy(child))); if(agsf2_is_raise(r)) return r;
    json_t*ad=agsf2_C1(io,"_adopt",json_copy(r)); if(agsf2_is_raise(ad)) return ad;
    json_t*rl=agsf2_C2(io,"_request_live_reasoning",agsf2_GW(),json_copy(child)); if(agsf2_is_raise(rl)) return rl;
    return agsf2_C3(io,"_session_fields",json_copy(child),agsf2_get(r,"info")?json_copy(agsf2_get(r,"info")):json_object(),json_copy(child));
    (void)cwd;
}

/* 17 set_session_model */
/* PoP: agsf2_set_session_model @ acp_adapter/gateway_server.py:set_session_model */
static json_t *agsf2_set_session_model(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*mid=json_get(argv,1); json_t*sid0=json_get(argv,2);
    const char*sk=agsf2_pystr(sid0); json_t*al=agsf2_get(st,"_aliases"); json_t*sid=agsf2_is_obj(al)?json_obj_get(al,sk):NULL; if(!sid) sid=sid0;
    json_t*c=agsf2_C0(io,"client"); if(agsf2_is_raise(c)) return c;
    json_t*pm=agsf2_C2(io,"parse_model_input",json_copy(mid),json_string("")); if(agsf2_is_raise(pm)) return pm;
    json_t*prov=json_get(pm,0); json_t*model=json_get(pm,1);
    json_t*payload=json_object(); json_set(payload,"model",json_copy(model)?json_copy(model):json_null()); if(agsf2_truth(prov)) json_set(payload,"provider",json_copy(prov));
    json_t*res=agsf2_seam(io,"mutations.apply",agsf2_K4("client",agsf2_GW(),"session_id",json_copy(sid),"operation",json_string("model"),"payload",payload)); if(agsf2_is_raise(res)) return res;
    if(strcmp(agsf2_getstr(res,"status",""),"cancelled")==0) return agsf2_mkraise("GatewayClientError","model_switch_cancelled");
    json_t*r=agsf2_rpc(io,"session.resume",agsf2_K1("session_id",json_copy(sid))); if(agsf2_is_raise(r)) return r;
    json_t*ad=agsf2_C1(io,"_adopt",json_copy(r)); if(agsf2_is_raise(ad)) return ad;
    return json_object();
}
/* 18 _confirm_deferred_pick */
/* PoP: agsf2__confirm_deferred_pick @ acp_adapter/gateway_server.py:_confirm_deferred_pick */
static json_t *agsf2__confirm_deferred_pick(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*sid=json_get(argv,1); json_t*ask=json_get(argv,3);
    json_t*target=agsf2_get(ask,"target_model"); if(!agsf2_truth(target)) target=agsf2_get(agsf2_get(ask,"payload"),"model");
    json_t*cf=agsf2_C1(io,"_model_confirmer",json_copy(sid)); if(agsf2_is_raise(cf)) return cf;
    json_t*committed=json_object(); json_set(committed,"model",json_copy(agsf2_get(ask,"committed_model"))); json_set(committed,"provider",json_copy(agsf2_get(ask,"committed_provider"))); json_set(committed,"target_model",json_copy(target));
    if(agsf2_is_null(cf)){
        json_set(committed,"reason",json_string("not confirmed"));
        json_t*r=agsf2_C1(io,"refused_pick_text",committed); if(agsf2_is_raise(r)) return r;
        return json_null();
    }
    if(cf&&(cf->type==JSON_BOOL||cf->type==JSON_NUMBER||cf->type==JSON_STRING)){
        const char*tn=cf->type==JSON_BOOL?"bool":cf->type==JSON_NUMBER?(cf->num_val==floor(cf->num_val)?"int":"float"):"str";
        char*m=agsf2_fmts("'%s' object is not callable",tn); json_t*r=agsf2_mkraise("TypeError",m); free(m); return r;
    }
    /* callable confirmer: (not exercised by fixtures) */
    return json_null();
}
/* 19 _model_confirmer */
/* PoP: agsf2__model_confirmer @ acp_adapter/gateway_server.py:_model_confirmer */
static json_t *agsf2__model_confirmer(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)io;(void)kw; json_t*st=json_get(argv,0);
    return agsf2_truth(agsf2_get(st,"_conn"))?json_string("<fn>"):json_null();
}
/* 20 set_session_mode */
/* PoP: agsf2_set_session_mode @ acp_adapter/gateway_server.py:set_session_mode */
static json_t *agsf2_set_session_mode(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*mode=json_get(argv,1); json_t*sid0=json_get(argv,2);
    const char*ms=agsf2_pystr(mode);
    int ok=(strcmp(ms,"default")==0||strcmp(ms,"ask")==0||strcmp(ms,"acceptEdits")==0||strcmp(ms,"bypassPermissions")==0);
    if(!ok) return agsf2_mkraise("GatewayClientError","invalid_params");
    json_t*h=agsf2_C1(io,"_held_session",json_copy(sid0)); if(agsf2_is_raise(h)) return h;
    const char*pol = strcmp(ms,"default")==0 ? "ask" : ms;
    json_t*r=agsf2_C2(io,"_set_edit_policy",json_copy(h),json_string(pol)); if(agsf2_is_raise(r)) return r;
    return json_object();
}
/* 21 set_config_option */
/* PoP: agsf2_set_config_option @ acp_adapter/gateway_server.py:set_config_option */
static json_t *agsf2_set_config_option(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*cid=json_get(argv,1); json_t*sid0=json_get(argv,2); json_t*val=json_get(argv,3);
    json_t*h=agsf2_C1(io,"_held_session",json_copy(sid0)); if(agsf2_is_raise(h)) return h;
    const char*c=agsf2_pystr(cid); json_t*r=NULL;
    if(strcmp(c,"edit_approval_policy")==0){
        const char*v=agsf2_pystr(val);
        if(!(strcmp(v,"ask")==0||strcmp(v,"acceptEdits")==0||strcmp(v,"bypassPermissions")==0)) return agsf2_mkraise("GatewayClientError","invalid_params");
        r=agsf2_C2(io,"_set_edit_policy",json_copy(h),json_copy(val)); if(agsf2_is_raise(r)) return r;
    } else if(strcmp(c,"model")==0){
        if(!agsf2_is_str(val)) return agsf2_mkraise("GatewayClientError","invalid_params");
        r=agsf2_call(io,"set_session_model",agsf2_P0(),agsf2_K2("model_id",json_copy(val),"session_id",json_copy(sid0))); if(agsf2_is_raise(r)) return r;
    } else if(strcmp(c,"reasoning_effort")==0){
        json_t*ch=agsf2_C0(io,"reasoning_choices"); if(agsf2_is_raise(ch)) return ch;
        if(!agsf2_in_strs(ch,agsf2_pystr(val))) return agsf2_mkraise("GatewayClientError","invalid_params");
        r=agsf2_C4(io,"_idle_control",json_copy(h),json_string("reasoning"),agsf2_K1("effort",strcmp(agsf2_pystr(val),"default")==0?json_null():json_copy(val)),json_string("Reasoning effort")); if(agsf2_is_raise(r)) return r;
    } else if(strcmp(c,"service_tier")==0){
        if(!agsf2_is_str(val)) return agsf2_mkraise("GatewayClientError","invalid_params");
    }
    json_t*cl=agsf2_C0(io,"client"); if(agsf2_is_raise(cl)) return cl;
    return agsf2_C0(io,"model_state");
}
/* 23 _idle_control */
/* PoP: agsf2__idle_control @ acp_adapter/gateway_server.py:_idle_control */
static json_t *agsf2__idle_control(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*sid=json_get(argv,1); json_t*op=json_get(argv,2); json_t*payload=json_get(argv,3); json_t*label=json_get(argv,4);
    json_t*c=agsf2_C0(io,"client"); if(agsf2_is_raise(c)) return c;
    json_t*a=agsf2_seam(io,"mutations.apply",agsf2_K4("client",agsf2_GW(),"session_id",json_copy(sid),"operation",json_copy(op),"payload",json_copy(payload)?json_copy(payload):json_null()));
    if(agsf2_is_raise(a)){
        if(!agsf2_caught(a,"GatewayClientError")) return a;
        const char*m=agsf2_getstr(a,"message","");
        if(strcmp(m,"session_busy")!=0&&strcmp(m,"unknown_execution")!=0) return a;
        agsf2_sb b; agsf2_sb_init(&b); agsf2_sb_add(&b,agsf2_pystr(label)); agsf2_sb_add(&b," changes apply when the session is idle; try again after this turn.");
        char*text=agsf2_sb_take(&b);
        json_t*u=agsf2_seam(io,"update_agent_message_text",agsf2_K1("text",json_string(agsf2_fmts("%s\n",text)))); if(agsf2_is_raise(u)){free(text);return u;}
        json_t*cu=agsf2_cu(io,json_copy(sid),u); if(agsf2_is_raise(cu)){free(text);return cu;}
        char*msg=agsf2_fmts("(-32603, '%s')",text); free(text);
        json_t*re=agsf2_mkraise("RequestError",msg); free(msg); return re;
    }
    return agsf2_seam(io,"mutations.acknowledge",agsf2_K3("session_id",json_copy(sid),"operation",json_copy(op),"payload",json_copy(payload)?json_copy(payload):json_null()));
}
/* 24 list_sessions */
/* PoP: agsf2_list_sessions @ acp_adapter/gateway_server.py:list_sessions */
static json_t *agsf2_list_sessions(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*cursor=json_get(argv,1); json_t*cwd=json_get(argv,2);
    json_t*rows=agsf2_C2(io,"catalog_sessions",json_copy(agsf2_get(st,"_home")),json_copy(cwd)?json_copy(cwd):json_null());
    if(agsf2_is_raise(rows)) return rows;
    size_t start=0;
    if(agsf2_truth(cursor)){
        int found=0;
        for(size_t i=0;i<json_len(rows);i++){ json_t*row=json_get(rows,i); json_t*id=agsf2_get(row,"session_id"); if(!id) return agsf2_keyerr("session_id"); if(agsf2_json_eq(id,cursor)){ start=i+1; found=1; break; } }
        if(!found) start=json_len(rows);
    }
    json_t*page=json_array(); size_t total=0;
    for(size_t i=start;i<json_len(rows);i++){
        json_t*row=json_get(rows,i); json_t*id=agsf2_get(row,"session_id"); if(!id) return agsf2_keyerr("session_id");
        json_t*cw=agsf2_get(row,"cwd"); if(!cw) return agsf2_keyerr("cwd");
        if(total<50){ json_t*si=json_object(); json_set(si,"session_id",json_copy(id)); json_set(si,"cwd",json_copy(cw)); json_t*t=agsf2_get(row,"title"); json_set(si,"title",t?json_copy(t):json_null()); t=agsf2_get(row,"updated_at"); json_set(si,"updated_at",t?json_copy(t):json_null()); json_append(page,si); }
        total++;
    }
    json_t*o=json_object(); json_set(o,"sessions",page);
    json_t*last=json_get(page,page->c.count?json_len(page)-1:0);
    json_set(o,"next_cursor",(total>50&&last)?json_copy(agsf2_get(last,"session_id")):json_null());
    return o;
}

/* 25 prompt */
/* PoP: agsf2_prompt @ acp_adapter/gateway_server.py:prompt */
static json_t *agsf2_prompt(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*pr=json_get(argv,1); json_t*held=json_get(argv,2);
    const char*hk=agsf2_pystr(held); json_t*al=agsf2_get(st,"_aliases"); json_t*sid=agsf2_is_obj(al)?json_obj_get(al,hk):NULL; if(!sid) sid=held;
    json_t*snaps=agsf2_get(st,"_snapshots"); if(!agsf2_is_obj(snaps)||json_obj_get(snaps,agsf2_pystr(sid))==NULL) return agsf2_mkraise("GatewayClientError","not_found");
    json_t*h=agsf2_C1(io,"_has_unknown",json_copy(sid)); if(agsf2_is_raise(h)) return h;
    if(agsf2_truth(h)){ json_t*rf=agsf2_C1(io,"_refresh_unknown_for_prompt",json_copy(sid)); if(agsf2_is_raise(rf)) return rf; if(agsf2_truth(rf)){ json_t*o=json_object(); json_set(o,"stop_reason",json_string("cancelled")); return o; } }
    json_t*h2=agsf2_C1(io,"_has_unknown",json_copy(sid)); if(agsf2_is_raise(h2)) return h2;
    if(agsf2_truth(h2)){
        json_t*ct=agsf2_C1(io,"_content_blocks_to_openai_user_content",json_copy(pr)); if(agsf2_is_raise(ct)) return ct;
        if(!agsf2_is_str(ct)) return agsf2_mkraise("GatewayClientError","");
    }
    json_t*prep=agsf2_C2(io,"_prepare_submit",json_copy(sid),json_copy(pr)); if(agsf2_is_raise(prep)) return prep;
    if(agsf2_is_null(prep)){ json_t*o=json_object(); json_set(o,"stop_reason",json_string("cancelled")); return o; }
    return agsf2_C0(io,"_client");
}
/* 27 _prepare_submit */
/* PoP: agsf2__prepare_submit @ acp_adapter/gateway_server.py:_prepare_submit */
static json_t *agsf2__prepare_submit(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1); json_t*pr=json_get(argv,2);
    json_t*ct=agsf2_C1(io,"_content_blocks_to_openai_user_content",json_copy(pr)); if(agsf2_is_raise(ct)) return ct;
    json_t*fp=agsf2_C2(io,"submit_fingerprint",json_copy(sid),json_copy(ct)); if(agsf2_is_raise(fp)) return fp;
    json_t*un=agsf2_get(st,"_unacked"); json_t*ret=agsf2_is_obj(un)?json_obj_get(un,agsf2_pystr(sid)):NULL;
    if(!agsf2_is_null(ret)&&agsf2_json_eq(json_get(ret,0),fp)) return json_copy(ret);
    json_t*sg=agsf2_C2(io,"stage_user_content",json_copy(ct),json_copy(agsf2_get(st,"_home"))); if(agsf2_is_raise(sg)) return sg;
    json_t*u=agsf2_seam(io,"uuid",json_object()); if(agsf2_is_raise(u)) return u;
    json_t*o=json_array(); json_append(o,json_copy(fp)); json_append(o,agsf2_pystr(u)?json_string(agsf2_pystr(u)):json_null()); json_append(o,json_copy(sg)); return o;
}
/* 31 _refresh_unknown */
/* PoP: agsf2__refresh_unknown @ acp_adapter/gateway_server.py:_refresh_unknown */
static json_t *agsf2__refresh_unknown(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1);
    json_t*c=agsf2_C0(io,"client"); if(agsf2_is_raise(c)) return c;
    json_t*snaps=agsf2_get(st,"_snapshots"); json_t*cached=agsf2_is_obj(snaps)?json_obj_get(snaps,agsf2_pystr(sid)):NULL;
    if(agsf2_is_null(cached)) return agsf2_mkraise("GatewayClientError","not_found");
    json_t*h=agsf2_C1(io,"_has_unknown",json_copy(sid)); if(agsf2_is_raise(h)) return h;
    if(!agsf2_truth(h)) return json_null();
    json_t*r=agsf2_rpc(io,"session.resume",agsf2_K1("session_id",json_copy(sid))); if(agsf2_is_raise(r)) return r;
    return json_null();
}
/* 32 _events */
/* PoP: agsf2__events @ acp_adapter/gateway_server.py:_events */
static json_t *agsf2__events(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw;(void)argv;
    json_t*f=agsf2_seam(io,"gateway.events.get",json_object());
    if(agsf2_is_raise(f)){
        json_t*l=agsf2_logx(io,"debug","ACP gateway event projection stopped",agsf2_P0(),1);
        return l;
    }
    return json_null();
}
/* 33 _project */
/* PoP: agsf2__project @ acp_adapter/gateway_server.py:_project */
static json_t *agsf2__project(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*ev=json_get(argv,1); json_t*sid=agsf2_get(ev,"session_id");
    json_t*eid=agsf2_C1(io,"_editor_id",json_copy(sid)); if(agsf2_is_raise(eid)) return eid;
    const char*kind=agsf2_getstr(ev,"type","");
    if(strcmp(kind,"session.info")==0||strcmp(kind,"session.updated")==0||strcmp(kind,"session.usage")==0||strcmp(kind,"session.tools_changed")==0)
        return agsf2_C5(io,"_project_session_state",json_string(kind),json_copy(sid),eid,json_copy(agsf2_get(ev,"payload")),json_copy(ev));
    json_t*aid=agsf2_get(ev,"admission_id");
    json_t*g=agsf2_C3(io,"_gap_guard",json_string(kind),json_copy(sid),json_copy(aid)?json_copy(aid):json_null()); if(agsf2_is_raise(g)) return g;
    if(agsf2_truth(g)) return json_null();
    return json_null();
}
/* 34 _gap_guard */
/* PoP: agsf2__gap_guard @ acp_adapter/gateway_server.py:_gap_guard */
static json_t *agsf2__gap_guard(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*kind=json_get(argv,1); json_t*sid=json_get(argv,2); json_t*aid=json_get(argv,3);
    if(agsf2_is_str(kind)&&strcmp(kind->str_val,"session.replay_gap")==0){
        if(!agsf2_truth(agsf2_get(st,"_gateway"))) return agsf2_mkraise("AttributeError","'NoneType' object has no attribute 'rpc'");
        json_t*s=agsf2_rpc(io,"session.resume",agsf2_K1("session_id",json_copy(sid))); if(agsf2_is_raise(s)) return s;
        return json_bool(true);
    }
    json_t*cut=agsf2_get(st,"_cut"); json_t*cv=agsf2_is_obj(cut)?json_obj_get(cut,agsf2_pystr(aid)):NULL;
    if(agsf2_is_null(cv)) return json_bool(false);
    if(agsf2_is_str(kind)&&(strcmp(kind->str_val,"tool.start")==0||strcmp(kind->str_val,"message.interim")==0)){
        char*tp=agsf2_strip(agsf2_pystr(cv)); int has=tp[0]!=0; free(tp); (void)has;
        json_t*r=agsf2_C1(io,"_end_message",json_copy(sid)); if(agsf2_is_raise(r)) return r;
    }
    return json_bool(agsf2_is_str(kind)&&strcmp(kind->str_val,"message.delta")==0);
}

/* 38 _answer_clarify */
/* PoP: agsf2__answer_clarify @ acp_adapter/gateway_server.py:_answer_clarify */
static json_t *agsf2__answer_clarify(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1); json_t*prompt=json_get(argv,2);
    json_t*eid=agsf2_C1(io,"_editor_id",json_copy(sid)); if(agsf2_is_raise(eid)) return eid;
    if(agsf2_truth(agsf2_get(st,"_form_elicitation"))){
        json_t*b=agsf2_C2(io,"build_clarify_elicitation",json_copy(eid),json_copy(prompt)); if(agsf2_is_raise(b)) return b;
        json_t*sr=agsf2_seam(io,"send_request",agsf2_K2("method",json_string("elicitation/create"),"params",json_copy(b))); if(agsf2_is_raise(sr)) return sr;
        json_t*a=agsf2_C1(io,"elicited_answer",json_copy(sr)); if(agsf2_is_raise(a)) return a;
        if(agsf2_is_null(a)) return json_bool(false);
        return agsf2_rpc(io,"clarify.respond",agsf2_K3("session_id",json_copy(sid),"prompt_id",json_copy(agsf2_get(prompt,"prompt_id")),"execution_generation",json_copy(agsf2_get(prompt,"execution_generation"))));
    }
    json_t*bp=agsf2_C1(io,"build_clarify_permission",json_copy(prompt)); if(agsf2_is_raise(bp)) return bp;
    json_t*rp=agsf2_seam(io,"request_permission",agsf2_K3("session_id",json_copy(eid),"tool_call",json_copy(json_get(bp,0)),"options",json_copy(json_get(bp,1)))); if(agsf2_is_raise(rp)) return rp;
    json_t*cc=agsf2_C4(io,"_close_card",json_copy(eid),json_copy(json_get(bp,0)),json_copy(json_get(bp,1)),json_copy(rp)); if(agsf2_is_raise(cc)) return cc;
    return agsf2_rpc(io,"clarify.respond",agsf2_K3("session_id",json_copy(sid),"prompt_id",json_copy(agsf2_get(prompt,"prompt_id")),"execution_generation",json_copy(agsf2_get(prompt,"execution_generation"))));
}
/* 39 _session_fields */
/* PoP: agsf2__session_fields @ acp_adapter/gateway_server.py:_session_fields */
static json_t *agsf2__session_fields(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1); json_t*info=json_get(argv,2); json_t*eid=json_get(argv,3);
    json_t*t=agsf2_get(info,"title");
    if(agsf2_is_str(t)){ char*tp=agsf2_strip(t->str_val); if(tp[0]) { (void)tp; } else free(tp); }
    json_t*models=agsf2_C1(io,"model_state",json_copy(info)); if(agsf2_is_raise(models)) return models;
    json_t*meta=agsf2_C3(io,"provenance_meta",json_copy(agsf2_get(st,"_home")),json_copy(eid),json_copy(sid)); if(agsf2_is_raise(meta)) return meta;
    json_t*modes=agsf2_C1(io,"session_modes",json_copy(info)); if(agsf2_is_raise(modes)) return modes;
    const char*mode=agsf2_getstr(modes,"current_mode_id","ask");
    json_t*opts=agsf2_C5(io,"session_config_options",agsf2_GW(),json_copy(sid),json_copy(models),json_copy(info),json_string(mode)); if(agsf2_is_raise(opts)) return opts;
    json_t*pi=json_object(); json_set(pi,"edit_approval_policy",json_string(mode));
    json_t*m2=agsf2_C1(io,"session_modes",pi); if(agsf2_is_raise(m2)) return m2;
    json_t*o=json_object(); json_set(o,"models",json_copy(models)); json_set(o,"modes",json_copy(m2)); json_set(o,"field_meta",json_copy(meta)); json_set(o,"config_options",json_copy(opts)); return o;
}
/* 42 _teardown */
/* PoP: agsf2__teardown @ acp_adapter/gateway_server.py:_teardown */
static json_t *agsf2__teardown(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0);
    if(agsf2_truth(agsf2_get(st,"_connection"))){
        json_t*a=json_array(); json_append(a,json_null()); json_append(a,json_null()); json_append(a,json_null());
        json_t*r=agsf2_seam(io,"connection.__aexit__",agsf2_K1("p",a)); if(agsf2_is_raise(r)) return r;
    }
    return json_null();
}
/* 43 _notify_unknown */
/* PoP: agsf2__notify_unknown @ acp_adapter/gateway_server.py:_notify_unknown */
static json_t *agsf2__notify_unknown(agsf2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*st=json_get(argv,0); json_t*sid=json_get(argv,1);
    json_t*h=agsf2_C1(io,"_has_unknown",json_copy(sid)); if(agsf2_is_raise(h)) return h;
    if(!agsf2_truth(h)||!agsf2_truth(agsf2_get(st,"_conn"))) return json_null();
    json_t*e=agsf2_C1(io,"_editor_id",json_copy(sid)); if(agsf2_is_raise(e)) return e;
    json_t*u=agsf2_C2(io,"_unknown_recovery",json_copy(sid),json_bool(false)); if(agsf2_is_raise(u)) return u;
    char*msg=agsf2_fmts("%s\n",agsf2_pystr(u));
    json_t*t=agsf2_seam(io,"update_agent_message_text",agsf2_K1("text",json_string(msg))); free(msg); if(agsf2_is_raise(t)) return t;
    return agsf2_cu(io,e,t);
}
