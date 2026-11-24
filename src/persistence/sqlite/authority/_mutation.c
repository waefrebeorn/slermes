/*
 * _mutation.c -- C11 port of the receipted session mutation at upstream
 *   persistence/sqlite/authority/_mutation.py.
 *
 * PoP: w23_mutation_6cb83f_mutate @ persistence/sqlite/authority/_mutation.py:mutate_runtime_session
 * PoP: w23_mutation_6cb83f_receipt_key @ persistence/sqlite/authority/_mutation.py:mutation_receipt_key
 * PoP: w23_mutation_6cb83f_parse @ persistence/sqlite/authority/_mutation.py:parse_mutation
 * PoP: w23_mutation_6cb83f_require_generation @ persistence/sqlite/authority/_mutation.py:_GENERATION_REQUIRED
 *
 * The sqlite handle is the seam. Upstream reads the epoch singleton, the
 * session row and the stored receipt and hands back the apply_action result
 * inside one transaction; this file is handed those same facts and owns the
 * ladder itself: the input checks in their exact order, parse_mutation's
 * closed command union, the receipt identity (canonical JSON + SHA-256), the
 * epoch fence, the replay short circuit, the revision and generation fences
 * and the commit arithmetic (revision bump, the session-gone event, the
 * receipt result shape).
 *
 * Pure C11, ASCII, no VLA, no strdup.
 */
#include "hermes_json.h"
#include "noc_w23_mutation_6cb83f.h"
#include "crypto.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WMUT_KEY_PREFIX "gateway.mutation.v1."
#define WMUT_TEXT_LIMIT 1024

/* ---------- canonical JSON (json.dumps sort_keys, separators (",",":")) ---------- */

typedef struct {
    char *p;
    size_t len;
    size_t cap;
    int oom;
} mu_buf;

static void mu_putc(mu_buf *b, char c) {
    if (b->oom) return;
    if (b->len + 2 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 256;
        char *np = (char *)realloc(b->p, nc);
        if (!np) { b->oom = 1; return; }
        b->p = np;
        b->cap = nc;
    }
    b->p[b->len++] = c;
    b->p[b->len] = '\0';
}

static void mu_puts(mu_buf *b, const char *s) {
    if (!s) return;
    while (*s) mu_putc(b, *s++);
}

static int mu_int_ok(double d) {
    if (d != d) return 0;
    if (d > 9.0e18 || d < -9.0e18) return 0;
    return (double)(long long)d == d;
}

static void mu_put_num(mu_buf *b, double d) {
    char tmp[64];
    int prec;
    if (mu_int_ok(d)) {
        snprintf(tmp, sizeof(tmp), "%lld", (long long)d);
        mu_puts(b, tmp);
        return;
    }
    for (prec = 15; prec <= 17; prec++) {
        snprintf(tmp, sizeof(tmp), "%.*g", prec, d);
        if (strtod(tmp, NULL) == d) break;
    }
    mu_puts(b, tmp);
}

static void mu_put_str(mu_buf *b, const char *s) {
    mu_putc(b, '"');
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '"':  mu_puts(b, "\\\""); break;
        case '\\': mu_puts(b, "\\\\"); break;
        case '\b': mu_puts(b, "\\b"); break;
        case '\f': mu_puts(b, "\\f"); break;
        case '\n': mu_puts(b, "\\n"); break;
        case '\r': mu_puts(b, "\\r"); break;
        case '\t': mu_puts(b, "\\t"); break;
        default:
            if (c < 32) {
                char esc[8];
                snprintf(esc, sizeof(esc), "\\u%04x", (unsigned)c);
                mu_puts(b, esc);
            } else {
                mu_putc(b, (char)c);
            }
            break;
        }
    }
    mu_putc(b, '"');
}

