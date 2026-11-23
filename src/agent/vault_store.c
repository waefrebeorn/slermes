/*
 * vault_store.c — C home for upstream Hermes agent/vault_store.py.
 *
 * PoP: w6_vaultstore_6d2c23_vault_kinds @ agent/vault_store.py:VAULT_KINDS
 * PoP: w6_vaultstore_6d2c23_login_identifier_types @ agent/vault_store.py:LOGIN_IDENTIFIER_TYPES
 * PoP: w6_vaultstore_6d2c23_fields @ agent/vault_store.py:PAYMENT_FIELDS / ADDRESS_FIELDS
 * PoP: w6_vaultstore_6d2c23_required_fields @ agent/vault_store.py:REQUIRED_FIELDS
 * PoP: w6_vaultstore_6d2c23_normalize_origin @ agent/vault_store.py:normalize_origin
 * PoP: w6_vaultstore_6d2c23_normalize_otp_secret @ agent/vault_store.py:normalize_otp_secret
 * PoP: w6_vaultstore_6d2c23_scrub_secret_from_text @ agent/vault_store.py:scrub_secret_from_text
 * PoP: w6_vaultstore_6d2c23_meta_dict @ agent/vault_store.py:VaultStore._meta + VaultItemMeta.to_dict
 *
 * The Fernet/keychain/file-I/O half of the module is deliberately out of scope:
 * these are its deterministic, secret-free helpers.
 */
#include "noc_w6_vaultstore_6d2c23.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *p; size_t len, cap; } buf_t;

static void bneed(buf_t *b, size_t extra)
{
    if (b->len + extra + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap * 2 : 128;
    while (nc < b->len + extra + 1) nc *= 2;
    char *np = (char *)realloc(b->p, nc);
    if (np) { b->p = np; b->cap = nc; }
}
static void baddn(buf_t *b, const char *s, size_t n)
{
    bneed(b, n);
    if (!b->p) return;
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void badd(buf_t *b, const char *s) { baddn(b, s, strlen(s)); }
static char *bdone(buf_t *b) { return b->p ? b->p : (char *)calloc(1, 1); }

static char *dupstr(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

static int truthy(const struct json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] != '\0';
    return json_len(v) > 0;
}

/* Python str() coercion for the scalar values used in metadata. */
static char *py_str_ex(const struct json_t *v, const char *dflt)
{
    if (!v) return dupstr(dflt ? dflt : "None");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_NUMBER) {
        char tmp[64];
        snprintf(tmp, sizeof tmp, "%.17g", v->num_val);
        return dupstr(tmp);
    }
    return dupstr("");
}

static char *trim_ascii(const char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *lower_dup(const char *s, size_t n)
{
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    for (size_t i = 0; i < n; i++) p[i] = (char)tolower((unsigned char)s[i]);
    p[n] = '\0';
    return p;
}

static int parse_int_strict(const char *s, long *out)
{
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s) return 0;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end) return 0;
    *out = v;
    return 1;
}

static int strncasecmp_generic(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 1;
    return 0;
}

static int hexval(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* urllib.parse.parse_qs value decoding (percent + '+' -> space). */
static char *pct_decode(const char *s, size_t n)
{
    buf_t b = {0};
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '%' && i + 2 < n) {
            int hi = hexval((unsigned char)s[i + 1]), lo = hexval((unsigned char)s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                char c = (char)((hi << 4) | lo);
                baddn(&b, &c, 1);
                i += 2;
                continue;
            }
        }
        char c = (s[i] == '+') ? ' ' : s[i];
        baddn(&b, &c, 1);
    }
    return bdone(&b);
}

/* ---- constants --------------------------------------------------------- */

struct json_t *w6_vaultstore_6d2c23_vault_kinds(void)
{
    struct json_t *a = json_array();
    json_append(a, json_string("login"));
    json_append(a, json_string("payment"));
    json_append(a, json_string("address"));
    return a;
}

struct json_t *w6_vaultstore_6d2c23_login_identifier_types(void)
{
    struct json_t *a = json_array();
    json_append(a, json_string("email"));
    json_append(a, json_string("phone"));
    json_append(a, json_string("username"));
    return a;
}

