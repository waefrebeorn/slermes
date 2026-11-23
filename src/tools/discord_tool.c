/*
 * discord_tool.c — real C11 port of upstream Hermes tools/discord_tool.py.
 *
 * PoP: discordtool_validate_snowflake @ tools/discord_tool.py:_validate_snowflake
 * PoP: discordtool_channel_type_name @ tools/discord_tool.py:_channel_type_name
 * PoP: discordtool_limit_param @ tools/discord_tool.py:_limit_param
 * PoP: discordtool_enrich_403 @ tools/discord_tool.py:_enrich_403
 * PoP: discordtool_token_cache_key @ tools/discord_tool.py:_token_cache_key
 * PoP: discordtool_available_actions @ tools/discord_tool.py:_available_actions
 *
 * Only the pure parsing/formatting/filtering helpers are ported; REST calls,
 * disk capability caches and background detection are runtime concerns.
 */
#include "noc_w6_discordtool_3ef112.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int snowflake_ok(const char *v) {
    size_t n, i;
    if (!v) return 0;
    n = strlen(v);
    if (n < 1 || n > 20) return 0;
    for (i = 0; i < n; i++) if (v[i] < '0' || v[i] > '9') return 0;
    return 1;
}

/* Python repr() of a str (single-quoted, backslash and quote escaped). */
static char *py_repr(const char *s) {
    size_t n = strlen(s ? s : ""), cap = 2 * n + 3, k = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    out[k++] = '\'';
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\'' || c == '\\') out[k++] = '\\';
        out[k++] = c;
    }
    out[k++] = '\'';
    out[k] = '\0';
    return out;
}

json_t *discordtool_validate_snowflake(const char *value, const char *param_name) {
    char *repr, *buf;
    json_t *r;
    if (snowflake_ok(value)) return json_null();
    repr = py_repr(value ? value : "");
    buf = (char *)malloc(strlen(repr ? repr : "") + strlen(param_name ? param_name : "") + 128);
    if (!repr || !buf) { free(repr); free(buf); return json_string(""); }
    sprintf(buf, "Invalid Discord %s: %s (expected 1-20 digit numeric string)",
            param_name ? param_name : "", repr);
    r = json_string(buf);
    free(repr); free(buf);
    return r;
}

json_t *discordtool_channel_type_name(const char *type_id) {
    long long id = strtoll(type_id ? type_id : "0", NULL, 10);
    const char *name = NULL;
    char buf[64];
    switch (id) {
        case 0: name = "text"; break;
        case 2: name = "voice"; break;
        case 4: name = "category"; break;
        case 5: name = "announcement"; break;
        case 10: name = "announcement_thread"; break;
        case 11: name = "public_thread"; break;
        case 12: name = "private_thread"; break;
        case 13: name = "stage"; break;
        case 15: name = "forum"; break;
        case 16: name = "media"; break;
        default: break;
    }
    if (name) return json_string(name);
    sprintf(buf, "unknown(%lld)", id);
    return json_string(buf);
}

/* int() of a decimal string with Python's whitespace/sign tolerance. */
static int py_int(const char *s, long long *out) {
    const char *p = s ? s : "";
    int sign = 1;
    long long v = 0;
    int any = 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') { if (*p == '-') sign = -1; p++; }
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); any = 1; p++; }
    while (*p && isspace((unsigned char)*p)) p++;
    if (!any || *p) return 0;
    *out = sign * v;
    return 1;
}

json_t *discordtool_limit_param(const char *limit, const char *default_value) {
    long long v;
    char buf[64];
    if (!py_int(limit, &v)) {
        if (!py_int(default_value, &v)) v = 50;
    }
    if (v > 100) v = 100;
    sprintf(buf, "%lld", v);
    return json_string(buf);
}

json_t *discordtool_enrich_403(const char *action, const char *body) {
    static const char *const HINTS[][2] = {
        {"pin_message", "Bot lacks MANAGE_MESSAGES permission in this channel. Ask the server admin to grant the bot a role that has MANAGE_MESSAGES, or a per-channel overwrite."},
        {"unpin_message", "Bot lacks MANAGE_MESSAGES permission in this channel."},
        {"delete_message", "Bot lacks MANAGE_MESSAGES permission in this channel, or cannot view the channel/message."},
        {"create_thread", "Bot lacks CREATE_PUBLIC_THREADS in this channel, or cannot view it."},
        {"add_role", "Either the bot lacks MANAGE_ROLES, or the target role sits higher than the bot's highest role. Roles can only be assigned below the bot's own position in the role hierarchy."},
        {"remove_role", "Either the bot lacks MANAGE_ROLES, or the target role sits higher than the bot's highest role."},
        {"fetch_messages", "Bot cannot view this channel (missing VIEW_CHANNEL or READ_MESSAGE_HISTORY)."},
        {"list_pins", "Bot cannot view this channel (missing VIEW_CHANNEL or READ_MESSAGE_HISTORY)."},
        {"channel_info", "Bot cannot view this channel (missing VIEW_CHANNEL)."},
        {"search_members", "Likely missing the Server Members privileged intent — enable it in the Discord Developer Portal under your bot's settings."},
        {"member_info", "Bot cannot see this guild member (missing Server Members intent or insufficient permissions)."},
    };
    const char *hint = NULL;
    char buf[4096];
    size_t i;
    for (i = 0; i < sizeof(HINTS) / sizeof(HINTS[0]); i++)
        if (!strcmp(HINTS[i][0], action ? action : "")) { hint = HINTS[i][1]; break; }
    if (hint)
        snprintf(buf, sizeof(buf), "Discord API 403 (forbidden) on '%s'. %s (Raw: %s)",
                 action ? action : "", hint, body ? body : "");
    else
        snprintf(buf, sizeof(buf), "Discord API 403 (forbidden) on '%s'. (Raw: %s)",
                 action ? action : "", body ? body : "");
    return json_string(buf);
}