static int mu_cmp_key(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void mu_put_node(mu_buf *b, const json_t *v) {
    size_t i;
    if (!v) { mu_puts(b, "null"); return; }
    switch (v->type) {
    case JSON_NULL:
        mu_puts(b, "null");
        break;
    case JSON_BOOL:
        mu_puts(b, v->bool_val ? "true" : "false");
        break;
    case JSON_NUMBER:
        mu_put_num(b, v->num_val);
        break;
    case JSON_STRING:
        mu_put_str(b, v->str_val ? v->str_val : "");
        break;
    case JSON_ARRAY:
        mu_putc(b, '[');
        for (i = 0; i < v->c.count; i++) {
            if (i) mu_putc(b, ',');
            mu_put_node(b, v->c.items[i]);
        }
        mu_putc(b, ']');
        break;
    case JSON_OBJECT: {
        const char **keys = NULL;
        size_t n = v->c.count;
        if (n) {
            keys = (const char **)malloc(n * sizeof(*keys));
            if (!keys) { b->oom = 1; return; }
        }
        for (i = 0; i < n; i++) keys[i] = v->c.keys[i];
        qsort(keys, n, sizeof(*keys), mu_cmp_key);
        mu_putc(b, '{');
        for (i = 0; i < n; i++) {
            json_t *child = json_obj_get(v, keys[i]);
            if (i) mu_putc(b, ',');
            mu_put_str(b, keys[i]);
            mu_putc(b, ':');
            mu_put_node(b, child);
        }
        mu_putc(b, '}');
        free(keys);
        break;
    }
    default:
        mu_puts(b, "null");
        break;
    }
}

static char *mu_canon(const json_t *v) {
    mu_buf b;
    memset(&b, 0, sizeof(b));
    mu_put_node(&b, v);
    if (b.oom || !b.p) { free(b.p); return NULL; }
    return b.p;
}

static char *mu_sha256_hex(const char *s, size_t n) {
    unsigned char digest[CRYPTO_SHA256_LEN];
    char *hex;
    size_t i;
    static const char *hexdig = "0123456789abcdef";
    crypto_sha256((const unsigned char *)s, n, digest);
    hex = (char *)malloc(CRYPTO_SHA256_LEN * 2 + 1);
    if (!hex) return NULL;
    for (i = 0; i < CRYPTO_SHA256_LEN; i++) {
        hex[i * 2] = hexdig[digest[i] >> 4];
        hex[i * 2 + 1] = hexdig[digest[i] & 0x0f];
    }
    hex[CRYPTO_SHA256_LEN * 2] = '\0';
    return hex;
}

/* admission_fingerprint(canonical_target=..., payload=...): sha256 hex of the
 * canonical {"payload":...,"target":...} object. */
static char *mu_fingerprint(const json_t *target, const json_t *payload) {
    json_t *env = json_object();
    char *canon;
    char *hex;
    if (!env) return NULL;
    json_set(env, "payload", payload ? json_copy(payload) : json_null());
    json_set(env, "target", target ? json_copy(target) : json_null());
    canon = mu_canon(env);
    json_free(env);
    if (!canon) return NULL;
    hex = mu_sha256_hex(canon, strlen(canon));
    free(canon);
    return hex;
}

/* ---------- small type predicates ---------- */

static int mu_is_int(const json_t *v) {
    return v && v->type == JSON_NUMBER && mu_int_ok(v->num_val);
}

static int mu_is_str(const json_t *v) {
    return v && v->type == JSON_STRING;
}

static int mu_is_bool(const json_t *v) {
    return v && v->type == JSON_BOOL;
}

/* Strict column reads: python's sqlite row hands the stored value back as is,
 * so a mistyped column must never be coerced into a number or a bool. */
static double mu_num(const json_t *obj, const char *key, double def) {
    const json_t *v = json_obj_get(obj, key);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

static int mu_true(const json_t *obj, const char *key) {
    const json_t *v = json_obj_get(obj, key);
    return v && v->type == JSON_BOOL && v->bool_val;
}

/* _text: a non-empty string of at most 1024 characters. */
static int mu_text_ok(const json_t *v) {
    if (!mu_is_str(v)) return 0;
    if (!v->str_val || !v->str_val[0]) return 0;
    return strlen(v->str_val) <= WMUT_TEXT_LIMIT;
}

static int mu_is_blank(const char *s, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\v' && c != '\f') return 0;
    }
    return 1;
}

static int mu_str_trimmed_nonblank(const char *s) {
    if (!s) return 0;
    return !mu_is_blank(s, strlen(s));
}

/* ---------- parse_mutation ---------- */

static const char *const WMUT_EFFORTS[] = {"minimal", "low", "medium", "high", "xhigh", "max", "ultra"};

/* hermes_constants.parse_reasoning_effort's accepted levels. */
static int mu_reasoning_level(const char *s) {
    char low[32];
    size_t i, n, k = 0;
    if (!s) return 0;
    n = strlen(s);
    while (k < n && (s[k] == ' ' || s[k] == '\t' || s[k] == '\n' || s[k] == '\r' ||
                     s[k] == '\v' || s[k] == '\f')) k++;
    n = strlen(s + k);
    while (n > 0 && (s[k + n - 1] == ' ' || s[k + n - 1] == '\t' || s[k + n - 1] == '\n' ||
                     s[k + n - 1] == '\r' || s[k + n - 1] == '\v' || s[k + n - 1] == '\f')) n--;
    if (n >= sizeof(low)) return 0;
    for (i = 0; i < n; i++) {
        char c = s[k + i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        low[i] = c;
    }
    low[n] = '\0';
    if (!strcmp(low, "none") || !strcmp(low, "false") || !strcmp(low, "disabled")) return 1;
    for (i = 0; i < sizeof(WMUT_EFFORTS) / sizeof(WMUT_EFFORTS[0]); i++)
        if (!strcmp(low, WMUT_EFFORTS[i])) return 1;
    return 0;
}

/* Every key of obj is in allowed. */
static int mu_under(const json_t *obj, const char *const *allowed, size_t n) {
    size_t i, j;
    for (i = 0; i < json_object_size(obj); i++) {
        const char *k = json_object_get_key_at(obj, i);
        int hit = 0;
        for (j = 0; j < n; j++) {
            if (k && !strcmp(k, allowed[j])) { hit = 1; break; }
        }
        if (!hit) return 0;
    }
    return 1;
}

/* _editor_fingerprint: exactly {"fingerprint": <str>}. */
static const char *mu_editor_fingerprint(const json_t *v) {
    const json_t *fp;
    if (!v || v->type != JSON_OBJECT) return NULL;
    if (json_object_size(v) != 1) return NULL;
    fp = json_obj_get(v, "fingerprint");
    if (!mu_is_str(fp)) return NULL;
    return fp->str_val ? fp->str_val : "";
}

static int mu_p_model(const json_t *p) {
    size_t i;
    static const char *const allowed[] = {"model", "provider", "confirm", "reasoning"};
    const json_t *reasoning;
    if (!json_obj_get(p, "model")) return 0;
    if (!mu_under(p, allowed, 4)) return 0;
    for (i = 0; i < json_object_size(p); i++) {
        const json_t *v = json_object_get_at(p, i);
        if (!mu_is_str(v)) return 0;
        if (!mu_str_trimmed_nonblank(v->str_val)) return 0;
    }
    reasoning = json_obj_get(p, "reasoning");
    if (reasoning && !mu_reasoning_level(reasoning->str_val)) return 0;
    return 1;
}

static int mu_p_branch(const json_t *p) {
    static const char *const allowed[] = {"title", "through_message_id", "mcp_servers"};
    const json_t *v;
    if (!mu_under(p, allowed, 3)) return 0;
    v = json_obj_get(p, "mcp_servers");
    if (v && !mu_editor_fingerprint(v)) return 0;
    v = json_obj_get(p, "title");
    if (v && !mu_is_str(v)) return 0;
    v = json_obj_get(p, "through_message_id");
    if (v && !(mu_is_int(v) && v->num_val > 0.0)) return 0;
    return 1;
}

static int mu_p_policy(const char *name, const json_t *p) {
    const json_t *v;
    if (!strcmp(name, "reasoning")) {
        if (json_object_size(p) != 1) return 0;
        v = json_obj_get(p, "effort");
        if (!v) return 0;
        if (v->type == JSON_NULL) return 1;
        return mu_is_str(v) && mu_reasoning_level(v->str_val);
    }
    if (!strcmp(name, "workspace")) {
        if (json_object_size(p) != 1) return 0;
        v = json_obj_get(p, "cwd");
        return mu_is_str(v) && mu_str_trimmed_nonblank(v->str_val);
    }
    if (!strcmp(name, "editor_mcp")) {
        if (json_object_size(p) != 1) return 0;
        return mu_editor_fingerprint(json_obj_get(p, "mcp_servers")) != NULL;
    }
    /* edit_policy */
    if (json_object_size(p) != 1) return 0;
    v = json_obj_get(p, "policy");
    if (!mu_is_str(v) || !v->str_val) return 0;
    return !strcmp(v->str_val, "ask") || !strcmp(v->str_val, "workspace_session") ||
           !strcmp(v->str_val, "session");
}

static int mu_p_compress(const json_t *p) {
    size_t i;
    static const char *const allowed[] = {"focus", "preview", "partial", "keep_last"};
    const json_t *v;
    if (!mu_under(p, allowed, 4)) return 0;
    for (i = 0; i < json_object_size(p); i++) {
        const char *k = json_object_get_key_at(p, i);
        const json_t *val = json_object_get_at(p, i);
        if (!k) return 0;
        if (!strcmp(k, "focus")) {
            if (!mu_is_str(val)) return 0;
        } else if (!strcmp(k, "preview") || !strcmp(k, "partial")) {
            if (!mu_is_bool(val)) return 0;
        } else if (!strcmp(k, "keep_last")) {
            if (!mu_is_int(val)) return 0;
        }
    }
    v = json_obj_get(p, "keep_last");
    if (v) {
        const json_t *part = json_obj_get(p, "partial");
        if (!(mu_is_bool(part) && part->bool_val && v->num_val >= 1.0)) return 0;
    }
    return 1;
}

static int mu_p_transcript(const char *name, const json_t *p) {
    static const char *const allowed[] = {"target_message_id", "preserve_compaction_handoff"};
    const json_t *v;
    if (!strcmp(name, "delete") || !strcmp(name, "reset"))
        return json_object_size(p) == 0;
    if (!strcmp(name, "import")) {
        if (json_object_size(p) != 1) return 0;
        v = json_obj_get(p, "sessions");
        return v && v->type == JSON_ARRAY;
    }
    /* rewind */
    if (!mu_under(p, allowed, 2)) return 0;
    v = json_obj_get(p, "target_message_id");
    if (!v) return 0;
    if (!(mu_is_int(v) && v->num_val > 0.0)) return 0;
    v = json_obj_get(p, "preserve_compaction_handoff");
    if (v && !mu_is_bool(v)) return 0;
    return 1;
}

/* METADATA_FIELDS: title: str, archived/hidden/pinned/unread: bool. */
static const char *mu_metadata_kind(const char *k) {
    if (!strcmp(k, "title")) return "str";
    if (!strcmp(k, "archived") || !strcmp(k, "hidden") || !strcmp(k, "pinned") || !strcmp(k, "unread"))
        return "bool";
    return NULL;
}

static int mu_p_metadata(const char *name, const json_t *p) {
    size_t i, n = json_object_size(p);
    if (!strcmp(name, "sidebar")) {
        if (n == 0) return 0;
    } else if (!strcmp(name, "rename")) {
        if (n != 1 || !json_obj_get(p, "title")) return 0;
    } else if (!strcmp(name, "archive")) {
        if (n != 1 || !json_obj_get(p, "archived")) return 0;
    } else {
        return 0;
    }
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(p, i);
        const json_t *v = json_object_get_at(p, i);
        const char *kind = k ? mu_metadata_kind(k) : NULL;
        if (!kind) return 0;
        if (!strcmp(kind, "str")) {
            if (!mu_is_str(v)) return 0;
        } else if (!mu_is_bool(v)) {
            return 0;
        }
    }
    return 1;
}

/* parse_mutation: an unknown operation or an invalid payload refuses. */
static int mu_parse_mutation(const json_t *op, const json_t *payload) {
    const char *name;
    if (!op || op->type != JSON_STRING || !op->str_val) return 0;
    if (!payload || payload->type != JSON_OBJECT) return 0;
    name = op->str_val;
    if (!strcmp(name, "model")) return mu_p_model(payload);
    if (!strcmp(name, "branch")) return mu_p_branch(payload);
    if (!strcmp(name, "compress")) return mu_p_compress(payload);
    if (!strcmp(name, "reasoning") || !strcmp(name, "workspace") ||
        !strcmp(name, "editor_mcp") || !strcmp(name, "edit_policy"))
        return mu_p_policy(name, payload);
    if (!strcmp(name, "delete") || !strcmp(name, "reset") || !strcmp(name, "import") ||
        !strcmp(name, "rewind"))
        return mu_p_transcript(name, payload);
    return mu_p_metadata(name, payload);
}

int w23_mutation_6cb83f_parse(const json_t *req) {
    const json_t *op = req && req->type == JSON_OBJECT ? json_obj_get(req, "operation") : NULL;
    const json_t *payload = req && req->type == JSON_OBJECT ? json_obj_get(req, "payload") : NULL;
    return mu_parse_mutation(op, payload);
}

int w23_mutation_6cb83f_require_generation(const json_t *operation) {
    const char *n;
    if (!operation || operation->type != JSON_STRING || !operation->str_val) return 0;
    n = operation->str_val;
    return !strcmp(n, "delete") || !strcmp(n, "rewind") || !strcmp(n, "reset") ||
           !strcmp(n, "branch") || !strcmp(n, "model") || !strcmp(n, "compress");
}

/* ---------- receipt identity ---------- */

static json_t *mu_receipt_key_nodes(const json_t *principal, const json_t *session_id,
                                    const json_t *request_id, const json_t *operation,
                                    const json_t *payload, const json_t *revision,
                                    const json_t *generation) {
    json_t *out, *ident, *fp;
    char *fp_hex, *digest_hex;
    char *key;

    ident = json_object();
    if (!ident) return NULL;
    json_set(ident, "principal", principal ? json_copy(principal) : json_null());
    json_set(ident, "request", request_id ? json_copy(request_id) : json_null());
    fp_hex = mu_fingerprint(session_id, ident);
    json_free(ident);
    if (!fp_hex) return NULL;
    key = (char *)malloc(strlen(WMUT_KEY_PREFIX) + strlen(fp_hex) + 1);
    if (!key) { free(fp_hex); return NULL; }
    strcpy(key, WMUT_KEY_PREFIX);
    strcat(key, fp_hex);
    free(fp_hex);

    fp = json_object();
    if (!fp) { free(key); return NULL; }
    json_set(fp, "operation", operation ? json_copy(operation) : json_null());
    json_set(fp, "payload", payload ? json_copy(payload) : json_null());
    json_set(fp, "expected_revision", revision ? json_copy(revision) : json_null());
    if (generation && generation->type != JSON_NULL)
        json_set(fp, "expected_generation", json_copy(generation));
    digest_hex = mu_fingerprint(session_id, fp);
    json_free(fp);
    if (!digest_hex) { free(key); return NULL; }

    out = json_object();
    if (!out) { free(key); free(digest_hex); return NULL; }
    json_set(out, "key", json_string(key));
    json_set(out, "digest", json_string(digest_hex));
    free(key);
    free(digest_hex);
    return out;
}

json_t *w23_mutation_6cb83f_receipt_key(const json_t *req) {
    if (!req || req->type != JSON_OBJECT) return json_null();
    return mu_receipt_key_nodes(json_obj_get(req, "principal_id"), json_obj_get(req, "session_id"),
                                json_obj_get(req, "request_id"), json_obj_get(req, "operation"),
                                json_obj_get(req, "payload"), json_obj_get(req, "expected_revision"),
                                json_obj_get(req, "expected_generation"));
}

/* ---------- the closed write ---------- */

static json_t *mu_refused(const char *reason) {
    json_t *o = json_object();
    json_set(o, "status", json_string("refused"));
    json_set(o, "reason", json_string(reason));
    return o;
}

static int mu_affected_has(const json_t *affected, const char *sid) {
    size_t i;
    if (!affected || affected->type != JSON_ARRAY || !sid) return 0;
    for (i = 0; i < json_len(affected); i++) {
        const json_t *e = json_get(affected, i);
        if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, sid)) return 1;
    }
    return 0;
}

