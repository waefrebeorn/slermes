/*
 * gw_conn_ssh_local_flow2.c — FLOW port of hermes_cli/gateway_connections_ssh.py
 * (LIVE authority, the local 127.0.0.1 tunnel facade; sibling of ssh_remote.py).
 * Self-contained: libjson only, all functions static gcl2_*. The harness includes
 * this .c by name, wires the universal seam to scripted results and records the
 * ordered trace.
 *
 * SEAM DOCTRINE (identical to gw_conn_ssh_flow2):
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returns a NEW json_t* (NULL == Python None). A raising live call is scripted
 * {"__raise__":"<ExcClass>","msg":...}; cores propagate where the live try/except
 * catches, and BaseException-only classes pass every `except Exception`. A MISSING
 * seam key fail-closes (RuntimeError); present JSON null is None; a JSON array is
 * one scripted result per call.
 *
 * Seam name table (name -> live call):
 *   http_get            _http_get(url, token); args {url, token} -> {"status":int,"body":obj}
 *   classify_proof      classify_ssh_reuse_proof(proof, nonce); args {proof, nonce}
 *   monotonic           time.monotonic(); args {}
 *   sleep               time.sleep(s); args {s}
 *   default_control_dir default_control_dir(root); args {root}
 *   control_socket_path control_socket_path(user,host,port,dir,key_path,oid); args {...}
 *   is_alive            ssh.is_alive(); args {}
 *   cancel_forward      ssh.cancel_forward(local,remote); args {local,remote}
 *   ssh_open            ssh.open(); args {}
 *   ssh_close           ssh.close(); args {}
 *   resolve_ssh_binary  resolve_ssh_binary(); args {}
 *   connect             connect(ConnectDeps(...)); args {ownership_id,profile,remote_hermes_path,reuse_token,ready_timeout_s}
 *   disconnect_remote   disconnect_remote(ssh, oid); args {ownership_id}
 *   login_shell_quote   login_shell_quote(cwd); args {value}
 *   build_exec_args     build_exec_args(endpoint, command); args {endpoint,command}
 *
 * Fixture domain: paths are plain strings (store_root is the identity for a
 * supplied root); ssh command text is projected away by the seam; the HTTP
 * response body is a JSON object; integral floats are kept out of int positions.
 * SshReachError(kind,message) projects to its message; dataclasses project to a
 * field dict; exceptions project to {__raise__,msg}.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>
#include <stdint.h>
#include "json.h"

#define GCL2_HEALTH_TIMEOUT_S 30.0
#define GCL2_DEFAULT_READY_TIMEOUT_S 120.0

/* ── universal seam + trace ── */
typedef struct gcl2_trace { json_t *events; } gcl2_trace_t;
typedef struct gcl2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gcl2_seams_t;

static void gcl2_trace_add(gcl2_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void gcl2_trace_free(gcl2_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *gcl2_call(const gcl2_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}

/* ── string/buffer helpers ── */
static char *gcl2_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}
typedef struct { char *p; size_t len, cap; } gcl2_buf;
static void gcl2_binit(gcl2_buf *b) { b->p = NULL; b->len = b->cap = 0; }
static void gcl2_bfree(gcl2_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static void gcl2_bput(gcl2_buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->len + n + 1) nc *= 2;
        char *np = realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, s, n); b->len += n; b->p[b->len] = '\0';
}
static void gcl2_bputs(gcl2_buf *b, const char *s) { gcl2_bput(b, s, strlen(s)); }
static void gcl2_bputc(gcl2_buf *b, char c) { gcl2_bput(b, &c, 1); }
static char *gcl2_btake(gcl2_buf *b) {
    if (!b->p) return strdup("");
    char *r = b->p; b->p = NULL; b->len = b->cap = 0; return r;
}

/* ── JSON value helpers ── */
static bool gcl2_jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool gcl2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *gcl2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static double gcl2_jnum(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static long long gcl2_jint(const json_t *v, long long d) {
    if (v && v->type == JSON_NUMBER) return (long long)v->num_val;
    return d;
}
static bool gcl2_jbool(const json_t *v, bool d) {
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
}
/* str(v) for the scalar domains used here (matches Python str() on the fixture domain). */
static char *gcl2_str_scalar(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == floor(d) && isfinite(d) && fabs(d) < 1e16) return gcl2_xsf("%.0f", d);
        char tmp[64]; snprintf(tmp, sizeof(tmp), "%.15g", d); return strdup(tmp);
    }
    return strdup("");
}
/* str(connection.get(k) or "") */
static char *gcl2_str_or_empty(const json_t *v) {
    if (!v || !gcl2_jtruth(v)) return strdup("");
    return gcl2_str_scalar(v);
}
/* int(connection.get(k) or def) */
static long long gcl2_int_or(const json_t *v, long long def) {
    if (!v || !gcl2_jtruth(v)) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_STRING) return strtoll(v->str_val ? v->str_val : "", NULL, 10);
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return def;
}
static bool gcl2_json_eq(const json_t *a, const json_t *b) {
    if (a == b) return true;
    if (!a || !b || a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL: return true;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
        case JSON_ARRAY:
            if (json_len(a) != json_len(b)) return false;
            for (size_t i = 0; i < json_len(a); i++)
                if (!gcl2_json_eq(json_get(a, i), json_get(b, i))) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!gcl2_json_eq(json_obj_get(a, a->c.keys[i]), json_obj_get(b, a->c.keys[i]))) return false;
            return true;
    }
    return false;
}

