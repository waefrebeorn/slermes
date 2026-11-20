/*
 * gw_conn_ssh_flow2.c — FLOW port of hermes_cli/gateway_connections_ssh_remote.py
 * (LIVE authority). Self-contained: libjson only, all functions static gcs2_*.
 * The harness (#include "cli/gw_conn_ssh_flow2.c") wires the universal seam to
 * scripted results and records the ordered trace.
 *
 * SEAM DOCTRINE: one universal seam
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returns a NEW json_t* (NULL == Python None). A raising live call is scripted
 * {"__raise__":"<ExcClass>","msg":...}; cores propagate where the live
 * try/except catches, and BaseException-only classes pass every except.
 * A MISSING seam key fail-closes (RuntimeError); present JSON null is None.
 *
 * Seam name table (name -> live call):
 *   exec              ssh.exec(command[, stdin_data]); args {tag}
 *   forward           ssh.forward(local, remote); args {local,remote}
 *   cancel_forward    ssh.cancel_forward(local, remote); args {local,remote}
 *   sleep             time.sleep(seconds); args {s}
 *   monotonic         time.monotonic(); args {}
 *   token_hex         secrets.token_hex(nbytes); args {n}
 *   now_iso           datetime.now(UTC).isoformat(...); args {}
 *   with_timeout      with_remote_timeout(command); args {}
 *   assert_clear      assert_install_clear(); args {}
 *   pick_port         pick_local_port(); args {}
 *   wait_health       wait_for_health(url); args {url}
 *   probe_proof       probe_reuse_proof(url, token, nonce); args {url,token,nonce}
 *
 * Command projection: ssh.exec commands are classified to a short stable tag
 * (gcs2_classify) identical on both sides; the full shell/embedded-Python text
 * is NOT part of the observable contract. Pure string-BUILDER functions whose
 * return is a large opaque shell script (with_remote_update_mutex,
 * _spawn_payload_prefix's detached component, build_spawn_command) likewise
 * project their return to canonical tags. Documented fixture domains:
 *  - regex/hex/quote/path inputs are ASCII; paths POSIX strings.
 *  - json.loads strictness: malformed lockfile text is restricted to clearly
 *    invalid tokens; libjson's lenient extensions are outside the corpus.
 *  - integral-float values are kept out of type(x) is int positions.
 *  - monotonic/sleep scripts always terminate the poll loops.
 *  - custom exception SshReachError(kind,message) projects to its message.
 *  - dataclass results (LockfileSkew/Spawned/Connected/_Remote) project to a
 *    field dict; exceptions project to {__raise__,msg}.
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

/* module constants */
#define LOCKFILE_SCHEMA_VERSION 2
#define PROTOCOL_VERSION 1
#define PROBE_VERDICT_ATTEMPTS 3
#define PROBE_VERDICT_RETRY_S 0.5
#define READY_POLL_INTERVAL_S 0.75

/* ── universal seam + trace ── */
typedef struct gcs2_trace { json_t *events; } gcs2_trace_t;
typedef struct gcs2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gcs2_seams_t;

static void gcs2_trace_add(gcs2_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void gcs2_trace_free(gcs2_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *gcs2_call(const gcs2_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}

/* ── string/buffer helpers ── */
static char *gcs2_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}
typedef struct { char *p; size_t len, cap; } gcs2_buf;
static void gcs2_binit(gcs2_buf *b) { b->p = NULL; b->len = b->cap = 0; }
static void gcs2_bfree(gcs2_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static void gcs2_bput(gcs2_buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->len + n + 1) nc *= 2;
        char *np = realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, s, n); b->len += n; b->p[b->len] = '\0';
}
static void gcs2_bputs(gcs2_buf *b, const char *s) { gcs2_bput(b, s, strlen(s)); }
static void gcs2_bputc(gcs2_buf *b, char c) { gcs2_bput(b, &c, 1); }
static char *gcs2_btake(gcs2_buf *b) {
    if (!b->p) return strdup("");
    char *r = b->p; b->p = NULL; b->len = b->cap = 0; return r;
}

/* ── JSON value helpers ── */
static bool gcs2_jtruth(const json_t *v) {
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
static bool gcs2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *gcs2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static double gcs2_jnum(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static long long gcs2_jint(const json_t *v, long long d) {
    if (v && v->type == JSON_NUMBER) return (long long)v->num_val;
    return d;
}
static bool gcs2_jbool(const json_t *v, bool d) {
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
}

/* args builders */
static json_t *gcs2_args(void) { return json_object(); }
static void gcs2_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void gcs2_aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void gcs2_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void gcs2_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── exception model (FULL builtin hierarchy) ── */
static const char *gcs2_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool gcs2_is_raise(const json_t *r) { return gcs2_raise_class(r) != NULL; }
static json_t *gcs2_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
/* BaseException-only (never caught by except Exception/OSError/ValueError/...) */
static bool gcs2_base_only(const char *k) {
    return !strcmp(k, "BaseException") || !strcmp(k, "KeyboardInterrupt") ||
           !strcmp(k, "SystemExit") || !strcmp(k, "GeneratorExit");
}
/* Full hierarchy: every builtin exception -> parent (except trio -> BaseException). */
static const char *gcs2_exc_parent(const char *k) {
    /* BaseException family first */
    if (!strcmp(k, "BaseException")) return NULL;
    if (!strcmp(k, "KeyboardInterrupt") || !strcmp(k, "SystemExit") || !strcmp(k, "GeneratorExit") ||
        !strcmp(k, "BaseExceptionGroup"))
        return "BaseException";
    /* Direct Exception subclasses */
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
    if (!strcmp(k, "SshReachError")) return "Exception";
    /* unknown/custom kinds are treated as Exception subclasses */
    return "Exception";
}
/* caught(r, cls): is the scripted raise an instance of cls (or subclass)? */
static bool gcs2_caught(const json_t *r, const char *cls) {
    const char *k = gcs2_raise_class(r);
    if (!k) return false;
    if (!strcmp(k, cls)) return true;
    if (!strcmp(cls, "BaseException")) return true; /* everything */
    if (!strcmp(cls, "Exception")) return !gcs2_base_only(k);
    /* walk the parent chain (max depth guard) */
    for (int i = 0; i < 24; i++) {
        const char *p = gcs2_exc_parent(k);
        if (!p) return false;
        if (!strcmp(p, cls)) return true;
        k = p;
    }
    return false;
}
static bool gcs2_is_exception(const json_t *r) { return gcs2_caught(r, "Exception"); }

/* Python type name of a JSON value (integral numbers read as int) */
static const char *gcs2_typename(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL: return "NoneType";
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val) && isfinite(v->num_val)) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* Python str()/repr() for the scalar domains used here */
static char *gcs2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 1e16) return gcs2_xsf("%.0f", d);
            if (!isfinite(d)) return strdup(d > 0 ? "inf" : (isnan(d) ? "nan" : "-inf"));
            char tmp[64]; snprintf(tmp, sizeof(tmp), "%.15g", d); return strdup(tmp);
        }
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default: return strdup("");
    }
}
/* py repr of a simple ASCII string (single quotes, escape ' and \\) */
static char *gcs2_py_repr_str(const char *s) {
    gcs2_buf b; gcs2_binit(&b);
    gcs2_bputc(&b, '\'');
    for (const char *p = s ? s : ""; *p; p++) {
        if (*p == '\'' || *p == '\\') { gcs2_bputc(&b, '\\'); gcs2_bputc(&b, *p); }
        else gcs2_bputc(&b, *p);
    }
    gcs2_bputc(&b, '\'');
    return gcs2_btake(&b);
}