static json_t *mu_string_list(const json_t *arr) {
    json_t *out = json_array();
    size_t i;
    if (!out) return NULL;
    if (arr && arr->type == JSON_ARRAY)
        for (i = 0; i < json_len(arr); i++) json_append(out, json_copy(json_get(arr, i)));
    return out;
}

static json_t *mu_single_id(const char *sid) {
    json_t *one = json_array();
    if (!one) return NULL;
    json_append(one, json_string(sid ? sid : ""));
    return one;
}

json_t *w23_mutation_6cb83f_mutate(const json_t *req) {
    const json_t *principal, *session_id, *request_id, *revision, *generation;
    const json_t *operation, *payload, *store, *s_epoch, *sess, *affected, *projection;
    const json_t *arg_epoch;
    const char *sid, *op;
    json_t *keypair, *out, *result, *gone = NULL;
    int session_present;
    double want_rev;

    if (!req || req->type != JSON_OBJECT) return mu_refused("invalid_params");

    principal = json_obj_get(req, "principal_id");
    session_id = json_obj_get(req, "session_id");
    request_id = json_obj_get(req, "request_id");
    revision = json_obj_get(req, "expected_revision");
    generation = json_obj_get(req, "expected_generation");
    operation = json_obj_get(req, "operation");
    payload = json_obj_get(req, "payload");

    /* for value in (principal_id, session_id, request_id): _text(value) */
    if (!mu_text_ok(principal) || !mu_text_ok(session_id) || !mu_text_ok(request_id))
        return mu_refused("invalid_params");
    /* if type(expected_revision) is not int or expected_revision < 0 */
    if (!mu_is_int(revision) || revision->num_val < 0.0) return mu_refused("invalid_params");
    if (!mu_parse_mutation(operation, payload)) return mu_refused("invalid_params");
    if (generation && generation->type != JSON_NULL) {
        if (!mu_is_int(generation) || generation->num_val < 0.0) return mu_refused("invalid_params");
    }
    if (w23_mutation_6cb83f_require_generation(operation) &&
        (!generation || generation->type == JSON_NULL))
        return mu_refused("invalid_params");

    sid = session_id->str_val;
    op = operation->str_val;
    want_rev = revision->num_val;

    /* the receipt identity is computed before the write, as upstream does */
    keypair = mu_receipt_key_nodes(principal, session_id, request_id, operation, payload,
                                   revision, generation);
    if (!keypair) return mu_refused("invalid_params");
    json_free(keypair);

    store = json_obj_get(req, "store");
    if (store && store->type != JSON_OBJECT) store = NULL;

    /* _epoch(conn, epoch): a missing, mistyped or stale epoch row refuses. */
    arg_epoch = json_obj_get(req, "epoch");
    s_epoch = json_obj_get(store, "epoch");
    if (!mu_is_int(arg_epoch) || !mu_is_int(s_epoch) || s_epoch->num_val != arg_epoch->num_val)
        return mu_refused("stale_epoch");

    /* replay_mutation_receipt(conn, key, digest): the exact retry, else refuse. */
    {
        const char *kind = json_get_str(store, "receipt", "none");
        if (kind && !strcmp(kind, "conflict")) return mu_refused("admission_conflict");
        if (kind && !strcmp(kind, "exact")) {
            const json_t *stored = json_obj_get(store, "receipt_result");
            out = json_object();
            if (!out) return mu_refused("invalid_params");
            json_set(out, "status", json_string("ok"));
            json_set(out, "result", stored ? json_copy(stored) : json_null());
            json_set(out, "gone", json_null());
            json_set(out, "replayed", json_bool(1));
            return out;
        }
    }

    sess = json_obj_get(store, "session");
    session_present = sess && sess->type == JSON_OBJECT;

    /* if session is None and operation != 'import': not_found */
    if (!session_present && strcmp(op, "import") != 0) return mu_refused("not_found");

    if ((session_present ? mu_num(sess, "runtime_revision", 0.0) : 0.0) != want_rev)
        return mu_refused("revision_conflict");
    if (generation && generation->type != JSON_NULL) {
        if (!session_present ||
            mu_num(sess, "runtime_generation", 0.0) != generation->num_val)
            return mu_refused("stale_generation");
    }

    affected = json_obj_get(store, "affected");
    projection = json_obj_get(store, "projection");
    if (!projection || projection->type != JSON_OBJECT) projection = NULL;

    /* A delete's receipt owes the retired ids; a first archive's owes the rows it moved. */
    if (!strcmp(op, "delete")) {
        const json_t *ids = projection ? json_obj_get(projection, "deleted_ids") : NULL;
        gone = json_object();
        if (!gone) return mu_refused("invalid_params");
        if (ids && ids->type == JSON_ARRAY && json_len(ids) > 0)
            json_set(gone, "session_ids", mu_string_list(ids));
        else
            json_set(gone, "session_ids", mu_single_id(sid));
        json_set(gone, "reason", json_string("delete"));
    } else if (payload && payload->type == JSON_OBJECT) {
        const json_t *arch = json_obj_get(payload, "archived");
        if (mu_is_bool(arch) && arch->bool_val && session_present &&
            !mu_true(sess, "archived")) {
            gone = json_object();
            if (!gone) return mu_refused("invalid_params");
            if (affected && affected->type == JSON_ARRAY && json_len(affected) > 0)
                json_set(gone, "session_ids", mu_string_list(affected));
            else
                json_set(gone, "session_ids", mu_single_id(sid));
            json_set(gone, "reason", json_string("archive"));
        }
    }

    result = json_object();
    if (!result) { json_free(gone); return mu_refused("invalid_params"); }
    json_set(result, "session_id", json_string(sid));
    if (session_present) {
        double rev = mu_num(sess, "runtime_revision", 0.0);
        if (mu_affected_has(affected, sid)) rev += 1.0;
        json_set(result, "revision", json_number(rev));
        json_set(result, "execution_generation",
                 json_number(mu_num(sess, "runtime_generation", 0.0)));
    } else {
        json_set(result, "revision", json_number(want_rev + 1.0));
        if (generation && generation->type != JSON_NULL)
            json_set(result, "execution_generation", json_copy(generation));
        else
            json_set(result, "execution_generation", json_null());
    }
    json_set(result, "operation", json_string(op));
    {
        size_t i;
        for (i = 0; projection && i < json_object_size(projection); i++) {
            const char *k = json_object_get_key_at(projection, i);
            if (!k) continue;
            json_set(result, k, json_copy(json_object_get_at(projection, i)));
        }
    }

    out = json_object();
    if (!out) { json_free(result); json_free(gone); return mu_refused("invalid_params"); }
    json_set(out, "status", json_string("ok"));
    json_set(out, "result", result);
    json_set(out, "gone", gone ? gone : json_null());
    json_set(out, "replayed", json_bool(0));
    return out;
}
