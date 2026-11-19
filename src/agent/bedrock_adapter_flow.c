/*
 * bedrock_adapter_flow.c — FLOW + leaf port of agent/bedrock_adapter.py
 * (request-conversion chains, model-id resolution, auth/region decision
 * branches, guardrail projection, reasoning replay policy, self-heal strip).
 *
 * Self-contained: libc + libjson only. Network / config / credential-chain
 * I/O runs through the injected brd_flow_seams_t pointers; the oracle harness
 * wires them to scripted results and records one trace event per call.
 *
 * Representation contract (shared with tests/oracle_bedrock_adapter_flow.py):
 * - None -> JSON null; Python tuples -> JSON arrays.
 * - bytes -> base64 ASCII string projection (both sides). _decode_redacted
 *   re-encodes the decoded bytes canonically (non-zero padding bits are
 *   dropped by Python's decoder, so "AB==" projects as "AA==").
 * - _image_block_from_data_url uses NON-strict b64decode: the fixture domain
 *   is canonical padded base64 (junk chars skipped), payloads with no
 *   alphabet chars (decode to b""), and payloads that raise (utf-8 fallback).
 *   Mid-string padding / trailing-data quirks of binascii are out of domain.
 * - str()/strip()/lower() operands are ASCII strings/null in fixtures;
 *   pystr() projects null->"None", bool->"True"/"False".
 * - strip_redacted_reasoning identity ("same object back") is projected as
 *   the harness-level "same" flag next to "ret".
 * - _reasoning_filter returns a filter FUNCTION live; projected as its name
 *   ("verbatim" / "without_reasoning" / "unsigned_reasoning").
 * - Uncaught exceptions -> {"__raise__": <class>, "msg": <str(exc)>}.
 * - Dict key order mirrors live insertion order (json_set replaces in place).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* ── trace ── */
typedef struct {
    json_t *events;   /* JSON array of seam events, NULL until first add */
} brd_flow_trace_t;

void brd_flow_trace_add(brd_flow_trace_t *t, json_t *ev) {
    if (!t || !ev) return;
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}

/* ── injected seams (all I/O; NULL return = Python None) ── */
typedef struct {
    void *ctx;
    const char *(*home_override)(void *ctx);                 /* str | None */
    const json_t *(*secret_scope)(void *ctx);                /* dict | None */
    const json_t *(*multiplex_active)(void *ctx);            /* any (truthy gate) */
    const json_t *(*load_config)(void *ctx, bool *raised);   /* dict | None | raise */
    const char *(*botocore_region)(void *ctx, bool *raised); /* str | None | raise */
    const char *(*resolve_profile)(void *ctx, const char *arn); /* str (cached outcome) */
} brd_flow_seams_t;

/* ── small helpers ── */
static bool jtruth(const json_t *v) {
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
static const json_t *jobj(const json_t *o, const char *k) {
    return json_obj_get(o, k);
}
static bool is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
/* malloc'd copy stripped of ASCII whitespace on both ends (str.strip). */
static char *xstrip(const char *s) {
    if (!s) s = "";
    while (*s && is_ws(*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && is_ws(e[-1])) e--;
    size_t n = (size_t)(e - s);
    char *d = malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}
static char *xlower(const char *s) {
    char *d = strdup(s ? s : "");
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = (char)tolower((unsigned char)*p);
    return d;
}
static bool str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
/* Python str() projection for the fixture domain (str/null/bool/int). */
static char *pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else snprintf(buf, sizeof(buf), "%.17g", d);
        return strdup(buf);
    }
    return strdup("");
}
static json_t *raise_desc(const char *cls, const char *msg) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(cls));
    json_set(e, "msg", json_string(msg ? msg : ""));
    return e;
}
/* dict.get(key, default) as a COPY: present value passes through verbatim
 * (including explicit null); absent key takes the string default. */
static json_t *get_copy_def(const json_t *o, const char *k, const char *def) {
    const json_t *v = jobj(o, k);
    return v ? json_copy(v) : json_string(def);
}

/* ── base64 ── */
static int b64_val(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
/* Decode canonical base64 ([A-Za-z0-9+/]* ={0,2}, len%4==0) to malloc'd
 * bytes. Returns NULL on any deviation. *outlen receives the byte count. */
static unsigned char *b64_decode_canonical(const char *s, size_t *outlen) {
    size_t n = strlen(s);
    if (n % 4 != 0) return NULL;
    size_t pads = 0;
    while (pads < n && s[n - 1 - pads] == '=') pads++;
    if (pads > 2) return NULL;
    for (size_t i = 0; i < n - pads; i++)
        if (b64_val((unsigned char)s[i]) < 0) return NULL;
    size_t outcap = n / 4 * 3 + 1;
    unsigned char *out = malloc(outcap);
    if (!out) return NULL;
    size_t olen = 0;
    for (size_t i = 0; i < n; i += 4) {
        int q[4] = {0, 0, 0, 0};
        int npad = 0;
        for (int k = 0; k < 4; k++) {
            char c = s[i + k];
            if (c == '=') { npad++; continue; }
            q[k] = b64_val((unsigned char)c);
        }
        out[olen++] = (unsigned char)((q[0] << 2) | (q[1] >> 4));
        if (npad < 2) out[olen++] = (unsigned char)(((q[1] & 15) << 4) | (q[2] >> 2));
        if (npad < 1) out[olen++] = (unsigned char)(((q[2] & 3) << 6) | q[3]);
    }
    *outlen = olen;
    return out;
}
static char *b64_encode(const unsigned char *data, size_t len) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t olen = (len + 2) / 3 * 4;
    char *out = malloc(olen + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        unsigned b0 = data[i];
        unsigned b1 = (i + 1 < len) ? data[i + 1] : 0;
        unsigned b2 = (i + 2 < len) ? data[i + 2] : 0;
        out[o++] = T[b0 >> 2];
        out[o++] = T[((b0 & 3) << 4) | (b1 >> 4)];
        out[o++] = (i + 1 < len) ? T[((b1 & 15) << 2) | (b2 >> 6)] : '=';
        out[o++] = (i + 2 < len) ? T[b2 & 63] : '=';
    }
    out[o] = '\0';
    return out;
}
/* _decode_redacted core: strict base64 (validate=True) on a non-empty str;
 * returns the canonical base64 PROJECTION of the decoded bytes, or NULL. */
