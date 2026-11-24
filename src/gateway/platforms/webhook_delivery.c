/*
 * webhook_delivery.c -- real C11 port of the destination rules of upstream
 * Hermes gateway/platforms/webhook_delivery.py.
 *
 * PoP: w22_webhookdel_ead437_validate_destination @ gateway/platforms/webhook_delivery.py:validate_destination
 * PoP: w22_webhookdel_ead437_snapshot_destination @ gateway/platforms/webhook_delivery.py:snapshot_destination
 * PoP: w22_webhookdel_ead437_route_digest @ gateway/platforms/webhook_delivery.py:route_digest
 *
 * Seams, all injected by the caller: the adapter's dynamic-route reload and
 * lookup (the route object arrives resolved), the home-channel lookup
 * (`home_chat_id`), `platform_registry`-registered plugin platform names
 * (`extra_platforms`), and the admission readback that `retained_destination`
 * performs (authority store, envelope restore, adapter identity) -- that
 * function stays upstream.
 *
 * The digest is self-contained SHA-256 so the gate does not depend on a
 * particular crypto object being linked.
 */
#include "hermes_json.h"
#include "noc_w22_webhookdel_ead437.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- minimal SHA-256 (same construction as src/gateway/bridged_env.c) */
typedef struct {
    uint32_t h[8];
    uint64_t bitlen;
    unsigned char buf[64];
    size_t buflen;
} wh_sha_ctx;

static const uint32_t WH_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
#define WH_ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))

