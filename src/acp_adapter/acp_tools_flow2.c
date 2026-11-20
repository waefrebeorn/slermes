/*
 * acp_tools_flow2.c — FLOW C11 port of the UNCLAIMED remainder of
 * acp_adapter/tools.py (41 of 47 top-level defs; the 6 flow1-claimed defs are
 * re-derived here faithfully for the standalone differential oracle).
 *
 * Loaded via #include by tests/t_port_acp_tools_flow2.c. Every function is
 * static at2_*; the harness dispatches by fixture "func" name.
 *
 * Collaborators that cross the C/Python boundary are UNIVERSAL SEAMS:
 *   uuid.uuid4, build_tool_preview, json.loads, json.dumps,
 *   JSONDecoder.raw_decode, acp.text_block, acp.tool_content,
 *   acp.tool_diff_content, extract_edit_diff, acp.start_tool_call,
 *   acp.update_tool_call, ToolCallLocation, logger.debug
 * All other Python helpers are transcribed for real. Seam scripts are per-call
 * queues (last repeats); an absent/exhausted script yields a RuntimeError
 * raise spec recorded as the event ret before propagation. Raise specs are
 * {"__raise__":kind,"message":...}; `except` sites walk the FULL builtin
 * exception hierarchy (AT2_EXC, generated from the gate interpreter).
 *
 * PoP annotations use qualified at2_* names; the 6 flow1 claims stay in force.
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
/* forward decls (used by helpers defined later in this TU) */
static json_t *json_array1(json_t*a);
static json_t *json_array2(json_t*a,json_t*b);
static json_t *json_array3(json_t*a,json_t*b,json_t*c);
static json_t *json_array_of(json_t*v);

/* ---------------- dynamic string builder ---------------- */
typedef struct { char *p; size_t n, cap; } at2_sb;
static void at2_sb_init(at2_sb *b){ b->cap=128; b->n=0; b->p=(char*)malloc(b->cap); if(b->p)b->p[0]=0; }
static void at2_sb_grow(at2_sb *b, size_t need){
    if(!b->p){ b->cap=128; b->n=0; b->p=(char*)malloc(b->cap); }
    if(b->n+need+1>b->cap){ while(b->n+need+1>b->cap) b->cap*=2; b->p=(char*)realloc(b->p,b->cap); }
}
static void at2_sb_addn(at2_sb*b,const char*s,size_t n){ if(!s||!n)return; at2_sb_grow(b,n); memcpy(b->p+b->n,s,n); b->n+=n; b->p[b->n]=0; }
static void at2_sb_add(at2_sb*b,const char*s){ if(s) at2_sb_addn(b,s,strlen(s)); }
static void at2_sb_addf(at2_sb*b,const char*f,...){ va_list ap; va_start(ap,f); int n=vsnprintf(NULL,0,f,ap); va_end(ap); if(n<0)n=0; char*t=(char*)malloc((size_t)n+1); va_start(ap,f); vsnprintf(t,(size_t)n+1,f,ap); va_end(ap); at2_sb_addn(b,t,(size_t)n); free(t); }
static void at2_sb_ch(at2_sb*b,char c){ at2_sb_grow(b,1); b->p[b->n++]=c; b->p[b->n]=0; }
static char *at2_sb_take(at2_sb*b){ char*r=b->p?b->p:(char*)""; b->p=NULL; b->n=b->cap=0; return r; }
static char *at2_dup(const char*s){ return strdup(s?s:""); }
static char *at2_fmts(const char*fmt,...){
    va_list ap; va_start(ap,fmt); int n=vsnprintf(NULL,0,fmt,ap); va_end(ap);
    if(n<0)n=0; char*b=(char*)malloc((size_t)n+1); va_start(ap,fmt); vsnprintf(b,(size_t)n+1,fmt,ap); va_end(ap); return b;
}

