/*
 * port_hermes_state_keys.c — Port of hermes_state_keys.py (+ the two
 * hermes_state_runtime.py key builders).
 *
 * Pure identity derivations the canonical store and the gateway both need.
 * The fingerprints are SHA-256 over a CANONICAL JSON encoding: sorted keys,
 * ','/':' separators, no spaces, non-ASCII left as UTF-8 — matching
 * ``json.dumps(..., ensure_ascii=False, sort_keys=True,
 * separators=(',', ':'), allow_nan=False)`` byte for byte, because the digest
 * is a durable on-disk identity.
 */

#include "port_hermes_state_keys.h"
#include "hash.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CRON_PRINCIPAL_ID "cron-owner"
#define CRON_REQUEST_PREFIX "cron:"
#define REPLACEMENT_PREFIX "gateway.queued_replacement.v1."
#define MUTATION_PREFIX "gateway.mutation.v1."

/* --- tiny string builder --- */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} sb_t;

static void sb_init(sb_t *sb)
{
    sb->cap = 64;
    sb->len = 0;
    sb->buf = (char *)malloc(sb->cap);
    if (sb->buf)
        sb->buf[0] = '\0';
}

static void sb_putc(sb_t *sb, char c)
{
    if (!sb->buf)
        return;
    if (sb->len + 2 > sb->cap) {
        size_t ncap = sb->cap * 2;
        char *nb = (char *)realloc(sb->buf, ncap);
        if (!nb)
            return;
        sb->buf = nb;
        sb->cap = ncap;
    }
    sb->buf[sb->len++] = c;
    sb->buf[sb->len] = '\0';
}

static void sb_puts(sb_t *sb, const char *s)
{
    if (!s)
        return;
    for (; *s; s++)
        sb_putc(sb, *s);
}

static void sb_free(sb_t *sb)
{
    free(sb->buf);
    sb->buf = NULL;
    sb->len = sb->cap = 0;
}

/* --- canonical JSON (sorted keys, compact separators, UTF-8) --- */

static void canon_string(sb_t *sb, const char *s)
{
    sb_putc(sb, '"');
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        switch (ch) {
        case '"': sb_puts(sb, "\\\""); break;
        case '\\': sb_puts(sb, "\\\\"); break;
        case '\n': sb_puts(sb, "\\n"); break;
        case '\r': sb_puts(sb, "\\r"); break;
        case '\t': sb_puts(sb, "\\t"); break;
        case '\b': sb_puts(sb, "\\b"); break;
        case '\f': sb_puts(sb, "\\f"); break;
        default:
            if (ch < 0x20) {
                char esc[8];
                snprintf(esc, sizeof(esc), "\\u%04x", ch);
                sb_puts(sb, esc);
            } else {
                sb_putc(sb, (char)ch);
            }
        }
    }
    sb_putc(sb, '"');
}

/* Python float repr: shortest string that round-trips. */
static void canon_number(sb_t *sb, double d)
{
    if (d != d || d == 1.0 / 0.0 || d == -1.0 / 0.0) {
        /* allow_nan=False: NaN/Inf are a hard error upstream; emit nothing. */
        sb_puts(sb, "null");
        return;
    }
    char buf[40];
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, d);
        if (strtod(buf, NULL) == d)
            break;
    }
    /* Python prints integral floats without a trailing .0 in JSON */
    sb_puts(sb, buf);
}

static void canon_value(sb_t *sb, const json_t *v);

static int cmp_keys(const void *a, const void *b)
{
    const char *const *ka = (const char *const *)a;
    const char *const *kb = (const char *const *)b;
    return strcmp(*ka, *kb);
}

static void canon_object(sb_t *sb, const json_t *obj)
{
    size_t n = json_object_size(obj);
    const char **keys = (const char **)malloc((n ? n : 1) * sizeof(char *));
    if (!keys)
        return;
    for (size_t i = 0; i < n; i++)
        keys[i] = json_object_get_key_at(obj, i);
    qsort(keys, n, sizeof(char *), cmp_keys);

    sb_putc(sb, '{');
    for (size_t i = 0; i < n; i++) {
        if (i)
            sb_putc(sb, ',');
        canon_string(sb, keys[i]);
        sb_putc(sb, ':');
        canon_value(sb, json_obj_get(obj, keys[i]));
    }
    sb_putc(sb, '}');
    free(keys);
}

static void canon_value(sb_t *sb, const json_t *v)
{
    if (!v || json_is_null(v)) {
        sb_puts(sb, "null");
    } else if (json_is_bool(v)) {
        sb_puts(sb, json_bool_value(v) ? "true" : "false");
    } else if (json_is_number(v)) {
        canon_number(sb, json_number_value(v));
    } else if (json_is_string(v)) {
        canon_string(sb, json_string_value(v));
    } else if (json_is_array(v)) {
        size_t n = json_len(v);
        sb_putc(sb, '[');
        for (size_t i = 0; i < n; i++) {
            if (i)
                sb_putc(sb, ',');
            canon_value(sb, json_get(v, i));
        }
        sb_putc(sb, ']');
    } else if (json_is_object(v)) {
        canon_object(sb, v);
    }
}

