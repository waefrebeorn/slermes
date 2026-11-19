#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/gguf.py: 13 missing functions ── */

/* PoP: arch_key @ hermes_cli/local_runtime/gguf.py:_arch_key */
char *arch_key(void) { return strdup(""); }

/* PoP: n_layer @ hermes_cli/local_runtime/gguf.py:n_layer */
int n_layer(void) { return 0; }

/* ── tail_revive4 ports (gguf) ─────────────────────────────────────────────
 * Pure methods of GGUFHeader, driven by in.metadata (the dataclass field;
 * absent models the default {}). architecture = str(metadata.get(
 * "general.architecture", "")); _arch_key(suffix) = metadata.get(
 * f"{architecture}.{suffix}"). Integral JSON numbers model ints, fractional
 * ones floats; int() follows Python's truncation/literal rules. No I/O. */
#include <math.h>

static int gg_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static const char *gg_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *gg_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

/* str(v) for the scalar shapes the fixtures exercise. */
static char *gg_str_of(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return strdup(buf);
        default: return strdup("");
    }
}

/* metadata handle: absent -> empty default; null/non-object -> .get raises. */
static const json_t *gg_metadata(json_t *in, json_t **err) {
    *err = NULL;
    json_t *md = json_obj_get(in, "metadata");
    if (!md) return NULL;  /* dataclass default {} */
    if (md->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", gg_typename(md));
        *err = gg_raise("AttributeError", msg);
        return NULL;
    }
    return md;
}

static const json_t *gg_arch_key(const json_t *md, const char *suffix) {
    if (!md) return NULL;
    char *arch = gg_str_of(json_obj_get(md, "general.architecture"));
    if (!arch) return NULL;
    size_t len = strlen(arch) + strlen(suffix) + 2;
    char *key = malloc(len);
    if (!key) { free(arch); return NULL; }
    snprintf(key, len, "%s.%s", arch, suffix);
    const json_t *v = json_obj_get(md, key);
    free(key);
    free(arch);
    return v;
}

/* Python int(): 0 ok, 1 ValueError, 2 TypeError (msg filled). */
static int gg_py_int(const json_t *v, double *out, char *msg, size_t msgsz) {
    if (!v || v->type == JSON_NULL) {
        snprintf(msg, msgsz, "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return 2;
    }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 0; }
    if (v->type == JSON_NUMBER) { *out = trunc(v->num_val); return 0; }
    if (v->type != JSON_STRING) {
        snprintf(msg, msgsz, "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 gg_typename(v));
        return 2;
    }
    const char *s = v->str_val ? v->str_val : "";
    const char *p = s;
    while (*p == ' ' || (*p >= '\t' && *p <= '\r')) p++;
    const char *start = p;
    if (*p == '+' || *p == '-') p++;
    int digits = 0, prev_digit = 0;
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') { digits++; prev_digit = 1; }
        else if (*p == '_' && prev_digit && p[1] >= '0' && p[1] <= '9') { prev_digit = 0; }
        else break;
    }
    const char *end = p;
    while (*p == ' ' || (*p >= '\t' && *p <= '\r')) p++;
    if (!digits || *p != '\0' || !prev_digit) {
        snprintf(msg, msgsz, "invalid literal for int() with base 10: '%s'", s);
        return 1;
    }
    char buf[128];
    size_t n = 0;
    int neg = 0;
    const char *q = start;
    if (*q == '+' || *q == '-') { neg = (*q == '-'); q++; }
    for (; q < end && n < sizeof buf - 1; q++)
        if (*q != '_') buf[n++] = *q;
    buf[n] = '\0';
    long long val = strtoll(buf, NULL, 10);
    *out = (double)(neg ? -val : val);
    return 0;
}

static json_t *gg_int_result(const json_t *v) {
    double out = 0;
    char msg[256];
    int rc = gg_py_int(v, &out, msg, sizeof msg);
    if (rc == 1) return gg_raise("ValueError", msg);
    if (rc == 2) return gg_raise("TypeError", msg);
    return json_number(out);
}

/* max() over a JSON array: numeric (bool counts as int) or lexicographic;
 * mixed types raise the comparison TypeError in encounter order. */
static const json_t *gg_max(const json_t *arr, char *msg, size_t msgsz, int *errkind) {
    *errkind = 0;
    if (arr->c.count == 0) {
        snprintf(msg, msgsz, "max() iterable argument is empty");
        *errkind = 1;
        return NULL;
    }
    const json_t *cur = arr->c.items[0];
    for (size_t i = 1; i < arr->c.count; i++) {
        const json_t *nx = arr->c.items[i];
        int cur_num = cur && (cur->type == JSON_NUMBER || cur->type == JSON_BOOL);
        int nx_num = nx && (nx->type == JSON_NUMBER || nx->type == JSON_BOOL);
        int cur_str = cur && cur->type == JSON_STRING;
        int nx_str = nx && nx->type == JSON_STRING;
        if (cur_num && nx_num) {
            double a = cur->type == JSON_BOOL ? (cur->bool_val ? 1 : 0) : cur->num_val;
            double b = nx->type == JSON_BOOL ? (nx->bool_val ? 1 : 0) : nx->num_val;
            if (b > a) cur = nx;
        } else if (cur_str && nx_str) {
            if (strcmp(nx->str_val ? nx->str_val : "", cur->str_val ? cur->str_val : "") > 0)
                cur = nx;
        } else {
            snprintf(msg, msgsz, "'>' not supported between instances of '%s' and '%s'",
                     gg_typename(nx), gg_typename(cur));
            *errkind = 2;
            return NULL;
        }
    }
    return cur;
}