static json_t *decode_redacted_node(const json_t *encoded) {
    if (!encoded || encoded->type != JSON_STRING || !encoded->str_val[0])
        return NULL;
    const char *s = encoded->str_val;
    for (const char *p = s; *p; p++)
        if ((unsigned char)*p > 127) return NULL;  /* ascii encode raises */
    size_t len = 0;
    unsigned char *raw = b64_decode_canonical(s, &len);
    if (!raw) return NULL;
    char *enc = b64_encode(raw, len);
    free(raw);
    if (!enc) return NULL;
    json_t *r = json_string(enc);
    free(enc);
    return r;
}

/* ── shared block builders ── */
static json_t *tool_use_block(const json_t *id, const json_t *name,
                              const json_t *input) {
    json_t *tu = json_object();
    json_set(tu, "toolUseId", id ? json_copy(id) : json_string(""));
    json_set(tu, "name", name ? json_copy(name) : json_string(""));
    json_set(tu, "input", input ? json_copy(input) : json_object());
    json_t *b = json_object();
    json_set(b, "toolUse", tu);
    return b;
}
/* _reasoning_text_block(text, signature): text passes only when str. */
static json_t *reasoning_text_block(const json_t *text, const json_t *signature) {
    json_t *rt = json_object();
    json_set(rt, "text", (text && text->type == JSON_STRING)
                           ? json_copy(text) : json_string(""));
    if (signature && signature->type == JSON_STRING && signature->str_val[0])
        json_set(rt, "signature", json_copy(signature));
    json_t *rc = json_object();
    json_set(rc, "reasoningText", rt);
    json_t *b = json_object();
    json_set(b, "reasoningContent", rc);
    return b;
}
/* _redacted_reasoning_block(encoded): dict | None. */
static json_t *redacted_reasoning_block(const json_t *encoded) {
    json_t *dec = decode_redacted_node(encoded);
    if (!dec) return NULL;
    json_t *rc = json_object();
    json_set(rc, "redactedContent", dec);
    json_t *b = json_object();
    json_set(b, "reasoningContent", rc);
    return b;
}
/* _parse_tool_args: json.loads on str ({} on failure), passthrough else. */
static json_t *parse_tool_args(const json_t *args) {
    if (!args || args->type != JSON_STRING)
        return args ? json_copy(args) : json_null();
    char *err = NULL;
    json_t *r = json_parse(args->str_val, &err);
    free(err);
    if (!r) return json_object();
    return r;
}
/* _safe_text: str(text) (None -> ""), placeholder when whitespace-only. */
static char *safe_text(const json_t *text) {
    char *s = (!text || text->type == JSON_NULL) ? strdup("") : pystr(text);
    if (!s) return NULL;
    char *t = xstrip(s);
    bool blank = !t || !t[0];
    free(t);
    if (blank) { free(s); return strdup("(empty)"); }
    return s;
}

/* ── URL helpers (urlparse subset for the fixture domain) ── */
/* hostname: netloc after last '@', before first ':', lowercased; NULL when
 * empty. path: from the first '/' after the authority up to '?' / '#'. */