/* ---------------- Python value semantics over json_t ---------------- */
static int at2_truth(json_t*v){
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
static int at2_is_str(json_t*v){ return v&&v->type==JSON_STRING; }
static int at2_is_arr(json_t*v){ return v&&v->type==JSON_ARRAY; }
static int at2_is_obj(json_t*v){ return v&&v->type==JSON_OBJECT; }
static int at2_is_null(json_t*v){ return !v||v->type==JSON_NULL; }
static int at2_is_false(json_t*v){ return v&&v->type==JSON_BOOL&&!v->bool_val; } /* `is False` */
static int at2_is_true(json_t*v){ return v&&v->type==JSON_BOOL&&v->bool_val; }   /* `is True` */
static int at2_eq1(json_t*v){ return v&&((v->type==JSON_NUMBER&&v->num_val==1.0)||(v->type==JSON_BOOL&&v->bool_val)); }
static int at2_is_intlike(json_t*v){ /* isinstance(x,int): ints & bool (not other numbers) */
    if(!v) return 0;
    if(v->type==JSON_BOOL) return 1;
    if(v->type==JSON_NUMBER) return v->num_val==floor(v->num_val);
    return 0;
}
static json_t *at2_get(json_t*o,const char*k){ return at2_is_obj(o)?json_obj_get(o,k):NULL; }
static const char *at2_getstr(json_t*o,const char*k,const char*d){ json_t*v=at2_get(o,k); return (v&&v->type==JSON_STRING)?v->str_val:d; }
static const char *at2_sarg(json_t*v,const char*d){ return (v&&v->type==JSON_STRING)?v->str_val:d; }
static int at2_isspace(unsigned char c){ return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f'||c==0x1c||c==0x1d||c==0x1e||c==0x1f; }
static char *at2_strip(const char*s){
    if(!s) return at2_dup("");
    size_t a=0,b=strlen(s);
    while(a<b&&at2_isspace((unsigned char)s[a]))a++;
    while(b>a&&at2_isspace((unsigned char)s[b-1]))b--;
    char*r=(char*)malloc(b-a+1); memcpy(r,s+a,b-a); r[b-a]=0; return r;
}
static char *at2_lstrip_hashes(const char*s){
    if(!s)return at2_dup(""); size_t i=0; while(s[i]=='#')i++; return at2_strip(s+i);
}
static char *at2_pystr(json_t*v);
static char *at2_repr(json_t*v){
    at2_sb b; at2_sb_init(&b);
    if(!v){ at2_sb_add(&b,"None"); return at2_sb_take(&b); }
    switch(v->type){
        case JSON_NULL: at2_sb_add(&b,"None"); break;
        case JSON_BOOL: at2_sb_add(&b,v->bool_val?"True":"False"); break;
        case JSON_NUMBER: { char*s=at2_pystr(v); at2_sb_add(&b,s); free(s); break; }
        case JSON_STRING: {
            at2_sb_ch(&b,'\'');
            const char*s=v->str_val?v->str_val:"";
            for(;*s;s++){
                if(*s=='\\'||*s=='\''){ at2_sb_ch(&b,'\\'); at2_sb_ch(&b,*s); }
                else if(*s=='\n'){ at2_sb_add(&b,"\\n"); }
                else if(*s=='\t'){ at2_sb_add(&b,"\\t"); }
                else if(*s=='\r'){ at2_sb_add(&b,"\\r"); }
                else at2_sb_ch(&b,*s);
            }
            at2_sb_ch(&b,'\''); break;
        }
        case JSON_ARRAY: {
            at2_sb_ch(&b,'[');
            for(size_t i=0;i<v->c.count;i++){ if(i)at2_sb_add(&b,", "); char*s=at2_repr(v->c.items[i]); at2_sb_add(&b,s); free(s); }
            at2_sb_ch(&b,']'); break;
        }
        case JSON_OBJECT: {
            at2_sb_ch(&b,'{');
            for(size_t i=0;i<v->c.count;i++){
                if(i)at2_sb_add(&b,", "); at2_sb_ch(&b,'\''); at2_sb_add(&b,v->c.keys[i]?v->c.keys[i]:"");
                at2_sb_add(&b,"': "); char*s=at2_repr(v->c.items[i]); at2_sb_add(&b,s); free(s);
            }
            at2_sb_ch(&b,'}'); break;
        }
    }
    return at2_sb_take(&b);
}
static char *at2_pystr(json_t*v){
    if(!v) return at2_dup("None");
    switch(v->type){
        case JSON_NULL: return at2_dup("None");
        case JSON_BOOL: return at2_dup(v->bool_val?"True":"False");
        case JSON_NUMBER: {
            double d=v->num_val;
            if(d==floor(d)&&fabs(d)<1e15){ return at2_fmts("%.0f",d); }
            char*tmp=at2_fmts("%.15g",d); return tmp;
        }
        case JSON_STRING: return at2_dup(v->str_val?v->str_val:"");
        default: return at2_repr(v);
    }
}
/* template.format(value): replace every "{}" with value's str() */
static char *at2_fmt(json_t*value,const char*tmpl,const char*fallback){
    if(!at2_truth(value)) return at2_dup(fallback?fallback:"");
    char*vs=at2_pystr(value); at2_sb b; at2_sb_init(&b);
    const char*p=tmpl?tmpl:"";
    while(*p){ if(p[0]=='{'&&p[1]=='}'){ at2_sb_add(&b,vs); p+=2; } else at2_sb_ch(&b,*p++); }
    free(vs); return at2_sb_take(&b);
}
static char *at2_plural(json_t*count,const char*word,const char*suf){
    char*cs=at2_pystr(count); at2_sb b; at2_sb_init(&b);
    at2_sb_add(&b,cs); at2_sb_ch(&b,' '); at2_sb_add(&b,word);
    if(!at2_eq1(count)) at2_sb_add(&b,suf?suf:"s");
    free(cs); return at2_sb_take(&b);
}
static json_t *at2_first(json_t*data,const char*keys[],int nk,json_t*def){
    for(int i=0;i<nk;i++){ json_t*v=at2_get(data,keys[i]); if(v&&at2_truth(v)) return v; }
    return def?def:json_string("");
}
static char *at2_arg(json_t*args,const char*keys[],int nk,const char*def){
    json_t*dflt=json_string(def?def:"");
    json_t*v=at2_first(args,keys,nk,dflt);
    char*s=at2_pystr(v); char*t=at2_strip(s); free(s);
    if(t[0]) return t; free(t); return at2_dup(def?def:"");
}
static json_t *at2_nonempty(json_t*v){
    if(v&&v->type==JSON_STRING){ char*t=at2_strip(v->str_val); int ok=t[0]; free(t); if(ok) return json_copy(v); }
    return NULL; /* None */
}

/* ---------------- full builtin exception hierarchy (75 rows) ---------------- */
typedef struct { const char *n, *p; } at2_exc_t;
static const at2_exc_t AT2_EXC[] = {
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"BufferError", "Exception"},
    {"BytesWarning", "Warning"},
    {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"},
    {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"},
    {"EnvironmentError", "Exception"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"},
    {"GeneratorExit", "BaseException"},
    {"IOError", "Exception"},
    {"ImportError", "Exception"},
    {"ImportWarning", "Warning"},
    {"IndentationError", "SyntaxError"},
    {"IndexError", "LookupError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"KeyError", "LookupError"},
    {"KeyboardInterrupt", "BaseException"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"NameError", "Exception"},
    {"NotADirectoryError", "OSError"},
    {"NotImplementedError", "RuntimeError"},
    {"OSError", "Exception"},
    {"OverflowError", "ArithmeticError"},
    {"PendingDeprecationWarning", "Warning"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"},
    {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"},
    {"SystemError", "Exception"},
    {"SystemExit", "BaseException"},
    {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"},
    {"TypeError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"WindowsError", "Exception"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"CancelledError", "BaseException"},
    {"_IncompleteInputError", "SyntaxError"},
    {"PythonFinalizationError", "Exception"},
    {NULL,NULL}
};
static const char *at2_exc_parent(const char*n){
    for(int i=0;AT2_EXC[i].n;i++) if(strcmp(AT2_EXC[i].n,n)==0) return AT2_EXC[i].p;
    return "Exception";
}
static int at2_exc_sub(const char*n,const char*target){
    int guard=0; const char*c=n;
    while(c&&*c&&guard++<40){ if(strcmp(c,"object")==0||strcmp(c,"type")==0) return 0; if(strcmp(c,target)==0) return 1; c=at2_exc_parent(c); }
    return 0;
}
static int at2_is_raise(json_t*v){ return v&&v->type==JSON_OBJECT&&json_obj_get(v,"__raise__")!=NULL; }
static int at2_caught(json_t*spec,const char*target){
    if(!spec) return 0;
    return at2_exc_sub(at2_getstr(spec,"__raise__","Exception"),target);
}
static int at2_caught2(json_t*spec,const char*a,const char*b){ return at2_caught(spec,a)||at2_caught(spec,b); }
static json_t *at2_mkraise(const char*kind,const char*msg){
    json_t*o=json_object(); json_set(o,"__raise__",json_string(kind)); json_set(o,"message",json_string(msg?msg:"")); return o;
}

/* ---------------- universal seam dispatcher ---------------- */
typedef struct {
    json_t *seams;   /* borrowed in.seams */
    json_t *cursors; /* owned */
    json_t *trace;   /* owned */
} at2_io_t;
static json_t *at2_seam(at2_io_t *io,const char *name,json_t *args){
    json_t *ret=NULL; int have=0;
    const json_t *sc=io->seams?json_obj_get(io->seams,name):NULL;
    if(sc){
        if(sc->type==JSON_ARRAY){
            double cur=json_get_num(io->cursors,name,0.0); size_t i=(size_t)cur;
            if(i<json_len(sc)){ ret=json_copy(json_get((json_t*)sc,i)); json_set(io->cursors,name,json_number(cur+1.0)); have=1; }
        } else { ret=json_copy((json_t*)sc); have=1; }
    }
    if(!have){ char*m=at2_fmts("no script for %s",name); ret=at2_mkraise("RuntimeError",m); free(m); }
    json_t *ev=json_object();
    json_set(ev,"ev",json_string(name));
    json_set(ev,"args",args?args:json_object());
    json_set(ev,"ret",json_copy(ret));
    json_append(io->trace,ev);
    return ret;
}
/* seam arg builders (each returns an owned object) */
static json_t *at2_A1(const char*k1,json_t*v1){ json_t*o=json_object(); json_set(o,k1,v1?v1:json_null()); return o; }
static json_t *at2_A2(const char*k1,json_t*v1,const char*k2,json_t*v2){ json_t*o=json_object(); json_set(o,k1,v1?v1:json_null()); json_set(o,k2,v2?v2:json_null()); return o; }
static json_t *at2_A3(const char*k1,json_t*v1,const char*k2,json_t*v2,const char*k3,json_t*v3){ json_t*o=json_object(); json_set(o,k1,v1?v1:json_null()); json_set(o,k2,v2?v2:json_null()); json_set(o,k3,v3?v3:json_null()); return o; }
static json_t *at2_A4(const char*k1,json_t*v1,const char*k2,json_t*v2,const char*k3,json_t*v3,const char*k4,json_t*v4){ json_t*o=json_object(); json_set(o,k1,v1?v1:json_null()); json_set(o,k2,v2?v2:json_null()); json_set(o,k3,v3?v3:json_null()); json_set(o,k4,v4?v4:json_null()); return o; }
static json_t *at2_A5(const char*k1,json_t*v1,const char*k2,json_t*v2,const char*k3,json_t*v3,const char*k4,json_t*v4,const char*k5,json_t*v5){ json_t*o=json_object(); json_set(o,k1,v1?v1:json_null()); json_set(o,k2,v2?v2:json_null()); json_set(o,k3,v3?v3:json_null()); json_set(o,k4,v4?v4:json_null()); json_set(o,k5,v5?v5:json_null()); return o; }
static json_t *at2_A6(const char*k1,json_t*v1,const char*k2,json_t*v2,const char*k3,json_t*v3,const char*k4,json_t*v4,const char*k5,json_t*v5,const char*k6,json_t*v6){ json_t*o=json_object(); json_set(o,k1,v1?v1:json_null()); json_set(o,k2,v2?v2:json_null()); json_set(o,k3,v3?v3:json_null()); json_set(o,k4,v4?v4:json_null()); json_set(o,k5,v5?v5:json_null()); json_set(o,k6,v6?v6:json_null()); return o; }

/* ---------------- string lstrip (leading ASCII/unicode-ish whitespace) ---------------- */
static char *at2_lstrip(const char*s){
    if(!s)return at2_dup(""); size_t i=0; while(s[i]&&at2_isspace((unsigned char)s[i]))i++; return at2_dup(s+i);
}
static int at2_startswith(const char*s,const char*pre){ return s&&pre&&strncmp(s,pre,strlen(pre))==0; }

/* _POLISHED_TOOLS membership (source literal) */
static const char *AT2_POLISHED[] = {
    "todo","memory","session_search","delegate_task",
    "read_file","write_file","patch","search_files","terminal","process","execute_code",
    "skill_view","skills_list","skill_manage","web_search","web_extract",
    "browser_navigate","browser_click","browser_type","browser_press","browser_scroll",
    "browser_back","browser_snapshot","browser_console","browser_get_images","browser_vision",
    "vision_analyze","image_generate","text_to_speech",
    "cronjob","send_message","clarify","discord","discord_admin",
    "feishu_doc_read","feishu_drive_list_comments","feishu_drive_list_comment_replies",
    "feishu_drive_reply_comment","feishu_drive_add_comment",
    "kanban_create","kanban_show","kanban_comment","kanban_complete",
    "kanban_block","kanban_request_review","kanban_request_changes",
    "kanban_link","kanban_heartbeat",NULL
};
static int at2_polished(const char*n){ if(!n)return 0; for(int i=0;AT2_POLISHED[i];i++) if(strcmp(n,AT2_POLISHED[i])==0)return 1; return 0; }

/* PoP: at2_get_tool_kind @ acp_adapter/tools.py:get_tool_kind (re-derived, flow1 claim retained) */
static json_t *at2_get_tool_kind(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)io;(void)kw;
    const char*n=at2_sarg(json_get(argv,0),"");
    static const struct{const char*k;const char*tools[12];}M[]={
      {"read",{"read_file","skill_view","skills_list","browser_snapshot","browser_vision","browser_get_images","vision_analyze",NULL}},
      {"edit",{"write_file","patch","skill_manage",NULL}},
      {"search",{"search_files",NULL}},
      {"execute",{"terminal","process","execute_code","browser_click","browser_type","browser_scroll","browser_press","browser_back","delegate_task","image_generate","text_to_speech",NULL}},
      {"fetch",{"web_search","web_extract","browser_navigate",NULL}},
      {"other",{"todo",NULL}},
      {"think",{"_thinking",NULL}},
    };
    for(int i=0;i<7;i++) for(int j=0;M[i].tools[j];j++) if(strcmp(n,M[i].tools[j])==0) return json_string(M[i].k);
    return json_string("other");
}
/* PoP: at2_make_tool_call_id @ acp_adapter/tools.py:make_tool_call_id (re-derived) */
static json_t *at2_make_tool_call_id(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)argv;(void)kw;
    json_t*r=at2_seam(io,"uuid.uuid4",json_object());
    if(at2_is_raise(r)) return r;
    const char*hex=(r&&r->type==JSON_STRING)?r->str_val:"";
    char buf[16]; size_t n=strlen(hex); if(n>12)n=12; memcpy(buf,hex,n); buf[n]=0;
    char*out=at2_fmts("tc-%s",buf); json_t*o=json_string(out); free(out); return o;
}
/* PoP: at2__text @ acp_adapter/tools.py:_text */
static json_t *at2__text(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*content=json_get(argv,0);
    json_t*b=at2_seam(io,"acp.text_block",at2_A1("content",content?json_copy(content):json_null()));
    if(at2_is_raise(b)) return b;
    return at2_seam(io,"acp.tool_content",at2_A1("block",b));
}
/* PoP: at2__arg @ acp_adapter/tools.py:_arg */
static json_t *at2__arg(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)io; json_t*args=json_get(argv,0);
    const char*keys[64]; int m=0; size_t n=json_len(argv);
    for(size_t i=1;i<n&&m<64;i++){ json_t*v=json_get(argv,i); keys[m++]=(v&&v->type==JSON_STRING)?v->str_val:""; }
    const char*def=at2_getstr((json_t*)kw,"default","");
    char*r=at2_arg(args,keys,m,def); json_t*o=json_string(r); free(r); return o;
}
/* PoP: at2__first @ acp_adapter/tools.py:_first */
static json_t *at2__first(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)io; json_t*data=json_get(argv,0);
    const char*keys[64]; int m=0; size_t n=json_len(argv);
    for(size_t i=1;i<n&&m<64;i++){ json_t*v=json_get(argv,i); keys[m++]=(v&&v->type==JSON_STRING)?v->str_val:""; }
    json_t*def=json_obj_get(kw,"default")?json_copy(json_obj_get((json_t*)kw,"default")):json_string("");
    json_t*r=at2_first(data,keys,m,def); json_free(def); return json_copy(r);
}
/* PoP: at2__fmt @ acp_adapter/tools.py:_fmt */
static json_t *at2__fmt(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)io;(void)kw; char*r=at2_fmt(json_get(argv,0),at2_sarg(json_get(argv,1),""),at2_sarg(json_get(argv,2),""));
    json_t*o=json_string(r); free(r); return o;
}
/* PoP: at2__nonempty @ acp_adapter/tools.py:_nonempty */
static json_t *at2__nonempty(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)io;(void)kw; return at2_nonempty(json_get(argv,0));
}
/* PoP: at2__plural @ acp_adapter/tools.py:_plural */
static json_t *at2__plural(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)io; const char*suf=json_obj_get(kw,"suffix")?at2_getstr((json_t*)kw,"suffix","s"):"s";
    char*r=at2_plural(json_get(argv,0),at2_sarg(json_get(argv,1),""),suf);
    json_t*o=json_string(r); free(r); return o;
}
/* PoP: at2__failure @ acp_adapter/tools.py:_failure */
static json_t *at2__failure(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)io;(void)kw; json_t*data=json_get(argv,0); json_t*prefix=json_get(argv,1);
    int failed=at2_is_false(at2_get(data,"success"))||at2_truth(at2_get(data,"error"));
    if(!failed) return NULL;
    json_t*err=at2_get(data,"error");
    char*es=err?at2_pystr(err):at2_dup("unknown error");
    char*ps=at2_pystr(prefix); char*r=at2_fmts("%s: %s",ps,es);
    free(ps);free(es); json_t*o=json_string(r); free(r); return o;
}
/* PoP: at2__truncate_text @ acp_adapter/tools.py:_truncate_text (re-derived) */
static json_t *at2__truncate_text(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)io; json_t*t=json_get(argv,0); const char*s=(t&&t->type==JSON_STRING)?t->str_val:"";
    int lim=5000;
    if(json_len(argv)>1&&json_get(argv,1)&&json_get(argv,1)->type==JSON_NUMBER) lim=(int)json_get(argv,1)->num_val;
    if(json_obj_get(kw,"limit")&&json_obj_get((json_t*)kw,"limit")->type==JSON_NUMBER) lim=(int)json_obj_get((json_t*)kw,"limit")->num_val;
    size_t len=strlen(s);
    if((int)len<=lim){ json_t*o=json_string(s); return o; }
    int cut=lim-100; if(cut<0)cut=0; if((size_t)cut>len)cut=(int)len;
    char*r=at2_fmts("%.*s\n... (%lu chars total, truncated)",cut,s,(unsigned long)len);
    json_t*o=json_string(r); free(r); return o;
}
/* PoP: at2__fenced_text @ acp_adapter/tools.py:_fenced_text (re-derived) */
static json_t *at2__fenced_text(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)io;(void)kw; const char*s=at2_sarg(json_get(argv,0),"");
    const char*lang=json_len(argv)>1?at2_sarg(json_get(argv,1),""):"";
    size_t longest=0,seglen=0; int idx=0;
    for(const char*p=s;;p++){
        if(*p=='`'){ if((idx&1)&&seglen>longest)longest=seglen; seglen=0; idx++; continue; }
        if(!*p) break; seglen++;
    }
    if((idx&1)&&seglen>longest) longest=seglen;
    size_t fn=longest+1; if(fn<3)fn=3;
    at2_sb b; at2_sb_init(&b);
    for(size_t i=0;i<fn;i++) at2_sb_ch(&b,'`');
    at2_sb_add(&b,lang); at2_sb_ch(&b,'\n'); at2_sb_add(&b,s); at2_sb_ch(&b,'\n');
    for(size_t i=0;i<fn;i++) at2_sb_ch(&b,'`');
    json_t*o=json_string(at2_sb_take(&b)); return o;
}
/* forward decl (used by coerce/_json_loads_maybe/_tool_result_failed/formatters) */
static json_t *at2__json_loads_maybe(at2_io_t *io,const json_t *argv,const json_t *kw);
/* PoP: at2_coerce_tool_args @ acp_adapter/tools.py:coerce_tool_args */
static json_t *at2_coerce_tool_args(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*raw=json_get(argv,0);
    if(raw&&raw->type==JSON_STRING){
        json_t*r=at2_seam(io,"json.loads",at2_A1("s",json_copy(raw)));
        if(at2_is_raise(r)){
            if(at2_caught(r,"Exception")){ json_t*o=json_object(); json_set(o,"raw",json_copy(raw)); return o; }
            return r;
        }
        raw=r;
    }
    return (raw&&raw->type==JSON_OBJECT)?json_copy(raw):json_object();
}
/* PoP: at2__args_json @ acp_adapter/tools.py:_args_json */
static json_t *at2__args_json(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*a=json_get(argv,0);
    json_t*ja=json_object(); json_set(ja,"obj",json_copy(a)); json_set(ja,"indent",json_number(2.0)); json_set(ja,"ensure_ascii",json_bool(true)); json_set(ja,"default",json_string("str"));
    json_t*r=at2_seam(io,"json.dumps",ja);
    if(at2_is_raise(r)){
        if(at2_caught2(r,"TypeError","ValueError")){ char*s=at2_pystr(a); json_t*o=json_string(s); free(s); return o; }
        return r;
    }
    return r;
}
/* PoP: at2__json_loads_maybe @ acp_adapter/tools.py:_json_loads_maybe */
static json_t *at2__json_loads_maybe(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*v=json_get(argv,0);
    if(!(v&&v->type==JSON_STRING)) return v?json_copy(v):json_null();
    json_t*r=at2_seam(io,"json.loads",at2_A1("s",json_copy(v)));
    if(!at2_is_raise(r)) return r;
    if(!at2_caught(r,"Exception")) return r;
    char*st=at2_lstrip(v->str_val);
    json_t*r2=at2_seam(io,"JSONDecoder.raw_decode",at2_A1("s",json_string(st))); free(st);
    if(!at2_is_raise(r2)) return r2;
    if(!at2_caught(r2,"Exception")) return r2;
    return NULL;
}
/* PoP: at2__tool_result_failed @ acp_adapter/tools.py:_tool_result_failed (re-derived) */
static json_t *at2__tool_result_failed(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*result=json_get(argv,0); json_t*tn=json_len(argv)>1?json_get(argv,1):NULL;
    if(result&&result->type==JSON_STRING&&at2_startswith(result->str_val,"Error executing tool '")) return json_bool(true);
    json_t*av=json_array1(result); json_t*data=at2__json_loads_maybe(io,av,json_object());
    if(at2_is_raise(data)) return data;
    if(!at2_is_obj(data)) return json_bool(false);
    json_t*ec=at2_get(data,"exit_code"); if(!ec) ec=at2_get(data,"returncode");
    int failed=0;
    if(at2_is_false(at2_get(data,"success"))||at2_is_false(at2_get(data,"ok"))) failed=1;
    if(at2_is_intlike(ec)&&ec&&!(ec->type==JSON_NUMBER&&ec->num_val==0.0)&&!(ec->type==JSON_BOOL&&!ec->bool_val)) failed=1;
    if(!failed){
        const char*name=(tn&&tn->type==JSON_STRING)?tn->str_val:NULL;
        if(at2_polished(name)&&at2_truth(at2_get(data,"error"))&&!at2_truth(at2_get(data,"content"))) failed=1;
    }
    return json_bool(failed?true:false);
}
/* PoP: at2_build_tool_title @ acp_adapter/tools.py:build_tool_title (re-derived) */
static json_t *at2_build_tool_title(at2_io_t *io,const json_t *argv,const json_t *kw){
    (void)kw; json_t*tn=json_get(argv,0); json_t*args=json_get(argv,1);
    json_t*r=at2_seam(io,"build_tool_preview",at2_A3("tool_name",json_copy(tn),"args",json_copy(args),"max_len",json_number(80.0)));
    if(at2_is_raise(r)) return r;
    if(at2_truth(r)){ char*ps=at2_pystr(r); const char*ns=(tn&&tn->type==JSON_STRING)?tn->str_val:""; char*out=at2_fmts("%s: %s",ns,ps); free(ps); json_t*o=json_string(out); free(out); return o; }
    return json_copy(tn);
}
static json_t *json_array1(json_t*a){ json_t*r=json_array(); json_append(r,a?a:json_null()); return r; }
static json_t *json_array2(json_t*a,json_t*b){ json_t*r=json_array(); json_append(r,a?a:json_null()); json_append(r,b?b:json_null()); return r; }
static json_t *json_array3(json_t*a,json_t*b,json_t*c){ json_t*r=json_array(); json_append(r,a?a:json_null()); json_append(r,b?b:json_null()); json_append(r,c?c:json_null()); return r; }
static json_t *json_array_of(json_t*v){ json_t*r=json_array(); json_append(r,v?v:json_null()); return r; }