/* json.dumps(...) -> malloc'd canonical text; NULL when the tree is empty. */
static char *canon_dumps(const json_t *v)
{
    sb_t sb;
    sb_init(&sb);
    canon_value(&sb, v);
    if (!sb.buf) {
        sb_free(&sb);
        return NULL;
    }
    return sb.buf;
}

/* --- hermes_state_keys.py --- */

/* PoP: cron_request_id @ hermes_state_keys.py:cron_request_id */
char *hsk_cron_request_id(const char *job_id, const char *execution_id)
{
    size_t n = strlen(CRON_REQUEST_PREFIX) + strlen(job_id ? job_id : "") + 1 +
               strlen(execution_id ? execution_id : "") + 1;
    char *out = (char *)malloc(n);
    if (!out)
        return NULL;
    snprintf(out, n, "%s%s:%s", CRON_REQUEST_PREFIX, job_id ? job_id : "",
             execution_id ? execution_id : "");
    return out;
}

/* PoP: prefix_range @ hermes_state_keys.py:prefix_range */
char *hsk_prefix_range(const char *prefix, char **hi)
{
    if (!prefix || !*prefix)
        return NULL;
    char *lo = strdup(prefix);
    size_t n = strlen(prefix);
    char *up = (char *)malloc(n + 1);
    if (!lo || !up) {
        free(lo);
        free(up);
        return NULL;
    }
    memcpy(up, prefix, n + 1);
    up[n - 1] = (char)(up[n - 1] + 1);
    if (hi)
        *hi = up;
    else
        free(up);
    return lo;
}

/* PoP: cron_request_range @ hermes_state_keys.py:cron_request_range */
char *hsk_cron_request_range(const char *job_id, char **hi)
{
    char *prefix = hsk_cron_request_id(job_id, "");
    if (!prefix)
        return NULL;
    char *lo = hsk_prefix_range(prefix, hi);
    free(prefix);
    return lo;
}

/* PoP: cron_run_from_request @ hermes_state_keys.py:cron_run_from_request */
int hsk_cron_run_from_request(const char *request_id, const char *job_id,
                              char **job_out, char **exec_out)
{
    if (!request_id || strncmp(request_id, CRON_REQUEST_PREFIX,
                               strlen(CRON_REQUEST_PREFIX)) != 0)
        return 0;

    char *job = NULL, *exec = NULL;
    if (!job_id) {
        const char *rest = request_id + strlen(CRON_REQUEST_PREFIX);
        const char *colon = strchr(rest, ':');
        if (!colon || strchr(colon + 1, ':'))
            return 0;                      /* not exactly one ':' */
        job = (char *)malloc((size_t)(colon - rest) + 1);
        if (!job)
            return 0;
        memcpy(job, rest, (size_t)(colon - rest));
        job[colon - rest] = '\0';
        exec = strdup(colon + 1);
    } else {
        char *prefix = hsk_cron_request_id(job_id, "");
        if (!prefix)
            return 0;
        size_t plen = strlen(prefix);
        if (strncmp(request_id, prefix, plen) == 0)
            exec = strdup(request_id + plen);
        else
            exec = strdup("");
        free(prefix);
        job = strdup(job_id);
    }
    if (!job || !exec || !*job || !*exec) {
        free(job);
        free(exec);
        return 0;
    }
    *job_out = job;
    *exec_out = exec;
    return 1;
}

/* PoP: legacy_cron_run_job @ hermes_state_keys.py:legacy_cron_run_job */
char *hsk_legacy_cron_run_job(const char *session_id)
{
    if (!session_id)
        return NULL;
    size_t n = strlen(session_id);
    /* tail: _\d{6} */
    if (n < 7)
        return NULL;
    size_t i = n - 6;
    for (size_t k = i; k < n; k++)
        if (!isdigit((unsigned char)session_id[k]))
            return NULL;
    if (session_id[i - 1] != '_')
        return NULL;
    /* date: _\d{8} */
    if (i - 1 < 9)
        return NULL;
    size_t j = i - 1 - 8;
    for (size_t k = j; k < i - 1; k++)
        if (!isdigit((unsigned char)session_id[k]))
            return NULL;
    if (session_id[j - 1] != '_')
        return NULL;
    /* prefix */
    if (j - 1 < 5 || strncmp(session_id, "cron_", 5) != 0)
        return NULL;
    size_t glen = (j - 1) - 5;
    char *out = (char *)malloc(glen + 1);
    if (!out)
        return NULL;
    memcpy(out, session_id + 5, glen);
    out[glen] = '\0';
    return out;
}

/* PoP: admission_fingerprint @ hermes_state_keys.py:admission_fingerprint */
char *hsk_admission_fingerprint(const char *canonical_target, json_t *payload)
{
    json_t *root = json_new_object();
    json_t *target = json_new_string(canonical_target ? canonical_target : "");
    json_object_set(root, "target", target);
    json_object_set(root, "payload", payload ? json_copy(payload) : json_new_object());
    char *encoded = canon_dumps(root);
    json_free(root);
    if (!encoded)
        return NULL;
    char *hex = hash_sha256_hex((const unsigned char *)encoded, strlen(encoded));
    free(encoded);
    return hex;
}