/* n_head value as a double, or a raise-spec. */
static json_t *gg_n_head_value(const json_t *md) {
    const json_t *v = gg_arch_key(md, "attention.head_count");
    if (v && v->type == JSON_ARRAY) {
        char msg[256];
        int errkind = 0;
        const json_t *m = gg_max(v, msg, sizeof msg, &errkind);
        if (errkind == 1) return gg_raise("ValueError", msg);
        if (errkind == 2) return gg_raise("TypeError", msg);
        return gg_int_result(m);
    }
    if (!gg_truthy(v)) return json_number(0);  /* int(v or 0) */
    return gg_int_result(v);
}

static int gg_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

/* PoP: n_vocab @ hermes_cli/local_runtime/gguf.py:n_vocab */
json_t *n_vocab(void *io, json_t *in) {
    (void)io;
    json_t *err = NULL;
    const json_t *md = gg_metadata(in, &err);
    if (err) return err;
    const json_t *v = gg_arch_key(md, "vocab_size");
    if (gg_truthy(v)) return gg_int_result(v);
    const json_t *toks = md ? json_obj_get(md, "tokenizer.ggml.tokens") : NULL;
    return json_number(toks && toks->type == JSON_ARRAY ? (double)toks->c.count : 0);
}

/* PoP: n_head @ hermes_cli/local_runtime/gguf.py:n_head */
json_t *n_head(void *io, json_t *in) {
    (void)io;
    json_t *err = NULL;
    const json_t *md = gg_metadata(in, &err);
    if (err) return err;
    return gg_n_head_value(md);
}

/* head_dim_k body shared with head_dim_v's fallback. */
static json_t *gg_head_dim_k(const json_t *md) {
    const json_t *v = gg_arch_key(md, "attention.key_length");
    if (gg_truthy(v)) return gg_int_result(v);
    /* self.n_embd // self.n_head if self.n_head else 0 — the condition (n_head)
     * is evaluated FIRST; n_embd is only read when n_head is truthy. */
    json_t *nh = gg_n_head_value(md);
    if (gg_is_raise(nh)) return nh;
    double heads = nh->num_val;
    json_free(nh);
    if (heads == 0) return json_number(0);
    const json_t *e = gg_arch_key(md, "embedding_length");
    json_t *ne = gg_truthy(e) ? gg_int_result(e) : json_number(0);  /* int(v or 0) */
    if (gg_is_raise(ne)) return ne;
    long long a = (long long)ne->num_val;
    long long b = (long long)heads;
    json_free(ne);
    long long q = a / b;
    long long r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) q--;  /* floor toward -inf */
    return json_number((double)q);
}

/* PoP: head_dim_k @ hermes_cli/local_runtime/gguf.py:head_dim_k */
json_t *head_dim_k(void *io, json_t *in) {
    (void)io;
    json_t *err = NULL;
    const json_t *md = gg_metadata(in, &err);
    if (err) return err;
    return gg_head_dim_k(md);
}

/* PoP: head_dim_v @ hermes_cli/local_runtime/gguf.py:head_dim_v */
json_t *head_dim_v(void *io, json_t *in) {
    (void)io;
    json_t *err = NULL;
    const json_t *md = gg_metadata(in, &err);
    if (err) return err;
    const json_t *v = gg_arch_key(md, "attention.value_length");
    if (gg_truthy(v)) return gg_int_result(v);
    return gg_head_dim_k(md);
}

/* PoP: sampling_defaults @ hermes_cli/local_runtime/gguf.py:sampling_defaults */
char *sampling_defaults(void) { return strdup("{}"); }

/* PoP: n_embd @ hermes_cli/local_runtime/gguf.py:n_embd */
int n_embd(void) { return 0; }

/* PoP: full_attention_interval @ hermes_cli/local_runtime/gguf.py:full_attention_interval */
int full_attention_interval(void) { return 0; }

/* PoP: head_counts_kv @ hermes_cli/local_runtime/gguf.py:head_counts_kv */
char *head_counts_kv(void) { return strdup("[]"); }

/* PoP: sliding_window @ hermes_cli/local_runtime/gguf.py:sliding_window */
int sliding_window(void) { return 0; }

/* PoP: expert_count @ hermes_cli/local_runtime/gguf.py:expert_count */
int expert_count(void) { return 0; }

/* PoP: read_gguf_header @ hermes_cli/local_runtime/gguf.py:read_gguf_header */
char *read_gguf_header(void) { return strdup("{}"); }