/* ---------------- line-list / string helpers ---------------- */
static void at2_lpush(json_t*a,const char*s){ json_append(a,json_string(s?s:"")); }
static void at2_lpushf(json_t*a,const char*f,...){ va_list ap; va_start(ap,f); int n=vsnprintf(NULL,0,f,ap); va_end(ap); if(n<0)n=0; char*b=(char*)malloc((size_t)n+1); va_start(ap,f); vsnprintf(b,(size_t)n+1,f,ap); va_end(ap); json_append(a,json_string(b)); free(b); }
static char *at2_join(json_t*a,const char*sep){ at2_sb b; at2_sb_init(&b); for(size_t i=0;i<json_len(a);i++){ if(i)at2_sb_add(&b,sep); json_t*v=json_get(a,i); at2_sb_add(&b,(v&&v->type==JSON_STRING)?v->str_val:""); } return at2_sb_take(&b); }
static json_t *at2_joinj(json_t*a,const char*sep){ char*s=at2_join(a,sep); json_t*o=json_string(s); free(s); return o; }
static void at2_addmore(json_t*lines,size_t len,int shown,const char*unit){ if((int)len>shown) at2_lpushf(lines,"... %d more%s",(int)len-shown,unit?unit:""); }
static char *at2_fstr(json_t*o,const char*k,const char*def){ json_t*v=at2_get(o,k); if(!v) return at2_dup(def); return at2_pystr(v); }
static char *at2_comma(long long v){
    char buf[64]; snprintf(buf,sizeof buf,"%lld",v);
    int len=(int)strlen(buf); int ins=(len-1)/3; at2_sb b; at2_sb_init(&b);
    for(int i=0;i<len;i++){ at2_sb_ch(&b,buf[i]); if((len-1-i)%3==0&&i<len-1&&ins>0) at2_sb_ch(&b,','); }
    return at2_sb_take(&b);
}
static char *at2_wsjoin(const char*s){
    at2_sb b; at2_sb_init(&b); const char*p=s?s:""; int first=1;
    while(*p){ while(*p&&at2_isspace((unsigned char)*p))p++; if(!*p)break; if(!first)at2_sb_ch(&b,' '); first=0; while(*p&&!at2_isspace((unsigned char)*p)) at2_sb_ch(&b,*p++); }
    return at2_sb_take(&b);
}
static char *at2_strip0(const char*s){ char*t=at2_strip(s); return t; }
static char *at2_orstr(json_t*o,const char*k,const char*def){ json_t*v=at2_get(o,k); if(!v||!at2_truth(v)) return at2_dup(def); return at2_pystr(v); }

/* PoP: at2__more @ acp_adapter/tools.py:_more */
static json_t *at2__more(at2_io_t*io,const json_t*argv,const json_t*kw){
    (void)io;(void)kw; json_t*items=json_get(argv,0); int shown=(int)(json_len(argv)>1&&json_get(argv,1)?json_get(argv,1)->num_val:0);
    const char*unit=json_len(argv)>2?at2_sarg(json_get(argv,2),""):"";
    json_t*a=json_array(); at2_addmore(a,json_len(items),shown,unit); return a;
}
/* PoP: at2__preview @ acp_adapter/tools.py:_preview */
static json_t *at2__preview(at2_io_t*io,const json_t*argv,const json_t*kw){
    (void)io;(void)kw; const char*label=at2_sarg(json_get(argv,0),"");
    json_t*value=json_get(argv,1); int lim=(int)(json_len(argv)>2?json_get(argv,2)->num_val:0);
    if(!at2_truth(value)) return json_string("");
    char*vs=at2_pystr(value); json_t*av=json_array1(json_string(vs)); json_t*kwo=json_object(); json_set(kwo,"limit",json_number(lim));
    json_t*tr=at2__truncate_text(io,av,kwo); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:"";
    char*out=at2_fmts("\n%s: %s",label,ts); free(vs); json_t*o=json_string(out); free(out); return o;
}
/* PoP: at2__start_todo @ acp_adapter/tools.py:_start_todo */
static json_t *at2__start_todo(at2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*args=json_get(argv,0); json_t*items=at2_get(args,"todos");
    if(!at2_is_arr(items)) return json_string("Reading todo list");
    json_t*lines=json_array(); at2_lpush(lines,"Updating todo list"); at2_lpush(lines,"");
    size_t n=json_len(items); int cap=n<8?(int)n:8;
    for(int i=0;i<cap;i++){ json_t*it=json_get(items,i); if(!at2_is_obj(it))continue;
        json_t*sv=at2_get(it,"status"); char*status=sv?at2_pystr(sv):at2_dup("pending");
        json_t*cv=at2_get(it,"content"); char*content;
        if(cv&&at2_truth(cv)) content=at2_pystr(cv);
        else { json_t*iv=at2_get(it,"id"); content=iv?at2_pystr(iv):at2_dup(""); }
        at2_lpushf(lines,"- %s: %s",status,content); free(status); free(content);
    }
    at2_addmore(lines,n,8,""); return at2_joinj(lines,"\n");
}
/* PoP: at2__start_skill_manage @ acp_adapter/tools.py:_start_skill_manage */
static json_t *at2__start_skill_manage(at2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*args=json_get(argv,0);
    const char*k1[1]={"action"}; char*action=at2_arg(args,k1,1,"manage");
    const char*name=at2_getstr(args,"name","?"); const char*fp=at2_getstr(args,"file_path","SKILL.md");
    char*path=at2_fmts("skills/%s/%s",name,fp);
    if(strcmp(action,"patch")==0){
        char*old=at2_orstr(args,"old_string",""); char*newv=at2_orstr(args,"new_string","");
        json_t*ov=old[0]?json_string(old):json_null();
        return at2_seam(io,"acp.tool_diff_content",at2_A3("path",json_string(path),"old_text",ov,"new_text",json_string(newv)));
    }
    if(strcmp(action,"edit")==0||strcmp(action,"create")==0){
        char*content=at2_orstr(args,"content","");
        return at2_seam(io,"acp.tool_diff_content",at2_A3("path",json_string(path),"old_text",json_null(),"new_text",json_string(content)));
    }
    if(strcmp(action,"write_file")==0){
        char*target=at2_orstr(args,"file_path","file"); char*p2=at2_fmts("skills/%s/%s",name,target);
        char*fc=at2_orstr(args,"file_content","");
        return at2_seam(io,"acp.tool_diff_content",at2_A3("path",json_string(p2),"old_text",json_null(),"new_text",json_string(fc)));
    }
    if(strcmp(action,"delete")==0||strcmp(action,"remove_file")==0){
        char*t=at2_orstr(args,"file_path",fp);
        char*out=at2_fmts("Removing %s from skill '%s'",t,name); json_t*o=json_string(out); free(out); return o;
    }
    char*out=at2_fmts("Running skill_manage action '%s' on skill '%s' (%s)",action,name,fp);
    json_t*o=json_string(out); free(out); return o;
}
/* PoP: at2__start_execute_code @ acp_adapter/tools.py:_start_execute_code */
static json_t *at2__start_execute_code(at2_io_t*io,const json_t*argv,const json_t*kw){
    const char*k1[1]={"code"}; char*code=at2_arg(json_get(argv,0),k1,1,"");
    size_t len=strlen(code); char*preview;
    if(len>1200){ preview=at2_fmts("%.*s\n... (%lu chars total, truncated)",1200,code,(unsigned long)len); }
    else preview=at2_dup(code);
    json_t*pv=json_string(preview); char*r=at2_fmt(pv,"Running Python helper script:\n\n```python\n{}\n```","Running Python helper script");
    json_t*o=json_string(r); free(r); free(preview); free(code); (void)io;(void)kw; return o;
}
/* PoP: at2__start_delegate @ acp_adapter/tools.py:_start_delegate */
static json_t *at2__start_delegate(at2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*args=json_get(argv,0); json_t*tasks=at2_get(args,"tasks");
    if(!at2_is_arr(tasks)||json_len(tasks)==0){
        const char*k1[1]={"goal"}; char*goal=at2_arg(args,k1,1,""); json_t*av=json_array1(json_string(goal));
        json_t*kw2=json_object(); json_set(kw2,"limit",json_number(800.0));
        json_t*tr=at2__truncate_text(io,av,kw2); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:"";
        json_t*tv=json_string(ts); char*fm=at2_fmt(tv,":\n{}","");
        char*out=at2_fmts("Delegating task%s",fm); free(fm); free(goal);
        json_t*o=json_string(out); free(out); return o;
    }
    json_t*lines=json_array(); at2_lpushf(lines,"Delegating %d tasks",(int)json_len(tasks)); at2_lpush(lines,"");
    size_t n=json_len(tasks); int cap=n<8?(int)n:8;
    for(int i=0;i<cap;i++){ json_t*t=json_get(tasks,i); if(!at2_is_obj(t))continue;
        const char*k1[1]={"goal"}; char*goal=at2_arg(t,k1,1,""); json_t*av=json_array1(json_string(goal));
        json_t*kw2=json_object(); json_set(kw2,"limit",json_number(160.0));
        json_t*tr=at2__truncate_text(io,av,kw2); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:"";
        const char*k2[1]={"role"}; char*role=at2_arg(t,k2,1,""); json_t*rv=json_string(role);
        char*fm=at2_fmt(rv," ({})","");
        at2_lpushf(lines,"%d. %s%s",i+1,ts,fm); free(fm); free(role); free(goal);
    }
    at2_addmore(lines,n,8,""); return at2_joinj(lines,"\n");
}
/* START_CONTENT_BUILDERS dispatch -> built value (str or non-str ACP block), or None */
static json_t *at2_start_builder(at2_io_t*io,const char*tool,json_t*arguments){
    if(strcmp(tool,"patch")==0){ char*mode=at2_fstr(arguments,"mode","replace"); char*path=at2_orstr(arguments,"path","patch input"); char*o=at2_fmts("Preparing %s edit for %s. Approval prompt shows the diff.",mode,path); free(mode);free(path); return json_string(o); }
    if(strcmp(tool,"write_file")==0){ json_t*pv=at2_get(arguments,"path"); json_t*pvv=pv?json_copy(pv):json_string(""); char*r=at2_fmt(pvv,"Preparing write to {}. Approval prompt shows the diff.","Preparing file write. Approval prompt shows the diff."); json_t*o=json_string(r); free(r); return o; }
    if(strcmp(tool,"terminal")==0){ char*c=at2_fstr(arguments,"command",""); char*o=at2_fmts("$ %s",c); free(c); return json_string(o); }
    if(strcmp(tool,"read_file")==0||strcmp(tool,"web_extract")==0) return NULL;
    if(strcmp(tool,"search_files")==0){ char*pat=at2_fstr(arguments,"pattern",""); char*tgt=at2_fstr(arguments,"target","content"); char*base=at2_fmts("Searching for '%s' (%s)",pat,tgt); json_t*pv=at2_get(arguments,"path"); json_t*pvv=pv?json_copy(pv):json_null(); char*fm=at2_fmt(pvv," in {}",""); char*o=at2_fmts("%s%s",base,fm); free(base);free(fm);free(pat);free(tgt); return json_string(o); }
    if(strcmp(tool,"todo")==0) return at2__start_todo(io,json_array_of(arguments),json_object());
    if(strcmp(tool,"skill_view")==0){ const char*k1[2]={"name","file_path"}; char*n=at2_arg(arguments,k1,1,"?"); char*f=at2_arg(arguments,k1+1,1,"SKILL.md"); char*o=at2_fmts("Loading skill '%s' (%s)",n,f); free(n);free(f); return json_string(o); }
    if(strcmp(tool,"skill_manage")==0) return at2__start_skill_manage(io,json_array_of(arguments),json_object());
    if(strcmp(tool,"execute_code")==0) return at2__start_execute_code(io,json_array_of(arguments),json_object());
    if(strcmp(tool,"web_search")==0){ const char*k1[1]={"query"}; char*q=at2_arg(arguments,k1,1,""); json_t*qv=json_string(q); char*r=at2_fmt(qv,"Searching the web for: {}","Searching the web"); json_t*o=json_string(r); free(r);free(q); return o; }
    if(strcmp(tool,"process")==0){ const char*k1[1]={"action"}; char*a=at2_arg(arguments,k1,1,"manage"); char*base=at2_fmts("Process action: %s",a); const char*k2[1]={"session_id"}; char*sid=at2_arg(arguments,k2,1,""); json_t*sv=json_string(sid); char*fm=at2_fmt(sv,"\nSession: {}",""); const char*k3[1]={"data"}; char*dat=at2_arg(arguments,k3,1,""); json_t*pr=at2__preview(io,json_array3(json_string("Input"),json_string(dat),json_number(500.0)),json_object()); char*pvs=(pr&&pr->type==JSON_STRING)?pr->str_val:""; char*o=at2_fmts("%s%s%s",base,fm,pvs); free(a);free(base);free(fm);free(sid);free(dat); return json_string(o); }
    if(strcmp(tool,"delegate_task")==0) return at2__start_delegate(io,json_array_of(arguments),json_object());
    if(strcmp(tool,"session_search")==0){ const char*k1[1]={"query"}; char*q=at2_arg(arguments,k1,1,""); json_t*qv=json_string(q); char*r=at2_fmt(qv,"Searching past sessions for: {}","Loading recent sessions"); json_t*o=json_string(r); free(r);free(q); return o; }
    if(strcmp(tool,"memory")==0){ const char*k1[2]={"action","target"}; char*a=at2_arg(arguments,k1,1,"manage"); char*t=at2_arg(arguments,k1+1,1,"memory"); char*base=at2_fmts("Memory %s (%s)",a,t); const char*k2[2]={"content","old_text"}; char*pv=at2_arg(arguments,k2,2,""); json_t*pr=at2__preview(io,json_array3(json_string("Preview"),json_string(pv),json_number(500.0)),json_object()); char*pvs=(pr&&pr->type==JSON_STRING)?pr->str_val:""; char*o=at2_fmts("%s%s",base,pvs); free(a);free(t);free(base);free(pv); return json_string(o); }
    return NULL;
}
/* PoP: at2_extract_locations @ acp_adapter/tools.py:extract_locations */
static json_t *at2_extract_locations(at2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*args=json_get(argv,0); json_t*path=at2_get(args,"path");
    if(!path||!at2_truth(path)) return json_array();
    json_t*ln=at2_get(args,"offset"); if(!ln||!at2_truth(ln)) ln=at2_get(args,"line"); if(!ln||!at2_truth(ln)) ln=json_null();
    json_t*r=at2_seam(io,"ToolCallLocation",at2_A2("path",json_copy(path),"line",json_copy(ln)));
    if(at2_is_raise(r)) return r; json_t*a=json_array(); json_append(a,r); return a;
}
/* PoP: at2_build_tool_abandoned @ acp_adapter/tools.py:build_tool_abandoned */
static json_t *at2_build_tool_abandoned(at2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; json_t*id=json_get(argv,0); json_t*tn=json_get(argv,1);
    json_t*kind=at2_get_tool_kind(io,json_array_of(tn?tn:json_null()),json_object());
    json_t*txt=at2__text(io,json_array1(json_string("This tool call ended without a result (blocked, denied, or interrupted).")),json_object());
    if(at2_is_raise(txt)) return txt;
    json_t*content=json_array(); json_append(content,txt);
    return at2_seam(io,"acp.update_tool_call",at2_A5("tool_call_id",json_copy(id),"kind",kind,"status",json_string("failed"),"content",content,"raw_output",json_null()));
}
/* forward decl */
static json_t *at2__build_tool_start(at2_io_t*,const json_t*,const json_t*);
/* PoP: at2_build_tool_start @ acp_adapter/tools.py:build_tool_start */
static json_t *at2_build_tool_start(at2_io_t*io,const json_t*argv,const json_t*kw){
    json_t*r=at2__build_tool_start(io,argv,kw);
    if(!at2_is_raise(r)||!at2_caught(r,"Exception")) return r;
    /* logger.debug("ACP tool-start render failed for %r: %s", tool_name, exc) */
    json_t*tn=json_get(argv,1); const char*tns=(tn&&tn->type==JSON_STRING)?tn->str_val:"None";
    char*exc=at2_pystr(r); char*msg=at2_fmts("ACP tool-start render failed for %r: %s",tns,exc); free(exc);
    json_t*lr=at2_seam(io,"logger.debug",at2_A3("msg",json_string(msg),"tool_name",json_copy(tn),"exc",json_copy(r))); free(msg);
    (void)lr;
    const char*safe=(at2_is_str(tn)&&at2_truth(tn))?tns:"tool";
    json_t*kind=at2_get_tool_kind(io,json_array1(json_string(safe)),json_object());
    return at2_seam(io,"acp.start_tool_call",at2_A6("tool_call_id",json_copy(json_get(argv,0)),"title",json_string(safe),"kind",kind,"content",json_null(),"locations",json_array(),"raw_input",json_null()));
}
/* PoP: at2__build_tool_start @ acp_adapter/tools.py:_build_tool_start */
static json_t *at2__build_tool_start(at2_io_t*io,const json_t*argv,const json_t*kw){
    json_t*id=json_get(argv,0); json_t*tn=json_get(argv,1); json_t*arguments=json_get(argv,2);
    const char*tool=(tn&&tn->type==JSON_STRING)?tn->str_val:"";
    json_t*edit=json_obj_get((json_t*)kw,"edit_diff");
    json_t*content=NULL; json_t*raw_input=NULL;
    if((strcmp(tool,"patch")==0||strcmp(tool,"write_file")==0)&&edit&&!at2_is_null(edit)){
        json_t*block=at2_seam(io,"acp.tool_diff_content",at2_A3("path",json_copy(at2_get(edit,"path")),"old_text",json_copy(at2_get(edit,"old_text")),"new_text",json_copy(at2_get(edit,"new_text"))));
        if(at2_is_raise(block)) return block;
        content=json_array(); json_append(content,block);
    } else {
        int known=0;
        static const char*BUILDERS[]={"patch","write_file","terminal","read_file","search_files","todo","skill_view","skill_manage","execute_code","web_search","web_extract","process","delegate_task","session_search","memory",NULL};
        for(int i=0;BUILDERS[i];i++) if(strcmp(tool,BUILDERS[i])==0){known=1;break;}
        if(known){
            json_t*built=at2_start_builder(io,tool,arguments?arguments:json_object());
            if(at2_is_raise(built)) return built;
            if(!built) content=NULL;
            else if(at2_is_str(built)){ json_t*txt=at2__text(io,json_array1(built),json_object()); if(at2_is_raise(txt))return txt; content=json_array(); json_append(content,txt); }
            else { content=json_array(); json_append(content,built); }
        } else if(at2_polished(tool)){
            json_t*aj=at2__args_json(io,json_array_of(arguments?arguments:json_null()),json_object());
            if(at2_is_raise(aj)) return aj;
            json_t*kw2=json_object(); json_set(kw2,"limit",json_number(1200.0));
            json_t*tr=at2__truncate_text(io,json_array1(aj),kw2); if(at2_is_raise(tr))return tr;
            json_t*txt=at2__text(io,json_array1(tr),json_object()); if(at2_is_raise(txt))return txt;
            content=json_array(); json_append(content,txt);
        } else if(!arguments||!at2_truth(arguments)){
            content=NULL;
        } else {
            json_t*aj=at2__args_json(io,json_array_of(arguments),json_object()); if(at2_is_raise(aj)) return aj;
            json_t*txt=at2__text(io,json_array1(aj),json_object()); if(at2_is_raise(txt))return txt;
            content=json_array(); json_append(content,txt); raw_input=json_copy(arguments);
        }
    }
    /* arg eval order in the start_tool_call call: title(build_tool_preview), kind, content, locations, raw_input */
    json_t*title=at2_build_tool_title(io,json_array2(json_copy(tn),json_copy(arguments?arguments:json_null())),json_object());
    if(at2_is_raise(title)) return title;
    json_t*kind=at2_get_tool_kind(io,json_array_of(tn?tn:json_null()),json_object());
    json_t*locs=at2_extract_locations(io,json_array_of(arguments?arguments:json_object()),json_object());
    if(at2_is_raise(locs)) return locs;
    return at2_seam(io,"acp.start_tool_call",at2_A6("tool_call_id",json_copy(id),"title",title,"kind",kind,"content",content?content:json_null(),"locations",locs,"raw_input",raw_input?raw_input:json_null()));
}
/* ================= completion formatters (wrapper + raw body) ================= */
static json_t *at2_wrap(at2_io_t*io,const json_t*argv,int text_fallback,
    json_t *(*raw)(at2_io_t*,const char*,json_t*,json_t*)){
    json_t*tn=json_get(argv,0); json_t*result=json_get(argv,1); json_t*args=json_get(argv,2);
    json_t*av=json_array1(result); json_t*data=at2__json_loads_maybe(io,av,json_object());
    if(at2_is_raise(data)) return data;
    if(at2_is_obj(data)) return raw(io,(tn&&tn->type==JSON_STRING)?tn->str_val:"",data,args&&at2_truth(args)?args:json_object());
    if(text_fallback) return at2_nonempty(result);
    return NULL;
}
static const char *at2_icon(const char*status,const char*kind){
    if(strcmp(kind,"todo")==0){
        if(!strcmp(status,"completed"))return "\xe2\x9c\x85"; if(!strcmp(status,"in_progress"))return "\xf0\x9f\x94\x84";
        if(!strcmp(status,"pending"))return "\xe2\x8f\xb3"; if(!strcmp(status,"cancelled"))return "\xe2\x9c\x97";
        return "\xe2\x80\xa2";
    }
    if(!strcmp(status,"completed"))return "\xe2\x9c\x85"; if(!strcmp(status,"failed")||!strcmp(status,"error"))return "\xe2\x9c\x97";
    if(!strcmp(status,"timeout"))return "\xe2\x8f\xb1"; if(!strcmp(status,"interrupted"))return "\xe2\x9a\xa0";
    return "\xe2\x80\xa2";
}
static int at2_todo_depth(char**ids,json_t**items,int cnt,json_t*item){
    int depth=0; char*seen[8]={0}; int ns=0; json_t*cur=item;
    for(int it=0;it<8;it++){
        json_t*pv=at2_get(cur,"parent"); char*par;
        if(pv&&at2_truth(pv)) par=at2_pystr(pv); else par=at2_dup("");
        if(!par[0]){free(par);break;}
        int found=-1; for(int i=0;i<cnt;i++) if(ids[i]&&strcmp(ids[i],par)==0){found=i;break;}
        int ins=0; for(int i=0;i<ns;i++) if(seen[i]&&strcmp(seen[i],par)==0){ins=1;break;}
        if(found<0||ins){free(par);break;}
        seen[ns++]=par; depth++; cur=items[found]; if(depth>=4)break;
    }
    return depth>4?4:depth;
}
/* PoP: at2__format_todo_result @ acp_adapter/tools.py:_format_todo_result */
static json_t *at2_raw_todo(at2_io_t*io,const char*tn,json_t*data,json_t*args){
    (void)tn;(void)io;(void)args; json_t*todos=at2_get(data,"todos");
    if(!at2_is_arr(todos)) return NULL;
    int cnt=(int)json_len(todos); json_t*items[512]; char*ids[512]; int m=0;
    for(int i=0;i<cnt&&m<512;i++){ json_t*t=json_get(todos,i); if(!at2_is_obj(t))continue;
        items[m]=t; json_t*iv=at2_get(t,"id"); ids[m]=(iv&&at2_truth(iv))?at2_pystr(iv):at2_dup(""); m++;
    }
    json_t*lines=json_array(); at2_lpush(lines,"**Todo list**"); at2_lpush(lines,"");
    for(int i=0;i<m;i++){
        json_t*it=items[i]; char*cv=at2_orstr(it,"content",""); if(!cv[0]){free(cv);cv=at2_orstr(it,"id","");}
        char*content=at2_strip(cv); free(cv);
        if(content[0]){
            int d=at2_todo_depth(ids,items,m,it);
            char*status=at2_orstr(it,"status","pending");
            const char*icon=at2_icon(status,"todo");
            char pad[32]; pad[0]=0; for(int k=0;k<d;k++){ if(strlen(pad)+2<sizeof pad)strcat(pad,"  "); }
            at2_lpushf(lines,"%s- %s %s",pad,icon,content); free(status);
        }
        free(content);
    }
    json_t*summary=at2_get(data,"summary");
    if(at2_is_obj(summary)&&at2_truth(summary)){
        char*cancelled=at2_fstr(summary,"cancelled","0");
        char*completed=at2_fstr(summary,"completed","0");
        char*inprog=at2_fstr(summary,"in_progress","0");
        char*pend=at2_fstr(summary,"pending","0");
        char*progress=at2_fmts("%s completed, %s in progress, %s pending",completed,inprog,pend);
        json_t*cv=at2_get(summary,"cancelled"); if(!cv) cv=json_number(0.0);
        if(at2_truth(cv)){ char*p2=at2_fmts("%s, %s cancelled",progress,cancelled); free(progress); progress=p2; }
        at2_lpush(lines,""); at2_lpushf(lines,"**Progress:** %s",progress);
        free(progress);free(cancelled);free(completed);free(inprog);free(pend);
    }
    return at2_joinj(lines,"\n");
}
static json_t *at2_fmt_todo(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,0,at2_raw_todo); }

