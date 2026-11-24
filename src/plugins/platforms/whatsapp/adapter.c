/*
 * adapter.c - C11 port of the pure logic of upstream
 * plugins/platforms/whatsapp/adapter.py (102 KB, 96 defs), split across the
 * four differential gates w27_adapter_42b735_p1 .. _p4.
 *
 * Ported here (the value-only decision core of the Baileys bridge adapter):
 *   p1  token + spawn-config identity: _safe_ints, _bridge_auth_headers,
 *       _bridge_auth_proof (HMAC-SHA256), _bridge_config_fingerprint
 *       (SHA-256 over the consumed-env key set), _fmt_uptime.
 *   p2  lifecycle + endpoint addressing: WhatsAppAdapter._bridge_url, the
 *       bridge.pid line parser behind _read_bridge_pidfile, the _bridge_died
 *       fatal-error classification (PASSKEY_REQUIRED_EXIT ->
 *       whatsapp_passkey_required) and _is_connected.
 *   p3  inbound shaping: _classify_bridge_message, _may_transcribe_at_gate,
 *       _inject_document_text (text-readable document inlining) and the
 *       _log_delivery_status log line.
 *   p4  outbound shaping: _bridge_media_type (extension table),
 *       _normalize_outbound_mentions (JID normalization + dedupe) and the
 *       module's bridge-facing constant tables.
 *
 * Seamed (not ported, I/O or transport by nature): the spawn/kill subsystem
 * (_listener_pids_on_port, _kill_port_process, _wait_pids_exit, spawn_owned,
 * _terminate_bridge_process, the psutil identity checks), every aiohttp call
 * (_probe_bridge_health, _post_bridge_message, send / send_* / edit / poll /
 * typing, _authenticated_bridge_health), the filesystem writers
 * (_load_or_create_bridge_token lock, _write_bridge_pidfile,
 * _write_bridge_fingerprint, _open_bridge_log, _file_content_hash,
 * _bridge_source_hash, _is_allowed_bridge_path, _ensure_bridge_deps), the
 * env/subprocess plumbing (_wenv, _bridge_env, _session_dir,
 * check_whatsapp_requirements, ensure_aiohttp), the async lifecycle
 * (connect / disconnect / _reuse_running_bridge / _poll_messages /
 * _build_message_event) and the gateway glue (_apply_yaml_config,
 * interactive_setup, register, _standalone_send).
 *
 * Every ported symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same-named upstream function applied to
 * the keys named after its parameters; a NULL json_t* is None.
 */
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w27_adapter_42b735_p1.h"
#include "noc_w27_adapter_42b735_p2.h"
#include "noc_w27_adapter_42b735_p3.h"
#include "noc_w27_adapter_42b735_p4.h"

/* ---- module constants mirrored from the Python source ---- */
#define WA_PASSKEY_REQUIRED_EXIT 78
#define WA_OWNER_REPLY_PREFIX "[owner reply] "
#define WA_MAX_TEXT_INJECT_BYTES (100 * 1024)
#define WA_SPLIT_THRESHOLD 6000

static const char *const WA_BRIDGE_PASSTHROUGH_ENV[] = {
    "WHATSAPP_ALLOWED_USERS", "WHATSAPP_ALLOW_FROM", "WHATSAPP_DM_POLICY", "WHATSAPP_GROUP_POLICY",
    "WHATSAPP_GROUP_ALLOWED_USERS", "WHATSAPP_GROUP_ALLOW_FROM", "WHATSAPP_REQUIRE_MENTION",
    "WHATSAPP_MENTION_PATTERNS", "WHATSAPP_FREE_RESPONSE_CHATS", "WHATSAPP_DEBUG",
    "WHATSAPP_FORWARD_OWNER_MESSAGES", "WHATSAPP_MAX_MESSAGE_LENGTH",
    "WHATSAPP_CHUNK_DELAY_MS", "WHATSAPP_SEND_TIMEOUT_MS",
};
#define WA_PASSTHROUGH_ENV_N (sizeof(WA_BRIDGE_PASSTHROUGH_ENV) / sizeof(WA_BRIDGE_PASSTHROUGH_ENV[0]))

/* _BRIDGE_CONSUMED_ENV = _BRIDGE_PASSTHROUGH_ENV + (the keys _bridge_env resolves itself) */
static const char *const WA_BRIDGE_CONSUMED_ENV_EXTRA[] = {
    "WHATSAPP_MODE", "WHATSAPP_REPLY_PREFIX", "WHATSAPP_SEND_READ_RECEIPTS",
    "HERMES_IMAGE_CACHE_DIR", "HERMES_AUDIO_CACHE_DIR", "HERMES_DOCUMENT_CACHE_DIR",
};
#define WA_CONSUMED_ENV_N (WA_PASSTHROUGH_ENV_N + \
    (sizeof(WA_BRIDGE_CONSUMED_ENV_EXTRA) / sizeof(WA_BRIDGE_CONSUMED_ENV_EXTRA[0])))

static const char *const WA_TEXT_INJECT_EXTS[] = {
    ".css", ".csv", ".html", ".js", ".json", ".log", ".md", ".py", ".ts", ".txt", ".xml", ".yaml", ".yml",
};
#define WA_TEXT_INJECT_EXTS_N (sizeof(WA_TEXT_INJECT_EXTS) / sizeof(WA_TEXT_INJECT_EXTS[0]))

/* ------------------------------------------------------------------ */
/* Python value helpers                                               */
/* ------------------------------------------------------------------ */

static int jd_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }

static char *jd_strdup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static char *jd_fmt(const char *fmt, ...) {
    va_list ap, ap2;
    int n;
    char *buf;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return jd_strdup(""); }
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { va_end(ap2); return jd_strdup(""); }
    vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

/* Python str() of a scalar JSON value (scalars only; no ported symbol is fed
 * a container where Python would repr() it).  Caller frees. */