static void url_split(const char *url, char **host_out, char **path_out) {
    const char *s = url ? url : "";
    const char *netloc = NULL;
    const char *sep = strstr(s, "://");
    if (sep && sep > s) {
        bool ok = isalpha((unsigned char)s[0]);
        for (const char *p = s; ok && p < sep; p++) {
            char c = *p;
            ok = isalnum((unsigned char)c) || c == '+' || c == '-' || c == '.';
        }
        if (ok) netloc = sep + 3;
    }
    char *host = NULL, *path = NULL;
    if (netloc) {
        const char *e = netloc;
        while (*e && *e != '/' && *e != '?' && *e != '#') e++;
        size_t nlen = (size_t)(e - netloc);
        char *nl = malloc(nlen + 1);
        memcpy(nl, netloc, nlen);
        nl[nlen] = '\0';
        char *at = strrchr(nl, '@');
        char *h = at ? at + 1 : nl;
        char *colon = strchr(h, ':');
        if (colon) *colon = '\0';
        if (h[0]) host = xlower(h);
        free(nl);
        if (*e == '/') {
            const char *pe = e;
            while (*pe && *pe != '?' && *pe != '#') pe++;
            path = malloc((size_t)(pe - e) + 1);
            memcpy(path, e, (size_t)(pe - e));
            path[pe - e] = '\0';
        } else {
            path = strdup("");
        }
    } else {
        const char *pe = s;
        while (*pe && *pe != '?' && *pe != '#') pe++;
        path = malloc((size_t)(pe - s) + 1);
        memcpy(path, s, (size_t)(pe - s));
        path[pe - s] = '\0';
    }
    *host_out = host;
    *path_out = path ? path : strdup("");
}
/* ^bedrock-mantle\.([a-z0-9-]+)\.api\.aws$ (IGNORECASE; host already lower). */
static char *mantle_host_region(const char *host) {
    if (!host) return NULL;
    static const char P[] = "bedrock-mantle.";
    static const char S[] = ".api.aws";
    size_t hl = strlen(host), pl = strlen(P), sl = strlen(S);
    if (hl <= pl + sl) return NULL;
    if (strncmp(host, P, pl) != 0) return NULL;
    if (strcmp(host + hl - sl, S) != 0) return NULL;
    size_t mlen = hl - pl - sl;
    for (size_t i = 0; i < mlen; i++) {
        char c = host[pl + i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return NULL;
    }
    char *r = malloc(mlen + 1);
    memcpy(r, host + pl, mlen);
    r[mlen] = '\0';
    return r;
}

/* ── model-id / policy constants (live tuples) ── */
static const char *const PROFILE_PREFIXES[] = {
    "global", "us", "eu", "apac", "ap", "au", "jp", "ca", "sa", "me", "af", "in",
};
static const char *const OPENAI_MODEL_IDS[] = {
    "openai.gpt-6-astra", "openai.gpt-6.1-sol", "openai.gpt-6.1-terra",
    "openai.gpt-6-sol", "openai.gpt-6-luna", "openai.gpt-5.5",
    "openai.gpt-5.6-sol", "openai.gpt-5.6-terra", "openai.gpt-5.6-luna",
    "openai.gpt-5.4",
};
static const char *const REPLAY_STRIP[] = {
    "deepseek.r1", "deepseek-r1", "moonshotai.kimi-k3",
};
static const char *const AWS_SCOPED_VARS[][2] = {
    {"aws_access_key_id", "AWS_ACCESS_KEY_ID"},
    {"aws_secret_access_key", "AWS_SECRET_ACCESS_KEY"},
    {"aws_session_token", "AWS_SESSION_TOKEN"},
    {"profile_name", "AWS_PROFILE"},
};
#define ARN_PROFILE_MARKER ":application-inference-profile/"

/* ── reasoning replay policy (seamed profile resolution) ── */
static const char *converse_replay_policy(const char *model,
                                          const brd_flow_seams_t *s) {
    if (!model) return "signed";
    if (strstr(model, ARN_PROFILE_MARKER))
        model = s->resolve_profile(s->ctx, model);
    char *low = xlower(model);
    const char *r;
    if (strstr(low, "claude")) r = "signed";
    else if (strstr(low, "openai.gpt-6")) r = "issuer";
    else {
        r = "unsigned";
        for (size_t i = 0; i < sizeof(REPLAY_STRIP) / sizeof(REPLAY_STRIP[0]); i++)
            if (strstr(low, REPLAY_STRIP[i])) { r = "strip"; break; }
    }
    free(low);
    return r;
}
static bool minted_outside_claude(const char *issuer, const brd_flow_seams_t *s) {
    if (!issuer) return false;
    if (strstr(issuer, ARN_PROFILE_MARKER)) {
        const char *resolved = s->resolve_profile(s->ctx, issuer);
        if (str_eq(resolved, issuer)) return false;
    }
    return !str_eq(converse_replay_policy(issuer, s), "signed");
}

/* ── content conversion helpers ── */
/* _image_block_from_data_url: data URL -> Converse image block (bytes
 * projected as base64). Non-strict b64decode over the fixture domain. */
static json_t *image_block_from_data_url(const char *url) {
    const char *comma = strchr(url, ',');
    size_t hlen = comma ? (size_t)(comma - url) : strlen(url);
    const char *data = comma ? comma + 1 : "";
    char *header = malloc(hlen + 1);
    memcpy(header, url, hlen);
    header[hlen] = '\0';
    char *media = strdup("");
    if (strncmp(header, "data:", 5) == 0) {
        const char *semi = strchr(header + 5, ';');
        size_t mlen = semi ? (size_t)(semi - (header + 5)) : strlen(header + 5);
        free(media);
        media = malloc(mlen + 1);
        memcpy(media, header + 5, mlen);
        media[mlen] = '\0';
    }
    free(header);
    if (!media[0]) { free(media); media = strdup("image/jpeg"); }
    char *format;
    char *slash = strrchr(media, '/');
    if (slash) format = strdup(slash + 1);
    else format = strdup("jpeg");
    free(media);
    /* lenient decode: non-ASCII raises; skip non-alphabet chars; the cleaned
     * payload must be canonical (or carry no alphabet chars -> b""). */
    unsigned char *raw = NULL;
    size_t rawlen = 0;
    bool ascii = true;
    for (const char *p = data; *p; p++)
        if ((unsigned char)*p > 127) { ascii = false; break; }
    if (ascii) {
        size_t n = strlen(data);
        char *clean = malloc(n + 1);
        size_t cn = 0, alpha = 0;
        for (size_t i = 0; i < n; i++) {
            unsigned char c = (unsigned char)data[i];
            if (b64_val(c) >= 0) { clean[cn++] = (char)c; alpha++; }
            else if (c == '=') clean[cn++] = (char)c;
        }
        clean[cn] = '\0';
        if (alpha == 0) {
            raw = malloc(1);
            rawlen = 0;
        } else {
            raw = b64_decode_canonical(clean, &rawlen);
        }
        free(clean);
    }
    if (!raw) {  /* decode raised -> data.encode("utf-8") */
        rawlen = strlen(data);
        raw = malloc(rawlen + 1);
        memcpy(raw, data, rawlen);
    }
    char *enc = b64_encode(raw, rawlen);
    free(raw);
    json_t *src = json_object();
    json_set(src, "bytes", json_string(enc ? enc : ""));
    free(enc);
    json_t *img = json_object();
    json_set(img, "format", json_string(format));
    json_set(img, "source", src);
    free(format);
    json_t *b = json_object();
    json_set(b, "image", img);
    return b;
}
/* _convert_content_to_converse (claimed sibling, transcribed as a helper). */
static json_t *convert_content_to_converse(const json_t *content) {
    json_t *blocks = json_array();
    if (!content || content->type != JSON_ARRAY) {
        char *t = safe_text(content);
        json_t *b = json_object();
        json_set(b, "text", json_string(t ? t : "(empty)"));
        free(t);
        json_append(blocks, b);
        return blocks;
    }
    for (size_t i = 0; i < json_len(content); i++) {
        const json_t *part = json_get(content, i);
        if (part && part->type == JSON_STRING) {
            char *t = safe_text(part);
            json_t *b = json_object();
            json_set(b, "text", json_string(t ? t : "(empty)"));
            free(t);
            json_append(blocks, b);
        } else if (part && part->type == JSON_OBJECT) {
            const char *type = json_get_str(part, "type", "");
            if (str_eq(type, "text")) {
                char *t = safe_text(jobj(part, "text"));
                json_t *b = json_object();
                json_set(b, "text", json_string(t ? t : "(empty)"));
                free(t);
                json_append(blocks, b);
            } else if (str_eq(type, "image_url")) {
                const json_t *iu = jobj(part, "image_url");
                const char *url = (iu && iu->type == JSON_OBJECT)
                                      ? json_get_str(iu, "url", "") : "";
                if (strncmp(url, "data:", 5) == 0) {
                    json_append(blocks, image_block_from_data_url(url));
                } else {
                    size_t n = strlen(url) + 16;
                    char *txt = malloc(n);
                    snprintf(txt, n, "[Image: %s]", url);
                    json_t *b = json_object();
                    json_set(b, "text", json_string(txt));
                    free(txt);
                    json_append(blocks, b);
                }
            }
        }
    }
    if (json_len(blocks) == 0) {
        json_t *b = json_object();
        json_set(b, "text", json_string("(empty)"));
        json_append(blocks, b);
    }
    return blocks;
}
/* _replay_ordered_blocks: exact captured block sequence; redacted reasoning
 * stored base64, undecodable entries skipped alone. */
static json_t *replay_ordered_blocks(const json_t *ordered) {
    json_t *out = json_array();
    if (!ordered || ordered->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(ordered); i++) {
        const json_t *block = json_get(ordered, i);
        if (!block || block->type != JSON_OBJECT) continue;
        const json_t *text = jobj(block, "text");
        const json_t *rc = jobj(block, "reasoningContent");
        const json_t *tu = jobj(block, "toolUse");
        if (text && text->type == JSON_STRING) {
            json_t *b = json_object();
            json_set(b, "text", json_copy(text));
            json_append(out, b);
        } else if (rc) {
            if (rc->type != JSON_OBJECT) continue;
            const json_t *rt = jobj(rc, "text");
            if (rt && rt->type == JSON_STRING)
                json_append(out, reasoning_text_block(rt, jobj(rc, "signature")));
            const json_t *enc = jobj(rc, "redactedContentBase64");
            if (enc && enc->type == JSON_STRING && enc->str_val[0]) {
                json_t *rb = redacted_reasoning_block(enc);
                if (rb) json_append(out, rb);
            }
        } else if (tu && tu->type == JSON_OBJECT) {
            json_append(out, tool_use_block(jobj(tu, "toolUseId"),
                                            jobj(tu, "name"), jobj(tu, "input")));
        }
    }
    return out;
}
/* _anthropic_ordered_blocks: Anthropic Messages blocks -> Converse blocks;
 * tool_use input re-sourced from the redacted tool_calls args map. */
static json_t *anthropic_ordered_blocks(const json_t *msg) {
    json_t *args = json_object();
    const json_t *tcs = jobj(msg, "tool_calls");
    if (tcs && tcs->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(tcs); i++) {
            const json_t *tc = json_get(tcs, i);
            if (!tc || tc->type != JSON_OBJECT) continue;
            const json_t *id = jobj(tc, "id");
            if (!id || id->type != JSON_STRING) continue;  /* fixture: str ids */
            const json_t *fn = jobj(tc, "function");
            const json_t *fobj = (fn && fn->type == JSON_OBJECT) ? fn : NULL;
            const json_t *a = jobj(fobj, "arguments");
            json_t *def = a ? NULL : json_string("{}");
            json_t *parsed = parse_tool_args(a ? a : def);
            json_free(def);
            json_set(args, id->str_val, parsed);
        }
    }
    json_t *out = json_array();
    const json_t *blocks = jobj(msg, "anthropic_content_blocks");
    if (blocks && blocks->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(blocks); i++) {
            const json_t *b = json_get(blocks, i);
            if (!b || b->type != JSON_OBJECT) continue;
            const char *type = json_get_str(b, "type", "");
            if (str_eq(type, "thinking")) {
                json_append(out, reasoning_text_block(jobj(b, "thinking"),
                                                      jobj(b, "signature")));
            } else if (str_eq(type, "redacted_thinking")) {
                json_t *rb = redacted_reasoning_block(jobj(b, "data"));
                if (rb) json_append(out, rb);
            } else if (str_eq(type, "text")) {
                const json_t *t = jobj(b, "text");
                if (t && t->type == JSON_STRING) {
                    char *s = xstrip(t->str_val);
                    if (s && s[0]) {
                        json_t *nb = json_object();
                        json_set(nb, "text", json_copy(t));
                        json_append(out, nb);
                    }
                    free(s);
                }
            } else if (str_eq(type, "tool_use")) {
                const json_t *id = jobj(b, "id");
                const json_t *input = NULL;
                if (id && id->type == JSON_STRING)
                    input = jobj(args, id->str_val);
                if (!input) input = jobj(b, "input");
                json_append(out, tool_use_block(id, jobj(b, "name"), input));
            }
        }
    }
    json_free(args);
    return out;
}
/* _reasoning_details_blocks: persisted thinking carrier, stored order. */
static json_t *reasoning_details_blocks(const json_t *msg, bool claude, bool own) {
    json_t *out = json_array();
    const json_t *details = jobj(msg, "reasoning_details");
    if (jtruth(details) && details->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(details); i++) {
            const json_t *d = json_get(details, i);
            if (!d || d->type != JSON_OBJECT) continue;
            const char *type = json_get_str(d, "type", "");
            if (str_eq(type, "thinking")) {
                const json_t *th = jobj(d, "thinking");
                if (th && th->type == JSON_STRING) {
                    char *s = xstrip(th->str_val);
                    bool nonblank = s && s[0];
                    free(s);
                    if (nonblank) {
                        const json_t *sig = jobj(d, "signature");
                        if ((claude || own) && jtruth(sig))
                            json_append(out, reasoning_text_block(th, sig));
                        else if (!claude)
                            json_append(out, reasoning_text_block(th, NULL));
                    }
                }
            } else if (str_eq(type, "redacted_thinking")) {
                const json_t *data = jobj(d, "data");
                if (!jtruth(data)) data = jobj(d, "redactedContentBase64");
                json_t *rb = redacted_reasoning_block(data);
                if (rb) json_append(out, rb);
            }
        }
    }
    if (json_len(out) == 0 && !claude) {
        const json_t *text = jobj(msg, "reasoning_content");
        if (text && text->type == JSON_STRING) {
            char *s = xstrip(text->str_val);
            if (s && s[0]) json_append(out, reasoning_text_block(text, NULL));
            free(s);
        }
    }
    return out;
}
/* _without_redacted_reasoning: blocks minus redacted reasoningContent
 * entries; NULL when not a list / nothing removed. */