/* PoP: at2__format_read_file_result @ acp_adapter/tools.py:_format_read_file_result */
static json_t *at2_raw_read_file(at2_io_t*io,const char*tn,json_t*data,json_t*a){
    (void)tn; json_t*err=at2_get(data,"error"); json_t*content=at2_get(data,"content");
    if(at2_truth(err)&&!at2_truth(content)){ char*es=at2_pystr(err); char*o=at2_fmts("Read failed: %s",es); free(es); json_t*r=json_string(o); free(o); return r; }
    if(!at2_is_str(content)) return NULL;
    json_t*rb=json_array();
    static const char*KEYS[2]={"offset","limit"}; static const char*LAB[2]={"from line","limit"};
    for(int i=0;i<2;i++){ json_t*v=at2_get(a,KEYS[i]); if(v&&at2_truth(v)){ char*vs=at2_pystr(v); at2_lpushf(rb,"%s %s",LAB[i],vs); free(vs); } }
    json_t*pv=at2_get(a,"path"); if(!pv||!at2_truth(pv)) pv=at2_get(data,"path"); if(!pv||!at2_truth(pv)) pv=json_string("file");
    char*ps=at2_pystr(pv); char*path=at2_strip(ps); free(ps);
    at2_sb h; at2_sb_init(&h); at2_sb_addf(&h,"Read %s",path);
    if(json_len(rb)>0){ char*joined=at2_join(rb,", "); at2_sb_addf(&h," (%s)",joined); free(joined); }
    json_t*tl=at2_get(data,"total_lines");
    if(tl&&tl->type!=JSON_NULL){ char*ts=at2_pystr(tl); at2_sb_addf(&h," \xe2\x80\x94 %s total lines",ts); free(ts); }
    json_t*kwargs=json_object(); json_set(kwargs,"language",json_string(""));
    json_t*fenced=at2__fenced_text(io,json_array2(json_copy(content),json_null()),json_object());
    char*body=at2_fmts("%s\n\n%s",h.p?h.p:"",(fenced&&fenced->type==JSON_STRING)?fenced->str_val:"");
    json_t*av=json_array1(json_string(body)); json_t*res=at2__truncate_text(io,av,json_object());
    free(body); free(path); free(h.p); return res;
}
static json_t *at2_fmt_read_file(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,0,at2_raw_read_file); }

/* PoP: at2__format_search_files_result @ acp_adapter/tools.py:_format_search_files_result */
static json_t *at2_raw_search(at2_io_t*io,const char*tn,json_t*data,json_t*args){
    (void)tn;(void)args; json_t*files=at2_get(data,"files"); json_t*matches=at2_get(data,"matches");
    if(at2_is_arr(files)){
        int shown=(int)json_len(files)<20?(int)json_len(files):20;
        json_t*tc=at2_get(data,"total_count"); json_t*cnt=tc?tc:json_number((double)json_len(files));
        char*pl=at2_plural(cnt,"file","s");
        json_t*lines=json_array(); at2_lpush(lines,"File search results"); at2_lpushf(lines,"Found %s; showing %d.",pl,shown); at2_lpush(lines,"");
        for(int i=0;i<shown;i++){ char*vs=at2_pystr(json_get(files,i)); at2_lpushf(lines,"- %s",vs); free(vs); }
        if(at2_truth(at2_get(data,"truncated"))||(int)json_len(files)>shown){ at2_lpush(lines,""); at2_lpush(lines,"Results truncated. Narrow the search, add path/file_glob, or use offset to page."); }
        char*joined=at2_join(lines,"\n"); json_t*av=json_array1(json_string(joined)); json_t*kw=json_object(); json_set(kw,"limit",json_number(7000.0));
        json_t*r=at2__truncate_text(io,av,kw); free(joined);free(pl); return r;
    }
    if(!at2_is_arr(matches)) return NULL;
    int shown=(int)json_len(matches)<12?(int)json_len(matches):12;
    json_t*tc=at2_get(data,"total_count"); json_t*cnt=tc?tc:json_number((double)json_len(matches));
    char*pl=at2_plural(cnt,"match","es");
    json_t*lines=json_array(); at2_lpush(lines,"Search results"); at2_lpushf(lines,"Found %s; showing %d.",pl,shown); at2_lpush(lines,"");
    for(int i=0;i<shown;i++){
        json_t*mt=json_get(matches,i);
        if(!at2_is_obj(mt)){ char*vs=at2_pystr(mt); at2_lpushf(lines,"- %s",vs); free(vs); continue; }
        const char*k3[3]={"path","file","filename"};
        json_t*pv=at2_first(mt,k3,3,json_string("?")); char*path=at2_pystr(pv);
        json_t*ln=at2_get(mt,"line"); if(!ln||!at2_truth(ln)) ln=at2_get(mt,"line_number");
        if(ln&&at2_truth(ln)){ char*ls=at2_pystr(ln); at2_lpushf(lines,"- %s:%s",path,ls); free(ls); }
        else at2_lpushf(lines,"- %s",path);
        json_t*cv=at2_get(mt,"content"); if(!cv||!at2_truth(cv)) cv=at2_get(mt,"text");
        if(cv&&at2_truth(cv)){
            char*cvs=at2_pystr(cv); char*stripped=at2_strip(cvs); free(cvs);
            if(stripped[0]){ char*ws=at2_wsjoin(stripped); json_t*av=json_array1(json_string(ws)); json_t*kw=json_object(); json_set(kw,"limit",json_number(300.0));
                json_t*tr=at2__truncate_text(io,av,kw); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:"";
                at2_lpushf(lines,"  %s",ts); free(ws); }
            free(stripped);
        }
        free(path);
    }
    if(at2_truth(at2_get(data,"truncated"))||(int)json_len(matches)>shown){ at2_lpush(lines,""); at2_lpush(lines,"Results truncated. Narrow the search, add file_glob, or use offset to page."); }
    char*joined=at2_join(lines,"\n"); json_t*av=json_array1(json_string(joined)); json_t*kw=json_object(); json_set(kw,"limit",json_number(7000.0));
    json_t*r=at2__truncate_text(io,av,kw); free(joined);free(pl); return r;
}
static json_t *at2_fmt_search(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,0,at2_raw_search); }