/* PoP: local_identity @ hermes_state_keys.py:local_identity */
char *hsk_local_identity(const char *profile_id, const char *principal_id,
                         const char *request_id)
{
    json_t *arr = json_new_array();
    json_array_append(arr, json_new_string(profile_id ? profile_id : ""));
    json_array_append(arr, json_new_string(principal_id ? principal_id : ""));
    json_array_append(arr, json_new_string(request_id ? request_id : ""));
    char *encoded = canon_dumps(arr);
    json_free(arr);
    if (!encoded)
        return NULL;
    char *hex = hash_sha256_hex((const unsigned char *)encoded, strlen(encoded));
    free(encoded);
    size_t n = strlen(hex) + 7;
    char *out = (char *)malloc(n);
    if (out)
        snprintf(out, n, "local-%s", hex ? hex : "");
    free(hex);
    return out;
}

/* PoP: profile_from_session_key_namespace @ hermes_state_keys.py:profile_from_session_key_namespace */
char *hsk_profile_from_session_key_namespace(const char *namespace)
{
    if (namespace && strcmp(namespace, "main") == 0)
        return strdup("default");
    if (namespace && strcmp(namespace, "main~") == 0)
        return strdup("main");
    return strdup(namespace ? namespace : "");
}

/* PoP: profile_from_session_key @ hermes_state_keys.py:profile_from_session_key */
char *hsk_profile_from_session_key(const char *session_key)
{
    if (!session_key || !*session_key)
        return NULL;
    const char *first = strchr(session_key, ':');
    if (!first)
        return NULL;
    if ((size_t)(first - session_key) != 5 ||
        strncmp(session_key, "agent", 5) != 0)
        return NULL;
    const char *rest = first + 1;
    const char *second = strchr(rest, ':');
    size_t ns_len = second ? (size_t)(second - rest) : strlen(rest);
    char *ns = (char *)malloc(ns_len + 1);
    if (!ns)
        return NULL;
    memcpy(ns, rest, ns_len);
    ns[ns_len] = '\0';
    char *out = hsk_profile_from_session_key_namespace(
        *ns ? ns : "main");
    free(ns);
    return out;
}

/* --- hermes_state_runtime.py --- */

/* PoP: replacement_binding @ hermes_state_runtime.py:replacement_binding */
char *hsr_replacement_binding(const char *principal_id, const char *session_id,
                              const char *request_id)
{
    json_t *payload = json_new_object();
    json_object_set(payload, "principal", json_new_string(principal_id ? principal_id : ""));
    json_object_set(payload, "request", json_new_string(request_id ? request_id : ""));
    char *fp = hsk_admission_fingerprint(session_id, payload);
    json_free(payload);
    if (!fp)
        return NULL;
    size_t n = strlen(REPLACEMENT_PREFIX) + strlen(fp) + 1;
    char *out = (char *)malloc(n);
    if (out)
        snprintf(out, n, "%s%s", REPLACEMENT_PREFIX, fp);
    free(fp);
    return out;
}

/* PoP: mutation_receipt_key @ hermes_state_runtime.py:mutation_receipt_key */
int hsr_mutation_receipt_key(const char *principal_id, const char *session_id,
                             const char *request_id, const char *operation,
                             json_t *payload, json_t *expected_revision,
                             json_t *expected_generation, char **key_out,
                             char **digest_out)
{
    json_t *id_payload = json_new_object();
    json_object_set(id_payload, "principal", json_new_string(principal_id ? principal_id : ""));
    json_object_set(id_payload, "request", json_new_string(request_id ? request_id : ""));
    char *id_fp = hsk_admission_fingerprint(session_id, id_payload);
    json_free(id_payload);
    if (!id_fp)
        return 0;
    size_t klen = strlen(MUTATION_PREFIX) + strlen(id_fp) + 1;
    char *key = (char *)malloc(klen);
    if (!key) {
        free(id_fp);
        return 0;
    }
    snprintf(key, klen, "%s%s", MUTATION_PREFIX, id_fp);
    free(id_fp);

    json_t *fingerprint = json_new_object();
    json_object_set(fingerprint, "operation", json_new_string(operation ? operation : ""));
    json_object_set(fingerprint, "payload", payload ? json_copy(payload) : json_new_object());
    json_object_set(fingerprint, "expected_revision",
                    expected_revision ? json_copy(expected_revision) : json_new_null());
    if (expected_generation)
        json_object_set(fingerprint, "expected_generation",
                        json_copy(expected_generation));
    char *digest = hsk_admission_fingerprint(session_id, fingerprint);
    json_free(fingerprint);
    if (!digest) {
        free(key);
        return 0;
    }
    *key_out = key;
    *digest_out = digest;
    return 1;
}