static json_t *without_redacted_reasoning(const json_t *blocks) {
    if (!blocks || blocks->type != JSON_ARRAY) return NULL;
    json_t *cleaned = json_array();
    for (size_t i = 0; i < json_len(blocks); i++) {
        const json_t *b = json_get(blocks, i);
        bool drop = false;
        if (b && b->type == JSON_OBJECT) {
            const json_t *rc = jobj(b, "reasoningContent");
            if (rc && rc->type == JSON_OBJECT && jobj(rc, "redactedContent"))
                drop = true;
        }
        if (!drop) json_append(cleaned, json_copy(b));
    }
    if (json_len(cleaned) == json_len(blocks)) {
        json_free(cleaned);
        return NULL;
    }
    return cleaned;
}

/* ════════════════════════ ported functions ════════════════════════ */

/* PoP: brd_flow_scoped_aws_session_kwargs @ agent/bedrock_adapter.py:scoped_aws_session_kwargs */
json_t *brd_flow_scoped_aws_session_kwargs(const brd_flow_seams_t *s) {
    const char *override = s->home_override(s->ctx);
    if (!override) return json_object();
    const json_t *scope = s->secret_scope(s->ctx);
    if (!jtruth(scope)) scope = NULL;
    json_t *kwargs = json_object();
    for (size_t i = 0; i < 4; i++) {
        const json_t *v = jobj(scope, AWS_SCOPED_VARS[i][1]);
        if (!v || v->type != JSON_STRING) continue;
        char *stripped = xstrip(v->str_val);
        if (stripped && stripped[0])
            json_set(kwargs, AWS_SCOPED_VARS[i][0], json_string(stripped));
        free(stripped);
    }
    bool complete = (jobj(kwargs, "aws_access_key_id") &&
                     jobj(kwargs, "aws_secret_access_key")) ||
                    jobj(kwargs, "profile_name");
    if (!complete) {
        const json_t *mx = s->multiplex_active(s->ctx);
        if (jtruth(mx)) {
            json_free(kwargs);
            return raise_desc("RuntimeError",
                "Bedrock auth is refused for this profile: it sets no complete "
                "AWS credential of its own, and under multiplexed profiles the "
                "ambient default chain (process env, ~/.aws, instance metadata) "
                "would sign this profile's calls with the LAUNCH context's "
                "identity. Set AWS_ACCESS_KEY_ID + AWS_SECRET_ACCESS_KEY in "
                "this profile's own config, or AWS_PROFILE to name a shared "
                "AWS config profile.");
        }
    }
    return kwargs;
}