/* PoP: at2__format_execute_code_result @ acp_adapter/tools.py:_format_execute_code_result */
static json_t *at2_raw_execute(at2_io_t*io,const char*tn,json_t*data,json_t*args){
    (void)tn;(void)args; json_t*lines=json_array();
    json_t*ec=at2_get(data,"exit_code");
    if(ec&&ec->type!=JSON_NULL){ char*es=at2_pystr(ec); at2_lpushf(lines,"Exit code: %s",es); free(es); }
    else at2_lpush(lines,"Execution complete");
    if(at2_truth(at2_get(data,"stdout_truncated"))){
        json_t*total=at2_get(data,"stdout_bytes_total"); json_t*captured=at2_get(data,"stdout_bytes_captured"); json_t*omitted=at2_get(data,"stdout_bytes_omitted");
        if(at2_is_intlike(total)&&at2_is_intlike(captured)&&at2_is_intlike(omitted)){
            char*cs=at2_comma((long long)(captured->type==JSON_BOOL?(captured->bool_val?1:0):(long long)captured->num_val));
            char*ts=at2_comma((long long)(total->type==JSON_BOOL?(total->bool_val?1:0):(long long)total->num_val));
            char*os=at2_comma((long long)(omitted->type==JSON_BOOL?(omitted->bool_val?1:0):(long long)omitted->num_val));
            at2_lpush(lines,""); at2_lpushf(lines,"Output truncated: captured %s of %s bytes (%s omitted).",cs,ts,os);
            free(cs);free(ts);free(os);
        } else { at2_lpush(lines,""); at2_lpush(lines,"Output truncated."); }
    }
    static const char*K[3]={"warning","output","error"}; static const char*L[3]={"Warning:","Output:","Error:"};
    for(int i=0;i<3;i++){
        json_t*v=at2_get(data,K[i]); char*vs;
        if(i==0){ if(!v||!at2_truth(v)){vs=at2_dup("");} else { char*p=at2_pystr(v); vs=at2_strip(p); free(p);} }
        else { vs=(v&&at2_truth(v))?at2_pystr(v):at2_dup(""); }
        if(vs[0]){ at2_lpush(lines,""); at2_lpush(lines,L[i]); at2_lpush(lines,vs); }
        free(vs);
    }
    char*joined=at2_join(lines,"\n"); json_t*av=json_array1(json_string(joined));
    json_t*r=at2__truncate_text(io,av,json_object()); free(joined); return r;
}
static json_t *at2_fmt_execute(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,1,at2_raw_execute); }

/* PoP: at2__format_skill_view_result @ acp_adapter/tools.py:_format_skill_view_result */
static json_t *at2_raw_skill_view(at2_io_t*io,const char*tn,json_t*data,json_t*args){
    (void)tn;(void)args;
    if(at2_is_false(at2_get(data,"success"))){ char*es=at2_fstr(data,"error","unknown error"); char*o=at2_fmts("Skill view failed: %s",es); free(es); json_t*r=json_string(o); free(o); return r; }
    char*content=at2_orstr(data,"content","");
    json_t*lines=json_array(); at2_lpush(lines,"**Skill loaded**"); at2_lpush(lines,"");
    char*nm=at2_orstr(data,"name","skill"); at2_lpushf(lines,"- **Name:** `%s`",nm); free(nm);
    json_t*fpv=at2_get(data,"file"); if(!fpv||!at2_truth(fpv)) fpv=at2_get(data,"path");
    char*fp=at2_orstr(data,"file","SKILL.md"); if(!fpv||!at2_truth(fpv)){free(fp);fp=at2_dup("SKILL.md");} else {free(fp);fp=at2_pystr(fpv);}
    at2_lpushf(lines,"- **File:** `%s`",fp); free(fp);
    char*desc=at2_orstr(data,"description",""); char*ds=at2_strip(desc); free(desc);
    if(ds[0]){ at2_lpushf(lines,"- **Description:** %s",ds); } free(ds);
    if(content[0]){ char*cs=at2_comma((long long)strlen(content)); at2_lpushf(lines,"- **Content:** %s chars loaded into agent context",cs); free(cs); }
    json_t*linked=at2_get(data,"linked_files");
    if(at2_is_obj(linked)&&at2_truth(linked)){
        int sum=0; for(size_t i=0;i<linked->c.count;i++){ json_t*v=linked->c.items[i]; if(at2_is_arr(v)) sum+=(int)json_len(v); }
        at2_lpushf(lines,"- **Linked files:** %d",sum);
    }
    /* headings: for each line in content.splitlines(): s=line.strip(); if s.startswith('#'): h=s.lstrip('#').strip(); if h keep <=8 */
    int nh=0; char*heads[8]={NULL};
    const char*p=content; while(*p){
        const char*e=strchr(p,'\n'); size_t len=e?(size_t)(e-p):strlen(p);
        char*line=(char*)malloc(len+1); memcpy(line,p,len); line[len]=0;
        if(len&&line[len-1]=='\r')line[len-1]=0;
        char*sl=at2_strip(line); free(line);
        if(sl[0]=='#'){ char*h=at2_lstrip_hashes(sl); if(h[0]&&nh<8)heads[nh++]=h; else free(h); }
        free(sl);
        if(!e)break; p=e+1;
    }
    if(nh>0){ at2_lpush(lines,""); at2_lpush(lines,"**Sections**"); for(int i=0;i<nh;i++){ at2_lpushf(lines,"- %s",heads[i]); free(heads[i]); } }
    at2_lpush(lines,""); at2_lpush(lines,"_Full skill content is available to the agent but hidden here to keep ACP readable._");
    free(content); return at2_joinj(lines,"\n");
}
static json_t *at2_fmt_skill_view(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,0,at2_raw_skill_view); }

/* PoP: at2__format_skill_manage_result @ acp_adapter/tools.py:_format_skill_manage_result */
static json_t *at2_raw_skill_manage(at2_io_t*io,const char*tn,json_t*data,json_t*a){
    (void)tn;(void)io; const char*k1[1]={"action"}; char*action=at2_arg(a,k1,1,"manage");
    json_t*nv=at2_get(a,"name"); if(!nv||!at2_truth(nv))nv=at2_get(data,"name");
    char*name=at2_orstr(a,"name","skill"); if(!nv||!at2_truth(nv)){free(name);name=at2_dup("skill");} else {free(name);name=at2_pystr(nv);} char*ns=at2_strip(name);free(name);name=ns; if(!name[0]){free(name);name=at2_dup("skill");}
    json_t*fv=at2_get(a,"file_path"); if(!fv||!at2_truth(fv))fv=at2_get(data,"file_path");
    char*fp=at2_orstr(a,"file_path","SKILL.md"); if(!fv||!at2_truth(fv)){free(fp);fp=at2_dup("SKILL.md");} else {free(fp);fp=at2_pystr(fv);} char*fs=at2_strip(fp);free(fp);fp=fs; if(!fp[0]){free(fp);fp=at2_dup("SKILL.md");}
    const char*status=at2_is_false(at2_get(data,"success"))?"\xe2\x9c\x97 Skill update failed":"\xe2\x9c\x85 Skill updated";
    json_t*lines=json_array(); at2_lpushf(lines,"**%s**",status); at2_lpush(lines,""); at2_lpushf(lines,"- **Action:** `%s`",action); at2_lpushf(lines,"- **Skill:** `%s`",name);
    if(strcmp(action,"delete")!=0) at2_lpushf(lines,"- **File:** `%s`",fp);
    json_t*mv=at2_get(data,"message"); if(!mv||!at2_truth(mv))mv=at2_get(data,"error");
    char*msg=at2_orstr(data,"message",""); if(!mv||!at2_truth(mv)){free(msg);msg=at2_dup("");} else {free(msg);msg=at2_pystr(mv);} char*ms=at2_strip(msg);free(msg);msg=ms;
    if(msg[0]) at2_lpushf(lines,"- **Result:** %s",msg);
    json_t*rv=at2_get(data,"replacements"); if(!rv||!at2_truth(rv))rv=at2_get(data,"replacement_count");
    if(rv&&rv->type!=JSON_NULL){ char*rs=at2_pystr(rv); at2_lpushf(lines,"- **Replacements:** %s",rs); free(rs); }
    json_t*pv=at2_get(data,"path"); if(pv&&at2_truth(pv)){ char*ps0=at2_pystr(pv); char*ps=at2_strip(ps0); free(ps0); if(ps[0])at2_lpushf(lines,"- **Path:** `%s`",ps); free(ps); }
    free(action);free(name);free(fp);free(msg); return at2_joinj(lines,"\n");
}
static json_t *at2_fmt_skill_manage(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,0,at2_raw_skill_manage); }

/* PoP: at2__format_web_search_result @ acp_adapter/tools.py:_format_web_search_result */
static json_t *at2_raw_web_search(at2_io_t*io,const char*tn,json_t*data,json_t*args){
    (void)tn;(void)args; json_t*dv=at2_get(data,"data"); json_t*web=at2_is_obj(dv)?at2_get(dv,"web"):at2_get(data,"web");
    if(!at2_is_arr(web)) return NULL;
    json_t*lines=json_array(); at2_lpushf(lines,"Web results: %d",(int)json_len(web));
    int cap=json_len(web)<10?(int)json_len(web):10;
    for(int i=0;i<cap;i++){
        json_t*it=json_get(web,i); if(!at2_is_obj(it))continue;
        char*url0=at2_orstr(it,"url",""); char*url=at2_strip(url0);free(url0);
        char*t0=at2_orstr(it,"title",""); if(!t0[0]){free(t0);t0=at2_orstr(it,"url","result");} char*title=at2_strip(t0);free(t0);
        if(url[0]) at2_lpushf(lines,"\xe2\x80\xa2 %s \xe2\x80\x94 %s",title,url);
        else at2_lpushf(lines,"\xe2\x80\xa2 %s",title);
        char*d0=at2_orstr(it,"description",""); char*desc=at2_strip(d0);free(d0);
        if(desc[0]) at2_lpushf(lines,"  %s",desc);
        free(url);free(title);free(desc);
    }
    char*joined=at2_join(lines,"\n"); json_t*av=json_array1(json_string(joined));
    json_t*r=at2__truncate_text(io,av,json_object()); free(joined); return r;
}
static json_t *at2_fmt_web_search(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,0,at2_raw_web_search); }

/* PoP: at2__format_web_extract_result @ acp_adapter/tools.py:_format_web_extract_result */
static json_t *at2_raw_web_extract(at2_io_t*io,const char*tn,json_t*data,json_t*args){
    (void)tn;(void)args;
    if(at2_is_false(at2_get(data,"success"))&&at2_truth(at2_get(data,"error"))){ char*es=at2_pystr(at2_get(data,"error")); char*o=at2_fmts("Web extract failed: %s",es); free(es); json_t*r=json_string(o); free(o); return r; }
    json_t*results=at2_get(data,"results"); if(!at2_is_arr(results)) return NULL;
    json_t*fails=json_array(); int cap=json_len(results)<10?(int)json_len(results):10;
    for(int i=0;i<cap;i++){
        json_t*it=json_get(results,i); char*e0=at2_is_obj(it)?at2_orstr(it,"error",""):at2_dup(""); char*err=at2_strip(e0);free(e0);
        if(!err[0]||strcmp(err,"None")==0||strcmp(err,"null")==0){free(err);continue;}
        char*u0=at2_is_obj(it)?at2_orstr(it,"url",""):at2_dup(""); char*url=at2_strip(u0);free(u0);
        char*t0=at2_is_obj(it)?at2_orstr(it,"title",""):at2_dup(""); if(!t0[0]){free(t0);t0=at2_dup(url);} if(!t0[0]){free(t0);t0=at2_dup("Untitled");} char*title=at2_strip(t0);free(t0);
        char*where=(url[0]&&strcmp(url,title)!=0)?at2_fmts(" \xe2\x80\x94 %s",url):at2_dup("");
        json_t*av=json_array1(json_string(err)); json_t*kw=json_object(); json_set(kw,"limit",json_number(500.0));
        json_t*tr=at2__truncate_text(io,av,kw); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:"";
        at2_lpushf(fails,"- %s%s\n  Error: %s",title,where,ts);
        free(where);free(title);free(url);free(err);
    }
    if(json_len(fails)==0) return NULL;
    char*pl=at2_plural(json_number((double)json_len(fails)),"URL","s");
    json_t*lines=json_array(); at2_lpushf(lines,"Web extract failed for %s",pl);
    for(size_t i=0;i<json_len(fails);i++) at2_lpush(lines,at2_sarg(json_get(fails,i),""));
    free(pl); return at2_joinj(lines,"\n");
}
static json_t *at2_fmt_web_extract(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,0,at2_raw_web_extract); }
/* PoP: at2__format_process_result @ acp_adapter/tools.py:_format_process_result */
static json_t *at2_raw_process(at2_io_t*io,const char*tn,json_t*data,json_t*args){
    (void)tn;
    if(at2_is_false(at2_get(data,"success"))&&at2_truth(at2_get(data,"error"))){ char*es=at2_pystr(at2_get(data,"error")); char*o=at2_fmts("Process error: %s",es); free(es); json_t*r=json_string(o); free(o); return r; }
    const char*k1[1]={"action"}; char*action=at2_arg(args,k1,1,"process");
    json_t*procs=at2_get(data,"processes");
    if(at2_is_arr(procs)){
        json_t*lines=json_array(); at2_lpushf(lines,"Processes: %d",(int)json_len(procs));
        int cap=json_len(procs)<20?(int)json_len(procs):20;
        for(int i=0;i<cap;i++){
            json_t*p=json_get(procs,i);
            if(!at2_is_obj(p)){ char*vs=at2_pystr(p); at2_lpushf(lines,"- %s",vs); free(vs); continue; }
            json_t*st=at2_get(p,"status"); char*status;
            if(!st||!at2_truth(st)){ json_t*ex=at2_get(p,"exited"); status=at2_dup((ex&&at2_truth(ex))?"exited":"running"); }
            else status=at2_pystr(st);
            json_t*bits=json_array(); at2_lpush(bits,status);
            static const char*K[2]={"pid","exit_code"}; static const char*L[2]={"pid","exit"};
            for(int j=0;j<2;j++){ json_t*v=at2_get(p,K[j]); if(v&&v->type!=JSON_NULL){ char*vs=at2_pystr(v); char*b=at2_fmts("%s %s",L[j],vs); at2_lpush(bits,b); free(b);free(vs); } }
            char*cmd0=at2_orstr(p,"command",""); char*cmd=at2_strip(cmd0);free(cmd0);
            const char*k2[2]={"session_id","id"}; json_t*sid=at2_first(p,k2,2,json_string("?")); char*ss=at2_pystr(sid);
            char*bb=at2_join(bits,", ");
            char*base=at2_fmts("- `%s` \xe2\x80\x94 %s",ss,bb);
            if(cmd[0]){ char*cut=cmd; char c120[121]; size_t cl=strlen(cmd); if(cl>120){memcpy(c120,cmd,120);c120[120]=0;cut=c120;} char*o=at2_fmts("%s \xe2\x80\x94 %s",base,cut); free(base); base=o; }
            at2_lpush(lines,base);
            free(base);free(bb);free(ss);free(status);free(cmd);
        }
        at2_addmore(lines,json_len(procs),20," process(es)"); return at2_joinj(lines,"\n");
    }
    json_t*sv=at2_get(data,"status"); if(!sv||!at2_truth(sv))sv=at2_get(data,"state");
    char*status=at2_orstr(data,"status",""); if(!sv||!at2_truth(sv)){free(status);status=at2_dup("");} else {free(status);status=at2_pystr(sv);} char*sts=at2_strip(status);free(status);status=sts; if(!status[0]){free(status);status=at2_dup(action);}
    json_t*dv=at2_get(data,"session_id"); if(!dv||!at2_truth(dv))dv=at2_get(args,"session_id");
    char*sid0=at2_orstr(data,"session_id",""); if(!dv||!at2_truth(dv)){free(sid0);sid0=at2_dup("");} else {free(sid0);sid0=at2_pystr(dv);} char*sid=at2_strip(sid0);free(sid0);
    json_t*lines=json_array();
    char*base=at2_fmts("Process %s: %s",action,status);
    if(sid[0]){ char*o=at2_fmts("%s (`%s`)",base,sid); free(base); base=o; }
    at2_lpush(lines,base); free(base);
    static const char*K[5]={"command","pid","exit_code","returncode","lines"}; static const char*L[5]={"Command","PID","Exit code","Exit code","Lines"};
    for(int j=0;j<5;j++){ json_t*v=at2_get(data,K[j]); if(v&&v->type!=JSON_NULL){ char*vs=at2_pystr(v); at2_lpushf(lines,"- **%s:** %s",L[j],vs); free(vs); } }
    const char*k3[4]={"output","new_output","log","stdout"}; json_t*out=at2_first(data,k3,4,json_null());
    const char*k4[2]={"error","stderr"}; json_t*err=at2_first(data,k4,2,json_null());
    static const char*OL[2]={"Output:","Error:"}; int lims[2]={5000,2000}; json_t*vals[2]={out,err};
    for(int j=0;j<2;j++){ if(at2_truth(vals[j])){ char*vs=at2_pystr(vals[j]); json_t*av=json_array1(json_string(vs)); json_t*kw=json_object(); json_set(kw,"limit",json_number(lims[j])); json_t*tr=at2__truncate_text(io,av,kw); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:""; at2_lpush(lines,""); at2_lpush(lines,OL[j]); at2_lpush(lines,ts); free(vs); } }
    json_t*msg=at2_get(data,"message");
    if(at2_truth(msg)&&!at2_truth(out)&&!at2_truth(err)){ char*ms=at2_pystr(msg); at2_lpush(lines,ms); free(ms); }
    char*joined=at2_join(lines,"\n"); json_t*av=json_array1(json_string(joined)); json_t*kw=json_object(); json_set(kw,"limit",json_number(7000.0));
    json_t*r=at2__truncate_text(io,av,kw); free(joined);free(action);free(status);free(sid); return r;
}
static json_t *at2_fmt_process(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,1,at2_raw_process); }