/* ---------- minimal SHA-256 (in-process, no libcrypto) ---------- */
typedef struct {
    uint32_t h[8];
    uint64_t bitlen;
    unsigned char buf[64];
    size_t buflen;
} sha256_ctx;
static const uint32_t K256[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
#define ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha256_block(sha256_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = ROR(w[i-15],7) ^ ROR(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = ROR(w[i-2],17) ^ ROR(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],hh=c->h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = ROR(e,6) ^ ROR(e,11) ^ ROR(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + K256[i] + w[i];
        uint32_t S0 = ROR(a,2) ^ ROR(a,13) ^ ROR(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=hh;
}
static void sha256_init(sha256_ctx *c) {
    c->h[0]=0x6a09e667u; c->h[1]=0xbb67ae85u; c->h[2]=0x3c6ef372u; c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu; c->h[5]=0x9b05688cu; c->h[6]=0x1f83d9abu; c->h[7]=0x5be0cd19u;
    c->bitlen=0; c->buflen=0;
}
static void sha256_update(sha256_ctx *c, const unsigned char *data, size_t n) {
    c->bitlen += (uint64_t)n * 8;
    while (n) {
        size_t take = 64 - c->buflen; if (take > n) take = n;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take; data += take; n -= take;
        if (c->buflen == 64) { sha256_block(c, c->buf); c->buflen = 0; }
    }
}
static void sha256_final(sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->bitlen;
    unsigned char pad = 0x80, zero = 0, len[8];
    sha256_update(c, &pad, 1);
    while (c->buflen != 56) sha256_update(c, &zero, 1);
    for (int i = 0; i < 8; ++i) len[i] = (unsigned char)(bits >> (56 - 8*i));
    sha256_update(c, len, 8);
    for (int i = 0; i < 8; ++i) {
        out[i*4]   = (unsigned char)(c->h[i] >> 24);
        out[i*4+1] = (unsigned char)(c->h[i] >> 16);
        out[i*4+2] = (unsigned char)(c->h[i] >> 8);
        out[i*4+3] = (unsigned char)c->h[i];
    }
}

json_t *discordtool_token_cache_key(const char *token) {
    sha256_ctx c;
    unsigned char dig[32];
    static const char hx[] = "0123456789abcdef";
    char out[17];
    sha256_init(&c);
    sha256_update(&c, (const unsigned char *)(token ? token : ""), strlen(token ? token : ""));
    sha256_final(&c, dig);
    for (int i = 0; i < 16; i++) {
        out[i] = (i % 2) ? hx[dig[i/2] & 15] : hx[dig[i/2] >> 4];
    }
    out[16] = '\0';
    return json_string(out);
}

static int jtruthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

json_t *discordtool_available_actions(const json_t *caps, const json_t *allowlist) {
    static const char *const ACTIONS[] = {
        "list_guilds","server_info","list_channels","channel_info","list_roles","member_info",
        "search_members","fetch_messages","list_pins","pin_message","unpin_message",
        "delete_message","create_thread","add_role","remove_role"
    };
    json_t *out = json_array();
    json_t *mc = caps && caps->type == JSON_OBJECT ? json_obj_get(caps, "has_members_intent") : NULL;
    int members_ok = mc ? jtruthy(mc) : 1;
    int restricted = allowlist && allowlist->type == JSON_ARRAY;
    for (size_t i = 0; i < sizeof(ACTIONS)/sizeof(ACTIONS[0]); i++) {
        int gated = !strcmp(ACTIONS[i], "member_info") || !strcmp(ACTIONS[i], "search_members");
        int allowed = 1;
        if (restricted) {
            allowed = 0;
            for (size_t j = 0; j < json_len(allowlist); j++) {
                json_t *v = json_get(allowlist, j);
                if (v && v->type == JSON_STRING && !strcmp(v->str_val, ACTIONS[i])) { allowed = 1; break; }
            }
        }
        if ((members_ok || !gated) && allowed) json_append(out, json_string(ACTIONS[i]));
    }
    return out;
}