/* PoP: brd_flow_resolve_bedrock_runtime_region @ agent/bedrock_adapter.py:resolve_bedrock_runtime_region */
char *brd_flow_resolve_bedrock_runtime_region(const json_t *config,
                                              const json_t *env,
                                              const brd_flow_seams_t *s) {
    json_t *local_cfg = NULL;
    if (!config) {
        local_cfg = json_object();
        config = local_cfg;
        bool raised = false;
        const json_t *loaded = s->load_config(s->ctx, &raised);
        if (!raised && loaded) config = loaded;
    }
    const json_t *bedrock = jobj(config, "bedrock");
    const json_t *region = jobj(jtruth(bedrock) ? bedrock : NULL, "region");
    char *r = NULL;
    if (jtruth(region)) {
        char *ps = pystr(region);
        r = xstrip(ps);
        free(ps);
        if (r && !r[0]) { free(r); r = NULL; }
    }
    if (r) { json_free(local_cfg); return r; }
    /* resolve_bedrock_region(): env chain, then botocore configured region. */
    const char *a = json_get_str(env, "AWS_REGION", "");
    const char *b = json_get_str(env, "AWS_DEFAULT_REGION", "");
    char *sa = xstrip(a), *sb = xstrip(b);
    char *explicit = (sa && sa[0]) ? sa : sb;
    if (explicit && explicit[0]) {
        char *dup = strdup(explicit);
        free(sa); free(sb);
        json_free(local_cfg);
        return dup;
    }
    free(sa); free(sb);
    bool raised = false;
    const char *br = s->botocore_region(s->ctx, &raised);
    char *out = strdup((!raised && br && br[0]) ? br : "us-east-1");
    json_free(local_cfg);
    return out;
}