struct json_t *w6_vaultstore_6d2c23_fields(const char *kind)
{
    struct json_t *o = json_object();
    if (kind && !strcmp(kind, "payment")) {
        json_set(o, "card_number", json_string("cc-number"));
        json_set(o, "cardholder_name", json_string("cc-name"));
        json_set(o, "exp_month", json_string("cc-exp-month"));
        json_set(o, "exp_year", json_string("cc-exp-year"));
        json_set(o, "cvc", json_string("cc-csc"));
        json_set(o, "billing_postal_code", json_string("postal-code"));
    } else if (kind && !strcmp(kind, "address")) {
        json_set(o, "address_line1", json_string("address-line1"));
        json_set(o, "address_line2", json_string("address-line2"));
        json_set(o, "city", json_string("address-level2"));
        json_set(o, "state", json_string("address-level1"));
        json_set(o, "postal_code", json_string("postal-code"));
        json_set(o, "country", json_string("country-name"));
    }
    return o;
}

struct json_t *w6_vaultstore_6d2c23_required_fields(const char *kind)
{
    struct json_t *a = json_array();
    if (kind && !strcmp(kind, "payment")) {
        json_append(a, json_string("card_number"));
        json_append(a, json_string("exp_month"));
        json_append(a, json_string("exp_year"));
        json_append(a, json_string("cvc"));
    } else if (kind && !strcmp(kind, "address")) {
        json_append(a, json_string("address_line1"));
        json_append(a, json_string("city"));
        json_append(a, json_string("postal_code"));
        json_append(a, json_string("country"));
    }
    return a;
}

/* ---- normalize_origin --------------------------------------------------- */

char *w6_vaultstore_6d2c23_normalize_origin(const char *url_or_origin)
{
    char *value = trim_ascii(url_or_origin ? url_or_origin : "");
    if (!value || !*value) { free(value); return NULL; }
    char *sep = strstr(value, "://");
    if (!sep) { free(value); return NULL; }
    char *scheme = lower_dup(value, (size_t)(sep - value));

    char *rest = sep + 3;
    size_t alen = strcspn(rest, "/?#");
    char *authority = (char *)malloc(alen + 1);
    if (!authority) { free(value); free(scheme); return NULL; }
    memcpy(authority, rest, alen);
    authority[alen] = '\0';

    char *at = strrchr(authority, '@');
    char *hostport = at ? at + 1 : authority;
    char *host = NULL;
    const char *portstr = NULL;

    if (*hostport == '[') {
        char *close = strchr(hostport, ']');
        if (!close) goto fail;
        host = lower_dup(hostport + 1, (size_t)(close - hostport - 1));
        if (close[1] == ':') portstr = close + 2;
    } else {
        char *colon = strrchr(hostport, ':');
        if (colon) {
            host = lower_dup(hostport, (size_t)(colon - hostport));
            portstr = colon + 1;
        } else {
            host = lower_dup(hostport, strlen(hostport));
        }
    }
    if (!host || !*host) goto fail;

    long port = -1;
    if (portstr && *portstr) {
        if (!parse_int_strict(portstr, &port) || port < 0 || port > 65535) goto fail;
    }

    int defport = -1;
    if (!strcmp(scheme, "http")) defport = 80;
    else if (!strcmp(scheme, "https")) defport = 443;
    if (port < 0 || port == defport) {
        size_t n = strlen(scheme) + 3 + strlen(host);
        char *out = (char *)malloc(n + 1);
        if (out) snprintf(out, n + 1, "%s://%s", scheme, host);
        free(value); free(scheme); free(authority); free(host);
        return out;
    }
    char pbuf[32];
    snprintf(pbuf, sizeof pbuf, "%ld", port);
    size_t n = strlen(scheme) + 3 + strlen(host) + 1 + strlen(pbuf);
    char *out = (char *)malloc(n + 1);
    if (out) snprintf(out, n + 1, "%s://%s:%s", scheme, host, pbuf);
    free(value); free(scheme); free(authority); free(host);
    return out;

fail:
    free(value); free(scheme); free(authority); free(host);
    return NULL;
}

/* ---- normalize_otp_secret ---------------------------------------------- */

typedef struct { char *key; char *val; } KV;

static char *query_get(KV *kv, size_t n, const char *key)
{
    for (size_t i = 0; i < n; i++)
        if (!strcmp(kv[i].key, key)) return kv[i].val;
    return NULL;
}