static char *jd_float_repr(double d) {
    /* Shortest round-tripping %g form: Python repr() for a non-integral float. */
    char buf[48];
    int prec;
    if (d != d) return jd_strdup("nan");
    if (d > 1.7976931348623157e308) return jd_strdup("inf");
    if (d < -1.7976931348623157e308) return jd_strdup("-inf");
    for (prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    return jd_strdup(buf);
}

static char *jd_pystr(const json_t *v) {
    if (jd_is_none(v)) return jd_strdup("None");
    switch (v->type) {
    case JSON_BOOL: return jd_strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        double d = v->num_val;
        /* libjson stores every number as a double, so an integral value is
         * rendered as an int -- the only shape the env/status values take. */
        if (d == floor(d) && fabs(d) < 1e16) return jd_fmt("%.0f", d);
        return jd_float_repr(d);
    }
    case JSON_STRING: return jd_strdup(v->str_val ? v->str_val : "");
    default: return jd_strdup("");
    }
}

/* Python truthiness of a JSON value. */
static int jd_truthy(const json_t *v) {
    if (jd_is_none(v)) return 0;
    switch (v->type) {
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_OBJECT: return json_object_size(v) > 0;   /* json_len() is array-only */
    default: return json_len(v) > 0;
    }
}

/* utils.is_truthy_value(value, default=False). */
static int jd_truthy_value(const json_t *v) {
    if (jd_is_none(v)) return 0;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n, i;
        char *low;
        int hit;
        while (*s && isspace((unsigned char)*s)) s++;
        n = strlen(s);
        while (n && isspace((unsigned char)s[n - 1])) n--;
        low = (char *)malloc(n + 1);
        if (!low) return 0;
        for (i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)s[i]);
        low[n] = '\0';
        hit = !strcmp(low, "1") || !strcmp(low, "true") || !strcmp(low, "yes") || !strcmp(low, "on");
        free(low);
        return hit;
    }
    return jd_truthy(v);
}

/* Object/string getters that treat an explicit null like an absent key. */
static const json_t *jd_get(const json_t *req, const char *key) {
    const json_t *v = (req && req->type == JSON_OBJECT) ? json_obj_get(req, key) : NULL;
    return jd_is_none(v) ? NULL : v;
}

static const char *jd_get_text(const json_t *req, const char *key) {
    const json_t *v = jd_get(req, key);
    if (!v || v->type != JSON_STRING || !v->str_val) return NULL;
    return v->str_val;
}

static double jd_num_of(const json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* Python int(text): optional sign, digits with single underscores between
 * digits, optional surrounding whitespace.  Returns 1 on success. */
static int jd_py_int(const char *text, long long *out) {
    const char *s = text ? text : "";
    const char *p;
    int neg = 0, digits = 0, prev_digit = 0;
    long long acc = 0;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    p = s;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            if (acc > (9223372036854775807LL - (*p - '0')) / 10) return 0;
            acc = acc * 10 + (*p - '0');
            digits++;
            prev_digit = 1;
            p++;
        } else if (*p == '_' && prev_digit) {
            prev_digit = 0;
            p++;
        } else {
            break;
        }
    }
    if (!digits || !prev_digit) return 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p) return 0;
    *out = neg ? -acc : acc;
    return 1;
}

/* os.path.splitext() of a path: (end of root, start of ext or NULL).  Leading
 * dots of the basename never form an extension. */
static void jd_splitext(const char *path, const char **root_end, const char **ext) {
    const char *base = path ? path : "";
    const char *sep = NULL, *dot = NULL, *q;
    size_t i;
    for (i = 0; base[i]; i++) {
        if (base[i] == '/' || base[i] == '\\') sep = base + i;
        if (base[i] == '.') dot = base + i;
    }
    if (dot && (!sep || dot > sep)) {
        for (q = (sep ? sep + 1 : base); q < dot; q++) {
            if (*q != '.') { *root_end = dot; *ext = dot; return; }
        }
    }
    *root_end = base + strlen(base);
    *ext = NULL;
}

/* Lowercased extension of *path* ("" when there is none), into buf. */
static size_t jd_ext_lower(const char *path, char *buf, size_t cap) {
    const char *root_end, *ext;
    size_t i = 0;
    jd_splitext(path, &root_end, &ext);
    (void)root_end;
    if (!ext || !*ext) { if (cap) buf[0] = '\0'; return 0; }
    while (ext[i] && i + 1 < cap) { buf[i] = (char)tolower((unsigned char)ext[i]); i++; }
    buf[i] = '\0';
    return i;
}

static const char *jd_basename(const char *path) {
    const char *base = path ? path : "";
    const char *q;
    for (q = base; *q; q++)
        if (*q == '/' || *q == '\\') base = q + 1;
    return base;
}

/* ------------------------------------------------------------------ */
/* SHA-256 + HMAC-SHA256 (token proofs and spawn fingerprints)        */
/* ------------------------------------------------------------------ */

typedef struct { uint32_t h[8]; uint64_t total; unsigned char buf[64]; size_t n; } sha256_ctx;

static const uint32_t SHA256_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u,
};

#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_compress(uint32_t h[8], const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, hh, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROTR32(w[i - 15], 7) ^ ROTR32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROTR32(w[i - 2], 17) ^ ROTR32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; hh = h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = ROTR32(e, 6) ^ ROTR32(e, 11) ^ ROTR32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t S0 = ROTR32(a, 2) ^ ROTR32(a, 13) ^ ROTR32(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        t1 = hh + S1 + ch + SHA256_K[i] + w[i];
        t2 = S0 + mj;
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