/* PoP: brd_flow_bedrock_region_from_runtime_url @ agent/bedrock_adapter.py:bedrock_region_from_runtime_url */
char *brd_flow_bedrock_region_from_runtime_url(const char *base_url) {
    static const char L[] = "bedrock-runtime.";
    const char *s = base_url ? base_url : "";
    const char *p = s;
    while ((p = strstr(p, L)) != NULL) {
        const char *q = p + strlen(L);
        const char *e = q;
        while (*e && ((*e >= 'a' && *e <= 'z') || (*e >= '0' && *e <= '9') || *e == '-'))
            e++;
        if (e > q && *e == '.') {
            size_t n = (size_t)(e - q);
            char *r = malloc(n + 1);
            memcpy(r, q, n);
            r[n] = '\0';
            return r;
        }
        p++;
    }
    return strdup("us-east-1");
}

/* PoP: brd_flow_parse_bedrock_openai_model_id @ agent/bedrock_adapter.py:parse_bedrock_openai_model_id */
json_t *brd_flow_parse_bedrock_openai_model_id(const json_t *model_id) {
    char *ps = (model_id && jtruth(model_id)) ? pystr(model_id) : strdup("");
    char *stripped = xstrip(ps);
    free(ps);
    char *norm = xlower(stripped);
    free(stripped);
    char *prefix = strdup("");
    char *dot = strchr(norm, '.');
    if (dot) {
        size_t plen = (size_t)(dot - norm);
        char *pfx = malloc(plen + 1);
        memcpy(pfx, norm, plen);
        pfx[plen] = '\0';
        bool known = false;
        for (size_t i = 0; i < sizeof(PROFILE_PREFIXES) / sizeof(PROFILE_PREFIXES[0]); i++)
            if (str_eq(pfx, PROFILE_PREFIXES[i])) { known = true; break; }
        if (known && strncmp(dot + 1, "openai.", 7) == 0) {
            char *rest = strdup(dot + 1);
            free(norm);
            norm = rest;
            free(prefix);
            prefix = pfx;
        } else {
            free(pfx);
        }
    }
    bool member = false;
    for (size_t i = 0; i < sizeof(OPENAI_MODEL_IDS) / sizeof(OPENAI_MODEL_IDS[0]); i++)
        if (str_eq(norm, OPENAI_MODEL_IDS[i])) { member = true; break; }
    json_t *out = NULL;
    if (member) {
        out = json_array();
        json_append(out, json_string(norm));
        json_append(out, json_string(prefix));
    }
    free(norm);
    free(prefix);
    return out;
}

/* PoP: brd_flow_mantle_url_parts @ agent/bedrock_adapter.py:_mantle_url_parts */
json_t *brd_flow_mantle_url_parts(const json_t *base_url) {
    char *ps = (base_url && jtruth(base_url)) ? pystr(base_url) : strdup("");
    char *host = NULL, *path = NULL;
    url_split(ps, &host, &path);
    free(ps);
    char *region = mantle_host_region(host);
    free(host);
    /* (parsed.path or "").rstrip("/").lower() */
    size_t n = strlen(path);
    while (n > 0 && path[n - 1] == '/') n--;
    path[n] = '\0';
    for (size_t i = 0; i < n; i++) path[i] = (char)tolower((unsigned char)path[i]);
    json_t *out = json_array();
    json_append(out, region ? json_string(region) : json_null());
    json_append(out, json_string(path));
    free(region);
    free(path);
    return out;
}