/* PoP: at2__format_delegate_result @ acp_adapter/tools.py:_format_delegate_result */
static json_t *at2_raw_delegate(at2_io_t*io,const char*tn,json_t*data,json_t*args){
    (void)args; json_t*results=at2_get(data,"results");
    if(!at2_is_arr(results)){
        json_t*e=at2_get(data,"error");
        if(e&&at2_truth(e)){ char*es=at2_pystr(e); char*o=at2_fmts("Delegation failed: %s",es); free(es); json_t*r=json_string(o); free(o); return r; }
        return NULL;
    }
    json_t*total=at2_get(data,"total_duration_seconds");
    char*pl=at2_plural(json_number((double)json_len(results)),"task","s");
    json_t*lines=json_array();
    char*base=at2_fmts("Delegation results: %s",pl);
    if(total&&total->type!=JSON_NULL){ char*ts=at2_pystr(total); char*o=at2_fmts("%s in %ss",base,ts); free(base); base=o; free(ts); }
    at2_lpush(lines,base); free(base);free(pl);
    for(size_t i=0;i<json_len(results);i++){
        json_t*it=json_get(results,i);
        if(!at2_is_obj(it)){ char*vs=at2_pystr(it); at2_lpushf(lines,"- %s",vs); free(vs); continue; }
        json_t*idx=at2_get(it,"task_index"); char*stat=at2_orstr(it,"status","unknown");
        char*head=at2_fmts("%s Task %s: %s",at2_icon(stat,"delegate"),
            (at2_is_intlike(idx)&&idx)?at2_fmts("%lld",(idx->type==JSON_BOOL?(idx->bool_val?1LL:0LL):(long long)idx->num_val)+1):at2_dup("?"),stat);
        json_t*bits=json_array();
        json_t*mv=at2_get(it,"model"); if(mv&&at2_truth(mv)){ char*vs=at2_pystr(mv); at2_lpush(bits,vs); free(vs); }
        json_t*role=at2_get(it,"_child_role"); if(role&&at2_truth(role)){ char*vs=at2_pystr(role); char*b=at2_fmts("role=%s",vs); at2_lpush(bits,b); free(b);free(vs); }
        json_t*ds=at2_get(it,"duration_seconds"); if(ds&&ds->type!=JSON_NULL){ char*vs=at2_pystr(ds); char*b=at2_fmts("%ss",vs); at2_lpush(bits,b); free(b);free(vs); }
        char*bb=at2_join(bits,", ");
        if(bb[0]){ char*o=at2_fmts("%s (%s)",head,bb); free(head); head=o; }
        at2_lpush(lines,""); at2_lpush(lines,head);
        static const char*K[2]={"summary","error"}; static const char*P[2]={"","Error: "}; int LIM[2]={1200,800};
        for(int j=0;j<2;j++){ char*v0=at2_orstr(it,K[j],""); char*v=at2_strip(v0);free(v0);
            if(v[0]){ json_t*av=json_array1(json_string(v)); json_t*kw=json_object(); json_set(kw,"limit",json_number(LIM[j])); json_t*tr=at2__truncate_text(io,av,kw); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:""; char*o=at2_fmts("%s%s",P[j],ts); at2_lpush(lines,o); free(o); }
            free(v);
        }
        json_t*trace=at2_get(it,"tool_trace"); if(!at2_is_arr(trace)) trace=json_array();
        json_t*names=json_array();
        for(size_t k=0;k<json_len(trace);k++){ json_t*t=json_get(trace,k); if(!at2_is_obj(t))continue; char*vs=at2_orstr(t,"tool","?"); at2_lpush(names,vs); free(vs); }
        if(json_len(names)>0){
            at2_sb b; at2_sb_init(&b); at2_sb_add(&b,"Tools: "); int cap=json_len(names)<12?(int)json_len(names):12;
            for(int k=0;k<cap;k++){ if(k)at2_sb_add(&b,", "); at2_sb_add(&b,at2_sarg(json_get(names,k),"")); }
            if(json_len(names)>12) at2_sb_addf(&b," (+%d)",(int)json_len(names)-12);
            char*o=at2_sb_take(&b); at2_lpush(lines,o); free(o);
        }
        free(head);free(bb);free(stat);
    }
    char*joined=at2_join(lines,"\n"); json_t*av=json_array1(json_string(joined)); json_t*kw=json_object(); json_set(kw,"limit",json_number(8000.0));
    json_t*r=at2__truncate_text(io,av,kw); free(joined); return r;
}
static json_t *at2_fmt_delegate(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,0,at2_raw_delegate); }

/* PoP: at2__format_session_search_result @ acp_adapter/tools.py:_format_session_search_result */
static json_t *at2_raw_session_search(at2_io_t*io,const char*tn,json_t*data,json_t*args){
    (void)tn;(void)args;
    if(at2_is_false(at2_get(data,"success"))){ char*es=at2_fstr(data,"error","unknown error"); char*o=at2_fmts("Session search failed: %s",es); free(es); json_t*r=json_string(o); free(o); return r; }
    json_t*results=at2_get(data,"results"); if(!at2_is_arr(results)) return NULL;
    json_t*mode=at2_get(data,"mode"); int recent=(mode&&at2_truth(mode)&&at2_is_str(mode)&&strcmp(mode->str_val,"recent")==0);
    json_t*lines=json_array();
    if(recent) at2_lpush(lines,"Recent sessions");
    else { json_t*qv=at2_get(data,"query"); char*fm=at2_fmt(qv," for `{}`",""); char*o=at2_fmts("Session search results%s",fm); at2_lpush(lines,o); free(o);free(fm); }
    if(json_len(results)==0){ json_t*m=at2_get(data,"message"); char*ms;
        if(m&&at2_truth(m)) ms=at2_pystr(m); else ms=at2_dup("No matching sessions found.");
        at2_lpush(lines,ms); free(ms); return at2_joinj(lines,"\n"); }
    for(size_t i=0;i<json_len(results);i++){
        json_t*it=json_get(results,i); if(!at2_is_obj(it))continue;
        const char*k1[2]={"title","when"}; json_t*tv=at2_first(it,k1,2,json_string("Untitled session")); char*t0=at2_pystr(tv); char*title=at2_strip(t0);free(t0);
        const char*k2[3]={"last_active","started_at","when"}; json_t*wv=at2_first(it,k2,3,json_string("")); char*w0=at2_pystr(wv); char*when=at2_strip(w0);free(w0);
        json_t*count=at2_get(it,"message_count");
        json_t*meta=json_array(); if(when[0])at2_lpush(meta,when);
        json_t*src=at2_get(it,"source"); if(src&&at2_truth(src)){ char*s0=at2_pystr(src); char*ss=at2_strip(s0);free(s0); if(ss[0])at2_lpush(meta,ss); free(ss); }
        if(count&&count->type!=JSON_NULL){ char*cs=at2_pystr(count); char*o=at2_fmts("%s msgs",cs); at2_lpush(meta,o); free(o);free(cs); }
        char*ms=at2_join(meta,", ");
        json_t*sid=at2_get(it,"session_id"); char*sids=(sid&&at2_truth(sid))?at2_pystr(sid):at2_dup("?");
        char*base=at2_fmts("- **%s** (`%s`)",title,sids);
        if(ms[0]){ char*o=at2_fmts("%s \xe2\x80\x94 %s",base,ms); free(base); base=o; }
        at2_lpush(lines,base);
        json_t*sm=at2_get(it,"summary"); if(!sm||!at2_truth(sm))sm=at2_get(it,"preview");
        char*sv=at2_orstr(it,"summary",""); if(!sm||!at2_truth(sm)){free(sv);sv=at2_dup("");} else {free(sv);sv=at2_pystr(sm);} char*ss=at2_strip(sv);free(sv);
        if(ss[0]){ char*ws=at2_wsjoin(ss); json_t*av=json_array1(json_string(ws)); json_t*kw=json_object(); json_set(kw,"limit",json_number(500.0)); json_t*tr=at2__truncate_text(io,av,kw); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:""; char*o=at2_fmts("  %s",ts); at2_lpush(lines,o); free(o);free(ws); }
        free(ss);free(ms);free(base);free(title);free(when);free(sids);
    }
    char*joined=at2_join(lines,"\n"); json_t*av=json_array1(json_string(joined)); json_t*kw=json_object(); json_set(kw,"limit",json_number(7000.0));
    json_t*r=at2__truncate_text(io,av,kw); free(joined); return r;
}
static json_t *at2_fmt_session_search(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,0,at2_raw_session_search); }

/* PoP: at2__format_memory_result @ acp_adapter/tools.py:_format_memory_result */
static json_t *at2_raw_memory(at2_io_t*io,const char*tn,json_t*data,json_t*args){
    (void)tn;(void)io; const char*k1[1]={"action"}; char*action=at2_arg(args,k1,1,"memory");
    json_t*tv=at2_get(data,"target"); if(!tv||!at2_truth(tv))tv=at2_get(args,"target");
    char*t0=at2_orstr(data,"target","memory"); if(!tv||!at2_truth(tv)){free(t0);t0=at2_dup("memory");} else {free(t0);t0=at2_pystr(tv);} char*target=t0;
    json_t*lines=json_array();
    if(at2_is_false(at2_get(data,"success"))){
        char*es=at2_orstr(data,"error","unknown error");
        at2_lpushf(lines,"\xe2\x9c\x97 Memory %s failed (%s)",action,target); at2_lpush(lines,es);
        json_t*ms=at2_get(data,"matches");
        if(at2_is_arr(ms)&&json_len(ms)>0){ at2_lpush(lines,"Matches:"); int cap=json_len(ms)<5?(int)json_len(ms):5;
            for(int i=0;i<cap;i++){ char*vs=at2_pystr(json_get(ms,i)); json_t*av=json_array1(json_string(vs)); json_t*kw=json_object(); json_set(kw,"limit",json_number(160.0)); json_t*tr=at2__truncate_text(NULL,av,kw); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:""; at2_lpushf(lines,"- %s",ts); free(ts);free(vs); } }
        free(es); free(action);free(target); return at2_joinj(lines,"\n");
    }
    at2_lpushf(lines,"\xe2\x9c\x85 Memory %s saved (%s)",action,target);
    json_t*m=at2_get(data,"message"); if(m&&at2_truth(m)){ char*ms=at2_pystr(m); at2_lpush(lines,ms); free(ms); }
    json_t*ec=at2_get(data,"entry_count"); if(ec&&ec->type!=JSON_NULL){ char*es=at2_pystr(ec); at2_lpushf(lines,"Entries: %s",es); free(es); }
    json_t*u=at2_get(data,"usage"); if(u&&at2_truth(u)){ char*us=at2_pystr(u); at2_lpushf(lines,"Usage: %s",us); free(us); }
    const char*k2[2]={"content","old_text"}; char*pv=at2_arg(args,k2,2,"");
    if(pv[0]){ json_t*av=json_array1(json_string(pv)); json_t*kw=json_object(); json_set(kw,"limit",json_number(300.0)); json_t*tr=at2__truncate_text(io,av,kw); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:""; char*o=at2_fmts("Preview: %s",ts); at2_lpush(lines,o); free(o); }
    free(pv);free(action);free(target); return at2_joinj(lines,"\n");
}
static json_t *at2_fmt_memory(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,0,at2_raw_memory); }