char *w6_vaultstore_6d2c23_normalize_otp_secret(const char *value)
{
    char *raw = trim_ascii(value ? value : "");
    if (!raw || !*raw) { free(raw); return (char *)calloc(1, 1); }

    int digits = 6, period = 30;
    char algo[16] = "SHA1";
    const char *seed_input = raw;

    if (!strncasecmp_generic(raw, "otpauth://", 10)) {
        char *p = raw + 10;
        size_t nlen = strcspn(p, "/?#");
        char *netloc = lower_dup(p, nlen);
        if (!netloc || strcmp(netloc, "totp")) { free(netloc); free(raw); return NULL; }
        free(netloc);
        char *q = strchr(p, '?');
        KV kv[32];
        size_t nkv = 0;
        if (q) {
            q++;
            char *end = strchr(q, '#');
            size_t qlen = end ? (size_t)(end - q) : strlen(q);
            char *query = (char *)malloc(qlen + 1);
            if (query) {
                memcpy(query, q, qlen);
                query[qlen] = '\0';
                char *walk = query;
                while (walk && *walk) {
                    char *part = walk;
                    char *amp = strchr(walk, '&');
                    if (amp) { *amp = '\0'; walk = amp + 1; } else walk = NULL;
                    char *eq = strchr(part, '=');
                    if (!eq || eq == part || nkv >= 32) continue;
                    size_t klen = (size_t)(eq - part);
                    char *k = lower_dup(part, klen);
                    char *v = pct_decode(eq + 1, strlen(eq + 1));
                    if (!*v) { free(k); free(v); continue; }  /* parse_qs drops blank values */
                    kv[nkv].key = k;
                    kv[nkv].val = v;
                    nkv++;
                }
                free(query);
            }
        }
        char *s = query_get(kv, nkv, "secret");
        seed_input = s ? s : "";
        char *d = query_get(kv, nkv, "digits");
        char *pd = query_get(kv, nkv, "period");
        char *a = query_get(kv, nkv, "algorithm");
        long dv, pv;
        if (d && (!parse_int_strict(d, &dv) || dv < 0)) goto otp_fail;
        if (pd && (!parse_int_strict(pd, &pv) || pv < 0)) goto otp_fail;
        if (d) digits = (int)dv;
        if (pd) period = (int)pv;
        if (a) {
            size_t j = 0;
            for (size_t i = 0; a[i] && j < 15; i++) {
                if (a[i] == '-') continue;
                algo[j++] = (char)toupper((unsigned char)a[i]);
            }
            algo[j] = '\0';
        }
        if (!((digits == 6 || digits == 7 || digits == 8) && period > 0 &&
              (!strcmp(algo, "SHA1") || !strcmp(algo, "SHA256") || !strcmp(algo, "SHA512"))))
            goto otp_fail;
        /* keep kv alive through seed building: seed_input points into kv values */
        buf_t seedbuf = {0};
        for (const char *c = seed_input; *c; c++) {
            if (isspace((unsigned char)*c) || *c == '-') continue;
            char up = (char)toupper((unsigned char)*c);
            baddn(&seedbuf, &up, 1);
        }
        size_t sl = seedbuf.len;
        while (sl > 0 && seedbuf.p[sl - 1] == '=') sl--;
        if (sl == 0) goto otp_fail;
        for (size_t i = 0; i < sl; i++) {
            char c = seedbuf.p[i];
            if (!((c >= 'A' && c <= 'Z') || (c >= '2' && c <= '7'))) goto otp_fail;
        }
        char *seed = (char *)malloc(sl + 1);
        if (seed) { memcpy(seed, seedbuf.p, sl); seed[sl] = '\0'; }
        free(seedbuf.p);
        for (size_t i = 0; i < nkv; i++) { free(kv[i].key); free(kv[i].val); }
        free(raw);
        if (!seed) return NULL;
        if (digits == 6 && period == 30 && !strcmp(algo, "SHA1")) return seed;
        char out[512];
        snprintf(out, sizeof out, "%s|%d|%d|%s", seed, digits, period, algo);
        free(seed);
        return dupstr(out);

    otp_fail:
        for (size_t i = 0; i < nkv; i++) { free(kv[i].key); free(kv[i].val); }
        free(raw);
        return NULL;
    }

    buf_t seedbuf = {0};
    for (const char *c = seed_input; *c; c++) {
        if (isspace((unsigned char)*c) || *c == '-') continue;
        char up = (char)toupper((unsigned char)*c);
        baddn(&seedbuf, &up, 1);
    }
    size_t sl = seedbuf.len;
    while (sl > 0 && seedbuf.p[sl - 1] == '=') sl--;
    if (sl == 0) { free(seedbuf.p); free(raw); return NULL; }
    for (size_t i = 0; i < sl; i++) {
        char c = seedbuf.p[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '2' && c <= '7'))) {
            free(seedbuf.p); free(raw); return NULL;
        }
    }
    seedbuf.p[sl] = '\0';
    free(raw);
    return seedbuf.p;
}