/* args builders */
static json_t *gcl2_args(void) { return json_object(); }
static void gcl2_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void gcl2_aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void gcl2_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void gcl2_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── exception model (FULL builtin hierarchy) ── */
static const char *gcl2_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool gcl2_is_raise(const json_t *r) { return gcl2_raise_class(r) != NULL; }
static json_t *gcl2_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool gcl2_base_only(const char *k) {
    return !strcmp(k, "BaseException") || !strcmp(k, "KeyboardInterrupt") ||
           !strcmp(k, "SystemExit") || !strcmp(k, "GeneratorExit");
}
static const char *gcl2_exc_parent(const char *k) {
    if (!strcmp(k, "BaseException")) return NULL;
    if (!strcmp(k, "KeyboardInterrupt") || !strcmp(k, "SystemExit") || !strcmp(k, "GeneratorExit") ||
        !strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "Exception")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "Exception";
    if (!strcmp(k, "ArithmeticError")) return "Exception";
    if (!strcmp(k, "BufferError")) return "Exception";
    if (!strcmp(k, "LookupError")) return "Exception";
    if (!strcmp(k, "AssertionError")) return "Exception";
    if (!strcmp(k, "AttributeError")) return "Exception";
    if (!strcmp(k, "EOFError")) return "Exception";
    if (!strcmp(k, "FloatingPointError")) return "ArithmeticError";
    if (!strcmp(k, "OverflowError")) return "ArithmeticError";
    if (!strcmp(k, "ZeroDivisionError")) return "ArithmeticError";
    if (!strcmp(k, "ImportError")) return "Exception";
    if (!strcmp(k, "ModuleNotFoundError")) return "ImportError";
    if (!strcmp(k, "IndexError")) return "LookupError";
    if (!strcmp(k, "KeyError")) return "LookupError";
    if (!strcmp(k, "MemoryError")) return "Exception";
    if (!strcmp(k, "NameError")) return "Exception";
    if (!strcmp(k, "UnboundLocalError")) return "NameError";
    if (!strcmp(k, "OSError")) return "Exception";
    if (!strcmp(k, "BlockingIOError")) return "OSError";
    if (!strcmp(k, "ChildProcessError")) return "OSError";
    if (!strcmp(k, "ConnectionError")) return "OSError";
    if (!strcmp(k, "BrokenPipeError")) return "ConnectionError";
    if (!strcmp(k, "ConnectionAbortedError")) return "ConnectionError";
    if (!strcmp(k, "ConnectionRefusedError")) return "ConnectionError";
    if (!strcmp(k, "ConnectionResetError")) return "ConnectionError";
    if (!strcmp(k, "FileExistsError")) return "OSError";
    if (!strcmp(k, "FileNotFoundError")) return "OSError";
    if (!strcmp(k, "InterruptedError")) return "OSError";
    if (!strcmp(k, "IsADirectoryError")) return "OSError";
    if (!strcmp(k, "NotADirectoryError")) return "OSError";
    if (!strcmp(k, "PermissionError")) return "OSError";
    if (!strcmp(k, "ProcessLookupError")) return "OSError";
    if (!strcmp(k, "TimeoutError")) return "OSError";
    if (!strcmp(k, "ReferenceError")) return "Exception";
    if (!strcmp(k, "RuntimeError")) return "Exception";
    if (!strcmp(k, "NotImplementedError")) return "RuntimeError";
    if (!strcmp(k, "RecursionError")) return "RuntimeError";
    if (!strcmp(k, "StopIteration")) return "Exception";
    if (!strcmp(k, "StopAsyncIteration")) return "Exception";
    if (!strcmp(k, "SyntaxError")) return "Exception";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "SystemError")) return "Exception";
    if (!strcmp(k, "TypeError")) return "Exception";
    if (!strcmp(k, "ValueError")) return "Exception";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    if (!strcmp(k, "UnicodeDecodeError")) return "UnicodeError";
    if (!strcmp(k, "UnicodeEncodeError")) return "UnicodeError";
    if (!strcmp(k, "UnicodeTranslateError")) return "UnicodeError";
    if (!strcmp(k, "Warning")) return "Exception";
    if (!strcmp(k, "DeprecationWarning")) return "Warning";
    if (!strcmp(k, "PendingDeprecationWarning")) return "Warning";
    if (!strcmp(k, "RuntimeWarning")) return "Warning";
    if (!strcmp(k, "SyntaxWarning")) return "Warning";
    if (!strcmp(k, "UserWarning")) return "Warning";
    if (!strcmp(k, "FutureWarning")) return "Warning";
    if (!strcmp(k, "ImportWarning")) return "Warning";
    if (!strcmp(k, "UnicodeWarning")) return "Warning";
    if (!strcmp(k, "BytesWarning")) return "Warning";
    if (!strcmp(k, "ResourceWarning")) return "Warning";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    /* project-specific custom exceptions (httpx + gateway) */
    if (!strcmp(k, "HTTPError")) return "Exception";
    if (!strcmp(k, "HTTPStatusError")) return "HTTPError";
    if (!strcmp(k, "ConnectTimeout")) return "TimeoutException";
    if (!strcmp(k, "TimeoutException")) return "TransportError";
    if (!strcmp(k, "TransportError")) return "HTTPError";
    if (!strcmp(k, "SshReachError")) return "Exception";
    /* unknown/custom kinds are treated as Exception subclasses */
    return "Exception";
}
static bool gcl2_caught(const json_t *r, const char *cls) {
    const char *k = gcl2_raise_class(r);
    if (!k) return false;
    if (!strcmp(k, cls)) return true;
    if (!strcmp(cls, "BaseException")) return true;
    if (!strcmp(cls, "Exception")) return !gcl2_base_only(k);
    for (int i = 0; i < 24; i++) {
        const char *p = gcl2_exc_parent(k);
        if (!p) return false;
        if (!strcmp(p, cls)) return true;
        k = p;
    }
    return false;
}
static bool gcl2_is_exception(const json_t *r) { return gcl2_caught(r, "Exception"); }