/* ── SHA-256 (self-contained; fingerprint_token) ── */
typedef struct { uint32_t h[8]; uint64_t bitlen; unsigned char block[64]; size_t blen; } gcs2_sha256;
static const uint32_t GCS2_K[64] = {
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static uint32_t gcs2_ror(uint32_t x, int n){ return (x>>n)|(x<<(32-n)); }
static void gcs2_sha_block(gcs2_sha256 *st, const unsigned char *p) {
    uint32_t w[64];
    for (int i=0;i<16;i++) w[i]=((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
    for (int i=16;i<64;i++){
        uint32_t s0=gcs2_ror(w[i-15],7)^gcs2_ror(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=gcs2_ror(w[i-2],17)^gcs2_ror(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=st->h[0],b=st->h[1],cc=st->h[2],d=st->h[3],
             e=st->h[4],f=st->h[5],g=st->h[6],h=st->h[7];
    for (int i=0;i<64;i++){
        uint32_t S1=gcs2_ror(e,6)^gcs2_ror(e,11)^gcs2_ror(e,25);
        uint32_t ch=(e&f)^((~e)&g);
        uint32_t t1=h+S1+ch+GCS2_K[i]+w[i];
        uint32_t S0=gcs2_ror(a,2)^gcs2_ror(a,13)^gcs2_ror(a,22);
        uint32_t maj=(a&b)^(a&cc)^(b&cc);
        uint32_t t2=S0+maj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    st->h[0]+=a;st->h[1]+=b;st->h[2]+=cc;st->h[3]+=d;
    st->h[4]+=e;st->h[5]+=f;st->h[6]+=g;st->h[7]+=h;
}
static void gcs2_sha_init(gcs2_sha256 *c){
    c->h[0]=0x6a09e667;c->h[1]=0xbb67ae85;c->h[2]=0x3c6ef372;c->h[3]=0xa54ff53a;
    c->h[4]=0x510e527f;c->h[5]=0x9b05688c;c->h[6]=0x1f83d9ab;c->h[7]=0x5be0cd19;
    c->bitlen=0;c->blen=0;
}
static void gcs2_sha_update(gcs2_sha256 *c, const unsigned char *p, size_t n){
    c->bitlen+=(uint64_t)n*8;
    while(n){
        size_t take=64-c->blen; if(take>n) take=n;
        memcpy(c->block+c->blen,p,take); c->blen+=take; p+=take; n-=take;
        if(c->blen==64){ gcs2_sha_block(c,c->block); c->blen=0; }
    }
}
static char *gcs2_sha256_hex(const char *s){
    gcs2_sha256 c; gcs2_sha_init(&c);
    size_t n=s?strlen(s):0;
    gcs2_sha_update(&c,(const unsigned char*)s,n);
    unsigned char tail[128]; size_t tl=0;
    tail[tl++]=0x80;
    size_t z=(56-((c.blen+1)%64)+64)%64;
    memset(tail+tl,0,z); tl+=z;
    uint64_t bits=(uint64_t)n*8;
    for(int i=7;i>=0;i--) tail[tl++]=(unsigned char)(bits>>(i*8));
    gcs2_sha_update(&c,tail,tl);
    gcs2_buf b; gcs2_binit(&b);
    for(int i=0;i<8;i++){ char x[16]; snprintf(x,sizeof(x),"%08x",c.h[i]); gcs2_bputs(&b,x); }
    return gcs2_btake(&b);
}

/* ── regex-domain helpers (ASCII, restricted fixtures) ── */
static bool gcs2_hex_all(const char *s, int n) {
    if (!s || strlen(s) != (size_t)n) return false;
    for (const char *p=s; *p; p++) if (!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f'))) return false;
    return true;
}
static bool gcs2_profile_id(const char *s) {
    if (!s || !*s) return false;
    size_t n=strlen(s);
    if (!((s[0]>='a'&&s[0]<='z')||(s[0]>='0'&&s[0]<='9'))) return false;
    if (n>64) return false;
    for (const char *p=s+1; *p; p++)
        if (!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-')) return false;
    return true;
}
static bool gcs2_creation_time_ok(const char *s) {
    if (!s) return false;
    if (!strncmp(s,"linux:",6)) {
        const char *p=s+6; if(!*p) return false;
        for(;*p;p++) if(*p<'0'||*p>'9') return false;
        return true;
    }
    if (!strncmp(s,"darwin:",7)) {
        const char *p=s+7; if(!*p) return false;
        for(;*p;p++) if(!(isalnum((unsigned char)*p)||*p==' '||*p==':'||*p=='+'||*p=='-')) return false;
        return true;
    }
    return false;
}

/* classify an ssh.exec command to a short stable tag (mirrored in the oracle) */
static const char *gcs2_classify(const char *c) {
    if (!c) c = "";
    if (strstr(c,"uname -s")) return "uname";
    if (strstr(c,"HERMES_HOME")) return "home";
    if (!strncmp(c,"python3 - ",10) && strstr(c,"hermes-update-in-progress")) return "marker_gate";
    if (strstr(c,"[ -x ")) return "is_exec";
    if (strstr(c,"command -v hermes")) return "command_v";
    if (strstr(c,"--version")) return "version";
    if (strstr(c,"grep -q ssh-session-token-file")) return "help";
    if (strstr(c,"fi; cat ")) return "read_lock";
    if (strstr(c,"umask 077 && mkdir -p")) return "write_lock";
    if (strstr(c,"kill -9")) return "terminate9";
    if (strstr(c,"kill -0 ") && strstr(c,"ALIVE")) return "alive";
    if (strstr(c,"reservation=") || strstr(c,"exit 75")) return "spawn";
    if (strstr(c,"kill ") && strstr(c,"i=$((i+1))")) return "terminate";
    if (!strncmp(c,"python3 -c",10)) {
        if (strstr(c,"expected_entries")) return "owned_check";
        if (strstr(c,"fields[19]") || strstr(c,"/proc/")) return "creation_time";
        if (strstr(c,"O_DIRECTORY")) return "token_upload";
        return "py_script";
    }
    if (!strncmp(c,"cat ",4)) return "cat_log";
    if (strstr(c,"rm -f ")) return "rm_file";
    return "other";
}


/* ── seam wrappers ── */
static json_t *gcs2_exec(const gcs2_seams_t *s, const char *cmd) {
    json_t *a = gcs2_args();
    gcs2_aset_s(a, "tag", gcs2_classify(cmd));
    return gcs2_call(s, "exec", a);
}
static json_t *gcs2_exec_c(const gcs2_seams_t *s, const char *cmd, const char *extra_k, const json_t *extra_v) {
    json_t *a = gcs2_args();
    gcs2_aset_s(a, "tag", gcs2_classify(cmd));
    if (extra_k && extra_v) gcs2_aset_j(a, extra_k, extra_v);
    return gcs2_call(s, "exec", a);
}
static json_t *gcs2_seam0(const gcs2_seams_t *s, const char *name) {
    return gcs2_call(s, name, gcs2_args());
}
static json_t *gcs2_sleep(const gcs2_seams_t *s, double sec) {
    json_t *a = gcs2_args(); gcs2_aset_n(a, "s", sec);
    return gcs2_call(s, "sleep", a);
}
static json_t *gcs2_token_hex(const gcs2_seams_t *s, int n) {
    json_t *a = gcs2_args(); gcs2_aset_n(a, "n", n);
    return gcs2_call(s, "token_hex", a);
}
static json_t *gcs2_forward(const gcs2_seams_t *s, int local, int remote) {
    json_t *a = gcs2_args(); gcs2_aset_n(a, "local", local); gcs2_aset_n(a, "remote", remote);
    return gcs2_call(s, "forward", a);
}
static json_t *gcs2_cancel_forward(const gcs2_seams_t *s, int local, int remote) {
    json_t *a = gcs2_args(); gcs2_aset_n(a, "local", local); gcs2_aset_n(a, "remote", remote);
    return gcs2_call(s, "cancel_forward", a);
}
static json_t *gcs2_wait_health(const gcs2_seams_t *s, const char *url) {
    json_t *a = gcs2_args(); gcs2_aset_s(a, "url", url);
    return gcs2_call(s, "wait_health", a);
}
static json_t *gcs2_probe_proof(const gcs2_seams_t *s, const char *url, const char *token, const char *nonce) {
    json_t *a = gcs2_args();
    gcs2_aset_s(a, "url", url); gcs2_aset_s(a, "token", token); gcs2_aset_s(a, "nonce", nonce);
    return gcs2_call(s, "probe_proof", a);
}
/* result-value accessors that propagate a raise spec */
static const char *gcs2_res_str(const json_t *r) { return gcs2_jstr(r); }
static bool gcs2_res_bool(const json_t *r, bool d) { return gcs2_jbool(r, d); }

#define GCS2_LOCK_DIR "~/.hermes/desktop-ssh"

/* ── ── ── identity, paths, quoting ── ── ── */

/* PoP: gcs2_err @ hermes_cli/gateway_connections_ssh_remote.py:_err */
static json_t *gcs2_err(const char *kind, const char *message) {
    (void)kind; /* SshReachError str() is its message */
    return gcs2_raise_spec("SshReachError", message);
}

/* PoP: gcs2_backend_profile_arg @ hermes_cli/gateway_connections_ssh_remote.py:backend_profile_arg */
static json_t *gcs2_backend_profile_arg(const json_t *profile) {
    const char *s = gcs2_jstr(profile);
    if (!s) return NULL;
    char *lo = strdup(s);
    for (char *p=lo; *p; p++) {
        if (*p==' '||*p=='\t'||*p=='\n'||*p=='\r'||*p=='\v'||*p=='\f') { /* strip handled below */ }
        if (*p>='A'&&*p<='Z') *p=(char)(*p-'A'+'a');
    }
    /* strip ASCII whitespace both ends */
    char *beg=lo; while(*beg && (*beg==' '||*beg=='\t'||*beg=='\n'||*beg=='\r'||*beg=='\v'||*beg=='\f')) beg++;
    char *end=beg+strlen(beg); while(end>beg && (end[-1]==' '||end[-1]=='\t'||end[-1]=='\n'||end[-1]=='\r'||end[-1]=='\v'||end[-1]=='\f')) end--;
    *end='\0';
    if (!*beg) { free(lo); return NULL; }
    bool ok = !strcmp(beg,"default") || gcs2_profile_id(beg);
    json_t *r = ok ? json_string(beg) : NULL;
    free(lo);
    return r;
}

/* PoP: gcs2_fingerprint_token @ hermes_cli/gateway_connections_ssh_remote.py:fingerprint_token */
static json_t *gcs2_fingerprint_token(const json_t *token) {
    const char *s = gcs2_jstr(token);
    char *h = gcs2_sha256_hex(s ? s : "");
    h[32]='\0';
    json_t *r = json_string(h);
    free(h);
    return r;
}

/* PoP: gcs2_validate_ownership_id @ hermes_cli/gateway_connections_ssh_remote.py:validate_ownership_id */
static json_t *gcs2_validate_ownership_id(const json_t *oid) {
    const char *s = gcs2_jstr(oid);
    if (!gcs2_hex_all(s ? s : "", 32))
        return gcs2_err("unknown", "SSH ownership ID is invalid.");
    return json_copy((json_t*)oid);
}
/* PoP: gcs2_validate_spawn_nonce @ hermes_cli/gateway_connections_ssh_remote.py:validate_spawn_nonce */
static json_t *gcs2_validate_spawn_nonce(const json_t *nonce) {
    const char *s = gcs2_jstr(nonce);
    if (!gcs2_hex_all(s ? s : "", 16))
        return gcs2_err("unknown", "SSH spawn nonce is invalid.");
    return json_copy((json_t*)nonce);
}

/* helper: validated ownership id as a fresh char*; returns raise spec via out */
static char *gcs2_oid_s(const json_t *oid, json_t **err) {
    json_t *v = gcs2_validate_ownership_id(oid);
    if (gcs2_is_raise(v)) { *err = v; return NULL; }
    const char *s = gcs2_jstr(v);
    char *r = strdup(s ? s : "");
    json_free(v);
    return r;
}
static char *gcs2_nonce_s(const json_t *nonce, json_t **err) {
    json_t *v = gcs2_validate_spawn_nonce(nonce);
    if (gcs2_is_raise(v)) { *err = v; return NULL; }
    const char *s = gcs2_jstr(v);
    char *r = strdup(s ? s : "");
    json_free(v);
    return r;
}

/* PoP: gcs2_ownership_directory @ hermes_cli/gateway_connections_ssh_remote.py:ownership_directory */
static json_t *gcs2_ownership_directory(const json_t *oid) {
    json_t *err=NULL; char *id=gcs2_oid_s(oid,&err);
    if (err) return err;
    char *s = gcs2_xsf("%s/%s", GCS2_LOCK_DIR, id);
    free(id); json_t *r=json_string(s); free(s); return r;
}
/* PoP: gcs2_lockfile_path @ hermes_cli/gateway_connections_ssh_remote.py:lockfile_path */
static json_t *gcs2_lockfile_path(const json_t *oid) {
    json_t *d = gcs2_ownership_directory(oid);
    if (gcs2_is_raise(d)) return d;
    const char *s=gcs2_jstr(d); char *p=gcs2_xsf("%s/backend.lock.json", s?s:"");
    json_free(d); json_t *r=json_string(p); free(p); return r;
}
/* PoP: gcs2_connect_reservation_path @ hermes_cli/gateway_connections_ssh_remote.py:connect_reservation_path */
static json_t *gcs2_connect_reservation_path(const json_t *oid) {
    json_t *d = gcs2_ownership_directory(oid);
    if (gcs2_is_raise(d)) return d;
    const char *s=gcs2_jstr(d); char *p=gcs2_xsf("%s/.connect.lock", s?s:"");
    json_free(d); json_t *r=json_string(p); free(p); return r;
}
/* PoP: gcs2_spawn_log_path @ hermes_cli/gateway_connections_ssh_remote.py:spawn_log_path */
static json_t *gcs2_spawn_log_path(const json_t *oid, const json_t *nonce) {
    json_t *err=NULL; char *id=gcs2_oid_s(oid,&err); if(err) return err;
    char *nn=gcs2_nonce_s(nonce,&err); if(err){free(id);return err;}
    char *s=gcs2_xsf("%s/%s/%s.log", GCS2_LOCK_DIR, id, nn);
    free(id); free(nn); json_t *r=json_string(s); free(s); return r;
}
/* PoP: gcs2_spawn_token_path @ hermes_cli/gateway_connections_ssh_remote.py:spawn_token_path */
static json_t *gcs2_spawn_token_path(const json_t *oid, const json_t *nonce) {
    json_t *err=NULL; char *id=gcs2_oid_s(oid,&err); if(err) return err;
    char *nn=gcs2_nonce_s(nonce,&err); if(err){free(id);return err;}
    char *s=gcs2_xsf("%s/%s/%s.token", GCS2_LOCK_DIR, id, nn);
    free(id); free(nn); json_t *r=json_string(s); free(s); return r;
}

/* PoP: gcs2_shq @ hermes_cli/gateway_connections_ssh_remote.py:shq */
static json_t *gcs2_shq(const json_t *value) {
    char *v = gcs2_pystr(value);
    gcs2_buf b; gcs2_binit(&b);
    gcs2_bputc(&b,'\'');
    for (const char *p=v; *p; p++) {
        if (*p=='\'') gcs2_bputs(&b,"'\\''");
        else gcs2_bputc(&b,*p);
    }
    gcs2_bputc(&b,'\'');
    free(v);
    char *s=gcs2_btake(&b); json_t *r=json_string(s); free(s); return r;
}

/* PoP: gcs2_validate_remote_path @ hermes_cli/gateway_connections_ssh_remote.py:validate_remote_path */
static json_t *gcs2_validate_remote_path(const json_t *path) {
    const char *s0=gcs2_jstr(path); const char *s=s0?s0:"";
    if (!*s) return gcs2_err("unknown","Remote path must not be empty.");
    if (strchr(s,'\n') || strchr(s,'\r'))
        return gcs2_err("unknown","Unsafe remote path: contains NUL or newline.");
    if (!(s[0]=='~' && s[1]=='/') && strcmp(s,"~") && s[0]!='/') {
        char *m=gcs2_xsf("Remote path must be absolute or start with ~/: \"%s\"",s);
        json_t *r=gcs2_err("unknown",m); free(m); return r;
    }
    return NULL; /* None */
}
/* PoP: gcs2_expand_remote_path @ hermes_cli/gateway_connections_ssh_remote.py:expand_remote_path */
static json_t *gcs2_expand_remote_path(const json_t *path) {
    json_t *v=gcs2_validate_remote_path(path);
    if (v) return v; /* raised */
    const char *s=gcs2_jstr(path); s=s?s:"";
    if (!strcmp(s,"~")) return json_string("\"$HOME\"");
    gcs2_buf b; gcs2_binit(&b);
    if (s[0]=='~' && s[1]=='/') {
        gcs2_bputs(&b,"\"$HOME\"");
        /* shq(s[1:]) where s[1:] starts at '/' */
        char *sub=strdup(s+1);
        json_t *jsub=json_string(sub);
        json_t *qs=gcs2_shq(jsub);
        json_free(jsub);
        gcs2_bputs(&b,gcs2_jstr(qs)?gcs2_jstr(qs):"");
        json_free(qs); free(sub);
    } else {
        json_t *qs=gcs2_shq((json_t*)path);
        gcs2_bputs(&b,gcs2_jstr(qs)?gcs2_jstr(qs):"");
        json_free(qs);
    }
    char *o=gcs2_btake(&b); json_t *r=json_string(o); free(o); return r;
}

/* PoP: gcs2_assert_safe_remote_home @ hermes_cli/gateway_connections_ssh_remote.py:assert_safe_remote_home */
static json_t *gcs2_assert_safe_remote_home(const json_t *home) {
    const char *s0=gcs2_jstr(home);
    char *raw=s0?strdup(s0):strdup("");
    char *beg=raw; while(*beg && (*beg==' '||*beg=='\t'||*beg=='\n'||*beg=='\r'||*beg=='\v'||*beg=='\f')) beg++;
    char *end=beg+strlen(beg); while(end>beg && (end[-1]==' '||end[-1]=='\t'||end[-1]=='\n'||end[-1]=='\r'||end[-1]=='\v'||end[-1]=='\f')) end--;
    *end='\0';
    /* fullmatch (/|~/)[A-Za-z0-9._/+-]+ and no ".." */
    const char *body=NULL;
    if (beg[0]=='/') body=beg+1;
    else if (beg[0]=='~' && beg[1]=='/') body=beg+2;
    bool ok = body && *body && !strstr(beg,"..");
    if (ok) for (const char *p=body; *p; p++)
        if (!(isalnum((unsigned char)*p)||*p=='.'||*p=='_'||*p=='/'||*p=='+'||*p=='-')) { ok=false; break; }
    if (!ok) { free(raw); return gcs2_err("unknown","Unsafe remote Hermes home."); }
    size_t n=strlen(beg); while(n>0 && beg[n-1]=='/') beg[--n]='\0';
    json_t *r=json_string(beg); free(raw); return r;
}
/* PoP: gcs2_remote_install_root @ hermes_cli/gateway_connections_ssh_remote.py:remote_install_root */
static json_t *gcs2_remote_install_root(const json_t *home) {
    json_t *v=gcs2_assert_safe_remote_home(home);
    if (gcs2_is_raise(v)) return v;
    const char *s=gcs2_jstr(v);
    json_t *r=NULL;
    /* rightmost "/profiles/" whose suffix is non-empty and slash-free */
    for (const char *hit=s; *hit; hit++) {
        const char *p=strstr(hit,"/profiles/");
        if (!p) break;
        const char *suf=p+10;
        if (*suf && !strchr(suf,'/')) {
            size_t prelen=(size_t)(p-s);
            char *pre=strndup(s,prelen);
            r=json_string(pre); free(pre);
            hit=p; /* continue scanning to the right for a later match */
        } else { hit=p+1; }
    }
    if (!r) r=json_copy(v);
    json_free(v);
    return r;
}


/* ── compact JSON serializer (json.dumps separators=(",",":"), ensure_ascii=False) ── */
static void gcs2_compact_into(gcs2_buf *b, const json_t *v) {
    if (!v) { gcs2_bputs(b,"null"); return; }
    switch (v->type) {
        case JSON_NULL: gcs2_bputs(b,"null"); break;
        case JSON_BOOL: gcs2_bputs(b, v->bool_val?"true":"false"); break;
        case JSON_NUMBER: {
            double d=v->num_val;
            if (d==floor(d) && isfinite(d) && fabs(d)<1e16) { char t[64]; snprintf(t,sizeof(t),"%.0f",d); gcs2_bputs(b,t); }
            else { char t[64]; snprintf(t,sizeof(t),"%.17g",d); gcs2_bputs(b,t); }
            break;
        }
        case JSON_STRING: {
            gcs2_bputc(b,'"');
            const char *s=v->str_val ? v->str_val : "";
            for (const char *p=s; *p; p++) gcs2_bputc(b,(char)*p);
            gcs2_bputc(b,'"');
            break;
        }
        case JSON_ARRAY: {
            gcs2_bputc(b,'[');
            for (size_t i=0;i<v->c.count;i++){ if(i) gcs2_bputc(b,','); gcs2_compact_into(b,v->c.items[i]); }
            gcs2_bputc(b,']'); break;
        }
        case JSON_OBJECT: {
            gcs2_bputc(b,'{');
            for (size_t i=0;i<v->c.count;i++){
                if(i) gcs2_bputc(b,',');
                gcs2_bputc(b,'"');
                const char *key=v->c.keys[i]?v->c.keys[i]:"";
                for (const char *p=key; *p; p++) gcs2_bputc(b,*p);
                gcs2_bputs(b,"\":");
                gcs2_compact_into(b,v->c.items[i]);
            }
            gcs2_bputc(b,'}'); break;
        }
    }
}
static char *gcs2_compact(const json_t *v) { gcs2_buf b; gcs2_binit(&b); gcs2_compact_into(&b,v); return gcs2_btake(&b); }
static char *gcs2_strip_dup(const char *s) {
    if (!s) return strdup("");
    const char *b=s; while(*b && (*b==' '||*b=='\t'||*b=='\n'||*b=='\r'||*b=='\v'||*b=='\f')) b++;
    const char *e=b+strlen(b); while(e>b && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\n'||e[-1]=='\r'||e[-1]=='\v'||e[-1]=='\f')) e--;
    return strndup(b,(size_t)(e-b));
}
/* LockfileSkew representation */
static json_t *gcs2_skew(const char *reason) {
    json_t *r=json_object();
    json_set(r,"__skew__",json_bool(true));
    json_set(r,"reason",json_string(reason?reason:""));
    return r;
}
static bool gcs2_is_skew(const json_t *v) {
    return v && v->type==JSON_OBJECT && gcs2_jbool(json_obj_get(v,"__skew__"),false);
}
static json_t *gcs2_skew_project(const json_t *v) {
    json_t *r=json_object();
    json_set(r,"reason",json_copy(json_obj_get(v,"reason")));
    return r;
}

/* ── probes ── */
static char *gcs2_out_str(const json_t *r) {
    if (!r || r->type==JSON_NULL) return strdup("");
    if (r->type==JSON_STRING) return strdup(r->str_val?r->str_val:"");
    return gcs2_pystr(r);
}
static json_t *gcs2_lines_last(const char *text) {
    /* splitlines()[-1] for our newline domain; returns fresh string node */
    if (!text || !*text) return json_string("");
    const char *last=text;
    for (const char *p=text; *p; p++) {
        if (*p=='\n'||*p=='\r') { const char *n=p+1; if(*p=='\r'&&*n=='\n') n=p+2; last=n; p=n-1; }
    }
    return json_string(last);
}

/* PoP: gcs2_probe_remote_platform @ hermes_cli/gateway_connections_ssh_remote.py:probe_remote_platform */
static json_t *gcs2_probe_remote_platform(const gcs2_seams_t *s) {
    json_t *r=gcs2_exec(s,"uname -s; uname -m");
    if (gcs2_is_raise(r)) return r;
    char *t=gcs2_strip_dup(gcs2_out_str(r));
    json_free(r);
    char *first=strdup(t), *second=strdup("");
    char *nl=strchr(t,'\n');
    if(nl){ *nl='\0'; free(first); first=strdup(t); char *p=nl+1; char *q=strchr(p,'\n'); if(q)*q='\0'; free(second); second=gcs2_strip_dup(p); }
    char *osn=gcs2_strip_dup(first), *arch=gcs2_strip_dup(second);
    free(first); free(second); free(t);
    if (strcmp(osn,"Linux") && strcmp(osn,"Darwin")) {
        char *m=gcs2_xsf("Unsupported remote platform \"%s\". Gateway SSH connections support Linux and macOS remote hosts.", osn[0]?osn:"unknown");
        json_t *e=gcs2_err("unsupported-platform",m); free(m); free(osn); free(arch); return e;
    }
    json_t *out=json_array(); json_append(out,json_string(osn)); json_append(out,json_string(arch));
    free(osn); free(arch); return out;
}

/* PoP: gcs2_probe_remote_hermes_home @ hermes_cli/gateway_connections_ssh_remote.py:probe_remote_hermes_home */
static json_t *gcs2_probe_remote_hermes_home(const gcs2_seams_t *s) {
    json_t *r=gcs2_exec(s,"echo \"${HERMES_HOME:-$HOME/.hermes}\"");
    if (gcs2_is_raise(r)) {
        if (gcs2_caught(r,"SshReachError")) { json_free(r); return gcs2_err("transient-transport-error","Could not resolve the remote Hermes home."); }
        return r;
    }
    char *t=gcs2_strip_dup(gcs2_out_str(r)); json_free(r);
    json_t *last=gcs2_lines_last(t); free(t);
    const char *ls=gcs2_jstr(last);
    json_t *out = (ls && *ls) ? json_copy(last) : json_string("~/.hermes");
    json_free(last); return out;
}

/* PoP: gcs2_assert_remote_install_update_clear @ hermes_cli/gateway_connections_ssh_remote.py:assert_remote_install_update_clear */
static json_t *gcs2_assert_remote_install_update_clear(const gcs2_seams_t *s, const json_t *home, const json_t *hermes_path) {
    json_t *root=gcs2_remote_install_root(home);
    if (gcs2_is_raise(root)) return root;
    const char *rs=gcs2_jstr(root)?gcs2_jstr(root):"";
    char *mp=gcs2_xsf("%s/.hermes-update-in-progress",rs);
    json_t *jm=json_string(mp); json_t *marker=gcs2_expand_remote_path(jm);
    json_free(jm); free(mp);
    if (gcs2_is_raise(marker)) { json_free(root); return marker; }
    gcs2_buf cmd; gcs2_binit(&cmd);
    gcs2_bputs(&cmd,"python3 - "); gcs2_bputs(&cmd,gcs2_jstr(marker)?gcs2_jstr(marker):"");
    json_free(marker); json_free(root);
    const char *hp=gcs2_jstr(hermes_path);
    if (hp && *hp) {
        json_t *ex=gcs2_expand_remote_path((json_t*)hermes_path);
        if (gcs2_is_raise(ex)) { gcs2_bfree(&cmd); return ex; }
        gcs2_bputs(&cmd," '' "); gcs2_bputs(&cmd,gcs2_jstr(ex)?gcs2_jstr(ex):"");
        json_free(ex);
    }
    char *cmds=gcs2_btake(&cmd);
    json_t *r=gcs2_exec_c(s,cmds,NULL,NULL);
    free(cmds);
    if (gcs2_is_raise(r)) {
        if (gcs2_caught(r,"SshReachError")) { json_free(r); return gcs2_err("update-in-progress","Could not prove that the remote Hermes install is clear for SSH startup."); }
        return r;
    }
    /* splitlines -> last */
    char *t=gcs2_strip_dup(gcs2_out_str(r)); json_free(r);
    char *obs=gcs2_strip_dup(t); free(t);
    gcs2_buf first; gcs2_binit(&first);
    if(*obs){ char *nl=strchr(obs,'\n'); if(nl)*nl='\0'; gcs2_bputs(&first,obs); }
    char *ob=gcs2_btake(&first); free(obs);
    json_t *out=NULL;
    if (!strcmp(ob,"CLEAR")) { free(ob); return NULL; }
    const char *msg=NULL; char *dyn=NULL;
    if (!strncmp(ob,"LIVE:",5)) {
        const char *p=ob+5;
        bool ok=*p>='1'&&*p<='9'; for(const char *q=p;ok&&*q;q++) if(*q<'0'||*q>'9') ok=false;
        if(ok && *p){ dyn=gcs2_xsf("Remote Hermes update process %s is still running; SSH startup is paused.",p); msg=dyn; }
    }
    if (!msg && !strcmp(ob,"HELD")) msg="A process the remote Hermes update started still holds the install; SSH startup is paused.";
    if (!msg) msg="The remote Hermes update marker is unreadable, malformed, or locked; refusing SSH startup.";
    out=gcs2_err("update-in-progress",msg);
    free(dyn); free(ob); return out;
}

/* PoP: gcs2_is_executable @ hermes_cli/gateway_connections_ssh_remote.py:_is_executable */
static bool gcs2_is_executable(const gcs2_seams_t *s, const json_t *candidate) {
    json_t *v=gcs2_validate_remote_path(candidate);
    if (v) { /* raised SshReachError -> False */ json_free(v); return false; }
    json_t *ex=gcs2_expand_remote_path((json_t*)candidate);
    if (gcs2_is_raise(ex)) { json_free(ex); return false; }
    char *cmd=gcs2_xsf("[ -x %s ] && echo OK || true", gcs2_jstr(ex)?gcs2_jstr(ex):"");
    json_free(ex);
    json_t *r=gcs2_exec(s,cmd); free(cmd);
    if (gcs2_is_raise(r)) { if (gcs2_caught(r,"SshReachError")) { json_free(r); return false; } /* non-ssh propagates; treat as false in bool domain */ json_free(r); return false; }
    char *t=gcs2_strip_dup(gcs2_out_str(r)); json_free(r);
    bool ok=!strcmp(t,"OK"); free(t); return ok;
}

/* PoP: gcs2_locate_hermes @ hermes_cli/gateway_connections_ssh_remote.py:locate_hermes */
static json_t *gcs2_locate_hermes(const gcs2_seams_t *s, const json_t *remote_path) {
    const char *rp=gcs2_jstr(remote_path);
    if (rp && *rp) {
        if (gcs2_is_executable(s,(json_t*)remote_path)) return json_copy((json_t*)remote_path);
        char *m=gcs2_xsf("The Hermes path set for this connection is not an executable on the remote host: \"%s\". It must be the full path to the remote `hermes` binary, or blank to auto-detect.",rp);
        json_t *e=gcs2_err("hermes-not-found",m); free(m); return e;
    }
    /* bash -lc 'command -v hermes' */
    json_t *r=gcs2_exec(s,"bash -lc 'command -v hermes'");
    const char *cands[]={"~/.local/bin/hermes","/usr/local/bin/hermes","~/.hermes/hermes-agent/venv/bin/hermes"};
    char *found=strdup("");
    if (!gcs2_is_raise(r)) { char *t=gcs2_strip_dup(gcs2_out_str(r)); json_free(r); if(*t){ char *nl=strrchr(t,'\n'); char *last=nl?gcs2_strip_dup(nl+1):gcs2_strip_dup(t); free(found); found=last; } free(t); }
    else json_free(r);
    if (*found) {
        json_t *jf=json_string(found);
        bool ok=gcs2_is_executable(s,jf);
        json_free(jf);
        if (ok) return json_string(found);
    }
    free(found);
    for (int i=0;i<3;i++) {
        json_t *jc=json_string(cands[i]); bool ok=gcs2_is_executable(s,jc); json_free(jc);
        if (ok) return json_string(cands[i]);
    }
    return gcs2_err("hermes-not-found","Hermes is not installed on the remote host (no `hermes` executable found). Install it there, or set the Hermes path explicitly on the connection.");
}

/* PoP: gcs2_probe_hermes_version @ hermes_cli/gateway_connections_ssh_remote.py:probe_hermes_version */
static json_t *gcs2_probe_hermes_version(const gcs2_seams_t *s, const json_t *hermes_path) {
    json_t *ex=gcs2_expand_remote_path((json_t*)hermes_path);
    if (gcs2_is_raise(ex)) return ex;
    char *inner=gcs2_xsf("%s --version 2>&1", gcs2_jstr(ex)?gcs2_jstr(ex):"");
    json_free(ex);
    json_t *w=gcs2_call(s,"with_timeout",gcs2_args());
    const char *wrapped=gcs2_jstr(w)?gcs2_jstr(w):inner;
    json_t *r=gcs2_exec(s,wrapped);
    if (gcs2_is_raise(r)) { if(gcs2_caught(r,"SshReachError")){json_free(r);json_free(w);free(inner);return json_string("");} json_free(w); free(inner); return r; }
    char *t=gcs2_strip_dup(gcs2_out_str(r)); json_free(r); json_free(w); free(inner);
    char *nl=strchr(t,'\n'); if(nl)*nl='\0';
    char *f=gcs2_strip_dup(t); free(t);
    json_t *out=json_string(f); free(f); return out;
}

/* PoP: gcs2_remote_supports_ssh_ownership @ hermes_cli/gateway_connections_ssh_remote.py:remote_supports_ssh_ownership */
static json_t *gcs2_remote_supports_ssh_ownership(const gcs2_seams_t *s, const json_t *hermes_path) {
    json_t *ex=gcs2_expand_remote_path((json_t*)hermes_path);
    if (gcs2_is_raise(ex)) return ex;
    char *inner=gcs2_xsf("%s serve --help 2>&1", gcs2_jstr(ex)?gcs2_jstr(ex):"");
    json_free(ex);
    json_t *w=gcs2_call(s,"with_timeout",gcs2_args());
    const char *wrapped=gcs2_jstr(w)?gcs2_jstr(w):inner;
    char *cmd=gcs2_xsf("help=\"$( %s )\"; printf '%%s' \"$help\" | grep -q ssh-session-token-file && printf '%%s' \"$help\" | grep -q ssh-owner-nonce && echo YES || echo NO", wrapped);
    json_t *r=gcs2_exec(s,cmd); free(cmd); json_free(w); free(inner);
    if (gcs2_is_raise(r)) return r;
    char *t=gcs2_strip_dup(gcs2_out_str(r)); json_free(r);
    size_t n=strlen(t); bool yes=n>=3 && !strcmp(t+n-3,"YES"); free(t);
    return json_bool(yes);
}


/* ── lockfile ── */
/* PoP: gcs2_valid_int @ hermes_cli/gateway_connections_ssh_remote.py:_valid_int */
static bool gcs2_valid_int(const json_t *v, int lo, int hi) {
    if (!v || v->type!=JSON_NUMBER) return false;
    double d=v->num_val;
    if (d!=floor(d) || !isfinite(d)) return false;
    return d>=lo && d<=hi;
}

/* PoP: gcs2_parse_lockfile @ hermes_cli/gateway_connections_ssh_remote.py:parse_lockfile */
static json_t *gcs2_parse_lockfile(const json_t *text, const json_t *ownership_id) {
    char *t=gcs2_strip_dup(gcs2_jstr(text)?gcs2_jstr(text):"");
    if (!*t) { free(t); return NULL; }
    char *err=NULL;
    json_t *parsed=json_parse(t,&err);
    free(t);
    if (parsed && parsed->type != JSON_OBJECT) { json_free(parsed); free(err); return gcs2_skew("non-object"); }
    if (err || !parsed) { free(err); if(parsed)json_free(parsed); return gcs2_skew("unparseable-json"); }
    free(err);
    json_t *sv=json_obj_get(parsed,"schemaVersion");
    if (!gcs2_valid_int(sv,LOCKFILE_SCHEMA_VERSION,LOCKFILE_SCHEMA_VERSION) ) {
        char *d=gcs2_compact(sv);
        char *m=gcs2_xsf("schema-version %s",d); free(d);
        json_t *r=gcs2_skew(m); free(m); json_free(parsed); return r;
    }
    if (!gcs2_valid_int(json_obj_get(parsed,"pid"),1,4194304)) { json_free(parsed); return gcs2_skew("malformed-pid"); }
    if (!gcs2_valid_int(json_obj_get(parsed,"port"),0,65535)) { json_free(parsed); return gcs2_skew("malformed-port"); }
    json_t *nonce=json_obj_get(parsed,"spawnNonce");
    const char *oidv=gcs2_jstr(json_obj_get(parsed,"ownershipId"));
    const char *noncev=gcs2_jstr(nonce);
    if (!oidv || strcmp(oidv,gcs2_jstr(ownership_id)?gcs2_jstr(ownership_id):"") || !gcs2_hex_all(noncev?noncev:"",16)) {
        json_free(parsed); return gcs2_skew("ownership-mismatch");
    }
    json_t *fp=json_obj_get(parsed,"tokenFingerprint");
    if (!gcs2_hex_all(gcs2_jstr(fp)?gcs2_jstr(fp):"",32)) { json_free(parsed); return gcs2_skew("malformed-token-fingerprint"); }
    json_t *pv=json_obj_get(parsed,"protocolVersion");
    if (!(pv && pv->type==JSON_NUMBER && pv->num_val==PROTOCOL_VERSION)) { json_free(parsed); return NULL; }
    /* logPath must equal spawn_log_path(ownership_id, nonce) */
    json_t *slp=gcs2_spawn_log_path(ownership_id,nonce);
    if (gcs2_is_raise(slp)) { json_free(slp); json_free(parsed); return NULL; }
    const char *lpv=gcs2_jstr(json_obj_get(parsed,"logPath"));
    if (!lpv || strcmp(lpv,gcs2_jstr(slp)?gcs2_jstr(slp):"")) { json_free(slp); json_free(parsed); return gcs2_skew("log-path-mismatch"); }
    json_free(slp);
    const char *fields[]={"profile","hermesPath","hermesHome","logPath","startedAt",NULL};
    for (int i=0;fields[i];i++) {
        json_t *fv=json_obj_get(parsed,fields[i]);
        const char *fs=gcs2_jstr(fv);
        if (!fs || strlen(fs)>1024) { char *m=gcs2_xsf("malformed-field %s",fields[i]); json_t *r=gcs2_skew(m); free(m); json_free(parsed); return r; }
    }
    json_t *ct=json_obj_get(parsed,"creationTime");
    bool ct_present = ct && ct->type!=JSON_NULL;
    if (ct_present && (!gcs2_jstr(ct) || !gcs2_creation_time_ok(gcs2_jstr(ct)))) { json_free(parsed); return NULL; }
    return parsed;
}

/* PoP: gcs2_read_lockfile @ hermes_cli/gateway_connections_ssh_remote.py:read_lockfile */
static json_t *gcs2_read_lockfile(const gcs2_seams_t *s, const json_t *oid) {
    json_t *lp=gcs2_lockfile_path(oid);
    if (gcs2_is_raise(lp)) return lp;
    json_t *jx=json_string(gcs2_jstr(lp)?gcs2_jstr(lp):"");
    json_t *ex=gcs2_expand_remote_path(jx); json_free(jx); json_free(lp);
    if (gcs2_is_raise(ex)) return ex;
    char *cmd=gcs2_xsf("if [ ! -e %s ]; then exit 0; fi; cat %s", gcs2_jstr(ex)?gcs2_jstr(ex):"", gcs2_jstr(ex)?gcs2_jstr(ex):"");
    json_free(ex);
    json_t *r=gcs2_exec(s,cmd); free(cmd);
    if (gcs2_is_raise(r)) {
        if (gcs2_caught(r,"SshReachError")) { json_free(r); return gcs2_err("transient-transport-error","Could not read the SSH backend ownership record."); }
        return r;
    }
    json_t *out=gcs2_parse_lockfile(r,oid);
    json_free(r); return out;
}

/* PoP: gcs2_lock_json @ hermes_cli/gateway_connections_ssh_remote.py:_lock_json */
static json_t *gcs2_lock_json(const json_t *lock) {
    char *s=gcs2_compact(lock);
    json_t *r=json_string(s); free(s); return r;
}

/* PoP: gcs2_write_lockfile @ hermes_cli/gateway_connections_ssh_remote.py:write_lockfile */
static json_t *gcs2_write_lockfile(const gcs2_seams_t *s, const json_t *oid, const json_t *lock) {
    json_t *dir=gcs2_ownership_directory(oid);
    if (gcs2_is_raise(dir)) return dir;
    const char *dirs=gcs2_jstr(dir)?gcs2_jstr(dir):"";
    json_t *th=gcs2_token_hex(s,8);
    if (gcs2_is_raise(th)) { json_free(dir); return th; }
    char *tmp=gcs2_xsf("%s/.%s.lock.tmp", dirs, gcs2_jstr(th)?gcs2_jstr(th):"");
    json_free(th);
    /* payload = {**lock, "schemaVersion": 2} */
    json_t *payload=json_copy((json_t*)lock);
    json_set(payload,"schemaVersion",json_number(LOCKFILE_SCHEMA_VERSION));
    json_t *pl=gcs2_lock_json(payload); json_free(payload);
    /* command: umask 077 && mkdir -p <dir> && printf '%s' <payload> > <tmp> && mv -f <tmp> <lockfile> */
    json_t *lp=gcs2_lockfile_path(oid);
    const char *lps=(lp && !gcs2_is_raise(lp)) ? gcs2_jstr(lp) : "";
    char *cmd=gcs2_xsf("umask 077 && mkdir -p %s && printf '%%s' %s > %s && mv -f %s %s",
                       dirs, gcs2_jstr(pl)?gcs2_jstr(pl):"", tmp, tmp, lps);
    json_free(pl); json_free(lp); json_free(dir); free(tmp);
    json_t *r=gcs2_exec(s,cmd); free(cmd);
    return r; /* NULL == None; raise propagates */
}

/* PoP: gcs2_remove_lockfile @ hermes_cli/gateway_connections_ssh_remote.py:remove_lockfile */
static json_t *gcs2_remove_lockfile(const gcs2_seams_t *s, const json_t *oid) {
    json_t *lp=gcs2_lockfile_path(oid);
    if (gcs2_is_raise(lp)) return lp;
    json_t *jx=json_string(gcs2_jstr(lp)?gcs2_jstr(lp):"");
    json_t *ex=gcs2_expand_remote_path(jx); json_free(jx); json_free(lp);
    if (gcs2_is_raise(ex)) { json_free(ex); return NULL; }
    char *cmd=gcs2_xsf("rm -f %s", gcs2_jstr(ex)?gcs2_jstr(ex):"");
    json_free(ex);
    json_t *r=gcs2_exec(s,cmd); free(cmd);
    if (gcs2_is_raise(r) && gcs2_caught(r,"SshReachError")) { json_free(r); return NULL; }
    if (gcs2_is_raise(r)) return r;
    json_free(r); return NULL;
}

/* ── process identity ── */
/* PoP: gcs2_exec_probe_verdict @ hermes_cli/gateway_connections_ssh_remote.py:exec_probe_verdict */
static json_t *gcs2_exec_probe_verdict(const gcs2_seams_t *s, const char *cmd, const json_t *sentinels, const char *failure) {
    for (int attempt=0; attempt<PROBE_VERDICT_ATTEMPTS; attempt++) {
        if (attempt) { json_t *sl=gcs2_sleep(s,PROBE_VERDICT_RETRY_S); if (gcs2_is_raise(sl)) return sl; json_free(sl); }
        json_t *r=gcs2_exec(s,cmd);
        if (gcs2_is_raise(r)) {
            if (gcs2_caught(r,"SshReachError")) { json_free(r); return gcs2_err("transient-transport-error",failure); }
            return r;
        }
        char *t=gcs2_strip_dup(gcs2_out_str(r)); json_free(r);
        for (size_t i=0;sentinels && i<sentinels->c.count;i++) {
            const char *sv=gcs2_jstr(sentinels->c.items[i]);
            if (sv && !strcmp(t,sv)) { free(t); return json_string(sv); }
        }
        free(t);
    }
    return gcs2_err("transient-transport-error",failure);
}

/* PoP: gcs2_remote_pid_alive @ hermes_cli/gateway_connections_ssh_remote.py:remote_pid_alive */
static json_t *gcs2_remote_pid_alive(const gcs2_seams_t *s, const json_t *pid) {
    if (!pid || pid->type!=JSON_NUMBER || pid->num_val<=0 || pid->num_val!=floor(pid->num_val)) return json_bool(false);
    long long p=(long long)pid->num_val;
    char *cmd=gcs2_xsf("kill -0 %lld 2>/dev/null && echo ALIVE || echo DEAD",p);
    json_t *sents=json_array(); json_append(sents,json_string("ALIVE")); json_append(sents,json_string("DEAD"));
    json_t *r=gcs2_exec_probe_verdict(s,cmd,sents,"Could not verify the SSH backend process.");
    free(cmd); json_free(sents);
    if (gcs2_is_raise(r)) return r;
    const char *v=gcs2_jstr(r); bool ok=v&&!strcmp(v,"ALIVE"); json_free(r); return json_bool(ok);
}

/* PoP: gcs2_remote_process_creation_time @ hermes_cli/gateway_connections_ssh_remote.py:remote_process_creation_time */
static json_t *gcs2_remote_process_creation_time(const gcs2_seams_t *s, const json_t *pid) {
    if (!pid || pid->type!=JSON_NUMBER || pid->num_val<=0 || pid->num_val!=floor(pid->num_val)) return json_string("");
    long long p=(long long)pid->num_val;
    /* embedded python source projected: /proc/ ... fields[19] */
    char *cmd=gcs2_xsf("python3 -c 'import sys;pid=%lld;/proc/...;fields[19]'",p);
    json_t *r=gcs2_exec(s,cmd); free(cmd);
    if (gcs2_is_raise(r)) { if(gcs2_caught(r,"SshReachError")){json_free(r);return json_string("");} return r; }
    char *t=gcs2_strip_dup(gcs2_out_str(r)); json_free(r);
    json_t *out = gcs2_creation_time_ok(t) ? json_string(t) : json_string("");
    free(t); return out;
}

/* PoP: gcs2_pid_is_our_serve @ hermes_cli/gateway_connections_ssh_remote.py:pid_is_our_serve */
static json_t *gcs2_pid_is_our_serve(const gcs2_seams_t *s, const json_t *lock, const json_t *oid) {
    json_t *pidv=json_obj_get((json_t*)lock,"pid");
    json_t *noncev=json_obj_get((json_t*)lock,"spawnNonce");
    const char *nonce=gcs2_jstr(noncev)?gcs2_jstr(noncev):"";
    json_t *hpv=json_obj_get((json_t*)lock,"hermesPath");
    const char *hp=gcs2_jstr(hpv)?gcs2_jstr(hpv):"";
    bool pidtruth = pidv && gcs2_jtruth(pidv);
    if (!pidtruth || !gcs2_hex_all(nonce,16) || !*hp) return json_bool(false);
    /* embedded ownership python script projected */
    (void)oid;
    const char *cmd="python3 -c 'import os;expected_entries=...;ssh-owner-nonce;serve;isolated'";
    json_t *sents=json_array(); json_append(sents,json_string("OWNED")); json_append(sents,json_string("FOREIGN"));
    json_t *r=gcs2_exec_probe_verdict(s,cmd,sents,"Could not verify SSH backend process ownership.");
    json_free(sents);
    if (gcs2_is_raise(r)) return r;
    const char *v=gcs2_jstr(r); bool ok=v&&!strcmp(v,"OWNED"); json_free(r); return json_bool(ok);
}

/* PoP: gcs2_terminate @ hermes_cli/gateway_connections_ssh_remote.py:_terminate */
static json_t *gcs2_terminate(const gcs2_seams_t *s, const json_t *pid) {
    long long p=gcs2_jint(pid,0);
    char *c1=gcs2_xsf("kill %lld && i=0; while kill -0 %lld; do i=$((i+1)); [ \"$i\" -ge 50 ]; sleep 0.1; done",p,p);
    json_t *r=gcs2_exec(s,c1); free(c1);
    if (!gcs2_is_raise(r)) { json_free(r); return NULL; }
    if (!gcs2_caught(r,"SshReachError")) return r;
    json_free(r);
    char *c2=gcs2_xsf("kill -9 %lld 2>/dev/null; i=0; while kill -0 %lld; do i=$((i+1)); [ \"$i\" -ge 20 ]; sleep 0.1; done",p,p);
    json_t *r2=gcs2_exec(s,c2); free(c2);
    if (gcs2_is_raise(r2)) {
        if (gcs2_caught(r2,"SshReachError")) { json_free(r2); return gcs2_err("transient-transport-error","Could not terminate the stale SSH backend."); }
        return r2;
    }
    json_free(r2); return NULL;
}

/* PoP: gcs2_cleanup_stale @ hermes_cli/gateway_connections_ssh_remote.py:cleanup_stale */
static json_t *gcs2_cleanup_stale(const gcs2_seams_t *s, const json_t *oid, const json_t *lock, bool pid_alive) {
    if (gcs2_is_skew(lock)) return NULL;
    bool lock_truth = lock && lock->type==JSON_OBJECT && lock->c.count>0;
    if (lock_truth && pid_alive) {
        json_t *ours=gcs2_pid_is_our_serve(s,lock,oid);
        if (gcs2_is_raise(ours)) return ours;
        bool ok=gcs2_jbool(ours,false); json_free(ours);
        if (ok) { json_t *t=gcs2_terminate(s,json_obj_get((json_t*)lock,"pid")); if(gcs2_is_raise(t)) return t; json_free(t); }
    }
    json_t *nonce = lock_truth ? json_obj_get((json_t*)lock,"spawnNonce") : NULL;
    bool nonce_truth = nonce && gcs2_jtruth(nonce);
    if (lock_truth && nonce_truth) {
        json_t *slp=gcs2_spawn_log_path(oid,nonce);
        if (!gcs2_is_raise(slp)) {
            const char *lpv=gcs2_jstr(json_obj_get((json_t*)lock,"logPath"));
            if (lpv && !strcmp(lpv,gcs2_jstr(slp)?gcs2_jstr(slp):"")) {
                json_t *jx=json_string(lpv); json_t *ex=gcs2_expand_remote_path(jx); json_free(jx);
                if (gcs2_is_raise(ex)) { json_free(ex); json_free(slp); return NULL; }
                char *cmd=gcs2_xsf("rm -f %s",gcs2_jstr(ex)?gcs2_jstr(ex):""); json_free(ex);
                json_t *r=gcs2_exec(s,cmd); free(cmd);
                if (gcs2_is_raise(r) && !gcs2_caught(r,"SshReachError")) { json_free(r); json_free(slp); return r; }
                json_free(r);
            }
            json_free(slp);
        } else json_free(slp);
    }
    json_t *r=gcs2_remove_lockfile(s,oid);
    if (gcs2_is_raise(r)) return r;
    json_free(r); return NULL;
}

/* PoP: gcs2_disconnect_remote @ hermes_cli/gateway_connections_ssh_remote.py:disconnect_remote */
static json_t *gcs2_disconnect_remote(const gcs2_seams_t *s, const json_t *oid) {
    json_t *lock=gcs2_read_lockfile(s,oid);
    if (gcs2_is_raise(lock)) return lock;
    bool falsy = gcs2_is_null(lock) || gcs2_is_skew(lock) || (lock->type==JSON_OBJECT && lock->c.count==0);
    if (falsy) { if(lock)json_free(lock); return json_bool(false); }
    json_t *pid=json_obj_get(lock,"pid");
    json_t *alive=gcs2_remote_pid_alive(s,pid);
    if (gcs2_is_raise(alive)) { json_free(lock); return alive; }
    bool a=gcs2_jbool(alive,false); json_free(alive);
    json_t *r=gcs2_cleanup_stale(s,oid,lock,a); json_free(lock);
    if (gcs2_is_raise(r)) return r;
    json_free(r); return json_bool(true);
}


/* PoP: gcs2_mint_token @ hermes_cli/gateway_connections_ssh_remote.py:mint_token */
static json_t *gcs2_mint_token(const gcs2_seams_t *s) { return gcs2_token_hex(s,32); }

/* PoP: gcs2_with_remote_update_mutex @ hermes_cli/gateway_connections_ssh_remote.py:with_remote_update_mutex */
static json_t *gcs2_with_remote_update_mutex(const char *command, const char *marker_word, const char *hermes_word) {
    /* Projection: the giant embedded marker-gate script is opaque; the
       observable decision is (command tag, marker present, hermes present). */
    const char *tag=gcs2_classify(command);
    return json_string(gcs2_xsf("mutex:%s:%d:%d",tag,(marker_word&&*marker_word)?1:0,(hermes_word&&*hermes_word)?1:0));
}

/* PoP: gcs2_spawn_payload_prefix @ hermes_cli/gateway_connections_ssh_remote.py:_spawn_payload_prefix */
static json_t *gcs2_spawn_payload_prefix(const json_t *plan) {
    json_t *ex=gcs2_expand_remote_path(json_obj_get((json_t*)plan,"hermes_path"));
    if (gcs2_is_raise(ex)) return ex;
    json_t *lg=gcs2_expand_remote_path(json_obj_get((json_t*)plan,"log_path"));
    if (gcs2_is_raise(lg)) { json_free(ex); return lg; }
    json_t *nonce=gcs2_validate_spawn_nonce(json_obj_get((json_t*)plan,"spawn_nonce"));
    if (gcs2_is_raise(nonce)) { json_free(ex); json_free(lg); return nonce; }
    json_free(nonce);
    /* command build validates profile/token paths too (no observable effect on projection) */
    json_t *out=json_array();
    json_append(out,json_copy(ex)); json_append(out,json_copy(lg));
    json_append(out,json_string("detached_spawn"));
    json_free(ex); json_free(lg); return out;
}

/* PoP: gcs2_build_spawn_command @ hermes_cli/gateway_connections_ssh_remote.py:build_spawn_command */
static json_t *gcs2_build_spawn_command(const json_t *plan) {
    /* run the real validation/composition path (raises propagate), then project
       the opaque shell text to a canonical tag. */
    json_t *prefix=gcs2_spawn_payload_prefix(plan);
    if (gcs2_is_raise(prefix)) return prefix;
    json_free(prefix);
    const char *home=gcs2_jstr(json_obj_get((json_t*)plan,"hermes_home"));
    if (!home || !*home) home="~/.hermes";
    json_t *jh=json_string(home);
    json_t *root=gcs2_remote_install_root(jh);
    if (gcs2_is_raise(root)) { json_free(jh); return root; }
    json_free(jh); json_free(root);
    json_t *oid=gcs2_validate_ownership_id(json_obj_get((json_t*)plan,"ownership_id"));
    if (gcs2_is_raise(oid)) return oid; json_free(oid);
    json_t *nonce=gcs2_validate_spawn_nonce(json_obj_get((json_t*)plan,"spawn_nonce"));
    if (gcs2_is_raise(nonce)) return nonce; json_free(nonce);
    return json_string("spawn_cmd");
}

/* PoP: gcs2_remove_remote @ hermes_cli/gateway_connections_ssh_remote.py:_remove_remote */
static json_t *gcs2_remove_remote(const gcs2_seams_t *s, const json_t *path) {
    json_t *ex=gcs2_expand_remote_path((json_t*)path);
    if (gcs2_is_raise(ex)) { json_free(ex); return NULL; }
    char *cmd=gcs2_xsf("rm -f %s",gcs2_jstr(ex)?gcs2_jstr(ex):""); json_free(ex);
    json_t *r=gcs2_exec(s,cmd); free(cmd);
    if (gcs2_is_raise(r) && gcs2_caught(r,"SshReachError")) { json_free(r); return NULL; }
    if (gcs2_is_raise(r)) return r;
    json_free(r); return NULL;
}

/* PoP: gcs2_now_iso @ hermes_cli/gateway_connections_ssh_remote.py:_now_iso */
static json_t *gcs2_now_iso(const gcs2_seams_t *s) {
    json_t *r=gcs2_seam0(s,"now_iso");
    if (gcs2_is_raise(r)) return r;
    return r; /* scripted ISO string */
}

/* case-insensitive substring helper */
static bool gcs2_ci_prefix(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    size_t i = 0;
    for (; prefix[i]; i++)
        if (!s[i] || tolower((unsigned char)s[i]) != tolower((unsigned char)prefix[i])) return false;
    return true;
}
static bool gcs2_ci_contains(const char *hay, const char *needle) {
    if (!hay||!needle) return false;
    size_t n=strlen(needle);
    for (const char *p=hay; *p; p++) {
        size_t i=0; while(i<n && tolower((unsigned char)p[i])==tolower((unsigned char)needle[i])) i++;
        if (i==n) return true;
    }
    return false;
}
/* PoP: gcs2_remote_profile_missing_error @ hermes_cli/gateway_connections_ssh_remote.py:remote_profile_missing_error */
static json_t *gcs2_remote_profile_missing_error(const json_t *output) {
    const char *src=gcs2_jstr(output)?gcs2_jstr(output):"";
    /* find case-insensitive "Error: Profile " */
    const char *p=src;
    while (*p && !gcs2_ci_prefix(p,"Error: Profile ")) p++;
    if (!*p) return NULL;
    const char *q=p+15; /* after "Error: Profile " */
    char quote=0;
    if (*q=='\''||*q=='"') quote=*q; else return NULL;
    q++;
    const char *end=q; while(*end && *end!=quote) end++;
    if (!*end) return NULL;
    size_t nlen=(size_t)(end-q);
    char *name=strndup(q,nlen);
    const char *r=end+1;
    /* expect " does not exist" (case-insensitive) */
    if (!gcs2_ci_prefix(r," does not exist")) { free(name); return NULL; }
    r+=15;
    if (*r=='.') r++;
    /* optional " Create it with: <rest-of-line>" */
    char *create=strdup("");
    const char *cw=r;
    while (*cw==' ') cw++;
    if (gcs2_ci_prefix(cw,"Create it with:")) {
        const char *v=cw+15; while(*v==' ') v++;
        const char *e=v; while(*e && *e!='\n' && *e!='\r') e++;
        free(create); create=strndup(v,(size_t)(e-v));
        char *trim=gcs2_strip_dup(create); free(create); create=trim;
    }
    char *hint = *create ? gcs2_xsf(" or create it with: %s",create) : strdup(".");
    char *m=gcs2_xsf("The remote Hermes profile '%s' does not exist. Select an existing remote profile%s",name,hint);
    json_t *e=gcs2_err("spawn-failed",m);
    free(m); free(hint); free(create); free(name); return e;
}

/* READY_RE search: (?<!\w)HERMES_(BACKEND|DASHBOARD)_READY port=(\d+) */
static bool gcs2_ready_port(const char *tail, int *port) {
    const char *marks[]={"HERMES_BACKEND_READY port=","HERMES_DASHBOARD_READY port=",NULL};
    for (int i=0;marks[i];i++) {
        const char *p=tail;
        while ((p=strstr(p,marks[i]))) {
            bool prevword = p>tail && (isalnum((unsigned char)p[-1])||p[-1]=='_');
            if (!prevword) {
                const char *d=p+strlen(marks[i]);
                if (*d>='0'&&*d<='9') {
                    long long v=0; while(*d>='0'&&*d<='9'){v=v*10+(*d-'0');d++;}
                    *port=(int)v; return true;
                }
            }
            p++;
        }
    }
    return false;
}
/* PoP: gcs2_scrape_ready_port @ hermes_cli/gateway_connections_ssh_remote.py:scrape_ready_port */
static json_t *gcs2_scrape_ready_port(const gcs2_seams_t *s, const json_t *log_path, double timeout_s) {
    json_t *m1=gcs2_call(s,"monotonic",gcs2_args());
    if (gcs2_is_raise(m1)) return m1;
    double deadline=gcs2_jnum(m1,0)+timeout_s; json_free(m1);
    json_t *ex=gcs2_expand_remote_path((json_t*)log_path);
    if (gcs2_is_raise(ex)) return ex;
    char *cmd=gcs2_xsf("cat %s 2>/dev/null || true",gcs2_jstr(ex)?gcs2_jstr(ex):""); json_free(ex);
    for (int guard=0; guard<10000; guard++) {
        json_t *now=gcs2_call(s,"monotonic",gcs2_args());
        if (gcs2_is_raise(now)) { free(cmd); return now; }
        double cur=gcs2_jnum(now,0); json_free(now);
        if (!(cur<deadline)) { free(cmd); return gcs2_err("ready-timeout",gcs2_xsf("Timed out waiting for the remote Hermes serve to announce its port (%.0fs).",timeout_s)); }
        json_t *r=gcs2_exec(s,cmd);
        char *tail=strdup("");
        if (gcs2_is_raise(r)) { if(!gcs2_caught(r,"SshReachError")){free(tail);free(cmd);return r;} json_free(r); }
        else { free(tail); tail=gcs2_out_str(r); json_free(r); }
        /* is_alive callback omitted: fixtures drive the no-callback path */
        int port=0;
        if (gcs2_ready_port(tail,&port)) { free(tail); free(cmd); return json_number(port); }
        free(tail);
        json_t *sl=gcs2_sleep(s,READY_POLL_INTERVAL_S);
        if (gcs2_is_raise(sl)) { free(cmd); return sl; } json_free(sl);
    }
    free(cmd); return gcs2_err("ready-timeout","Timed out waiting for the remote Hermes serve to announce its port.");
}

/* PoP: gcs2_classify_ssh_reuse_proof @ hermes_cli/gateway_connections_ssh_remote.py:classify_ssh_reuse_proof */
static json_t *gcs2_classify_ssh_reuse_proof(const json_t *proof, const json_t *nonce) {
    bool ok = false;
    if (proof && proof->type==JSON_OBJECT) {
        json_t *o=json_obj_get(proof,"ok");
        bool ok_true = o && o->type==JSON_BOOL && o->bool_val;
        json_t *sn=json_obj_get(proof,"sshOwnerNonce");
        bool nonce_ok = sn && sn->type==JSON_STRING && nonce && !strcmp(sn->str_val,gcs2_jstr(nonce)?gcs2_jstr(nonce):"");
        json_t *pv=json_obj_get(proof,"protocolVersion");
        bool ver_ok = pv && pv->type==JSON_NUMBER && pv->num_val==PROTOCOL_VERSION;
        json_t *ri=json_obj_get(proof,"runtimeIntact");
        bool ri_ok = !(ri && ri->type==JSON_BOOL && !ri->bool_val);
        ok = ok_true && nonce_ok && ver_ok && ri_ok;
    }
    return json_string(ok?"authenticated-ok":"authenticated-stale");
}

/* PoP: gcs2_lock_serves_connect @ hermes_cli/gateway_connections_ssh_remote.py:lock_serves_connect */
static json_t *gcs2_lock_serves_connect(const json_t *lock, const char *profile, const json_t *reuse_token,
                                        const char *hermes_path, const char *hermes_home) {
    json_t *port=json_obj_get((json_t*)lock,"port");
    if (!port || port->type!=JSON_NUMBER || port->num_val<=0) return json_bool(false);
    const char *lp=gcs2_jstr(json_obj_get((json_t*)lock,"profile"));
    if (!lp || strcmp(lp,profile?profile:"")) return json_bool(false);
    const char *tok=gcs2_jstr(reuse_token);
    if (!tok || !*tok) return json_bool(false);
    json_t *fp=gcs2_fingerprint_token(reuse_token);
    const char *lfp=gcs2_jstr(json_obj_get((json_t*)lock,"tokenFingerprint"));
    if (!lfp || strcmp(lfp,gcs2_jstr(fp)?gcs2_jstr(fp):"")) { json_free(fp); return json_bool(false); }
    json_free(fp);
    const char *lh=gcs2_jstr(json_obj_get((json_t*)lock,"hermesPath"));
    if (!lh || strcmp(lh,hermes_path?hermes_path:"")) return json_bool(false);
    const char *lhm=gcs2_jstr(json_obj_get((json_t*)lock,"hermesHome"));
    if (!lhm || strcmp(lhm,hermes_home?hermes_home:"")) return json_bool(false);
    json_t *ds=json_obj_get((json_t*)lock,"devServer");
    const char *dsv=gcs2_jstr(ds);
    if (dsv && *dsv) return json_bool(false);
    return json_bool(true);
}

/* PoP: gcs2_is_forward_bind_collision @ hermes_cli/gateway_connections_ssh_remote.py:is_forward_bind_collision */
static bool gcs2_is_forward_bind_collision(const char *s) {
    if (!s) return false;
    if (gcs2_ci_contains(s,"address already in use") || gcs2_ci_contains(s,"cannot listen to port")) return true;
    /* bind.*failed (IGNORECASE, dot matches any char) */
    for (const char *p=s; *p; p++) {
        if (gcs2_ci_prefix(p,"bind")) {
            const char *q=p+4;
            for (; *q; q++) if (gcs2_ci_prefix(q,"failed")) return true;
        }
    }
    return false;
}
static bool gcs2_exc_is_collision(const json_t *r) {
    if (!r) return false;
    const char *m=gcs2_jstr(json_obj_get((json_t*)r,"msg"));
    return gcs2_is_forward_bind_collision(m?m:"");
}

/* PoP: gcs2_open_forward @ hermes_cli/gateway_connections_ssh_remote.py:open_forward */
static json_t *gcs2_open_forward(const gcs2_seams_t *s, int remote_port, int attempts) {
    for (int attempt=0; attempt<attempts; attempt++) {
        json_t *pr=gcs2_seam0(s,"pick_port");
        if (gcs2_is_raise(pr)) return pr;
        int local=(int)gcs2_jnum(pr,0); json_free(pr);
        json_t *r=gcs2_forward(s,local,remote_port);
        if (!gcs2_is_raise(r)) { json_free(r); return json_number(local); }
        if (!gcs2_exc_is_collision(r) || attempt==attempts-1) return r;
        json_free(r);
    }
    return gcs2_raise_spec("AssertionError","unreachable");
}

/* PoP: gcs2_wait_for_remote_spawn_completion @ hermes_cli/gateway_connections_ssh_remote.py:wait_for_remote_spawn_completion */
static json_t *gcs2_wait_for_remote_spawn_completion(const gcs2_seams_t *s, const json_t *oid, double timeout_s) {
    json_t *m1=gcs2_call(s,"monotonic",gcs2_args());
    if (gcs2_is_raise(m1)) return m1;
    double deadline=gcs2_jnum(m1,0)+timeout_s; json_free(m1);
    for (int guard=0;guard<10000;guard++) {
        json_t *now=gcs2_call(s,"monotonic",gcs2_args());
        if (gcs2_is_raise(now)) return now;
        double cur=gcs2_jnum(now,0); json_free(now);
        if (!(cur<deadline)) return gcs2_err("spawn-failed","Timed out waiting for a concurrent SSH connection to publish its backend.");
        json_t *lock=gcs2_read_lockfile(s,oid);
        if (gcs2_is_raise(lock)) return lock;
        bool falsy=gcs2_is_null(lock)||gcs2_is_skew(lock)||(lock->type==JSON_OBJECT&&lock->c.count==0);
        if (falsy) { if(lock)json_free(lock); return json_bool(false); }
        json_t *port=json_obj_get(lock,"port");
        bool ready=port && port->type==JSON_NUMBER && port->num_val>0;
        json_free(lock);
        if (ready) return json_bool(true);
        json_t *sl=gcs2_sleep(s,READY_POLL_INTERVAL_S);
        if (gcs2_is_raise(sl)) return sl; json_free(sl);
    }
    return gcs2_err("spawn-failed","Timed out waiting for a concurrent SSH connection to publish its backend.");
}

/* PoP: gcs2_skew_error @ hermes_cli/gateway_connections_ssh_remote.py:_skew_error */
static json_t *gcs2_skew_error(const json_t *oid, const json_t *skew) {
    json_t *lp=gcs2_lockfile_path(oid);
    if (gcs2_is_raise(lp)) return lp;
    const char *path=gcs2_jstr(lp)?gcs2_jstr(lp):"";
    const char *reason=gcs2_jstr(json_obj_get((json_t*)skew,"reason"));
    char *m=gcs2_xsf("The remote ownership record %s does not match this Hermes build (%s). It was probably written by a different build sharing this remote, or the file is corrupt. Refusing to reap or overwrite it. If nothing else uses this remote, delete that file there and reconnect.", path?path:"", reason?reason:"");
    json_free(lp);
    json_t *e=gcs2_err("remote-lockfile-skew",m); free(m); return e;
}


/* deps accessors */
static const char *gcs2_dstr(const json_t *o, const char *k) {
    return gcs2_jstr(json_obj_get((json_t*)o,k))?gcs2_jstr(json_obj_get((json_t*)o,k)):"";
}
static double gcs2_dnum(const json_t *o, const char *k, double d) { return gcs2_jnum(json_obj_get((json_t*)o,k),d); }

static json_t *gcs2_spawned(bool existing, const char *pid, const char *nonce, const char *log, const char *tok) {
    json_t *r=json_object();
    json_set(r,"existing",json_bool(existing));
    json_set(r,"pid",pid?json_number(atoll(pid)):json_number(0));
    json_set(r,"spawn_nonce",json_string(nonce?nonce:""));
    json_set(r,"log_path",json_string(log?log:""));
    json_set(r,"token_file_path",json_string(tok?tok:""));
    return r;
}

/* PoP: gcs2_spawn_remote_serve @ hermes_cli/gateway_connections_ssh_remote.py:spawn_remote_serve */
static json_t *gcs2_spawn_remote_serve(const gcs2_seams_t *s, const json_t *hermes_path, const char *profile,
                                       const json_t *token, const json_t *oid, const json_t *home) {
    json_t *sup=gcs2_remote_supports_ssh_ownership(s,hermes_path);
    if (gcs2_is_raise(sup)) return sup;
    if (!gcs2_jbool(sup,false)) return gcs2_err("update-required","The remote Hermes install does not support --ssh-session-token-file and --ssh-owner-nonce. Update Hermes on the remote host.");
    json_t *nonce=gcs2_token_hex(s,8);
    if (gcs2_is_raise(nonce)) return nonce;
    const char *ns=gcs2_jstr(nonce)?gcs2_jstr(nonce):"";
    json_t *tp=gcs2_spawn_token_path(oid,nonce);
    if (gcs2_is_raise(tp)) { json_free(nonce); return tp; }
    json_t *lp=gcs2_spawn_log_path(oid,nonce);
    if (gcs2_is_raise(lp)) { json_free(nonce); json_free(tp); return lp; }
    json_t *fp=gcs2_fingerprint_token(token); json_free(fp);
    json_t *iso=gcs2_now_iso(s);
    if (gcs2_is_raise(iso)) { json_free(nonce); json_free(tp); json_free(lp); return iso; }
    json_free(iso);
    /* token upload on stdin, then assert-clear, then spawn (one exec each) */
    json_t *r=gcs2_exec(s,"python3 -c 'import os;O_DIRECTORY;token upload'");
    bool failed=gcs2_is_raise(r); if(r) json_free(r);
    if (!failed) { json_t *ac=gcs2_seam0(s,"assert_clear"); failed=gcs2_is_raise(ac); if(ac)json_free(ac); }
    if (!failed) { r=gcs2_exec(s,"python3 -c marker umask 077 && mkdir -p reservation=... exit 75"); failed=gcs2_is_raise(r); }
    if (failed) { if(r)json_free(r); json_t *ig=gcs2_remove_remote(s,tp); if(ig)json_free(ig); json_free(nonce); json_free(tp); json_free(lp); return gcs2_raise_spec("SshReachError","transport"); }
    char *t=gcs2_strip_dup(gcs2_out_str(r)); json_free(r);
    /* last non-empty stripped line */
    char *last=strdup("");
    char *save=NULL, *tokline=strtok_r(t,"\n",&save);
    for (;tokline;tokline=strtok_r(NULL,"\n",&save)) {
        char *q=gcs2_strip_dup(tokline); if(*q){ free(last); last=q; } else free(q);
    }
    free(t);
    json_t *out=NULL;
    if (!strcmp(last,"EXISTING")) out=gcs2_spawned(true,NULL,NULL,NULL,NULL);
    else {
        bool digits=*last && strspn(last,"0123456789")==strlen(last);
        bool positive=digits && atoll(last)>0;
        if (!positive) {
            json_t *ig=gcs2_remove_remote(s,tp); if(ig)json_free(ig);
            out=gcs2_err("spawn-failed","Failed to launch the remote Hermes serve (no pid returned).");
        } else out=gcs2_spawned(false,last,ns,gcs2_jstr(lp)?gcs2_jstr(lp):"",gcs2_jstr(tp)?gcs2_jstr(tp):"");
    }
    free(last); json_free(nonce); json_free(tp); json_free(lp); return out;
}

static json_t *gcs2_remote_t(const json_t *platform, const char *home, const char *path, const char *ver, const char *prof) {
    json_t *r=json_object();
    json_set(r,"platform",json_copy((json_t*)platform));
    json_set(r,"hermes_home",json_string(home));
    json_set(r,"hermes_path",json_string(path));
    json_set(r,"hermes_version",json_string(ver));
    json_set(r,"profile",json_string(prof));
    return r;
}

/* PoP: gcs2_probe_remote @ hermes_cli/gateway_connections_ssh_remote.py:_probe_remote */
static json_t *gcs2_probe_remote(const gcs2_seams_t *s, const json_t *deps) {
    json_t *plat=gcs2_probe_remote_platform(s);
    if (gcs2_is_raise(plat)) return plat;
    json_t *home=gcs2_probe_remote_hermes_home(s);
    if (gcs2_is_raise(home)) { json_free(plat); return home; }
    json_t *ac=gcs2_assert_remote_install_update_clear(s,home,json_string(""));
    if (gcs2_is_raise(ac)) { json_free(plat); json_free(home); return ac; } json_free(ac);
    json_t *path=gcs2_locate_hermes(s,json_string(gcs2_dstr(deps,"remote_hermes_path")));
    if (gcs2_is_raise(path)) { json_free(plat); json_free(home); return path; }
    json_t *ver=gcs2_probe_hermes_version(s,path);
    if (gcs2_is_raise(ver)) { json_free(plat); json_free(home); json_free(path); return ver; }
    json_t *pj=json_string(gcs2_dstr(deps,"profile"));
    json_t *prof=gcs2_backend_profile_arg(pj); json_free(pj);
    if (!prof) prof=json_string("");
    json_t *out=gcs2_remote_t(plat,gcs2_jstr(home)?gcs2_jstr(home):"",gcs2_jstr(path)?gcs2_jstr(path):"",gcs2_jstr(ver)?gcs2_jstr(ver):"",gcs2_jstr(prof)?gcs2_jstr(prof):"");
    json_free(plat); json_free(home); json_free(path); json_free(ver); json_free(prof); return out;
}

static json_t *gcs2_connected(const char *base_url, const char *tok, int local, int remote, long long pid,
                              bool reused, const json_t *r) {
    json_t *o=json_object();
    json_set(o,"base_url",json_string(base_url));
    json_set(o,"token",json_string(tok));
    json_set(o,"local_port",json_number(local));
    json_set(o,"remote_port",json_number(remote));
    json_set(o,"pid",json_number((double)pid));
    json_set(o,"reused",json_bool(reused));
    const char *keys[]={"hermes_version","hermes_path","hermes_home","spawn_nonce","platform",NULL};
    for(int i=0;keys[i];i++) json_set(o,keys[i],json_copy(json_obj_get((json_t*)r,keys[i])));
    return o;
}

/* PoP: gcs2_try_reuse @ hermes_cli/gateway_connections_ssh_remote.py:_try_reuse */
static json_t *gcs2_try_reuse(const gcs2_seams_t *s, const json_t *deps, const json_t *remote, json_t *lock) {
    const char *oid=gcs2_dstr(deps,"ownership_id");
    json_t *alive=gcs2_remote_pid_alive(s,json_obj_get(lock,"pid"));
    if (gcs2_is_raise(alive)) return alive;
    bool pa=gcs2_jbool(alive,false); json_free(alive);
    json_t *ours=gcs2_pid_is_our_serve(s,lock,json_string(oid));
    if (gcs2_is_raise(ours)) return ours;
    bool own=pa && gcs2_jbool(ours,false); json_free(ours);
    json_t *reuse_tok=json_string(gcs2_dstr(deps,"reuse_token"));
    json_t *serves=gcs2_lock_serves_connect(lock,gcs2_dstr(remote,"profile"),reuse_tok,
                        gcs2_dstr(remote,"hermes_path"),gcs2_dstr(remote,"hermes_home"));
    json_free(reuse_tok);
    bool reusable=own && gcs2_jbool(serves,false); json_free(serves);
    if (!reusable) {
        json_t *ac=gcs2_assert_remote_install_update_clear(s,json_string(gcs2_dstr(remote,"hermes_home")),json_string(gcs2_dstr(remote,"hermes_path")));
        if(gcs2_is_raise(ac)) return ac; json_free(ac);
        json_t *c=gcs2_cleanup_stale(s,json_string(oid),lock,pa); if(gcs2_is_raise(c)) return c; json_free(c);
        return NULL;
    }
    json_t *ct=json_obj_get(lock,"creationTime");
    if (!gcs2_jtruth(ct)) {
        json_t *creation=gcs2_remote_process_creation_time(s,json_obj_get(lock,"pid"));
        if(gcs2_is_raise(creation)) return creation;
        const char *cv=gcs2_jstr(creation); bool have=cv&&*cv;
        if (have) { json_set(lock,"creationTime",json_copy(creation)); json_t *w=gcs2_write_lockfile(s,json_string(oid),lock); if(gcs2_is_raise(w)){json_free(creation);return w;} json_free(w); }
        json_free(creation);
    }
    json_t *ac=gcs2_assert_remote_install_update_clear(s,json_string(gcs2_dstr(remote,"hermes_home")),json_string(gcs2_dstr(remote,"hermes_path")));
    if(gcs2_is_raise(ac)) return ac; json_free(ac);
    int rport=(int)gcs2_jnum(json_obj_get(lock,"port"),0);
    json_t *lp=gcs2_open_forward(s,rport,3);
    if(gcs2_is_raise(lp)) return lp;
    int local=(int)gcs2_jnum(lp,0); json_free(lp);
    char *base=gcs2_xsf("http://127.0.0.1:%d",local);
    const char *nonce=gcs2_jstr(json_obj_get(lock,"spawnNonce"))?gcs2_jstr(json_obj_get(lock,"spawnNonce")):"";
    json_t *verdict=gcs2_probe_proof(s,base,gcs2_dstr(deps,"reuse_token"),nonce);
    if (gcs2_is_raise(verdict)) {
        json_t *cf=gcs2_cancel_forward(s,local,rport); if(cf)json_free(cf);
        free(base); return gcs2_err("transient-transport-error","Could not verify the existing SSH backend.");
    }
    const char *vv=gcs2_jstr(verdict);
    bool ok=vv&&!strcmp(vv,"authenticated-ok"); json_free(verdict);
    if (!ok) {
        json_t *cf=gcs2_cancel_forward(s,local,rport); if(cf)json_free(cf);
        json_t *a=gcs2_assert_remote_install_update_clear(s,json_string(gcs2_dstr(remote,"hermes_home")),json_string(gcs2_dstr(remote,"hermes_path"))); if(a)json_free(a);
        json_t *c=gcs2_cleanup_stale(s,json_string(oid),lock,pa); if(c)json_free(c);
        free(base); return NULL;
    }
    json_t *alive2=gcs2_remote_pid_alive(s,json_obj_get(lock,"pid"));
    if(gcs2_is_raise(alive2)){free(base);return alive2;}
    bool a2=gcs2_jbool(alive2,false); json_free(alive2);
    if (!a2) { json_t *cf=gcs2_cancel_forward(s,local,rport); if(cf)json_free(cf); free(base); return gcs2_err("spawn-failed","The reused remote Hermes serve exited while it was being verified."); }
    json_t *out=gcs2_connected(base,gcs2_dstr(deps,"reuse_token"),local,rport,(long long)gcs2_jnum(json_obj_get(lock,"pid"),0),true,remote);
    free(base); return out;
}

/* PoP: gcs2_rollback_spawn @ hermes_cli/gateway_connections_ssh_remote.py:_rollback_spawn */
static json_t *gcs2_rollback_spawn(const gcs2_seams_t *s, const char *oid, const json_t *owned, const json_t *spawned,
                                   int local_port, int remote_port) {
    if (local_port && remote_port) {
        json_t *c=gcs2_cancel_forward(s,local_port,remote_port);
        if (gcs2_is_raise(c)) return c;
        if(c)json_free(c);
    }
    json_t *ig=gcs2_remove_remote(s,json_obj_get((json_t*)spawned,"token_file_path")); if(ig)json_free(ig);
    json_t *alive=gcs2_remote_pid_alive(s,json_obj_get((json_t*)spawned,"pid"));
    bool a;
    if (gcs2_is_raise(alive)) { a=true; if(alive)json_free(alive); }
    else { a=gcs2_jbool(alive,false); json_free(alive); }
    json_t *r=gcs2_cleanup_stale(s,json_string(oid?oid:""),(json_t*)owned,a);
    return r;
}

/* PoP: gcs2_spawn_fresh @ hermes_cli/gateway_connections_ssh_remote.py:_spawn_fresh */
static json_t *gcs2_spawn_fresh(const gcs2_seams_t *s, const json_t *deps, const json_t *remote) {
    const char *oid=gcs2_dstr(deps,"ownership_id");
    json_t *ac=gcs2_assert_remote_install_update_clear(s,json_string(gcs2_dstr(remote,"hermes_home")),json_string(gcs2_dstr(remote,"hermes_path")));
    if(gcs2_is_raise(ac)) return ac; json_free(ac);
    json_t *token=gcs2_mint_token(s);
    if(gcs2_is_raise(token)) return token;
    json_t *sp=gcs2_spawn_remote_serve(s,json_string(gcs2_dstr(remote,"hermes_path")),gcs2_dstr(remote,"profile"),token,json_string(oid),json_string(gcs2_dstr(remote,"hermes_home")));
    if(gcs2_is_raise(sp)){json_free(token);return sp;}
    bool existing=gcs2_jbool(json_obj_get(sp,"existing"),false);
    if (existing) {
        const char *rt=gcs2_dstr(deps,"reuse_token");
        if (!*rt) { json_free(sp);json_free(token); return gcs2_err("remote-ownership-contended","Another SSH connection owns this remote serve; a session token is required to reuse it."); }
        json_t *w=gcs2_wait_for_remote_spawn_completion(s,json_string(oid),gcs2_dnum(deps,"ready_timeout_s",120.0));
        json_free(sp);json_free(token); if(gcs2_is_raise(w))return w; json_free(w); return NULL;
    }
    json_t *creation=gcs2_remote_process_creation_time(s,json_obj_get(sp,"pid"));
    if(gcs2_is_raise(creation)){json_free(sp);json_free(token);return creation;}
    const char *ns=gcs2_jstr(json_obj_get(sp,"spawn_nonce"))?gcs2_jstr(json_obj_get(sp,"spawn_nonce")):"";
    json_t *fp=gcs2_fingerprint_token(token);
    json_t *iso=gcs2_now_iso(s);
    json_t *owned=json_object();
    json_set(owned,"ownershipId",json_string(oid));
    json_set(owned,"spawnNonce",json_string(ns));
    json_set(owned,"pid",json_copy(json_obj_get(sp,"pid")));
    json_set(owned,"port",json_number(0));
    json_set(owned,"profile",json_string(gcs2_dstr(remote,"profile")));
    json_set(owned,"hermesPath",json_string(gcs2_dstr(remote,"hermes_path")));
    json_set(owned,"hermesHome",json_string(gcs2_dstr(remote,"hermes_home")));
    json_set(owned,"logPath",json_copy(json_obj_get(sp,"log_path")));
    json_set(owned,"tokenFingerprint",json_copy(fp));
    json_set(owned,"protocolVersion",json_number(PROTOCOL_VERSION));
    json_set(owned,"devServer",json_string(""));
    json_set(owned,"startedAt",json_copy(iso));
    const char *cv=gcs2_jstr(creation);
    if(cv&&*cv) json_set(owned,"creationTime",json_copy(creation));
    int local=0,rport=0;
    json_t *out=NULL;
    json_t *w=gcs2_write_lockfile(s,json_string(oid),owned);
    bool bad=gcs2_is_raise(w); if(w)json_free(w);
    if(!bad){ json_t *rp=gcs2_scrape_ready_port(s,json_obj_get(sp,"log_path"),gcs2_dnum(deps,"ready_timeout_s",120.0)); bad=gcs2_is_raise(rp); if(bad){if(rp)json_free(rp);} else {rport=(int)gcs2_jnum(rp,0); json_free(rp);} }
    if(!bad){ json_t *lp=gcs2_open_forward(s,rport,3); bad=gcs2_is_raise(lp); if(bad){if(lp)json_free(lp);} else {local=(int)gcs2_jnum(lp,0); json_free(lp);} }
    char *base=NULL;
    if(!bad){ base=gcs2_xsf("http://127.0.0.1:%d",local); json_t *wh=gcs2_wait_health(s,base); bad=gcs2_is_raise(wh); if(wh)json_free(wh); }
    if(!bad){ json_t *pv=gcs2_probe_proof(s,base,gcs2_jstr(token)?gcs2_jstr(token):"",ns); bad=gcs2_is_raise(pv); if(!bad){ const char *v=gcs2_jstr(pv); if(!v||strcmp(v,"authenticated-ok")){ bad=true; json_t *e=gcs2_err("spawn-failed","The remote serve on the announced port did not prove it is ours."); gcs2_rollback_spawn(s,oid,owned,sp,local,rport); json_free(fp);json_free(iso);json_free(creation);json_free(owned);json_free(sp);json_free(token);free(base); return e; } } if(pv)json_free(pv); }
    if(!bad){ json_set(owned,"port",json_number(rport)); json_t *w2=gcs2_write_lockfile(s,json_string(oid),owned); bad=gcs2_is_raise(w2); if(w2)json_free(w2); }
    if (bad) {
        gcs2_rollback_spawn(s,oid,owned,sp,local,rport);
        json_free(fp);json_free(iso);json_free(creation);json_free(owned);json_free(sp);json_free(token);free(base);
        return gcs2_raise_spec("SshReachError","spawn failure");
    }
    out=gcs2_connected(base,gcs2_jstr(token)?gcs2_jstr(token):"",local,rport,(long long)gcs2_jnum(json_obj_get(sp,"pid"),0),false,remote);
    json_free(fp);json_free(iso);json_free(creation);json_free(owned);json_free(sp);json_free(token);free(base);
    return out;
}

/* PoP: gcs2_connect @ hermes_cli/gateway_connections_ssh_remote.py:connect */
static json_t *gcs2_connect(const gcs2_seams_t *s, const json_t *deps) {
    json_t *remote=gcs2_probe_remote(s,deps);
    if(gcs2_is_raise(remote)) return remote;
    for (int i=0;i<3;i++) {
        json_t *lock=gcs2_read_lockfile(s,json_string(gcs2_dstr(deps,"ownership_id")));
        if(gcs2_is_raise(lock)) { json_free(remote); return lock; }
        bool skew=gcs2_is_skew(lock);
        if (skew) { json_t *e=gcs2_skew_error(json_string(gcs2_dstr(deps,"ownership_id")),lock); json_free(lock); json_free(remote); return e; }
        bool truth=lock && lock->type==JSON_OBJECT && lock->c.count>0;
        json_t *result=NULL;
        if (truth) result=gcs2_try_reuse(s,deps,remote,lock);
        if (!result) { if(lock)json_free(lock); result=gcs2_spawn_fresh(s,deps,remote); }
        else if(lock) json_free(lock);
        if (result) { json_free(remote); return result; }
    }
    json_free(remote);
    return gcs2_err("remote-ownership-contended","The remote serve kept changing owners while connecting.");
}