/* PoP: at2__format_edit_result @ acp_adapter/tools.py:_format_edit_result (no decorator) */
static json_t *at2_fmt_edit(at2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; const char*tn=at2_sarg(json_get(argv,0),""); json_t*result=json_get(argv,1); json_t*args=json_get(argv,2);
    json_t*av=json_array1(result); json_t*data=at2__json_loads_maybe(io,av,json_object());
    if(at2_is_raise(data)) return data;
    json_t*pv=at2_get(args,"path"); char*p0=(pv&&at2_truth(pv))?at2_pystr(pv):at2_dup("file"); char*path=at2_strip(p0);free(p0);
    char*done=at2_fmts("\xe2\x9c\x85 %s completed",tn);
    if(path[0]){ char*o=at2_fmts("%s for `%s`",done,path); free(done); done=o; }
    if(!at2_is_obj(data)){
        json_t*tx=at2_nonempty(result);
        if(tx){ json_t*ta=json_array1(tx); json_t*kw2=json_object(); json_set(kw2,"limit",json_number(3000.0)); json_t*r=at2__truncate_text(io,ta,kw2); free(done);free(path); return r; }
        free(path); json_t*o=json_string(done); free(done); return o;
    }
    char*prefix=at2_fmts("%s failed for %s",tn,path);
    json_t*fail=at2__failure(io,json_array2(json_copy(data),json_string(prefix)),json_object()); free(prefix);
    if(fail&&!at2_is_raise(fail)){ json_t*o=fail; free(done);free(path); return o; }
    json_t*lines=json_array(); at2_lpush(lines,done);
    char*mv=at2_orstr(data,"message",""); char*ms=at2_strip(mv);free(mv); if(ms[0])at2_lpush(lines,ms); free(ms);
    json_t*rv=at2_get(data,"replacements"); if(!rv||!at2_truth(rv))rv=at2_get(data,"replacement_count");
    if(rv&&rv->type!=JSON_NULL){ char*rs=at2_pystr(rv); at2_lpushf(lines,"Replacements: %s",rs); free(rs); }
    json_t*fm=at2_get(data,"files_modified");
    if(at2_is_arr(fm)&&json_len(fm)>0){ at2_sb b; at2_sb_init(&b); at2_sb_add(&b,"Files: "); int cap=json_len(fm)<8?(int)json_len(fm):8;
        for(int i=0;i<cap;i++){ if(i)at2_sb_add(&b,", "); char*vs=at2_pystr(json_get(fm,i)); at2_sb_addf(&b,"`%s`",vs); free(vs); }
        char*o=at2_sb_take(&b); at2_lpush(lines,o); free(o); }
    free(done);free(path); return at2_joinj(lines,"\n");
}

/* PoP: at2__format_browser_result @ acp_adapter/tools.py:_format_browser_result */
static json_t *at2_raw_browser(at2_io_t*io,const char*tn,json_t*data,json_t*args){
    (void)args; char*pfx=at2_fmts("%s failed",tn); json_t*fail=at2__failure(io,json_array2(json_copy(data),json_string(pfx)),json_object()); free(pfx);
    if(fail&&!at2_is_raise(fail)) return fail;
    json_t*images=NULL;
    if(strcmp(tn,"browser_get_images")==0){ images=at2_get(data,"images"); if(!images||!at2_truth(images))images=at2_get(data,"data"); }
    if(at2_is_arr(images)){
        json_t*lines=json_array(); at2_lpushf(lines,"Images found: %d",(int)json_len(images));
        int cap=json_len(images)<12?(int)json_len(images):12;
        for(int i=0;i<cap;i++){ json_t*img=json_get(images,i); if(!at2_is_obj(img))continue;
            char*u0=at2_orstr(img,"url",""); if(!u0[0]){free(u0);u0=at2_orstr(img,"src","");} char*u=at2_strip(u0);free(u0);
            char*a0=at2_orstr(img,"alt",""); char*alt=at2_strip(a0);free(a0);
            if(alt[0]){ if(u[0])at2_lpushf(lines,"- %s \xe2\x80\x94 %s",alt,u); else at2_lpushf(lines,"- %s",alt); }
            else { if(u[0])at2_lpushf(lines,"- image \xe2\x80\x94 %s",u); else at2_lpush(lines,"- image"); }
            free(alt);free(u);
        }
        char*joined=at2_join(lines,"\n"); json_t*av=json_array1(json_string(joined)); json_t*kw=json_object(); json_set(kw,"limit",json_number(5000.0)); json_t*r=at2__truncate_text(io,av,kw); free(joined); return r;
    }
    const char*k1[4]={"title","url","status"}; json_t*tv=at2_first(data,k1,3,json_string(tn)); char*t0=at2_pystr(tv); char*title=at2_strip(t0);free(t0);
    json_t*lines=json_array(); at2_lpush(lines,title);
    json_t*url=at2_get(data,"url");
    if(url&&at2_truth(url)){ char*us=at2_pystr(url); if(strcmp(us,title)!=0)at2_lpush(lines,us); free(us); }
    const char*k2[5]={"text","content","snapshot","analysis","message"}; json_t*txtv=at2_first(data,k2,5,json_string("")); char*tx0=at2_pystr(txtv); char*tx=at2_strip(tx0);free(tx0);
    if(tx[0]){ json_t*av=json_array1(json_string(tx)); json_t*kw=json_object(); json_set(kw,"limit",json_number(5000.0)); json_t*tr=at2__truncate_text(io,av,kw); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:""; at2_lpush(lines,""); at2_lpush(lines,ts); }
    free(tx);free(title);
    char*joined=at2_join(lines,"\n"); json_t*av=json_array1(json_string(joined)); json_t*kw=json_object(); json_set(kw,"limit",json_number(7000.0)); json_t*r=at2__truncate_text(io,av,kw); free(joined); return r;
}
static json_t *at2_fmt_browser(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,1,at2_raw_browser); }

/* PoP: at2__format_media_or_cron_result @ acp_adapter/tools.py:_format_media_or_cron_result */
static json_t *at2_raw_media(at2_io_t*io,const char*tn,json_t*data,json_t*args){
    (void)args; char*pfx=at2_fmts("%s failed",tn); json_t*fail=at2__failure(io,json_array2(json_copy(data),json_string(pfx)),json_object()); free(pfx);
    if(fail&&!at2_is_raise(fail)) return fail;
    static const char*K[9]={"file_path","path","url","image_url","job_id","id","status","message","next_run"};
    json_t*lines=json_array(); char*head=at2_fmts("\xe2\x9c\x85 %s completed",tn); at2_lpush(lines,head); free(head);
    for(int i=0;i<9;i++){ json_t*v=at2_get(data,K[i]); if(v&&at2_truth(v)){ char*vs=at2_pystr(v); at2_lpushf(lines,"- **%s:** %s",K[i],vs); free(vs); } }
    return at2_joinj(lines,"\n");
}
static json_t *at2_fmt_media(at2_io_t*io,const json_t*argv,const json_t*kw){ (void)kw; return at2_wrap(io,argv,1,at2_raw_media); }
/* ================= recursive structured value + generic formatter ================= */
static int at2_emptyish(json_t*v){
    if(!v||v->type==JSON_NULL) return 1;
    if(v->type==JSON_STRING) return v->str_val[0]==0;
    if(v->type==JSON_ARRAY||v->type==JSON_OBJECT) return v->c.count==0;
    return 0;
}
static json_t **at2_struct_value(at2_io_t*io,const char*key,json_t*value,int indent,int max_depth,int max_items,int *out_n);
static json_t **at2_sv_cat(at2_io_t*io,int indent,const char*label,int max_depth,int max_items,const char*text,int *n,json_t***arr){
    (void)io;(void)max_depth;(void)max_items; char pad[64]; pad[0]=0; for(int i=0;i<indent&&i<31;i++)strcat(pad,"  ");
    char*line;
    if(label&&label[0]) line=at2_fmts("%s- %s %s",pad,label,text);
    else line=at2_fmts("%s- %s",pad,text);
    json_t**r=(json_t**)realloc(*arr,(*n+1)*sizeof(json_t*)); r[*n]=json_string(line); (*n)++; *arr=r; free(line); return r;
}
static void at2_sv_push(json_t***arr,int*n,const char*s){ json_t**r=(json_t**)realloc(*arr,(*n+1)*sizeof(json_t*)); r[*n]=json_string(s?s:""); (*n)++; *arr=r; }
static void at2_sv_pushf(json_t***arr,int*n,const char*f,...){ va_list ap; va_start(ap,f); int z=vsnprintf(NULL,0,f,ap); va_end(ap); if(z<0)z=0; char*b=(char*)malloc((size_t)z+1); va_start(ap,f); vsnprintf(b,(size_t)z+1,f,ap); va_end(ap); at2_sv_push(arr,n,b); free(b); }
/* returns a json ARRAY of strings */
static json_t *at2__format_structured_value(at2_io_t*io,const json_t*argv,const json_t*kw){
    const char*key=at2_sarg(json_get(argv,0),""); json_t*value=json_get(argv,1);
    int indent=json_obj_get(kw,"indent")?(int)json_obj_get((json_t*)kw,"indent")->num_val:0;
    int max_depth=json_obj_get(kw,"max_depth")?(int)json_obj_get((json_t*)kw,"max_depth")->num_val:3;
    int max_items=json_obj_get(kw,"max_items")?(int)json_obj_get((json_t*)kw,"max_items")->num_val:8;
    (void)io; json_t*out=json_array();
    char pad[64]; pad[0]=0; for(int i=0;i<indent&&i<31;i++)strcat(pad,"  ");
    char bullet[70]; snprintf(bullet,sizeof bullet,"%s- ",pad);
    char*label=key&&key[0]?at2_fmts("**%s:**",key):at2_dup("");
    if(at2_emptyish(value)) { free(label); return out; }
    if(max_depth<=0){
        char*prev;
        if(at2_is_obj(value)||at2_is_arr(value)){
            json_t*ja=json_object(); json_set(ja,"obj",json_copy(value)); json_set(ja,"indent",json_null());
            json_set(ja,"ensure_ascii",json_bool(false)); json_set(ja,"default",json_string("str"));
            json_t*jr=at2_seam(io,"json.dumps",ja); if(at2_is_raise(jr)) return jr;
            prev=(jr&&jr->type==JSON_STRING)?at2_dup(jr->str_val):at2_pystr(jr);
        } else prev=at2_pystr(value);
        json_t*av=json_array1(json_string(prev)); json_t*kw2=json_object(); json_set(kw2,"limit",json_number(240.0));
        json_t*tr=at2__truncate_text(io,av,kw2); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:"";
        char*line;
        if(label[0]) line=at2_fmts("%s%s %s",bullet,label,ts); else line=at2_fmts("%s%s",bullet,ts);
        at2_lpush(out,line); free(line);free(prev);free(label); return out;
    }
    if(at2_is_obj(value)){
        char*head=label[0]?at2_fmts("%s%s",bullet,label):at2_fmts("%s%d fields",bullet,(int)value->c.count);
        at2_lpush(out,head); free(head);
        int shown=0;
        for(size_t i=0;i<value->c.count;i++){
            json_t*cv=value->c.items[i]; if(at2_emptyish(cv))continue;
            json_t*kw2=json_object(); json_set(kw2,"indent",json_number(indent+1)); json_set(kw2,"max_depth",json_number(max_depth-1)); json_set(kw2,"max_items",json_number(max_items));
            json_t*av=json_array2(json_string(value->c.keys[i]?value->c.keys[i]:""),json_copy(cv));
            json_t*sub=at2__format_structured_value(io,av,kw2);
            for(size_t k=0;k<json_len(sub);k++) at2_lpush(out,at2_sarg(json_get(sub,k),""));
            shown++;
            if(shown>=max_items){ if((int)value->c.count>shown) at2_lpushf(out,"%s  - ... %d more fields",pad,(int)value->c.count-shown); break; }
        }
        free(label); return out;
    }
    if(at2_is_arr(value)){
        char*pl=at2_plural(json_number((double)json_len(value)),"item","s");
        char*line; if(label[0])line=at2_fmts("%s%s %s",bullet,label,pl); else line=at2_fmts("%s%s",bullet,pl);
        at2_lpush(out,line); free(line);free(pl);
        int cap=json_len(value)<(size_t)max_items?(int)json_len(value):max_items;
        for(int i=0;i<cap;i++){
            json_t*item=json_get(value,i);
            if(at2_is_obj(item)){
                const char*k[5]={"content","message","title","name","id"};
                json_t*hv=at2_first(item,k,5,json_string("")); char*h0=at2_pystr(hv); char*head=at2_strip(h0);free(h0);
                if(head[0]){
                    json_t*av=json_array1(json_string(head)); json_t*kw2=json_object(); json_set(kw2,"limit",json_number(220.0));
                    json_t*tr=at2__truncate_text(io,av,kw2); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:"";
                    at2_lpushf(out,"%s  %d. %s",pad,i+1,ts);
                    static const char*CK[8]={"id","status","type","scope","quality_score","score","path","url"};
                    for(int q=0;q<8;q++){ json_t*v=at2_get(item,CK[q]); if(!at2_emptyish(v)){ char*vs=at2_pystr(v); json_t*av2=json_array1(json_string(vs)); json_t*kw3=json_object(); json_set(kw3,"limit",json_number(180.0)); json_t*tr2=at2__truncate_text(io,av2,kw3); char*ts2=(tr2&&tr2->type==JSON_STRING)?tr2->str_val:""; at2_lpushf(out,"%s    - **%s:** %s",pad,CK[q],ts2); free(vs); } }
                } else {
                    at2_lpushf(out,"%s  %d.",pad,i+1);
                    int c2=0; for(size_t q=0;q<item->c.count&&c2<max_items;q++){ json_t*cv=item->c.items[q];
                        json_t*kw2=json_object(); json_set(kw2,"indent",json_number(indent+2)); json_set(kw2,"max_depth",json_number(max_depth-1)); json_set(kw2,"max_items",json_number(max_items));
                        json_t*av=json_array2(json_string(item->c.keys[q]?item->c.keys[q]:""),json_copy(cv));
                        json_t*sub=at2__format_structured_value(io,av,kw2);
                        for(size_t k=0;k<json_len(sub);k++)at2_lpush(out,at2_sarg(json_get(sub,k),"")); c2++;
                    }
                }
                free(head);
            } else if(at2_is_arr(item)){
                at2_lpushf(out,"%s  %d. %d items",pad,i+1,(int)json_len(item));
                int cap2=json_len(item)<(size_t)max_items?(int)json_len(item):max_items;
                for(int q=0;q<cap2;q++){ json_t*kw2=json_object(); json_set(kw2,"indent",json_number(indent+2)); json_set(kw2,"max_depth",json_number(max_depth-1)); json_set(kw2,"max_items",json_number(max_items));
                    json_t*av=json_array2(json_string(""),json_copy(json_get(item,q))); json_t*sub=at2__format_structured_value(io,av,kw2);
                    for(size_t k=0;k<json_len(sub);k++)at2_lpush(out,at2_sarg(json_get(sub,k),"")); }
            } else {
                char*vs=at2_pystr(item); json_t*av=json_array1(json_string(vs)); json_t*kw2=json_object(); json_set(kw2,"limit",json_number(240.0)); json_t*tr=at2__truncate_text(io,av,kw2); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:""; at2_lpushf(out,"%s  %d. %s",pad,i+1,ts); free(vs);
            }
        }
        if((int)json_len(value)>max_items) at2_lpushf(out,"%s  ... %d more items",pad,(int)json_len(value)-max_items);
        free(label); return out;
    }
    char*vs=at2_pystr(value); json_t*av=json_array1(json_string(vs)); json_t*kw2=json_object(); json_set(kw2,"limit",json_number(500.0)); json_t*tr=at2__truncate_text(io,av,kw2); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:"";
    char*line; if(label[0])line=at2_fmts("%s%s %s",bullet,label,ts); else line=at2_fmts("%s%s",bullet,ts); at2_lpush(out,line); free(line);free(vs);free(label); return out;
}
static json_t **at2_struct_value(at2_io_t*io,const char*key,json_t*value,int indent,int max_depth,int max_items,int *out_n){
    (void)io;(void)key;(void)value;(void)indent;(void)max_depth;(void)max_items;(void)out_n; return NULL; /* unused shim */
}
static void at2_struct_extend(json_t*lines,json_t*sub){ for(size_t k=0;k<json_len(sub);k++) at2_lpush(lines,at2_sarg(json_get(sub,k),"")); }

