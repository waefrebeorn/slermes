/* tests/watercup/watercup.h — C trace-cup tracer.
 *
 * Records an order-sensitive execution signature ("the shape the water
 * carves") for a function, so a faithful port and LIVE Python emit the SAME
 * cup hash over the same inputs. A stub that `return json_object()` produces
 * an empty/degenerate cup and never matches.
 *
 * Zero-cost in production: define WCUP_DISABLED to compile every macro to
 * nothing. The matcher (match.py) is the only consumer.
 *
 * Cup event alphabet (must match watercup_py.py EXACTLY):
 *   'B' + depth + ':' + ('1'|'0')   branch taken (true/false)
 *   'L' + depth                   loop iteration tick
 *   'R' + depth                   recursion entry
 *   'r' + depth                   recursion exit
 *   'V' + hashhex                 significant intermediate value
 * The whole stream is folded with FNV-1a into a 64-bit cup hash.
 */
#ifndef WATERCUP_H
#define WATERCUP_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifdef WCUP_DISABLED
#  define WCUP_BEGIN(fn)
#  define WCUP_END(fn)
#  define WCUP_BRANCH(cond)          ((void)(cond))
#  define WCUP_LOOP_TICK(depth)      ((void)(depth))
#  define WCUP_RECURSE_IN(depth)     ((void)(depth))
#  define WCUP_RECURSE_OUT(depth)    ((void)(depth))
#  define WCUP_VAL(ptr,len)          ((void)(ptr),(void)(len))
#  define WCUP_VALINT(v)             ((void)(v))
#  define WCUP_FINALIZE(out,outsz)   do { (out)[0]='\0'; } while(0)
#else

typedef struct {
    uint64_t h;
    int depth;
    char buf[256];
} wcup_t;

/* FNV-1a 64 */
static inline void wcup_mix(wcup_t *c, const void *data, int len) {
    const unsigned char *p = (const unsigned char *)data;
    for (int i = 0; i < len; i++) {
        c->h ^= p[i];
        c->h *= 0x100000001b3ULL;
    }
}

static inline void wcup_begin(wcup_t *c, const char *fn) {
    c->h = 0xcbf29ce484222325ULL;
    c->depth = 0;
    wcup_mix(c, fn, (int)strlen(fn));
    wcup_mix(c, "|", 1);
}
static inline void wcup_end(wcup_t *c) { (void)c; }
static inline void wcup_branch(wcup_t *c, int taken) {
    char e[16]; int n = snprintf(e, sizeof e, "B%d:%c", c->depth, taken ? '1' : '0');
    wcup_mix(c, e, n);
}
static inline void wcup_loop(wcup_t *c) {
    char e[16]; int n = snprintf(e, sizeof e, "L%d", c->depth);
    wcup_mix(c, e, n);
}
static inline void wcup_rec_in(wcup_t *c) {
    char e[16]; int n = snprintf(e, sizeof e, "R%d", ++c->depth);
    wcup_mix(c, e, n);
}
static inline void wcup_rec_out(wcup_t *c) {
    char e[16]; int n = snprintf(e, sizeof e, "r%d", c->depth);
    wcup_mix(c, e, n);
    if (c->depth > 0) c->depth--;
}
static inline void wcup_val(wcup_t *c, const void *ptr, int len) {
    /* hash the value; prefix with 'V' so value events differ from control */
    char tag = 'V';
    wcup_mix(c, &tag, 1);
    wcup_mix(c, ptr, len);
}
static inline void wcup_valstr(wcup_t *c, const char *s) {
    /* hash a canonical STRING value (same encoding the Python side uses).
     * Use this (not wcup_val) for cross-language value parity. */
    char tag = 'V';
    wcup_mix(c, &tag, 1);
    wcup_mix(c, s, (int)strlen(s));
}
static inline void wcup_finalize(wcup_t *c, char *out, int outsz) {
    snprintf(out, outsz, "%016llx", (unsigned long long)c->h);
}

/* User-facing macros (carry an implicit wcup_t named _wc) */
#define WCUP_BEGIN(fn)   wcup_t _wc; wcup_begin(&_wc, fn)
#define WCUP_END(fn)     wcup_end(&_wc)
#define WCUP_BRANCH(cond)    do { wcup_branch(&_wc, !!(cond)); } while(0)
#define WCUP_LOOP_TICK()     do { wcup_loop(&_wc); } while(0)
#define WCUP_RECURSE_IN()    do { wcup_rec_in(&_wc); } while(0)
#define WCUP_RECURSE_OUT()   do { wcup_rec_out(&_wc); } while(0)
#define WCUP_VAL(ptr,len)    do { wcup_val(&_wc, (ptr), (len)); } while(0)
#define WCUP_VALSTR(s)       do { wcup_valstr(&_wc, (s)); } while(0)
/* Stale-generator compat: 4,011 harnesses call WCUP_VALINT, which the
 * original generator emitted but never defined. The python twin hashes
 * str(result) via valstr - so the int form mixes b"V" + the decimal
 * string, byte-identical to the oracle. */
#define WCUP_VALINT(v)       do { char _wcvi[32]; snprintf(_wcvi, sizeof _wcvi, "%lld", (long long)(v)); wcup_valstr(&_wc, _wcvi); } while(0)
#define WCUP_FINALIZE(out,sz) wcup_finalize(&_wc, (out), (sz))

#endif /* WCUP_DISABLED */
#endif /* WATERCUP_H */