/* PoP: brd_flow_bedrock_guardrail_config @ agent/bedrock_adapter.py:bedrock_guardrail_config */
json_t *brd_flow_bedrock_guardrail_config(const json_t *config,
                                          const brd_flow_seams_t *s) {
    json_t *local_cfg = NULL;
    if (!config) {
        local_cfg = json_object();
        config = local_cfg;
        bool raised = false;
        const json_t *loaded = s->load_config(s->ctx, &raised);
        if (!raised && loaded) config = loaded;
    }
    const json_t *bedrock = jobj(config, "bedrock");
    const json_t *gr = jobj(jtruth(bedrock) ? bedrock : NULL, "guardrail");
    if (!jtruth(gr)) gr = NULL;
    const json_t *gid = jobj(gr, "guardrail_identifier");
    const json_t *gver = jobj(gr, "guardrail_version");
    json_t *out = NULL;
    if (jtruth(gid) && jtruth(gver)) {
        out = json_object();
        json_set(out, "guardrailIdentifier", json_copy(gid));
        json_set(out, "guardrailVersion", json_copy(gver));
        const json_t *spm = jobj(gr, "stream_processing_mode");
        if (jtruth(spm)) json_set(out, "streamProcessingMode", json_copy(spm));
        const json_t *trace = jobj(gr, "trace");
        if (jtruth(trace)) json_set(out, "trace", json_copy(trace));
    }
    json_free(local_cfg);
    return out;
}

/* PoP: brd_flow_bedrock_guardrail_headers @ agent/bedrock_adapter.py:bedrock_guardrail_headers */
json_t *brd_flow_bedrock_guardrail_headers(const json_t *config,
                                           const brd_flow_seams_t *s) {
    json_t *gr = brd_flow_bedrock_guardrail_config(config, s);
    json_t *headers = json_object();
    if (!jtruth(gr)) { json_free(gr); return headers; }
    char *id = pystr(jobj(gr, "guardrailIdentifier"));
    char *ver = pystr(jobj(gr, "guardrailVersion"));
    json_set(headers, "X-Amzn-Bedrock-GuardrailIdentifier", json_string(id));
    json_set(headers, "X-Amzn-Bedrock-GuardrailVersion", json_string(ver));
    free(id);
    free(ver);
    const json_t *trace = jobj(gr, "trace");
    char *ts = trace ? pystr(trace) : strdup("");
    char *tl = xlower(ts);
    free(ts);
    if (str_eq(tl, "enabled") || str_eq(tl, "enabled_full") || str_eq(tl, "true"))
        json_set(headers, "X-Amzn-Bedrock-Trace", json_string("ENABLED"));
    free(tl);
    json_free(gr);
    return headers;
}

/* PoP: brd_flow_anthropic_response_guardrail_intervened @ agent/bedrock_adapter.py:anthropic_response_guardrail_intervened */
bool brd_flow_anthropic_response_guardrail_intervened(const json_t *response) {
    const json_t *extra = jobj(response, "model_extra");
    if (!jtruth(extra)) extra = NULL;
    const json_t *action = jobj(extra, "amazon-bedrock-guardrailAction");
    char *s = action ? pystr(action) : strdup("");
    for (char *p = s; *p; p++) *p = (char)toupper((unsigned char)*p);
    bool r = str_eq(s, "INTERVENED");
    free(s);
    return r;
}

/* PoP: brd_flow_parse_tool_args @ agent/bedrock_adapter.py:_parse_tool_args */
json_t *brd_flow_parse_tool_args(const json_t *args) {
    return parse_tool_args(args);
}

/* PoP: brd_flow_decode_redacted @ agent/bedrock_adapter.py:_decode_redacted */
json_t *brd_flow_decode_redacted(const json_t *encoded) {
    return decode_redacted_node(encoded);
}

/* PoP: brd_flow_image_block_from_data_url @ agent/bedrock_adapter.py:_image_block_from_data_url */
json_t *brd_flow_image_block_from_data_url(const char *url) {
    return image_block_from_data_url(url ? url : "");
}

/* PoP: brd_flow_system_blocks @ agent/bedrock_adapter.py:_system_blocks */
json_t *brd_flow_system_blocks(const json_t *content) {
    json_t *out = json_array();
    const json_t *parts = NULL;
    json_t *single = NULL;
    if (content && content->type == JSON_STRING) {
        single = json_array();
        json_append(single, json_copy(content));
        parts = single;
    } else if (content && content->type == JSON_ARRAY) {
        parts = content;
    }
    if (parts) {
        for (size_t i = 0; i < json_len(parts); i++) {
            const json_t *part = json_get(parts, i);
            const json_t *text = part;
            if (part && part->type == JSON_OBJECT &&
                str_eq(json_get_str(part, "type", ""), "text"))
                text = jobj(part, "text");
            if (text && text->type == JSON_STRING) {
                char *s = xstrip(text->str_val);
                if (s && s[0]) {
                    json_t *b = json_object();
                    json_set(b, "text", json_copy(text));
                    json_append(out, b);
                }
                free(s);
            }
        }
    }
    json_free(single);
    return out;
}

/* PoP: brd_flow_replay_ordered_blocks @ agent/bedrock_adapter.py:_replay_ordered_blocks */
json_t *brd_flow_replay_ordered_blocks(const json_t *ordered_blocks) {
    return replay_ordered_blocks(ordered_blocks);
}

/* PoP: brd_flow_reasoning_details_blocks @ agent/bedrock_adapter.py:_reasoning_details_blocks */
json_t *brd_flow_reasoning_details_blocks(const json_t *msg, bool claude, bool own) {
    return reasoning_details_blocks(msg, claude, own);
}