/* ── SHA-256 (self-contained, one-shot with explicit length) ── */
typedef struct { uint32_t h[8]; uint64_t bitlen; unsigned char block[64]; size_t blen; } gcl2_sha256;
static const uint32_t GCL2_K[64] = {
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static uint32_t gcl2_ror(uint32_t x, int n){ return (x>>n)|(x<<(32-n)); }
static void gcl2_sha_block(gcl2_sha256 *st, const unsigned char *p) {
    uint32_t w[64];
    for (int i=0;i<16;i++) w[i]=((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
    for (int i=16;i<64;i++){
        uint32_t s0=gcl2_ror(w[i-15],7)^gcl2_ror(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=gcl2_ror(w[i-2],17)^gcl2_ror(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=st->h[0],b=st->h[1],cc=st->h[2],d=st->h[3],
             e=st->h[4],f=st->h[5],g=st->h[6],h=st->h[7];
    for (int i=0;i<64;i++){
        uint32_t S1=gcl2_ror(e,6)^gcl2_ror(e,11)^gcl2_ror(e,25);
        uint32_t ch=(e&f)^((~e)&g);
        uint32_t t1=h+S1+ch+GCL2_K[i]+w[i];
        uint32_t S0=gcl2_ror(a,2)^gcl2_ror(a,13)^gcl2_ror(a,22);
        uint32_t maj=(a&b)^(a&cc)^(b&cc);
        uint32_t t2=S0+maj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    st->h[0]+=a;st->h[1]+=b;st->h[2]+=cc;st->h[3]+=d;
    st->h[4]+=e;st->h[5]+=f;st->h[6]+=g;st->h[7]+=h;
}
static void gcl2_sha_init(gcl2_sha256 *c){
    c->h[0]=0x6a09e667;c->h[1]=0xbb67ae85;c->h[2]=0x3c6ef372;c->h[3]=0xa54ff53a;
    c->h[4]=0x510e527f;c->h[5]=0x9b05688c;c->h[6]=0x1f83d9ab;c->h[7]=0x5be0cd19;
    c->bitlen=0;c->blen=0;
}
static void gcl2_sha_update(gcl2_sha256 *c, const unsigned char *p, size_t n){
    c->bitlen+=(uint64_t)n*8;
    while(n){
        size_t take=64-c->blen; if(take>n) take=n;
        memcpy(c->block+c->blen,p,take); c->blen+=take; p+=take; n-=take;
        if(c->blen==64){ gcl2_sha_block(c,c->block); c->blen=0; }
    }
}
static char *gcl2_sha256_hex_n(const char *s, size_t n){
    gcl2_sha256 c; gcl2_sha_init(&c);
    gcl2_sha_update(&c,(const unsigned char*)s,n);
    unsigned char tail[128]; size_t tl=0;
    tail[tl++]=0x80;
    size_t z=(56-((c.blen+1)%64)+64)%64;
    memset(tail+tl,0,z); tl+=z;
    uint64_t bits=(uint64_t)n*8;
    for(int i=7;i>=0;i--) tail[tl++]=(unsigned char)(bits>>(i*8));
    gcl2_sha_update(&c,tail,tl);
    gcl2_buf b; gcl2_binit(&b);
    for(int i=0;i<8;i++){ char x[16]; snprintf(x,sizeof(x),"%08x",c.h[i]); gcl2_bputs(&b,x); }
    return gcl2_btake(&b);
}

/* ── seam wrappers ── */
static json_t *gcl2_http_get(const gcl2_seams_t *s, const char *url, const char *token) {
    json_t *a = gcl2_args();
    gcl2_aset_s(a, "url", url); gcl2_aset_s(a, "token", token ? token : "");
    return gcl2_call(s, "http_get", a);
}
static json_t *gcl2_seam0(const gcl2_seams_t *s, const char *name) {
    return gcl2_call(s, name, gcl2_args());
}
static json_t *gcl2_monotonic(const gcl2_seams_t *s) { return gcl2_seam0(s, "monotonic"); }
static json_t *gcl2_sleep(const gcl2_seams_t *s, double sec) {
    json_t *a = gcl2_args(); gcl2_aset_n(a, "s", sec);
    return gcl2_call(s, "sleep", a);
}
static json_t *gcl2_classify_proof(const gcl2_seams_t *s, const json_t *proof, const char *nonce) {
    json_t *a = gcl2_args(); gcl2_aset_j(a, "proof", proof); gcl2_aset_s(a, "nonce", nonce);
    return gcl2_call(s, "classify_proof", a);
}
static json_t *gcl2_default_control_dir(const gcl2_seams_t *s, const char *root) {
    json_t *a = gcl2_args(); gcl2_aset_s(a, "root", root);
    return gcl2_call(s, "default_control_dir", a);
}
static json_t *gcl2_control_socket_path(const gcl2_seams_t *s, const char *user, const char *host,
        long long port, const char *dir, const char *key_path, const char *oid) {
    json_t *a = gcl2_args();
    gcl2_aset_s(a, "user", user); gcl2_aset_s(a, "host", host); gcl2_aset_n(a, "port", (double)port);
    gcl2_aset_s(a, "control_dir", dir); gcl2_aset_s(a, "key_path", key_path);
    gcl2_aset_s(a, "ownership_id", oid);
    return gcl2_call(s, "control_socket_path", a);
}
static json_t *gcl2_is_alive(const gcl2_seams_t *s) { return gcl2_seam0(s, "is_alive"); }
static json_t *gcl2_cancel_forward(const gcl2_seams_t *s, long long local, long long remote) {
    json_t *a = gcl2_args(); gcl2_aset_n(a, "local", (double)local); gcl2_aset_n(a, "remote", (double)remote);
    return gcl2_call(s, "cancel_forward", a);
}
static json_t *gcl2_ssh_open(const gcl2_seams_t *s) { return gcl2_seam0(s, "ssh_open"); }
static json_t *gcl2_ssh_close(const gcl2_seams_t *s) { return gcl2_seam0(s, "ssh_close"); }
static json_t *gcl2_resolve_ssh_binary(const gcl2_seams_t *s) { return gcl2_seam0(s, "resolve_ssh_binary"); }
static json_t *gcl2_connect(const gcl2_seams_t *s, const char *oid, const char *profile,
        const char *remote_path, const char *reuse_token, double timeout_s) {
    json_t *a = gcl2_args();
    gcl2_aset_s(a, "ownership_id", oid); gcl2_aset_s(a, "profile", profile);
    gcl2_aset_s(a, "remote_hermes_path", remote_path); gcl2_aset_s(a, "reuse_token", reuse_token);
    gcl2_aset_n(a, "ready_timeout_s", timeout_s);
    return gcl2_call(s, "connect", a);
}
static json_t *gcl2_disconnect_remote(const gcl2_seams_t *s, const char *oid) {
    json_t *a = gcl2_args(); gcl2_aset_s(a, "ownership_id", oid);
    return gcl2_call(s, "disconnect_remote", a);
}
static json_t *gcl2_login_shell_quote(const gcl2_seams_t *s, const char *value) {
    json_t *a = gcl2_args(); gcl2_aset_s(a, "value", value);
    return gcl2_call(s, "login_shell_quote", a);
}
static json_t *gcl2_build_exec_args(const gcl2_seams_t *s, const json_t *endpoint, const char *command) {
    json_t *a = gcl2_args(); gcl2_aset_j(a, "endpoint", endpoint); gcl2_aset_s(a, "command", command);
    return gcl2_call(s, "build_exec_args", a);
}

/* ── ── ── cores ── ── ── */

/* PoP: gcl2_err @ hermes_cli/gateway_connections_ssh.py:_err (SshReachError re-export) */
static json_t *gcl2_err(const char *kind, const char *message) {
    (void)kind; /* SshReachError str() is its message */
    return gcl2_raise_spec("SshReachError", message ? message : "");
}

/* PoP: gcl2_ownership_id @ hermes_cli/gateway_connections_ssh.py:ownership_id */
static json_t *gcl2_ownership_id(const json_t *root, const json_t *cid) {
    const char *rs = gcl2_jstr(root) ? gcl2_jstr(root) : "None";
    const char *cs = gcl2_jstr(cid) ? gcl2_jstr(cid) : "None";
    const char *pre = "hermes-gateway-connections";
    size_t plen = strlen(pre), rlen = strlen(rs), clen = strlen(cs);
    size_t n = plen + 1 + rlen + 1 + clen;
    char *buf = malloc(n + 1);
    memcpy(buf, pre, plen); buf[plen] = '\0';
    memcpy(buf + plen + 1, rs, rlen); buf[plen + 1 + rlen] = '\0';
    memcpy(buf + plen + 2 + rlen, cs, clen); buf[n] = '\0';
    char *h = gcl2_sha256_hex_n(buf, n);
    free(buf);
    h[32] = '\0';
    json_t *out = json_string(h);
    free(h);
    return out;
}

/* PoP: gcl2_fingerprint @ hermes_cli/gateway_connections_ssh.py:_fingerprint */
static json_t *gcl2_fingerprint(const json_t *connection) {
    static const char *KEYS[6] = {"host","user","port","keyPath","remoteHermesPath","remoteProfile"};
    json_t *a = json_array();
    for (int i = 0; i < 6; i++) {
        char *s = gcl2_str_or_empty(connection ? json_obj_get((json_t*)connection, KEYS[i]) : NULL);
        json_append(a, json_string(s)); free(s);
    }
    return a;
}

/* PoP: gcl2_target @ hermes_cli/gateway_connections_ssh.py:_target */
static json_t *gcl2_target(const json_t *connected) {
    json_t *o = json_object();
    const json_t *c = connected;
    gcl2_aset_s(o, "base_url", gcl2_jstr(json_obj_get((json_t*)c, "base_url")));
    gcl2_aset_s(o, "token", gcl2_jstr(json_obj_get((json_t*)c, "token")));
    json_set(o, "local_port", json_number((double)gcl2_jint(json_obj_get((json_t*)c, "local_port"), 0)));
    json_set(o, "remote_port", json_number((double)gcl2_jint(json_obj_get((json_t*)c, "remote_port"), 0)));
    json_set(o, "pid", json_number((double)gcl2_jint(json_obj_get((json_t*)c, "pid"), 0)));
    json_set(o, "reused", json_bool(gcl2_jbool(json_obj_get((json_t*)c, "reused"), false)));
    gcl2_aset_s(o, "hermes_version", gcl2_jstr(json_obj_get((json_t*)c, "hermes_version")));
    return o;
}

/* PoP: gcl2_health_ok @ hermes_cli/gateway_connections_ssh.py:_health_ok */
static json_t *gcl2_health_ok(const gcl2_seams_t *s, const char *base_url) {
    char *url = gcl2_xsf("%s/api/health", base_url ? base_url : "");
    json_t *r = gcl2_http_get(s, url, "");
    free(url);
    if (gcl2_is_raise(r)) {
        if (gcl2_caught(r, "HTTPError")) { json_free(r); return json_bool(false); }
        return r;
    }
    double status = gcl2_jnum(json_obj_get(r, "status"), 0);
    const json_t *body = json_obj_get(r, "body");
    bool ok = (status == 200.0) && body && body->type == JSON_OBJECT &&
              json_obj_get((json_t*)body, "ok") && json_obj_get((json_t*)body, "ok")->type == JSON_BOOL &&
              json_obj_get((json_t*)body, "ok")->bool_val == true;
    json_free(r);
    return json_bool(ok);
}

/* PoP: gcl2_probe_reuse_proof @ hermes_cli/gateway_connections_ssh.py:_probe_reuse_proof */
static json_t *gcl2_probe_reuse_proof(const gcl2_seams_t *s, const char *base_url,
        const char *token, const char *nonce) {
    char *url = gcl2_xsf("%s/api/ssh/ownership", base_url ? base_url : "");
    json_t *r = gcl2_http_get(s, url, token);
    free(url);
    if (gcl2_is_raise(r)) return r; /* no except here */
    double status = gcl2_jnum(json_obj_get(r, "status"), 0);
    if (status == 401.0 || status == 403.0 || status == 404.0) {
        json_free(r); return json_string("authenticated-stale");
    }
    if (status >= 400.0) {
        char *m = gcl2_xsf("status %.0f", status);
        json_t *ex = gcl2_raise_spec("HTTPStatusError", m);
        free(m); json_free(r); return ex;
    }
    json_t *proof = json_obj_get(r, "body") ? json_copy(json_obj_get(r, "body")) : json_null();
    json_free(r);
    json_t *out = gcl2_classify_proof(s, proof, nonce);
    json_free(proof);
    return out;
}

/* PoP: gcl2_wait_for_health @ hermes_cli/gateway_connections_ssh.py:_wait_for_health */
static json_t *gcl2_wait_for_health(const gcl2_seams_t *s, const char *base_url) {
    json_t *m0 = gcl2_monotonic(s);
    if (gcl2_is_raise(m0)) return m0;
    double deadline = gcl2_jnum(m0, 0) + GCL2_HEALTH_TIMEOUT_S;
    json_free(m0);
    for (;;) {
        json_t *m = gcl2_monotonic(s);
        if (gcl2_is_raise(m)) return m;
        double now = gcl2_jnum(m, 0);
        json_free(m);
        if (now < deadline) {
            json_t *hr = gcl2_health_ok(s, base_url);
            if (gcl2_is_raise(hr)) return hr;
            bool ok = gcl2_jtruth(hr);
            json_free(hr);
            if (ok) return NULL;
            json_t *sl = gcl2_sleep(s, 0.25);
            if (gcl2_is_raise(sl)) return sl;
            json_free(sl);
            continue;
        }
        break;
    }
    return gcl2_err("ready-timeout",
        "The forwarded remote serve never answered /api/health (30s).");
}

/* PoP: gcl2_live_reuse @ hermes_cli/gateway_connections_ssh.py:_live_reuse */
static json_t *gcl2_live_reuse(const gcl2_seams_t *s, const json_t *tunnel) {
    json_t *al = gcl2_is_alive(s);
    if (gcl2_is_raise(al)) return al;
    bool alive = gcl2_jtruth(al); json_free(al);
    if (!alive) return json_bool(false);
    const char *base_url = gcl2_jstr(json_obj_get(json_obj_get((json_t*)tunnel, "connected"), "base_url"));
    json_t *hr = gcl2_health_ok(s, base_url);
    if (gcl2_is_raise(hr)) return hr;
    bool healthy = gcl2_jtruth(hr); json_free(hr);
    if (!healthy) return json_bool(false);
    const char *token = gcl2_jstr(json_obj_get(json_obj_get((json_t*)tunnel, "connected"), "token"));
    const char *nonce = gcl2_jstr(json_obj_get(json_obj_get((json_t*)tunnel, "connected"), "spawn_nonce"));
    json_t *pr = gcl2_probe_reuse_proof(s, base_url, token, nonce);
    if (gcl2_is_raise(pr)) {
        if (gcl2_caught(pr, "HTTPError")) { json_free(pr); return json_bool(false); }
        return pr;
    }
    const char *v = gcl2_jstr(pr);
    bool res = v && !strcmp(v, "authenticated-ok");
    json_free(pr);
    return json_bool(res);
}

/* PoP: gcl2_endpoint @ hermes_cli/gateway_connections_ssh.py:_endpoint */
static json_t *gcl2_endpoint(const gcl2_seams_t *s, const json_t *connection,
        const json_t *root, const json_t *oid) {
    char *host = gcl2_str_or_empty(json_obj_get((json_t*)connection, "host"));
    char *user = gcl2_str_or_empty(json_obj_get((json_t*)connection, "user"));
    long long port = gcl2_int_or(json_obj_get((json_t*)connection, "port"), 22);
    char *key_path = gcl2_str_or_empty(json_obj_get((json_t*)connection, "keyPath"));
    const char *roots = gcl2_jstr(root) ? gcl2_jstr(root) : "";
    json_t *dir = gcl2_default_control_dir(s, roots);
    if (gcl2_is_raise(dir)) { free(host); free(user); free(key_path); return dir; }
    const char *dirs = gcl2_jstr(dir) ? gcl2_jstr(dir) : "";
    json_t *path = gcl2_control_socket_path(s, user, host, port, dirs, key_path,
                                             gcl2_jstr(oid) ? gcl2_jstr(oid) : "");
    json_free(dir);
    free(host); free(user); free(key_path);
    if (gcl2_is_raise(path)) return path;
    json_t *o = json_object();
    /* host/user/key_path use str(x or ""); rebuild exactly to mirror str() */
    { char *h = gcl2_str_or_empty(json_obj_get((json_t*)connection, "host"));
      char *u = gcl2_str_or_empty(json_obj_get((json_t*)connection, "user"));
      char *k = gcl2_str_or_empty(json_obj_get((json_t*)connection, "keyPath"));
      gcl2_aset_s(o, "host", h); gcl2_aset_s(o, "user", u);
      json_set(o, "port", json_number((double)port));
      gcl2_aset_s(o, "key_path", k);
      gcl2_aset_s(o, "control_path", gcl2_jstr(path));
      free(h); free(u); free(k); }
    json_free(path);
    return o;
}

/* PoP: gcl2_teardown @ hermes_cli/gateway_connections_ssh.py:_teardown */
static json_t *gcl2_teardown(const gcl2_seams_t *s, const json_t *tunnel) {
    const json_t *c = json_obj_get((json_t*)tunnel, "connected");
    long long lp = gcl2_jint(json_obj_get((json_t*)c, "local_port"), 0);
    long long rp = gcl2_jint(json_obj_get((json_t*)c, "remote_port"), 0);
    json_t *cf = gcl2_cancel_forward(s, lp, rp);
    if (gcl2_is_raise(cf)) return cf;
    json_free(cf);
    const char *oid = gcl2_jstr(json_obj_get((json_t*)tunnel, "ownership_id"));
    json_t *dr = gcl2_disconnect_remote(s, oid);
    json_t *cl = gcl2_ssh_close(s); /* finally */
    if (gcl2_is_raise(cl)) { json_free(dr); return cl; }
    json_free(cl);
    return dr;
}

/* PoP: gcl2_tunnel_status @ hermes_cli/gateway_connections_ssh.py:tunnel_status */
static json_t *gcl2_tunnel_status(const json_t *tunnel) {
    if (gcl2_is_null(tunnel)) return NULL;
    const json_t *c = json_obj_get((json_t*)tunnel, "connected");
    json_t *o = json_object();
    json_set(o, "localPort", json_number((double)gcl2_jint(json_obj_get((json_t*)c, "local_port"), 0)));
    json_set(o, "remotePort", json_number((double)gcl2_jint(json_obj_get((json_t*)c, "remote_port"), 0)));
    json_set(o, "pid", json_number((double)gcl2_jint(json_obj_get((json_t*)c, "pid"), 0)));
    json_set(o, "reused", json_bool(gcl2_jbool(json_obj_get((json_t*)c, "reused"), false)));
    gcl2_aset_s(o, "hermesVersion", gcl2_jstr(json_obj_get((json_t*)c, "hermes_version")));
    gcl2_aset_s(o, "host", gcl2_jstr(json_obj_get((json_t*)tunnel, "host")));
    return o;
}

/* PoP: gcl2_disconnect @ hermes_cli/gateway_connections_ssh.py:disconnect */
static json_t *gcl2_disconnect(const gcl2_seams_t *s, const json_t *tunnel) {
    if (gcl2_is_null(tunnel)) return json_bool(false);
    json_t *td = gcl2_teardown(s, tunnel);
    if (gcl2_is_raise(td)) return td;
    json_free(td);
    return json_bool(true);
}

/* forward decls for the two mutually-referencing orchestrators */
static json_t *gcl2_bootstrap(const gcl2_seams_t *s, const json_t *connection,
                              const json_t *root, const json_t *previous);
static json_t *gcl2_ensure_tunnel(const gcl2_seams_t *s, const json_t *connection,
                                  const json_t *root, const json_t *existing);

/* PoP: gcl2_bootstrap @ hermes_cli/gateway_connections_ssh.py:_bootstrap */
static json_t *gcl2_bootstrap(const gcl2_seams_t *s, const json_t *connection,
        const json_t *root, const json_t *previous) {
    const char *cid = gcl2_jstr(json_obj_get((json_t*)connection, "id"));
    if (!cid) cid = "";
    json_t *oid = gcl2_ownership_id(root, json_string(cid));
    const char *oids = gcl2_jstr(oid);
    /* _endpoint(...) then resolve_ssh_binary() then ssh.open() */
    json_t *ep = gcl2_endpoint(s, connection, root, oid);
    if (gcl2_is_raise(ep)) { json_free(oid); return ep; }
    json_free(ep);
    json_t *sb = gcl2_resolve_ssh_binary(s);
    if (gcl2_is_raise(sb)) { json_free(oid); return sb; }
    json_free(sb);
    json_t *op = gcl2_ssh_open(s);
    if (gcl2_is_raise(op)) { json_free(oid); return op; }
    json_free(op);
    const char *reuse = "";
    if (!gcl2_is_null(previous))
        reuse = gcl2_jstr(json_obj_get(json_obj_get((json_t*)previous, "connected"), "token"));
    const char *prof = gcl2_jstr(json_obj_get((json_t*)connection, "remoteProfile"));
    const char *rp = gcl2_jstr(json_obj_get((json_t*)connection, "remoteHermesPath"));
    json_t *conn = gcl2_connect(s, oids, prof ? prof : "", rp ? rp : "", reuse ? reuse : "",
                                GCL2_DEFAULT_READY_TIMEOUT_S);
    if (gcl2_is_raise(conn)) {
        json_t *cl = gcl2_ssh_close(s); /* except BaseException: close; raise */
        if (gcl2_is_raise(cl)) { json_free(conn); json_free(oid); return cl; }
        json_free(cl); json_free(oid);
        return conn;
    }
    json_t *out = gcl2_target(conn);
    json_free(conn); json_free(oid);
    return out;
}

/* PoP: gcl2_ensure_tunnel @ hermes_cli/gateway_connections_ssh.py:ensure_tunnel */
static json_t *gcl2_ensure_tunnel(const gcl2_seams_t *s, const json_t *connection,
        const json_t *root, const json_t *existing) {
    char *cid = gcl2_str_or_empty(json_obj_get((json_t*)connection, "id"));
    if (!cid[0]) { free(cid); return gcl2_err("unknown", "SSH connection entry has no id."); }
    json_t *fp = gcl2_fingerprint(connection);
    bool fp_eq = !gcl2_is_null(existing) &&
                 gcl2_json_eq(fp, json_obj_get((json_t*)existing, "fingerprint"));
    if (!gcl2_is_null(existing) && fp_eq) {
        json_t *lr = gcl2_live_reuse(s, existing);
        if (gcl2_is_raise(lr)) { json_free(fp); free(cid); return lr; }
        bool reuse = gcl2_jtruth(lr); json_free(lr);
        if (reuse) {
            json_t *out = gcl2_target(json_obj_get((json_t*)existing, "connected"));
            json_free(fp); free(cid); return out;
        }
    }
    if (!gcl2_is_null(existing)) {
        if (!fp_eq) {
            json_t *td = gcl2_teardown(s, existing);
            if (gcl2_is_raise(td)) { json_free(fp); free(cid); return td; }
            json_free(td);
            existing = NULL;
        } else {
            const json_t *c = json_obj_get((json_t*)existing, "connected");
            json_t *cf = gcl2_cancel_forward(s,
                gcl2_jint(json_obj_get((json_t*)c, "local_port"), 0),
                gcl2_jint(json_obj_get((json_t*)c, "remote_port"), 0));
            if (gcl2_is_raise(cf)) { json_free(fp); free(cid); return cf; }
            json_free(cf);
        }
    }
    json_free(fp); free(cid);
    return gcl2_bootstrap(s, connection, root, existing);
}

/* PoP: gcl2_interactive_shell_argv @ hermes_cli/gateway_connections_ssh.py:interactive_shell_argv */
static json_t *gcl2_interactive_shell_argv(const gcl2_seams_t *s, const json_t *connection,
        const json_t *cwd, const json_t *root, const json_t *existing) {
    json_t *et = gcl2_ensure_tunnel(s, connection, root, existing);
    if (gcl2_is_raise(et)) return et;
    json_free(et);
    if (gcl2_is_null(existing))
        return gcl2_err("unknown", "The SSH connection changed while starting the terminal; retry.");
    const char *cwd_s = gcl2_jstr(cwd);
    const char *command = "exec \"$SHELL\" -l";
    char *cmd_owned = NULL;
    if (cwd_s && cwd_s[0]) {
        json_t *q = gcl2_login_shell_quote(s, cwd_s);
        if (gcl2_is_raise(q)) return q;
        cmd_owned = gcl2_xsf("cd %s 2>/dev/null; %s", gcl2_jstr(q) ? gcl2_jstr(q) : "", command);
        json_free(q);
        command = cmd_owned;
    }
    json_t *be = gcl2_build_exec_args(s, json_obj_get((json_t*)existing, "endpoint"), command);
    free(cmd_owned);
    if (gcl2_is_raise(be)) return be;
    json_t *out = json_array();
    const char *sb = gcl2_jstr(json_obj_get((json_t*)existing, "ssh_binary"));
    json_append(out, json_string(sb ? sb : "ssh"));
    json_append(out, json_string("-tt"));
    if (be->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(be); i++) json_append(out, json_copy(json_get(be, i)));
    json_free(be);
    return out;
}

/* PoP: gcl2_close_all @ hermes_cli/gateway_connections_ssh.py:close_all */
static json_t *gcl2_close_all(const gcl2_seams_t *s, const json_t *tunnels) {
    if (gcl2_is_null(tunnels) || tunnels->type != JSON_ARRAY) return NULL;
    for (size_t i = 0; i < json_len(tunnels); i++) {
        json_t *r = gcl2_disconnect(s, json_get(tunnels, i));
        if (gcl2_is_raise(r)) {
            if (gcl2_caught(r, "SshReachError")) { json_free(r); continue; }
            return r;
        }
        json_free(r);
    }
    return NULL;
}

/* ── PoP annotations (registry credit; definitions above) ──
 * PoP: gcl2_ownership_id @ hermes_cli/gateway_connections_ssh.py:ownership_id
 * PoP: gcl2_fingerprint @ hermes_cli/gateway_connections_ssh.py:_fingerprint
 * PoP: gcl2_target @ hermes_cli/gateway_connections_ssh.py:_target
 * PoP: gcl2_health_ok @ hermes_cli/gateway_connections_ssh.py:_health_ok
 * PoP: gcl2_probe_reuse_proof @ hermes_cli/gateway_connections_ssh.py:_probe_reuse_proof
 * PoP: gcl2_wait_for_health @ hermes_cli/gateway_connections_ssh.py:_wait_for_health
 * PoP: gcl2_live_reuse @ hermes_cli/gateway_connections_ssh.py:_live_reuse
 * PoP: gcl2_endpoint @ hermes_cli/gateway_connections_ssh.py:_endpoint
 * PoP: gcl2_teardown @ hermes_cli/gateway_connections_ssh.py:_teardown
 * PoP: gcl2_tunnel_status @ hermes_cli/gateway_connections_ssh.py:tunnel_status
 * PoP: gcl2_disconnect @ hermes_cli/gateway_connections_ssh.py:disconnect
 * PoP: gcl2_bootstrap @ hermes_cli/gateway_connections_ssh.py:_bootstrap
 * PoP: gcl2_ensure_tunnel @ hermes_cli/gateway_connections_ssh.py:ensure_tunnel
 * PoP: gcl2_interactive_shell_argv @ hermes_cli/gateway_connections_ssh.py:interactive_shell_argv
 * PoP: gcl2_close_all @ hermes_cli/gateway_connections_ssh.py:close_all
 */