/* ---- scrub_secret_from_text -------------------------------------------- */

static char *replace_all(const char *s, const char *needle, const char *repl)
{
    size_t nl = strlen(needle);
    buf_t b = {0};
    const char *p = s;
    while (*p) {
        const char *hit = strstr(p, needle);
        if (!hit) { badd(&b, p); break; }
        baddn(&b, p, (size_t)(hit - p));
        badd(&b, repl);
        p = hit + nl;
    }
    return bdone(&b);
}

static char *scrub_password_echoes(const char *s)
{
    buf_t b = {0};
    size_t i = 0;
    while (s[i]) {
        if (!strncmp(s + i, "password", 8)) {
            size_t j = i + 8;
            if (s[j] == '\'' || s[j] == '"') j++;
            while (isspace((unsigned char)s[j])) j++;
            if (s[j] == ':' || s[j] == '=') {
                size_t k = j + 1;
                while (isspace((unsigned char)s[k])) k++;
                if (s[k] && !isspace((unsigned char)s[k])) {
                    baddn(&b, s + i, k - i);
                    badd(&b, "[REDACTED]");
                    while (s[k] && !isspace((unsigned char)s[k])) k++;
                    i = k;
                    continue;
                }
            }
        }
        baddn(&b, s + i, 1);
        i++;
    }
    return bdone(&b);
}

char *w6_vaultstore_6d2c23_scrub_secret_from_text(const char *text,
                                                  const struct json_t *secret)
{
    char *cur = dupstr(text ? text : "");
    if (secret && secret->type == JSON_OBJECT) {
        size_t n = json_object_size(secret);
        for (size_t i = 0; i < n; i++) {
            json_t *v = json_object_get_at(secret, i);
            if (v && v->type == JSON_STRING && v->str_val && strlen(v->str_val) >= 3) {
                char *next = replace_all(cur, v->str_val, "[REDACTED]");
                free(cur);
                cur = next;
            }
        }
    }
    char *out = scrub_password_echoes(cur);
    free(cur);
    return out;
}

/* ---- meta_dict ---------------------------------------------------------- */

struct json_t *w6_vaultstore_6d2c23_meta_dict(const struct json_t *rec)
{
    struct json_t *out = json_object();
    if (!rec || rec->type != JSON_OBJECT) {
        json_set(out, "id", json_string(""));
        json_set(out, "kind", json_string(""));
        json_set(out, "label", json_string(""));
        json_set(out, "origin", json_null());
        json_set(out, "created_at", json_string(""));
        return out;
    }

    const struct json_t *idv = json_obj_get(rec, "id");
    const struct json_t *kindv = json_obj_get(rec, "kind");
    const struct json_t *labelv = json_obj_get(rec, "label");
    const struct json_t *createdv = json_obj_get(rec, "created_at");
    const struct json_t *originv = json_obj_get(rec, "origin");
    const struct json_t *ident = json_obj_get(rec, "identifier");
    const struct json_t *idt = json_obj_get(rec, "identifier_type");
    const struct json_t *secret = json_obj_get(rec, "secret");

    char *s;
    s = py_str_ex(idv, "");      json_set(out, "id", json_string(s)); free(s);
    s = py_str_ex(kindv, "");    json_set(out, "kind", json_string(s)); free(s);
    s = py_str_ex(labelv, "");   json_set(out, "label", json_string(s)); free(s);
    json_set(out, "origin", originv ? json_copy(originv) : json_null());
    s = py_str_ex(createdv, ""); json_set(out, "created_at", json_string(s)); free(s);

    if (truthy(ident)) {
        json_set(out, "identifier", json_copy(ident));
        json_set(out, "identifier_type", idt ? json_copy(idt) : json_null());
    }
    int has_otp = 0;
    if (secret && secret->type == JSON_OBJECT)
        has_otp = truthy(json_obj_get(secret, "otp_secret"));
    if (has_otp) json_set(out, "has_otp", json_bool(1));
    return out;
}