/* PoP: brd_flow_assistant_blocks @ agent/bedrock_adapter.py:_assistant_blocks */
json_t *brd_flow_assistant_blocks(const json_t *msg, const json_t *content,
                                  bool claude, bool own) {
    const json_t *ordered = jobj(msg, "bedrock_content_blocks");
    if (ordered && ordered->type == JSON_ARRAY) {
        json_t *r = replay_ordered_blocks(ordered);
        if (json_len(r) > 0) return r;
        json_free(r);
    }
    if (claude) {
        json_t *r = anthropic_ordered_blocks(msg);
        if (json_len(r) > 0) return r;
        json_free(r);
    }
    json_t *blocks = reasoning_details_blocks(msg, claude, own);
    if (content && content->type == JSON_STRING) {
        char *s = xstrip(content->str_val);
        if (s && s[0]) {
            json_t *b = json_object();
            json_set(b, "text", json_copy(content));
            json_append(blocks, b);
        }
        free(s);
    } else if (content && content->type == JSON_ARRAY) {
        json_t *conv = convert_content_to_converse(content);
        for (size_t i = 0; i < json_len(conv); i++)
            json_append(blocks, json_copy(json_get(conv, i)));
        json_free(conv);
    }
    const json_t *tcs = jobj(msg, "tool_calls");
    if (!jtruth(tcs)) tcs = NULL;
    if (tcs && tcs->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(tcs); i++) {
            const json_t *tc = json_get(tcs, i);
            if (!tc || tc->type != JSON_OBJECT) continue;
            const json_t *fn = jobj(tc, "function");
            if (!fn) fn = NULL;  /* get("function", {}) default below */
            const json_t *a = jobj(fn, "arguments");
            json_t *def = a ? NULL : json_string("{}");
            json_t *parsed = parse_tool_args(a ? a : def);
            json_free(def);
            json_append(blocks, tool_use_block(jobj(tc, "id"),
                                               jobj(fn, "name"), parsed));
            json_free(parsed);
        }
    }
    return blocks;
}

/* PoP: brd_flow_converse_replay_policy @ agent/bedrock_adapter.py:_converse_replay_policy */
const char *brd_flow_converse_replay_policy(const char *model,
                                            const brd_flow_seams_t *s) {
    return converse_replay_policy(model, s);
}

/* PoP: brd_flow_reasoning_filter @ agent/bedrock_adapter.py:_reasoning_filter */
json_t *brd_flow_reasoning_filter(const char *policy, const char *issuer,
                                  bool own, bool in_loop,
                                  const brd_flow_seams_t *s) {
    const char *prior, *loop;
    if (str_eq(policy, "signed")) { prior = "verbatim"; loop = "verbatim"; }
    else if (str_eq(policy, "strip")) { prior = "without_reasoning"; loop = "verbatim"; }
    else if (str_eq(policy, "unsigned")) { prior = "unsigned_reasoning"; loop = "verbatim"; }
    else if (str_eq(policy, "issuer")) { prior = "without_reasoning"; loop = "without_reasoning"; }
    else {
        size_t n = strlen(policy) + 3;
        char *msg = malloc(n);
        snprintf(msg, n, "'%s'", policy);
        json_t *e = raise_desc("KeyError", msg);
        free(msg);
        return e;
    }
    const char *r;
    if (own) {
        r = (in_loop || str_eq(policy, "unsigned") || str_eq(policy, "issuer"))
                ? "verbatim" : prior;
    } else if (issuer) {
        r = (str_eq(policy, "signed") && minted_outside_claude(issuer, s))
                ? "without_reasoning" : prior;
    } else {
        r = in_loop ? loop : prior;
    }
    return json_string(r);
}

/* PoP: brd_flow_strip_redacted_reasoning @ agent/bedrock_adapter.py:strip_redacted_reasoning */
json_t *brd_flow_strip_redacted_reasoning(const json_t *kwargs, bool *same) {
    *same = true;
    const json_t *messages = jobj(kwargs, "messages");
    if (!messages || messages->type != JSON_ARRAY) return json_copy(kwargs);
    size_t n = json_len(messages);
    json_t **cleaned = calloc(n ? n : 1, sizeof(json_t *));
    bool any = false;
    for (size_t i = 0; i < n; i++) {
        const json_t *msg = json_get(messages, i);
        const json_t *content = (msg && msg->type == JSON_OBJECT)
                                    ? jobj(msg, "content") : NULL;
        cleaned[i] = without_redacted_reasoning(content);
        if (cleaned[i]) any = true;
    }
    if (!any) {
        for (size_t i = 0; i < n; i++) json_free(cleaned[i]);
        free(cleaned);
        return json_copy(kwargs);
    }
    *same = false;
    json_t *out = json_copy(kwargs);
    json_t *new_msgs = json_array();
    for (size_t i = 0; i < n; i++) {
        const json_t *msg = json_get(messages, i);
        if (!cleaned[i]) {
            json_append(new_msgs, json_copy(msg));
        } else if (json_len(cleaned[i]) > 0) {
            json_t *m = json_copy(msg);
            json_set(m, "content", cleaned[i]);  /* adopts */
            cleaned[i] = NULL;
            json_append(new_msgs, m);
        } else {
            json_free(cleaned[i]);  /* turn left empty -> dropped */
            cleaned[i] = NULL;
        }
    }
    free(cleaned);
    json_set(out, "messages", new_msgs);
    return out;
}