static const char *AT2_PRIORITY[]={"message","status","id","task_id","issue_id","title","name","entity_id","state","service","url","path","file_path","count","total","next_run",NULL};
static int at2_priority(const char*k){ for(int i=0;AT2_PRIORITY[i];i++) if(strcmp(k,AT2_PRIORITY[i])==0)return 1; return 0; }
static int at2_skipkey(const char*k){ return !strcmp(k,"success")||!strcmp(k,"raw")||!strcmp(k,"content")||!strcmp(k,"entries"); }

/* PoP: at2__format_generic_structured_result @ acp_adapter/tools.py:_format_generic_structured_result */
static json_t *at2__format_generic_structured_result(at2_io_t*io,const json_t*argv,const json_t*kw){
    const char*tn=at2_sarg(json_get(argv,0),""); json_t*result=json_get(argv,1);
    int fallback=json_obj_get(kw,"fallback_to_text")?at2_truth(json_obj_get((json_t*)kw,"fallback_to_text")):1;
    json_t*av=json_array1(result); json_t*data=at2__json_loads_maybe(io,av,json_object());
    if(at2_is_raise(data)) return data;
    if(!at2_is_obj(data)&&!at2_is_arr(data)) return fallback?at2_nonempty(result):NULL;
    json_t*lines=json_array();
    if(at2_is_arr(data)){
        char*pl=at2_plural(json_number((double)json_len(data)),"item","s"); at2_lpushf(lines,"%s: %s",tn,pl); free(pl);
        int cap=json_len(data)<12?(int)json_len(data):12;
        for(int i=0;i<cap;i++){ json_t*it=json_get(data,i);
            if(at2_is_obj(it)||at2_is_arr(it)){ json_t*kw2=json_object(); json_set(kw2,"indent",json_number(0)); json_set(kw2,"max_depth",json_number(2)); json_set(kw2,"max_items",json_number(6)); json_t*av2=json_array2(json_string(""),json_copy(it)); json_t*sub=at2__format_structured_value(io,av2,kw2); at2_struct_extend(lines,sub); }
            else { char*vs=at2_pystr(it); json_t*av2=json_array1(json_string(vs)); json_t*kw2=json_object(); json_set(kw2,"limit",json_number(240.0)); json_t*tr=at2__truncate_text(io,av2,kw2); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:""; at2_lpushf(lines,"- %s",ts); free(vs); }
        }
        at2_addmore(lines,json_len(data),12," items");
        char*joined=at2_join(lines,"\n"); json_t*av2=json_array1(json_string(joined)); json_t*kw2=json_object(); json_set(kw2,"limit",json_number(5000.0)); json_t*r=at2__truncate_text(io,av2,kw2); free(joined); return r;
    }
    char*pfx=at2_fmts("%s failed",tn); json_t*fail=at2__failure(io,json_array2(json_copy(data),json_string(pfx)),json_object()); free(pfx);
    if(fail&&!at2_is_raise(fail)) return fail;
    json_t*sv=at2_get(data,"success");
    char*head=at2_is_true(sv)?at2_fmts("\xe2\x9c\x85 %s completed",tn):at2_fmts("%s result",tn); at2_lpush(lines,head); free(head);
    for(int i=0;AT2_PRIORITY[i];i++){ const char*k=AT2_PRIORITY[i]; json_t*v=at2_get(data,k); if(!at2_emptyish(v)){ char*vs=at2_pystr(v); json_t*av2=json_array1(json_string(vs)); json_t*kw2=json_object(); json_set(kw2,"limit",json_number(500.0)); json_t*tr=at2__truncate_text(io,av2,kw2); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:""; at2_lpushf(lines,"- **%s:** %s",k,ts); free(vs); } }
    for(size_t i=0;i<data->c.count;i++){
        const char*k=data->c.keys[i]?data->c.keys[i]:""; json_t*v=data->c.items[i];
        if(at2_priority(k)||at2_skipkey(k)||at2_emptyish(v))continue;
        json_t*kw2=json_object(); json_set(kw2,"indent",json_number(0)); json_set(kw2,"max_depth",json_number(3)); json_set(kw2,"max_items",json_number(8));
        json_t*av2=json_array2(json_string(k),json_copy(v)); json_t*sub=at2__format_structured_value(io,av2,kw2); at2_struct_extend(lines,sub);
        if((int)json_len(lines)>=40){ at2_lpush(lines,"- ... more fields truncated"); break; }
    }
    json_t*ct=at2_get(data,"content");
    if(ct&&ct->type==JSON_STRING){ char*s0=at2_strip(ct->str_val); if(s0[0]){ json_t*av2=json_array1(json_string(s0)); json_t*kw2=json_object(); json_set(kw2,"limit",json_number(1500.0)); json_t*tr=at2__truncate_text(io,av2,kw2); char*ts=(tr&&tr->type==JSON_STRING)?tr->str_val:""; at2_lpush(lines,""); at2_lpush(lines,ts); } free(s0); }
    char*joined=at2_join(lines,"\n"); json_t*av2=json_array1(json_string(joined)); json_t*kw2=json_object(); json_set(kw2,"limit",json_number(7000.0)); json_t*r=at2__truncate_text(io,av2,kw2); free(joined); return r;
}

/* PoP: at2__parse_unified_diff_content @ acp_adapter/tools.py:_parse_unified_diff_content */
static json_t *at2__parse_unified_diff_content(at2_io_t*io,const json_t*argv,const json_t*kw){
    (void)kw; const char*text=at2_sarg(json_get(argv,0),"");
    json_t*content=json_array(); json_t*oldp=NULL,*newp=NULL; json_t*oldl=json_array(),*newl=json_array();
    #define FLUSH() do { \
        json_t*path=(newp&&at2_truth(newp)&&strcmp(newp->str_val,"/dev/null")!=0)?newp:oldp; \
        if(path&&at2_truth(path)&&strcmp(at2_sarg(path,""),"/dev/null")!=0){ \
            char*ps=at2_pystr(path); char*ps2=at2_strip(ps); free(ps); \
            const char*outp=(strncmp(ps2,"a/",2)==0||strncmp(ps2,"b/",2)==0)?ps2+2:ps2; \
            char*ot=json_len(oldl)>0?at2_join(oldl,"\n"):NULL; char*nt=at2_join(newl,"\n"); \
            json_t*ov=ot?json_string(ot):json_null(); json_t*nv=json_string(nt); \
            json_t*r=at2_seam(io,"acp.tool_diff_content",at2_A3("path",json_string(outp),"old_text",ov,"new_text",nv)); \
            if(at2_is_raise(r)) return r; json_append(content,r); free(ot);free(nt);free(ps2); \
        } \
        oldp=NULL;newp=NULL; oldl=json_array(); newl=json_array(); \
    } while(0)
    const char*p=text;
    while(*p){
        const char*e=strchr(p,'\n'); size_t len=e?(size_t)(e-p):strlen(p);
        char*line=(char*)malloc(len+1); memcpy(line,p,len); line[len]=0;
        if(len&&line[len-1]=='\r')line[len-1]=0;
        if(strncmp(line,"--- ",4)==0){ FLUSH(); char*s=at2_strip(line+4); oldp=json_string(s); free(s); }
        else if(strncmp(line,"+++ ",4)==0){ char*s=at2_strip(line+4); newp=json_string(s); free(s); }
        else if(strncmp(line,"@@",2)==0 || (!oldp&&!newp)) { }
        else if(line[0]=='+'){ at2_lpush(newl,line[1]?line+1:""); }
        else if(line[0]=='-'){ at2_lpush(oldl,line[1]?line+1:""); }
        else if(line[0]==' '){ at2_lpush(oldl,line[1]?line+1:""); at2_lpush(newl,line[1]?line+1:""); }
        free(line); if(!e)break; p=e+1;
    }
    FLUSH();
    return content;
}
/* formatter dispatch by tool name */
static json_t *at2_dispatch_formatter(at2_io_t*io,const char*tn,json_t*result,json_t*args){
    json_t*av=json_array3(json_string(tn),result,args);
    if(!strcmp(tn,"todo")) return at2_fmt_todo(io,av,json_object());
    if(!strcmp(tn,"read_file")) return at2_fmt_read_file(io,av,json_object());
    if(!strcmp(tn,"write_file")||!strcmp(tn,"patch")) return at2_fmt_edit(io,av,json_object());
    if(!strcmp(tn,"search_files")) return at2_fmt_search(io,av,json_object());
    if(!strcmp(tn,"execute_code")) return at2_fmt_execute(io,av,json_object());
    if(!strcmp(tn,"process")) return at2_fmt_process(io,av,json_object());
    if(!strcmp(tn,"delegate_task")) return at2_fmt_delegate(io,av,json_object());
    if(!strcmp(tn,"session_search")) return at2_fmt_session_search(io,av,json_object());
    if(!strcmp(tn,"memory")) return at2_fmt_memory(io,av,json_object());
    if(!strcmp(tn,"skill_view")) return at2_fmt_skill_view(io,av,json_object());
    if(!strcmp(tn,"skill_manage")) return at2_fmt_skill_manage(io,av,json_object());
    if(!strcmp(tn,"web_search")) return at2_fmt_web_search(io,av,json_object());
    if(!strcmp(tn,"browser_navigate")||!strcmp(tn,"browser_snapshot")||!strcmp(tn,"browser_vision")||!strcmp(tn,"browser_get_images")) return at2_fmt_browser(io,av,json_object());
    if(!strcmp(tn,"vision_analyze")||!strcmp(tn,"image_generate")||!strcmp(tn,"cronjob")) return at2_fmt_media(io,av,json_object());
    json_t*kw=json_object(); json_set(kw,"fallback_to_text",json_bool(at2_polished(tn)?true:false));
    return at2__format_generic_structured_result(io,json_array2(json_string(tn),result),kw);
}
/* PoP: at2__build_tool_complete_content @ acp_adapter/tools.py:_build_tool_complete_content */
static json_t *at2__build_tool_complete_content(at2_io_t*io,const json_t*argv,const json_t*kw){
    const char*tn=at2_sarg(json_get(argv,0),""); json_t*result=json_get(argv,1);
    json_t*fa=json_obj_get((json_t*)kw,"function_args");
    if(strcmp(tn,"skill_manage")==0){
        json_t*e=at2_seam(io,"extract_edit_diff",at2_A4("tool_name",json_string(tn),"result",json_copy(result),"function_args",json_copy(fa?fa:json_null()),"snapshot",json_copy(json_obj_get((json_t*)kw,"snapshot"))));
        if(at2_is_raise(e)){ if(!at2_caught(e,"Exception")) return e; }
        else if(at2_is_str(e)&&at2_truth(json_string(at2_strip(e->str_val)))){
            json_t*dc=at2__parse_unified_diff_content(io,json_array1(e),json_object());
            if(at2_is_raise(dc)) return dc;
            if(json_len(dc)>0) return dc;
        }
    }
    json_t*text=at2_dispatch_formatter(io,tn,result,fa?fa:json_null());
    if(at2_is_raise(text)) return text;
    if(at2_truth(text)){ json_t*tx=at2__text(io,json_array1(text),json_object()); if(at2_is_raise(tx))return tx; json_t*c=json_array(); json_append(c,tx); return c; }
    json_t*av=json_array1(result); json_t*tr=at2__truncate_text(io,(result&&at2_truth(result))?av:json_array1(json_string("")),json_object());
    if(at2_is_raise(tr)) return tr;
    json_t*tx=at2__text(io,json_array1(tr),json_object()); if(at2_is_raise(tx))return tx; json_t*c=json_array(); json_append(c,tx); return c;
}
/* PoP: at2_build_tool_complete @ acp_adapter/tools.py:build_tool_complete */
static json_t *at2_build_tool_complete(at2_io_t*io,const json_t*argv,const json_t*kw){
    const char*tn=at2_sarg(json_get(argv,1),""); json_t*id=json_get(argv,0); json_t*result=json_get(argv,2);
    json_t*fa=json_len(argv)>3?json_get(argv,3):json_obj_get((json_t*)kw,"function_args");
    int iserr=json_obj_get((json_t*)kw,"is_error")?at2_truth(json_obj_get((json_t*)kw,"is_error")):0;
    json_t*content;
    if(strcmp(tn,"web_extract")==0){
        json_t*et=at2_fmt_web_extract(io,json_array3(json_string(tn),result,fa?fa:json_null()),json_object());
        if(at2_is_raise(et)) return et;
        content=NULL; if(at2_truth(et)){ json_t*tx=at2__text(io,json_array1(et),json_object()); if(at2_is_raise(tx))return tx; content=json_array();json_append(content,tx); }
    } else {
        content=at2__build_tool_complete_content(io,json_array3(json_string(tn),result,fa?fa:json_null()),kw);
        if(at2_is_raise(content)) return content;
    }
    json_t*sa=json_array1(result); json_t*sd=at2__json_loads_maybe(io,sa,json_object()); if(at2_is_raise(sd)) return sd;
    int structured=at2_is_obj(sd)||at2_is_arr(sd);
    int failed=iserr;
    if(!failed){ json_t*fr=at2__tool_result_failed(io,json_array2(json_copy(result),json_string(tn)),json_object()); if(at2_is_raise(fr)) return fr; failed=at2_truth(fr); }
    json_t*raw=NULL;
    if(!at2_polished(tn)&&!structured) raw=json_copy(result);
    return at2_seam(io,"acp.update_tool_call",at2_A5("tool_call_id",json_copy(id),"kind",at2_get_tool_kind(io,json_array1(json_string(tn)),json_object()),"status",json_string(failed?"failed":"completed"),"content",content?content:json_null(),"raw_output",raw?raw:json_null()));
}