static void wh_sha_block(wh_sha_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    uint32_t a, b, cc, d, e, f, g, hh;
    int i;
    for (i = 0; i < 16; ++i)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (i = 16; i < 64; ++i) {
        uint32_t s0 = WH_ROR(w[i-15],7) ^ WH_ROR(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = WH_ROR(w[i-2],17) ^ WH_ROR(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a=c->h[0]; b=c->h[1]; cc=c->h[2]; d=c->h[3]; e=c->h[4]; f=c->h[5]; g=c->h[6]; hh=c->h[7];
    for (i = 0; i < 64; ++i) {
        uint32_t S1 = WH_ROR(e,6) ^ WH_ROR(e,11) ^ WH_ROR(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + WH_K[i] + w[i];
        uint32_t S0 = WH_ROR(a,2) ^ WH_ROR(a,13) ^ WH_ROR(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=hh;
}

static void wh_sha_init(wh_sha_ctx *c) {
    c->h[0]=0x6a09e667u; c->h[1]=0xbb67ae85u; c->h[2]=0x3c6ef372u; c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu; c->h[5]=0x9b05688cu; c->h[6]=0x1f83d9abu; c->h[7]=0x5be0cd19u;
    c->bitlen = 0;
    c->buflen = 0;
}

static void wh_sha_update(wh_sha_ctx *c, const unsigned char *data, size_t n) {
    c->bitlen += (uint64_t)n * 8;
    while (n) {
        size_t take = 64 - c->buflen;
        if (take > n) take = n;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take;
        data += take;
        n -= take;
        if (c->buflen == 64) { wh_sha_block(c, c->buf); c->buflen = 0; }
    }
}

static void wh_sha_final(wh_sha_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->bitlen;
    unsigned char pad = 0x80;
    unsigned char zero = 0;
    unsigned char lenb[8];
    int i;
    wh_sha_update(c, &pad, 1);
    while (c->buflen != 56) wh_sha_update(c, &zero, 1);
    for (i = 0; i < 8; ++i) lenb[i] = (unsigned char)(bits >> (56 - 8*i));
    wh_sha_update(c, lenb, 8);
    for (i = 0; i < 8; ++i) {
        out[i*4]   = (unsigned char)(c->h[i] >> 24);
        out[i*4+1] = (unsigned char)(c->h[i] >> 16);
        out[i*4+2] = (unsigned char)(c->h[i] >> 8);
        out[i*4+3] = (unsigned char)c->h[i];
    }
}

static char *wh_sha256_hex(const unsigned char *data, size_t n) {
    static const char hx[] = "0123456789abcdef";
    wh_sha_ctx c;
    unsigned char dig[32];
    char *out;
    int i;
    wh_sha_init(&c);
    wh_sha_update(&c, data, n);
    wh_sha_final(&c, dig);
    out = (char *)malloc(65);
    if (!out) return NULL;
    for (i = 0; i < 32; ++i) {
        out[i*2] = hx[dig[i] >> 4];
        out[i*2+1] = hx[dig[i] & 15];
    }
    out[64] = '\0';
    return out;
}

/* ---------- helpers ---------- */

#define WH_MAX_KEYS 32

static int wh_in_list(const char *const *list, const char *key) {
    int i;
    for (i = 0; list[i]; i++) {
        if (!strcmp(list[i], key)) return 1;
    }
    return 0;
}

static const char *const WH_ALLOWED_KEYS[] = {
    "deliver", "deliver_extra", "profile", "mirror", "route", NULL
};

/* `isinstance(x, str)` for a decoded value. */
static int wh_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int wh_is_dict(const json_t *v) { return v && v->type == JSON_OBJECT; }

/* str(value) for the scalar shapes `str(number).isdigit()` / `int(number)` see. */
static char *wh_py_str(const json_t *v) {
    char buf[64];
    const char *src;
    size_t n;
    char *out;
    if (!v || v->type == JSON_NULL) { src = "None"; }
    else if (v->type == JSON_BOOL) { src = v->bool_val ? "True" : "False"; }
    else if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        src = buf;
    } else if (v->type == JSON_STRING) { src = v->str_val ? v->str_val : ""; }
    else { src = ""; }
    n = strlen(src);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, src, n + 1);
    return out;
}

/* `str(x).isdigit()` over ASCII digits (non-empty). */
static int wh_all_digits(const char *s) {
    size_t i;
    if (!s || !s[0]) return 0;
    for (i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
    }
    return 1;
}

static long long wh_digits_ll(const char *s) {
    long long acc = 0;
    size_t i;
    for (i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') break;
        if (acc > (9223372036854775807LL - (s[i] - '0')) / 10LL) return -1;
        acc = acc * 10 + (s[i] - '0');
    }
    return acc;
}

/* `_REPO_RE.fullmatch(r"[A-Za-z0-9._-]+/[A-Za-z0-9._-]+")` */
static int wh_repo_matches(const char *s) {
    size_t i, slash = 0, before = 0, after = 0;
    if (!s) return 0;
    for (i = 0; s[i]; i++) {
        char c = s[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                 c == '.' || c == '_' || c == '-';
        if (c == '/') {
            slash++;
            if (slash > 1) return 0;
            continue;
        }
        if (!ok) return 0;
        if (slash == 0) before++;
        else after++;
    }
    return slash == 1 && before > 0 && after > 0;
}

/* `_is_known_platform(target)`: builtin set plus plugin-registered names. */
static int wh_known_platform(const char *target, const json_t *extra_platforms) {
    static const char *const BUILTIN[] = {
        "telegram", "discord", "slack", "signal", "sms", "whatsapp", "matrix", "mattermost",
        "email", "dingtalk", "feishu", "wecom", "wecom_callback", "weixin",
        "bluebubbles", "qqbot", NULL
    };
    size_t i;
    if (!target) return 0;
    if (wh_in_list(BUILTIN, target)) return 1;
    if (extra_platforms && extra_platforms->type == JSON_ARRAY) {
        for (i = 0; i < extra_platforms->c.count; i++) {
            const json_t *v = extra_platforms->c.items[i];
            if (wh_is_str(v) && v->str_val && !strcmp(v->str_val, target)) return 1;
        }
    }
    return 0;
}

static json_t *wh_refusal(const char *reason) { return json_string(reason); }

/* ---------- validate_destination ---------- */

json_t *w22_webhookdel_ead437_validate_destination(const json_t *delivery,
                                                   const json_t *extra_platforms) {
    const json_t *target_v, *extra, *profile_v, *mirror_v, *route_v;
    const char *target;
    json_t *result, *kept;
    size_t i;

    if (!wh_is_dict(delivery)) return wh_refusal("invalid_params");
    if (!json_obj_get(delivery, "deliver") || !json_obj_get(delivery, "deliver_extra")) {
        return wh_refusal("invalid_params");           /* the two required keys */
    }
    for (i = 0; i < json_object_size(delivery); i++) {
        const char *k = json_object_get_key_at(delivery, i);
        if (!k) continue;
        if (!wh_in_list(WH_ALLOWED_KEYS, k)) return wh_refusal("invalid_params");
    }
    profile_v = json_obj_get(delivery, "profile");
    if (profile_v && profile_v->type != JSON_NULL && !wh_is_str(profile_v)) return wh_refusal("invalid_params");
    mirror_v = json_obj_get(delivery, "mirror");
    if (mirror_v && mirror_v->type != JSON_BOOL) return wh_refusal("invalid_params");
    route_v = json_obj_get(delivery, "route");
    if (route_v && route_v->type != JSON_NULL && !wh_is_str(route_v)) return wh_refusal("invalid_params");

    target_v = json_obj_get(delivery, "deliver");
    extra = json_obj_get(delivery, "deliver_extra");
    if (!wh_is_str(target_v) || !wh_is_dict(extra)) return wh_refusal("invalid_params");
    target = target_v->str_val ? target_v->str_val : "";

    if (!strcmp(target, "github_comment")) {
        const json_t *repo = json_obj_get(extra, "repo");
        const json_t *number = json_obj_get(extra, "pr_number");
        char *num_text = wh_py_str(number);
        int ok = wh_is_str(repo) && wh_repo_matches(repo->str_val) && wh_all_digits(num_text) &&
                 wh_digits_ll(num_text) > 0;
        free(num_text);
        if (!ok) return wh_refusal("invalid_params");
    } else if (strcmp(target, "log") && !wh_known_platform(target, extra_platforms)) {
        return wh_refusal("invalid_params");
    }

    result = json_object();
    json_set(result, "deliver", json_string(target));
    kept = json_object();
    if (!strcmp(target, "log")) {
        /* fields = () */
    } else if (!strcmp(target, "github_comment")) {
        static const char *const FIELDS[] = {"repo", "pr_number", NULL};
        for (i = 0; FIELDS[i]; i++) {
            const json_t *v = json_obj_get(extra, FIELDS[i]);
            if (v) json_set(kept, FIELDS[i], json_copy(v));
        }
    } else {
        static const char *const FIELDS[] = {"chat_id", "message_thread_id", "thread_id", NULL};
        for (i = 0; FIELDS[i]; i++) {
            const json_t *v = json_obj_get(extra, FIELDS[i]);
            if (v) json_set(kept, FIELDS[i], json_copy(v));
        }
    }
    json_set(result, "deliver_extra", kept);

    if (profile_v && wh_is_str(profile_v) && profile_v->str_val && profile_v->str_val[0]) {
        json_set(result, "profile", json_copy(profile_v));
    }
    if (mirror_v && mirror_v->type == JSON_BOOL && mirror_v->bool_val) {
        json_set(result, "mirror", json_bool(true));
        if (route_v && route_v->type == JSON_STRING && route_v->str_val && route_v->str_val[0]) {
            json_set(result, "route", json_copy(route_v));
        }
    }
    return result;
}

/* ---------- snapshot_destination ---------- */

json_t *w22_webhookdel_ead437_snapshot_destination(const json_t *delivery,
                                                   const json_t *extra_platforms,
                                                   const char *home_chat_id) {
    json_t *result = w22_webhookdel_ead437_validate_destination(delivery, extra_platforms);
    const char *target;
    json_t *extra;

    if (!result || result->type != JSON_OBJECT) return result;   /* the refusal rides through */
    target = json_get_str(result, "deliver", "");
    extra = json_obj_get(result, "deliver_extra");
    if (strcmp(target, "log") && strcmp(target, "github_comment")) {
        const json_t *chat = extra ? json_obj_get(extra, "chat_id") : NULL;
        int has_chat = chat && chat->type == JSON_STRING && chat->str_val && chat->str_val[0];
        if (!has_chat) {
            if (!home_chat_id) {                                     /* no home channel */
                json_free(result);
                return wh_refusal("not_found");
            }
            json_set(extra, "chat_id", json_string(home_chat_id));
        }
    }
    return result;
}

/* ---------- route_digest ---------- */

/* json.dumps(sort_keys=True): order every object's keys by codepoint. */
static void wh_sort_keys(json_t *v) {
    size_t i, j;
    if (!v) return;
    if (v->type == JSON_OBJECT) {
        for (i = 0; i + 1 < v->c.count; i++) {
            for (j = 0; j + 1 < v->c.count - i; j++) {
                if (strcmp(v->c.keys[j], v->c.keys[j + 1]) > 0) {
                    char *tk = v->c.keys[j];
                    json_t *tv = v->c.items[j];
                    v->c.keys[j] = v->c.keys[j + 1];
                    v->c.items[j] = v->c.items[j + 1];
                    v->c.keys[j + 1] = tk;
                    v->c.items[j + 1] = tv;
                }
            }
        }
        for (i = 0; i < v->c.count; i++) wh_sort_keys(v->c.items[i]);
        return;
    }
    if (v->type == JSON_ARRAY) {
        for (i = 0; i < v->c.count; i++) wh_sort_keys(v->c.items[i]);
    }
}

char *w22_webhookdel_ead437_route_digest(const json_t *route) {
    json_t *copy;
    char *text, *hex;
    if (!route || route->type == JSON_NULL) return NULL;     /* route is None: not_found */
    copy = json_copy(route);
    if (!copy) return NULL;
    wh_sort_keys(copy);
    text = json_serialize(copy);                              /* compact, separators (",", ":") */
    json_free(copy);
    if (!text) return NULL;
    hex = wh_sha256_hex((const unsigned char *)text, strlen(text));
    free(text);
    return hex;
}