static void sha256_init(sha256_ctx *c) {
    c->h[0] = 0x6a09e667u; c->h[1] = 0xbb67ae85u; c->h[2] = 0x3c6ef372u; c->h[3] = 0xa54ff53au;
    c->h[4] = 0x510e527fu; c->h[5] = 0x9b05688cu; c->h[6] = 0x1f83d9abu; c->h[7] = 0x5be0cd19u;
    c->total = 0;
    c->n = 0;
}

static void sha256_update(sha256_ctx *c, const void *data, size_t n) {
    const unsigned char *p = (const unsigned char *)data;
    c->total += (uint64_t)n;
    while (n) {
        size_t take = 64 - c->n;
        if (take > n) take = n;
        memcpy(c->buf + c->n, p, take);
        c->n += take;
        p += take;
        n -= take;
        if (c->n == 64) { sha256_compress(c->h, c->buf); c->n = 0; }
    }
}

static void sha256_final(sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->total * 8;
    unsigned char pad = 0x80;
    unsigned char zero = 0x00;
    unsigned char lenb[8];
    int i;
    sha256_update(c, &pad, 1);
    while (c->n != 56) sha256_update(c, &zero, 1);
    for (i = 0; i < 8; i++) lenb[i] = (unsigned char)((bits >> (56 - 8 * i)) & 0xff);
    sha256_update(c, lenb, 8);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(c->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(c->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(c->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(c->h[i]);
    }
}

static void sha256_digest(const void *data, size_t n, unsigned char out[32]) {
    sha256_ctx c;
    sha256_init(&c);
    sha256_update(&c, data, n);
    sha256_final(&c, out);
}

static void hmac_sha256(const unsigned char *key, size_t keylen,
                        const unsigned char *msg, size_t msglen, unsigned char out[32]) {
    unsigned char k[64], ipad[64], opad[64], inner[32];
    sha256_ctx c;
    size_t i;
    memset(k, 0, sizeof k);
    if (keylen > 64) sha256_digest(key, keylen, k);
    else memcpy(k, key, keylen);
    for (i = 0; i < 64; i++) { ipad[i] = (unsigned char)(k[i] ^ 0x36); opad[i] = (unsigned char)(k[i] ^ 0x5c); }
    sha256_init(&c);
    sha256_update(&c, ipad, 64);
    sha256_update(&c, msg, msglen);
    sha256_final(&c, inner);
    sha256_init(&c);
    sha256_update(&c, opad, 64);
    sha256_update(&c, inner, 32);
    sha256_final(&c, out);
}

static const char HEXDIGITS[] = "0123456789abcdef";

static char *jd_hex(const unsigned char *buf, size_t n) {
    char *s = (char *)malloc(n * 2 + 1);
    size_t i;
    if (!s) return jd_strdup("");
    for (i = 0; i < n; i++) {
        s[i * 2] = HEXDIGITS[(buf[i] >> 4) & 0xf];
        s[i * 2 + 1] = HEXDIGITS[buf[i] & 0xf];
    }
    s[n * 2] = '\0';
    return s;
}

/* ------------------------------------------------------------------ */
/* Gate p1 - token + spawn-config identity                            */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_42b735_p1_safe_ints @ adapter.py:_safe_ints */
json_t *w27_adapter_42b735_p1_safe_ints(const json_t *req) {
    const json_t *tokens = jd_get(req, "tokens");
    json_t *out = json_array();
    size_t i, n = tokens ? json_len(tokens) : 0;
    for (i = 0; i < n; i++) {
        const json_t *tok = json_get(tokens, i);
        if (!tok) continue;
        if (tok->type == JSON_NUMBER) {
            json_append(out, json_number(trunc(tok->num_val)));
        } else if (tok->type == JSON_BOOL) {
            json_append(out, json_number(tok->bool_val ? 1 : 0));
        } else if (tok->type == JSON_STRING) {
            long long v;
            if (jd_py_int(tok->str_val, &v)) json_append(out, json_number((double)v));
        }
    }
    return out;
}

/* PoP: w27_adapter_42b735_p1_auth_headers @ adapter.py:_bridge_auth_headers */
json_t *w27_adapter_42b735_p1_auth_headers(const json_t *req) {
    const char *token = jd_get_text(req, "token");
    json_t *o = json_object();
    char *value = jd_fmt("Bearer %s", token ? token : "");
    json_set(o, "Authorization", json_string(value));
    free(value);
    return o;
}

/* PoP: w27_adapter_42b735_p1_auth_proof @ adapter.py:_bridge_auth_proof */
json_t *w27_adapter_42b735_p1_auth_proof(const json_t *req) {
    const char *token = jd_get_text(req, "token");
    const char *challenge = jd_get_text(req, "challenge");
    unsigned char mac[32];
    char *hex;
    json_t *out;
    if (!token) token = "";
    if (!challenge) challenge = "";
    hmac_sha256((const unsigned char *)token, strlen(token),
                (const unsigned char *)challenge, strlen(challenge), mac);
    hex = jd_hex(mac, 32);
    out = json_string(hex);
    free(hex);
    return out;
}

/* PoP: w27_adapter_42b735_p1_config_fingerprint @ adapter.py:_bridge_config_fingerprint */
json_t *w27_adapter_42b735_p1_config_fingerprint(const json_t *req) {
    const json_t *env = jd_get(req, "env");
    const char *package_hash = jd_get_text(req, "package_hash");
    sha256_ctx ctx;
    unsigned char digest[32];
    char *hex, *short16;
    json_t *out;
    size_t i;

    if (!package_hash) package_hash = "";
    sha256_init(&ctx);
    sha256_update(&ctx, "package.json", 12);
    sha256_update(&ctx, "\x00", 1);
    sha256_update(&ctx, package_hash, strlen(package_hash));
    sha256_update(&ctx, "\x00", 1);
    for (i = 0; i < WA_CONSUMED_ENV_N; i++) {
        const char *key = (i < WA_PASSTHROUGH_ENV_N)
            ? WA_BRIDGE_PASSTHROUGH_ENV[i]
            : WA_BRIDGE_CONSUMED_ENV_EXTRA[i - WA_PASSTHROUGH_ENV_N];
        const json_t *v = (env && env->type == JSON_OBJECT) ? json_obj_get(env, key) : NULL;
        sha256_update(&ctx, key, strlen(key));
        if (v) {
            char *rendered = jd_pystr(v);
            sha256_update(&ctx, "\x01", 1);
            sha256_update(&ctx, rendered, strlen(rendered));
            free(rendered);
        } else {
            sha256_update(&ctx, "\x00", 1);
        }
        sha256_update(&ctx, "\x00", 1);
    }
    sha256_final(&ctx, digest);
    hex = jd_hex(digest, 32);
    short16 = jd_strdup(hex);
    if (short16 && strlen(short16) > 16) short16[16] = '\0';
    out = json_string(short16 ? short16 : "");
    free(hex);
    free(short16);
    return out;
}

/* PoP: w27_adapter_42b735_p1_fmt_uptime @ adapter.py:_fmt_uptime */
json_t *w27_adapter_42b735_p1_fmt_uptime(const json_t *req) {
    const json_t *v = jd_get(req, "seconds");
    long long secs = 0;
    int have = 0;
    if (v && v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == d && d > -9.3e18 && d < 9.3e18) { secs = (long long)d; have = 1; }
    } else if (v && v->type == JSON_BOOL) {
        secs = v->bool_val ? 1 : 0;
        have = 1;
    } else if (v && v->type == JSON_STRING) {
        /* float() accepts digit-group underscores ("1_0" == 10.0): drop the
         * legal separators, and reject any other underscore. */
        const char *src = v->str_val ? v->str_val : "";
        size_t n = strlen(src), k = 0, i;
        char *clean = (char *)malloc(n + 1);
        int bad = 0;
        if (!clean) return json_string("");
        for (i = 0; i < n; i++) {
            if (src[i] == '_') {
                if (i == 0 || i + 1 >= n || !isdigit((unsigned char)src[i - 1]) ||
                    !isdigit((unsigned char)src[i + 1])) { bad = 1; break; }
                continue;
            }
            clean[k++] = src[i];
        }
        clean[k] = '\0';
        if (!bad) {
            char *end = NULL;
            double d = strtod(clean, &end);
            if (end && end != clean) {
                while (*end && isspace((unsigned char)*end)) end++;
                if (!*end && d == d && d > -9.3e18 && d < 9.3e18) { secs = (long long)d; have = 1; }
            }
        }
        free(clean);
    }
    if (!have) return json_string("");
    if (secs < 0) secs = 0;
    {
        long long days = secs / 86400;
        long long rem = secs % 86400;
        long long hours = rem / 3600;
        long long minutes = (rem % 3600) / 60;
        long long seconds = rem % 60;
        char buf[80];
        if (days) snprintf(buf, sizeof buf, "%lldd %lldh", days, hours);
        else if (hours) snprintf(buf, sizeof buf, "%lldh %lldm", hours, minutes);
        else if (minutes) snprintf(buf, sizeof buf, "%lldm", minutes);
        else snprintf(buf, sizeof buf, "%llds", seconds);
        return json_string(buf);
    }
}

json_t *w27_adapter_42b735_p1_dispatch(const char *func, const json_t *req) {
    if (!func) return json_string("UNKNOWN_FUNC");
    if (!strcmp(func, "safe_ints")) return w27_adapter_42b735_p1_safe_ints(req);
    if (!strcmp(func, "auth_headers")) return w27_adapter_42b735_p1_auth_headers(req);
    if (!strcmp(func, "auth_proof")) return w27_adapter_42b735_p1_auth_proof(req);
    if (!strcmp(func, "config_fingerprint")) return w27_adapter_42b735_p1_config_fingerprint(req);
    if (!strcmp(func, "fmt_uptime")) return w27_adapter_42b735_p1_fmt_uptime(req);
    return json_string("UNKNOWN_FUNC");
}

/* ------------------------------------------------------------------ */
/* Gate p2 - lifecycle + endpoint addressing                          */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_42b735_p2_bridge_url @ adapter.py:WhatsAppAdapter._bridge_url */
json_t *w27_adapter_42b735_p2_bridge_url(const json_t *req) {
    const json_t *port = jd_get(req, "port");
    const char *path = jd_get_text(req, "path");
    char *url;
    json_t *out;
    if (!path) path = "";
    url = jd_fmt("http://127.0.0.1:%.0f/%s", jd_num_of(port, 0.0), path);
    out = json_string(url);
    free(url);
    return out;
}

/* PoP: w27_adapter_42b735_p2_pidfile_parse @ adapter.py:_read_bridge_pidfile */
json_t *w27_adapter_42b735_p2_pidfile_parse(const json_t *req) {
    const json_t *v = jd_get(req, "text");
    const char *text = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
    json_t *out = json_array();
    char *copy, *cursor;
    long long vals[3] = {0, 0, 0};
    int have[3] = {0, 0, 0};
    int idx = 0;
    int valid = 1;

    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        text += 3;
    copy = jd_strdup(text);
    if (!copy) return out;
    /* read_text() is universal-newline: CRLF and a lone CR both end a line. */
    {
        char *r = copy, *w = copy;
        while (*r) {
            if (*r == '\r') {
                *w++ = '\n';
                r++;
                if (*r == '\n') r++;
            } else {
                *w++ = *r++;
            }
        }
        *w = '\0';
    }
    cursor = copy;
    while (cursor && idx < 3) {
        char *s = cursor;
        char *nl = strchr(cursor, '\n');
        size_t len;
        if (nl) { *nl = '\0'; cursor = nl + 1; } else cursor = NULL;
        while (*s && isspace((unsigned char)*s)) s++;
        len = strlen(s);
        while (len && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
        if (idx == 0) {
            if (jd_py_int(s, &vals[0])) have[0] = 1;
            else { valid = 0; break; }        /* int(lines[0]) raised -> (None, None, None) */
        } else if (*s) {
            if (jd_py_int(s, &vals[idx])) have[idx] = 1;
            else { valid = 0; break; }        /* int(lines[i]) raised -> (None, None, None) */
        }
        idx++;
    }
    if (!valid || !have[0]) {
        json_append(out, json_null());
        json_append(out, json_null());
        json_append(out, json_null());
    } else {
        int i;
        for (i = 0; i < 3; i++) {
            if (have[i]) json_append(out, json_number((double)vals[i]));
            else json_append(out, json_null());
        }
    }
    free(copy);
    return out;
}

/* PoP: w27_adapter_42b735_p2_bridge_died @ adapter.py:WhatsAppAdapter._bridge_died */
json_t *w27_adapter_42b735_p2_bridge_died(const json_t *req) {
    const json_t *rc = jd_get(req, "rc");
    int has_process = jd_truthy(jd_get(req, "has_process"));
    const char *code = jd_get_text(req, "code");
    const char *detail = jd_get_text(req, "detail");
    const char *log = jd_get_text(req, "log");
    int retryable = 1;
    char *message;
    json_t *out;

    if (!code || !code[0]) code = "whatsapp_bridge_exited";
    if (!detail) detail = "";
    if (has_process && rc && rc->type == JSON_NUMBER && rc->num_val == (double)WA_PASSKEY_REQUIRED_EXIT) {
        code = "whatsapp_passkey_required";
        detail = "WhatsApp requires passkey verification. Run `hermes whatsapp --passkey-browser EXECUTABLE` "
                 "in a local interactive terminal, then restart the gateway.";
        retryable = 0;
    }
    /* f"... (see {self._bridge_log})" -- an unset _bridge_log renders as None. */
    message = jd_fmt("%s (see %s)", detail, log ? log : "None");
    out = json_object();
    json_set(out, "code", json_string(code));
    json_set(out, "detail", json_string(message));
    json_set(out, "retryable", json_bool(retryable));
    free(message);
    return out;
}

/* PoP: w27_adapter_42b735_p2_is_connected @ adapter.py:_is_connected */
json_t *w27_adapter_42b735_p2_is_connected(const json_t *req) {
    const json_t *cfg = jd_get(req, "config");
    const char *env = jd_get_text(req, "env");
    const char *s;
    size_t n, i;
    char *low;
    int hit;

    if (cfg && cfg->type == JSON_OBJECT) {
        int enabled = json_get_bool(cfg, "enabled", 0);
        if (enabled && jd_truthy(json_obj_get(cfg, "extra"))) return json_bool(1);
    }
    /* (value or "").strip().lower() in {"true", "1", "yes"} */
    s = env ? env : "";
    while (*s && isspace((unsigned char)*s)) s++;
    n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    low = (char *)malloc(n + 1);
    if (!low) return json_bool(0);
    for (i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)s[i]);
    low[n] = '\0';
    hit = !strcmp(low, "true") || !strcmp(low, "1") || !strcmp(low, "yes");
    free(low);
    return json_bool(hit);
}

json_t *w27_adapter_42b735_p2_dispatch(const char *func, const json_t *req) {
    if (!func) return json_string("UNKNOWN_FUNC");
    if (!strcmp(func, "bridge_url")) return w27_adapter_42b735_p2_bridge_url(req);
    if (!strcmp(func, "pidfile_parse")) return w27_adapter_42b735_p2_pidfile_parse(req);
    if (!strcmp(func, "bridge_died")) return w27_adapter_42b735_p2_bridge_died(req);
    if (!strcmp(func, "is_connected")) return w27_adapter_42b735_p2_is_connected(req);
    return json_string("UNKNOWN_FUNC");
}

/* ------------------------------------------------------------------ */
/* Gate p3 - inbound shaping                                          */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_42b735_p3_classify_bridge_message @ adapter.py:WhatsAppAdapter._classify_bridge_message */
json_t *w27_adapter_42b735_p3_classify_bridge_message(const json_t *req) {
    const json_t *data = jd_get(req, "data");
    const json_t *mt = (data && data->type == JSON_OBJECT) ? json_obj_get(data, "mediaType") : NULL;
    char *media_type;
    const char *result;

    if (mt && mt->type == JSON_STRING) media_type = jd_strdup(mt->str_val ? mt->str_val : "");
    else if (jd_is_none(mt)) media_type = jd_strdup("");
    else media_type = jd_pystr(mt);

    if (!strcmp(media_type, "location") || !strcmp(media_type, "live_location")) result = "location";
    else if (!strcmp(media_type, "sticker")) result = "sticker";
    else if (!jd_truthy((data && data->type == JSON_OBJECT) ? json_obj_get(data, "hasMedia") : NULL)) result = "text";
    else if (strstr(media_type, "image")) result = "photo";
    else if (strstr(media_type, "video")) result = "video";
    else if (strstr(media_type, "ptt")) result = "voice";
    else if (strstr(media_type, "audio")) result = "audio";
    else result = "document";
    free(media_type);
    return json_string(result);
}

/* PoP: w27_adapter_42b735_p3_may_transcribe_at_gate @ adapter.py:WhatsAppAdapter._may_transcribe_at_gate */
json_t *w27_adapter_42b735_p3_may_transcribe_at_gate(const json_t *req) {
    const json_t *data = jd_get(req, "data");
    const json_t *extra = jd_get(req, "extra");
    const json_t *mt = (data && data->type == JSON_OBJECT) ? json_obj_get(data, "mediaType") : NULL;
    const char *media_type = (mt && mt->type == JSON_STRING && mt->str_val) ? mt->str_val : "";
    int gate;

    if (!jd_truthy((data && data->type == JSON_OBJECT) ? json_obj_get(data, "isGroup") : NULL)) return json_bool(0);
    if (strcmp(media_type, "ptt") != 0) return json_bool(0);
    gate = jd_truthy_value((extra && extra->type == JSON_OBJECT)
                           ? json_obj_get(extra, "voice_mention_patterns") : extra);
    return json_bool(gate);
}

/* _TEXT_INJECT_EXTS membership for a path. */
static int wa_is_text_inject_ext(const char *name) {
    char ext[32];
    size_t n = jd_ext_lower(name, ext, sizeof ext);
    size_t i;
    if (!n) return 0;
    for (i = 0; i < WA_TEXT_INJECT_EXTS_N; i++)
        if (!strcmp(ext, WA_TEXT_INJECT_EXTS[i])) return 1;
    return 0;
}

/* PoP: w27_adapter_42b735_p3_inject_document_text @ adapter.py:WhatsAppAdapter._inject_document_text */
json_t *w27_adapter_42b735_p3_inject_document_text(const json_t *req) {
    const json_t *docs = jd_get(req, "docs");
    const char *body_in = jd_get_text(req, "body");
    char *body = jd_strdup(body_in ? body_in : "");
    json_t *flags = json_array();
    json_t *out = json_array();
    size_t i, n = docs ? json_len(docs) : 0;

    for (i = 0; i < n; i++) {
        const json_t *doc = json_get(docs, i);
        const char *name = doc ? jd_get_text(doc, "name") : NULL;
        const char *content = doc ? jd_get_text(doc, "content") : NULL;
        const json_t *size_v = (doc && doc->type == JSON_OBJECT) ? json_obj_get(doc, "size") : NULL;
        double size;
        size_t raw_len;
        char *display, *injection, *next;

        if (!name) name = "";
        if (!content) content = "";
        if (!wa_is_text_inject_ext(name)) { json_append(flags, json_bool(0)); continue; }
        raw_len = strlen(content);
        size = size_v ? jd_num_of(size_v, (double)raw_len) : (double)raw_len;
        if (size > (double)WA_MAX_TEXT_INJECT_BYTES) { json_append(flags, json_bool(0)); continue; }
        /* read_text(encoding="utf-8-sig") drops a leading BOM. */
        if ((unsigned char)content[0] == 0xEF && (unsigned char)content[1] == 0xBB &&
            (unsigned char)content[2] == 0xBF) content += 3;

        /* p.name.split("_", 2) strips the doc_<hex>_ prefix for display. */
        {
            const char *base = jd_basename(name);
            char *cur = jd_strdup(base);
            char *parts[3] = {NULL, NULL, NULL};
            int count = 0;
            if (cur) {
                char *rest = cur;
                char *u;
                while (count < 2 && (u = strchr(rest, '_')) != NULL) {
                    *u = '\0';
                    parts[count++] = rest;
                    rest = u + 1;
                }
                parts[count++] = rest;
                display = jd_strdup((count >= 3 && parts[2]) ? parts[2] : base);
                free(cur);
            } else {
                display = jd_strdup(base);
            }
        }
        injection = jd_fmt("[Content of %s]:\n%s", display ? display : "", content);
        next = (body && *body) ? jd_fmt("%s\n\n%s", injection, body) : jd_strdup(injection);
        free(body);
        body = next;
        free(injection);
        free(display);
        json_append(flags, json_bool(1));
    }
    json_append(out, json_string(body ? body : ""));
    json_append(out, flags);
    free(body);
    return out;
}

/* PoP: w27_adapter_42b735_p3_delivery_status_line @ adapter.py:WhatsAppAdapter._log_delivery_status */
json_t *w27_adapter_42b735_p3_delivery_status_line(const json_t *req) {
    const json_t *data = jd_get(req, "data");
    const char *name = jd_get_text(req, "name");
    int is_obj = data && data->type == JSON_OBJECT;
    const json_t *rid = is_obj ? json_obj_get(data, "recipientId") : NULL;
    const json_t *err = is_obj ? json_obj_get(data, "error") : NULL;
    char *mid = jd_pystr(is_obj ? json_obj_get(data, "messageId") : NULL);
    char *chat = jd_pystr(is_obj ? json_obj_get(data, "chatId") : NULL);
    char *status = jd_pystr(is_obj ? json_obj_get(data, "status") : NULL);
    char *ts = jd_pystr(is_obj ? json_obj_get(data, "timestamp") : NULL);
    char *recipient, *error, *line;
    json_t *out;

    if (!name) name = "";
    if (jd_truthy(rid)) {
        char *r = jd_pystr(rid);
        recipient = jd_fmt(" recipient=%s", r ? r : "");
        free(r);
    } else {
        recipient = jd_strdup("");
    }
    if (jd_truthy(err)) {
        char *e = jd_pystr(err);
        error = jd_fmt(" error=%s", e ? e : "");
        free(e);
    } else {
        error = jd_strdup("");
    }
    line = jd_fmt("[%s] WhatsApp delivery status: message=%s chat=%s status=%s%s at=%s%s",
                  name, mid ? mid : "", chat ? chat : "", status ? status : "",
                  recipient ? recipient : "", ts ? ts : "", error ? error : "");
    out = json_string(line);
    free(mid); free(chat); free(status); free(ts); free(recipient); free(error); free(line);
    return out;
}

json_t *w27_adapter_42b735_p3_dispatch(const char *func, const json_t *req) {
    if (!func) return json_string("UNKNOWN_FUNC");
    if (!strcmp(func, "classify_bridge_message")) return w27_adapter_42b735_p3_classify_bridge_message(req);
    if (!strcmp(func, "may_transcribe_at_gate")) return w27_adapter_42b735_p3_may_transcribe_at_gate(req);
    if (!strcmp(func, "inject_document_text")) return w27_adapter_42b735_p3_inject_document_text(req);
    if (!strcmp(func, "delivery_status_line")) return w27_adapter_42b735_p3_delivery_status_line(req);
    return json_string("UNKNOWN_FUNC");
}

/* ------------------------------------------------------------------ */
/* Gate p4 - outbound shaping + constant tables                       */
/* ------------------------------------------------------------------ */

/* _WA_EXT_MEDIA_TYPE: extension -> bridge mediaType. */
static const char *wa_ext_media_type(const char *ext) {
    static const char *const images[] = {".jpg", ".jpeg", ".png", ".webp", ".gif", NULL};
    static const char *const videos[] = {".mp4", ".mov", ".avi", ".mkv", ".webm", ".3gp", NULL};
    static const char *const audios[] = {".ogg", ".opus", ".mp3", ".wav", ".m4a", ".flac", NULL};
    int i;
    if (!ext || !*ext) return "document";
    for (i = 0; images[i]; i++) if (!strcmp(ext, images[i])) return "image";
    for (i = 0; videos[i]; i++) if (!strcmp(ext, videos[i])) return "video";
    for (i = 0; audios[i]; i++) if (!strcmp(ext, audios[i])) return "audio";
    return "document";
}

/* PoP: w27_adapter_42b735_p4_bridge_media_type @ adapter.py:_bridge_media_type */
json_t *w27_adapter_42b735_p4_bridge_media_type(const json_t *req) {
    const char *path = jd_get_text(req, "file_path");
    int is_voice = jd_truthy(jd_get(req, "is_voice"));
    int force_document = jd_truthy(jd_get(req, "force_document"));
    char ext[32];
    if (force_document) return json_string("document");
    if (is_voice) return json_string("audio");
    jd_ext_lower(path ? path : "", ext, sizeof ext);
    return json_string(wa_ext_media_type(ext));
}

/* gateway.whatsapp_identity.to_whatsapp_jid, inlined. */
static char *wa_to_jid(const char *value) {
    char *s;
    const char *v = value ? value : "";
    size_t n, a = 0, b;
    if (!*v) return jd_strdup("");
    n = strlen(v);
    b = n;
    while (a < n && isspace((unsigned char)v[a])) a++;
    while (b > a && isspace((unsigned char)v[b - 1])) b--;
    s = (char *)malloc(b - a + 1);
    if (!s) return jd_strdup("");
    memcpy(s, v + a, b - a);
    s[b - a] = '\0';
    if (!*s) return s;
    if (strchr(s, ':') && strchr(s, '@')) {
        char *at = strchr(s, '@');
        char *colon = strchr(s, ':');
        char *out = jd_fmt("%.*s@%s", (int)(colon - s), s, at + 1);
        free(s);
        return out;
    }
    if (strchr(s, '@')) return s;
    {
        /* _BARE_PHONE_RE: ^\+?[\d\s().\-]+$ */
        size_t i = 0;
        int ok = 1, digits = 0;
        char *out, *d;
        if (s[i] == '+') i++;
        if (!s[i]) ok = 0;
        for (; s[i]; i++) {
            unsigned char c = (unsigned char)s[i];
            if (c >= '0' && c <= '9') { digits++; continue; }
            if (c == ' ' || c == '(' || c == ')' || c == '.' || c == '-' ||
                c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') continue;
            ok = 0;
            break;
        }
        if (!ok || !digits) return s;
        out = (char *)malloc((size_t)digits + strlen("@s.whatsapp.net") + 1);
        if (!out) return s;
        d = out;
        for (i = 0; s[i]; i++)
            if (s[i] >= '0' && s[i] <= '9') *d++ = s[i];
        strcpy(d, "@s.whatsapp.net");
        free(s);
        return out;
    }
}

/* gateway.whatsapp_identity.normalize_whatsapp_mention_jid, inlined. */
static char *wa_mention_jid(const char *value) {
    char *jid = wa_to_jid(value);
    char *at, *out = NULL;
    if (!jid) return jd_strdup("");
    at = strchr(jid, '@');
    if (at && at != jid) {
        size_t i;
        int user_ok = 1;
        for (i = 0; i < (size_t)(at - jid); i++)
            if (jid[i] < '0' || jid[i] > '9') { user_ok = 0; break; }
        if (user_ok && (!strcmp(at + 1, "s.whatsapp.net") || !strcmp(at + 1, "lid"))) out = jd_strdup(jid);
    }
    free(jid);
    return out ? out : jd_strdup("");
}

/* PoP: w27_adapter_42b735_p4_normalize_outbound_mentions @ adapter.py:_normalize_outbound_mentions */
json_t *w27_adapter_42b735_p4_normalize_outbound_mentions(const json_t *req) {
    const json_t *mentions = jd_get(req, "mentions");
    json_t *out = json_array();
    size_t i, n = mentions ? json_len(mentions) : 0;
    for (i = 0; i < n; i++) {
        const json_t *m = json_get(mentions, i);
        char *raw_owned, *jid;
        const char *raw;
        size_t j;
        int dup = 0;
        if (!m) continue;
        if (m->type == JSON_STRING) {
            raw_owned = NULL;
            raw = m->str_val ? m->str_val : "";
        } else {
            /* to_whatsapp_jid(str(value)): a number/bool mention still normalizes. */
            raw_owned = jd_pystr(m);
            raw = raw_owned ? raw_owned : "";
        }
        jid = wa_mention_jid(raw);
        free(raw_owned);
        if (!jid || !*jid) { free(jid); continue; }
        for (j = 0; j < json_len(out); j++) {
            const json_t *seen = json_get(out, j);
            if (seen && seen->type == JSON_STRING && seen->str_val && !strcmp(seen->str_val, jid)) {
                dup = 1;
                break;
            }
        }
        if (!dup) json_append(out, json_string(jid));
        free(jid);
    }
    return out;
}

/* PoP: w27_adapter_42b735_p4_constants @ adapter.py:<module> (bridge-facing tables) */
json_t *w27_adapter_42b735_p4_constants(void) {
    json_t *o = json_object();
    json_t *arr;
    size_t i;

    json_set(o, "passkey_required_exit", json_number(WA_PASSKEY_REQUIRED_EXIT));
    json_set(o, "owner_reply_prefix", json_string(WA_OWNER_REPLY_PREFIX));
    json_set(o, "max_text_inject_bytes", json_number(WA_MAX_TEXT_INJECT_BYTES));
    json_set(o, "split_threshold", json_number(WA_SPLIT_THRESHOLD));

    arr = json_array();
    for (i = 0; i < WA_TEXT_INJECT_EXTS_N; i++) json_append(arr, json_string(WA_TEXT_INJECT_EXTS[i]));
    json_set(o, "text_inject_exts", arr);

    arr = json_array();
    for (i = 0; i < WA_PASSTHROUGH_ENV_N; i++) json_append(arr, json_string(WA_BRIDGE_PASSTHROUGH_ENV[i]));
    json_set(o, "bridge_passthrough_env", arr);

    arr = json_array();
    for (i = 0; i < WA_CONSUMED_ENV_N; i++)
        json_append(arr, json_string(i < WA_PASSTHROUGH_ENV_N
            ? WA_BRIDGE_PASSTHROUGH_ENV[i]
            : WA_BRIDGE_CONSUMED_ENV_EXTRA[i - WA_PASSTHROUGH_ENV_N]));
    json_set(o, "bridge_consumed_env", arr);

    {
        json_t *m = json_object();
        json_set(m, "location", json_string("location"));
        json_set(m, "live_location", json_string("location"));
        json_set(m, "sticker", json_string("sticker"));
        json_set(o, "native_media_types", m);
    }
    {
        json_t *a = json_array();
        static const char *const needles[] = {"image", "video", "ptt", "audio"};
        static const char *const kinds[] = {"photo", "video", "voice", "audio"};
        for (i = 0; i < 4; i++) {
            json_t *row = json_array();
            json_append(row, json_string(needles[i]));
            json_append(row, json_string(kinds[i]));
            json_append(a, row);
        }
        json_set(o, "media_needles", a);
    }
    {
        json_t *m = json_object();
        static const char *const keys[] = {"photo", "voice", "audio", "video", "document"};
        static const char *const labels[] = {"image", "audio", "audio", "video", "document"};
        static const char *const mimes[] = {"image/jpeg", "audio/ogg", "audio/mpeg", "video/mp4", ""};
        for (i = 0; i < 5; i++) {
            json_t *row = json_array();
            json_append(row, json_string(labels[i]));
            json_append(row, json_string(mimes[i]));
            json_set(m, keys[i], row);
        }
        json_set(o, "media_info", m);
    }
    {
        json_t *m = json_object();
        json_set(m, "image", json_string("photo"));
        json_set(m, "video", json_string("video"));
        json_set(m, "audio", json_string("voice"));
        json_set(m, "document", json_string("document"));
        json_set(o, "media_type_by_bridge_kind", m);
    }
    {
        json_t *m = json_object();
        static const char *const keys[] = {"image", "video", "gif", "audio", "ptt", "document", "sticker"};
        static const char *const vals[] = {"image/jpeg", "video/mp4", "video/mp4", "audio/ogg",
                                           "audio/ogg", "application/octet-stream", "image/webp"};
        for (i = 0; i < 7; i++) json_set(m, keys[i], json_string(vals[i]));
        json_set(o, "quoted_mime_by_bridge_kind", m);
    }
    {
        json_t *m = json_object();
        static const char *const ext[] = {".jpg", ".jpeg", ".png", ".webp", ".gif",
                                          ".mp4", ".mov", ".avi", ".mkv", ".webm", ".3gp",
                                          ".ogg", ".opus", ".mp3", ".wav", ".m4a", ".flac"};
        for (i = 0; i < sizeof ext / sizeof ext[0]; i++)
            json_set(m, ext[i], json_string(wa_ext_media_type(ext[i])));
        json_set(o, "wa_ext_media_type", m);
    }
    {
        json_t *a = json_array();
        static const char *const rows[][3] = {
            {"require_mention", "WHATSAPP_REQUIRE_MENTION", "lower"},
            {"dm_policy", "WHATSAPP_DM_POLICY", "lower"},
            {"group_policy", "WHATSAPP_GROUP_POLICY", "lower"},
            {"mention_patterns", "WHATSAPP_MENTION_PATTERNS", "json"},
            {"free_response_chats", "WHATSAPP_FREE_RESPONSE_CHATS", "csv"},
            {"allow_from", "WHATSAPP_ALLOWED_USERS", "csv"},
            {"group_allow_from", "WHATSAPP_GROUP_ALLOWED_USERS", "csv"},
        };
        for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
            json_t *row = json_array();
            json_append(row, json_string(rows[i][0]));
            json_append(row, json_string(rows[i][1]));
            json_append(row, json_string(rows[i][2]));
            json_append(a, row);
        }
        json_set(o, "yaml_bridge", a);
    }
    return o;
}

json_t *w27_adapter_42b735_p4_dispatch(const char *func, const json_t *req) {
    if (!func) return json_string("UNKNOWN_FUNC");
    if (!strcmp(func, "bridge_media_type")) return w27_adapter_42b735_p4_bridge_media_type(req);
    if (!strcmp(func, "normalize_outbound_mentions")) return w27_adapter_42b735_p4_normalize_outbound_mentions(req);
    if (!strcmp(func, "constants")) return w27_adapter_42b735_p4_constants();
    return json_string("UNKNOWN_FUNC");
}

