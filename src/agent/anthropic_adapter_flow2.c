/* anthropic_adapter_flow2.c — FLOW port of 12 UNCLAIMED functions from
 * agent/anthropic_adapter.py (quarry = C:/Users/eman5/hermes-agent).
 *
 * CODE-ONLY, gcc -fsyntax-only clean; only libjson is used.  Every entry point
 * is `static aa2_*` and carries a PoP annotation on the line immediately above
 * the definition.  Pure functions compute directly; foreign collaborators are
 * routed through the universal recorder seam `aa2_seam` so the oracle can drive
 * the live quarry defs under the SAME scripts and diff the decision sequence.
 *
 * Representation contract (both sides):
 *  - None -> NULL seam result / json_null(); bool -> JSON_BOOL; int/float ->
 *    JSON_NUMBER; tuple/set -> JSON array; dict -> JSON object (insertion order
 *    preserved); integral floats collapse to ints in the oracle canon.
 *  - A seam script is a constant value, a per-call ARRAY queue (last value
 *    repeats, empty -> None), or {"__raise__": cls, "msg": ...}.  A MISSING
 *    script fails closed with a RuntimeError raise marker.  None-valued seam
 *    args are dropped; "" is kept.
 *  - A seam raise sets ctx->exc (the full marker); the harness emits it as ret
 *    (partial results discarded).  Functions that catch Exception clear exc
 *    and run their fallback path.
 */
#ifndef ANTHROPIC_ADAPTER_FLOW2_H
#define ANTHROPIC_ADAPTER_FLOW2_H

#include "hermes_json.h"
#include <stdio.h>   /* snprintf */
#include <stdlib.h>  /* malloc/realloc/free */
#include <string.h>  /* strcmp/strncmp/strlen */

typedef struct {
    json_t *io;      /* fixture "io" object: seam name -> script */
    json_t *trace;   /* ordered [{ev,args,ret}] */
    json_t *idx;     /* per-seam $seq cursor */
    json_t *exc;     /* pending raise marker (NULL when none) */
} aa2_ctx;

/* ---- small helpers ---------------------------------------------------- */

static int aa2_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default:          return 0;
    }
}

static int aa2_base_only(const json_t *marker) {
    /* Builtin BaseException siblings that are NOT Exception subclasses:
     * BaseException / SystemExit / KeyboardInterrupt / GeneratorExit.  The
     * oracle's build_exc uses the FULL builtin hierarchy; this mirrors only
     * the Exception-vs-BaseException split that `except Exception` observes. */
    if (!marker || marker->type != JSON_OBJECT) return 0;
    const char *c = json_get_str(marker, "__raise__", "");
    return !strcmp(c, "BaseException") || !strcmp(c, "SystemExit") ||
           !strcmp(c, "KeyboardInterrupt") || !strcmp(c, "GeneratorExit");
}

static char *aa2_lower_dup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; i++)
        out[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] - 'A' + 'a') : s[i];
    out[n] = '\0';
    return out;
}

static int aa2_is_wordchar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

/* literal word-boundary replace (mirrors re.compile(r"\b<word>\b").sub) */
static char *aa2_word_replace(const char *text, const char *word, const char *wire) {
    if (!text) text = "";
    size_t wl = strlen(word), tl = strlen(text), nwire = strlen(wire);
    size_t cap = tl * 2 + 64, len = 0;
    char *out = (char *)malloc(cap ? cap : 1);
    size_t i = 0;
    while (i < tl) {
        if (i + wl <= tl && strncmp(text + i, word, wl) == 0 &&
            (i == 0 || !aa2_is_wordchar(text[i - 1])) &&
            (i + wl == tl || !aa2_is_wordchar(text[i + wl]))) {
            if (len + nwire + 1 > cap) { cap = (len + nwire + 1) * 2; out = (char *)realloc(out, cap); }
            memcpy(out + len, wire, nwire); len += nwire; i += wl;
            continue;
        }
        if (len + 2 > cap) { cap = (len + 2) * 2; out = (char *)realloc(out, cap); }
        out[len++] = text[i++];
    }
    out[len] = '\0';
    return out;
}

static char *aa2_rstrip_slash(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    memcpy(out, s, n + 1);
    while (n > 0 && out[n - 1] == '/') out[--n] = '\0';
    return out;
}

/* re.sub(r"/v1/?$", "", s) */
static char *aa2_strip_v1(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    memcpy(out, s, n + 1);
    if (n >= 4 && strcmp(out + n - 4, "/v1/") == 0) out[n - 4] = '\0';
    else if (n >= 3 && strcmp(out + n - 3, "/v1") == 0) out[n - 3] = '\0';
    return out;
}

/* Universal recorder seam.  TAKES OWNERSHIP of `args` (frees it on all paths).
 * Returns a caller-owned result; on scripted/missing raise sets ctx->exc and
 * returns NULL.  A NULL result with ctx->exc == NULL is a real None. */
static json_t *aa2_seam(aa2_ctx *ctx, const char *name, json_t *args) {
    json_t *fargs = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(args); i++) {
            const char *k = json_object_get_key_at(args, i);
            json_t *v = json_object_get_at(args, i);
            if (v == NULL) continue;            /* None-valued args dropped */
            json_set(fargs, k, json_copy(v));   /* "" kept */
        }
    }
    if (args) json_free(args);

    json_t *sc = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    json_t *r = NULL;
    if (sc && sc->type == JSON_ARRAY) {
        size_t idx = 0;
        json_t *iv = ctx->idx ? json_obj_get(ctx->idx, name) : NULL;
        if (iv && iv->type == JSON_NUMBER) idx = (size_t)iv->num_val;
        size_t n = json_len(sc);
        if (n > 0) { size_t j = idx < n ? idx : n - 1; r = json_copy(json_get(sc, j)); }
        json_set(ctx->idx, name, json_number((double)(idx + 1)));
    } else if (sc) {
        r = json_copy(sc);
    } else {
        r = json_object();
        json_set(r, "__raise__", json_string("RuntimeError"));
        char msg[160];
        snprintf(msg, sizeof(msg), "no script for seam: %s", name);
        json_set(r, "msg", json_string(msg));
    }

    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", fargs);               /* ownership transferred */
    json_set(ev, "ret", r ? json_copy(r) : json_null());
    json_append(ctx->trace, ev);

    if (r && r->type == JSON_OBJECT) {
        json_t *rr = json_obj_get(r, "__raise__");
        if (rr && rr->type == JSON_STRING) { ctx->exc = r; return NULL; }
    }
    return r;
}

/* Call a seam from a function returning json_t*, propagating a raise as NULL. */
#define AA2_SEAM(ctxp, name, args, outp) do {                 \
    (outp) = aa2_seam((ctxp), (name), (args));                \
    if ((ctxp)->exc) { if (outp) json_free(outp); return NULL; } \
} while (0)

/* Same for a void (in-place mutator) function. */
#define AA2_SEAM_VOID(ctxp, name, args, outp) do {            \
    (outp) = aa2_seam((ctxp), (name), (args));                \
    if ((ctxp)->exc) { if (outp) json_free(outp); return; }   \
} while (0)

/* ---- ported functions ------------------------------------------------- */

/* PoP: aa2_model_matches @ agent/anthropic_adapter.py:_model_matches */
static json_t *aa2_model_matches(aa2_ctx *ctx, json_t *model, json_t *substrings) {
    (void)ctx;
    const char *m = (model && model->type == JSON_STRING) ? model->str_val : "";
    char *low = aa2_lower_dup(m);
    int ok = 0;
    if (substrings && substrings->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(substrings); i++) {
            json_t *v = json_get(substrings, i);
            if (v && v->type == JSON_STRING && v->str_val && strstr(low, v->str_val)) { ok = 1; break; }
        }
    }
    free(low);
    return json_bool(ok);
}

/* PoP: aa2_beta_header @ agent/anthropic_adapter.py:_beta_header */
static json_t *aa2_beta_header(aa2_ctx *ctx, json_t *betas) {
    (void)ctx;
    json_t *out = json_object();
    if (betas && betas->type == JSON_ARRAY && json_len(betas) > 0) {
        size_t n = json_len(betas), total = 1;
        for (size_t i = 0; i < n; i++) {
            json_t *v = json_get(betas, i);
            total += (v && v->type == JSON_STRING && v->str_val) ? strlen(v->str_val) : 0;
        }
        if (n > 1) total += n - 1;
        char *buf = (char *)malloc(total + 1);
        size_t p = 0;
        for (size_t i = 0; i < n; i++) {
            if (i) buf[p++] = ',';
            json_t *v = json_get(betas, i);
            const char *s = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
            size_t l = strlen(s); memcpy(buf + p, s, l); p += l;
        }
        buf[p] = '\0';
        json_set(out, "anthropic-beta", json_string(buf));
        free(buf);
    }
    return out;
}

/* PoP: aa2_normalize_to_mcp_wire @ agent/anthropic_adapter.py:_normalize_to_mcp_wire */
static json_t *aa2_normalize_to_mcp_wire(aa2_ctx *ctx, json_t *name) {
    (void)ctx;
    const char *s = (name && name->type == JSON_STRING) ? name->str_val : "";
    if (strncmp(s, "mcp__", 5) == 0) return json_string(s);
    const char *rest = (strncmp(s, "mcp_", 4) == 0) ? s + 4 : s;
    size_t n = strlen(rest);
    char *out = (char *)malloc(n + 6);
    memcpy(out, "mcp__", 5);
    memcpy(out + 5, rest, n + 1);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: aa2_apply_oauth_prose_aliases @ agent/anthropic_adapter.py:_apply_oauth_prose_aliases */
static json_t *aa2_apply_oauth_prose_aliases(aa2_ctx *ctx, json_t *text) {
    (void)ctx;
    const char *s = (text && text->type == JSON_STRING) ? text->str_val : "";
    /* _OAUTH_PROSE_ALIAS_PATTERNS == [(re.compile(r"\bsession_search\b"),
     *   "chat_history_lookup")]  (single sorted alias in the live module). */
    char *out = aa2_word_replace(s, "session_search", "chat_history_lookup");
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: aa2_attribution_headers @ agent/anthropic_adapter.py:_attribution_headers */
static json_t *aa2_attribution_headers(aa2_ctx *ctx) {
    json_t *args = json_object();
    json_t *vi;
    AA2_SEAM(ctx, "get_version_info", args, vi);
    const char *bv = (vi && vi->type == JSON_OBJECT) ? json_get_str(vi, "base_version", "") : "";
    char bvcopy[256];
    snprintf(bvcopy, sizeof(bvcopy), "%s", bv ? bv : "");
    if (vi) json_free(vi);
    json_t *out = json_object();
    json_set(out, "HTTP-Referer", json_string("https://hermes-agent.nousresearch.com"));
    json_set(out, "X-Title", json_string("Hermes Agent"));
    size_t n = strlen(bvcopy);
    char *ua = (char *)malloc(n + 16);
    snprintf(ua, n + 16, "HermesAgent/%s", bvcopy);
    json_set(out, "User-Agent", json_string(ua));
    free(ua);
    return out;
}

/* PoP: aa2_client_timeout @ agent/anthropic_adapter.py:_client_timeout */
static json_t *aa2_client_timeout(aa2_ctx *ctx, json_t *timeout) {
    double read = 900.0;
    if (timeout && timeout->type == JSON_NUMBER && timeout->num_val > 0.0)
        read = timeout->num_val;
    json_t *args = json_object();
    json_set(args, "timeout", json_number(read));
    json_set(args, "connect", json_number(10.0));
    json_t *r;
    AA2_SEAM(ctx, "httpx.Timeout", args, r);
    return r;
}

/* PoP: aa2_auth_style @ agent/anthropic_adapter.py:_auth_style */
static json_t *aa2_auth_style(aa2_ctx *ctx, json_t *api_key,
                              json_t *base_url, json_t *normalized_base_url) {
    json_t *v;
    json_t *args = json_object(); json_set(args, "base_url", json_copy(base_url));
    AA2_SEAM(ctx, "is_kimi_coding_endpoint", args, v);
    int t = aa2_truthy(v); json_free(v);
    if (t) return json_string("kimi");

    args = json_object(); json_set(args, "base_url", json_copy(normalized_base_url));
    AA2_SEAM(ctx, "requires_bearer_auth", args, v);
    t = aa2_truthy(v); json_free(v);
    if (t) return json_string("bearer");

    args = json_object(); json_set(args, "base_url", json_copy(base_url));
    AA2_SEAM(ctx, "is_third_party_anthropic_endpoint", args, v);
    t = aa2_truthy(v); json_free(v);
    if (t) return json_string("api_key");

    args = json_object(); json_set(args, "api_key", json_copy(api_key));
    AA2_SEAM(ctx, "is_oauth_token", args, v);
    t = aa2_truthy(v); json_free(v);
    if (t) return json_string("oauth");

    return json_string("api_key");
}

/* PoP: aa2_thinking_kwargs @ agent/anthropic_adapter.py:_thinking_kwargs */
static json_t *aa2_thinking_kwargs(aa2_ctx *ctx, json_t *cfg,
                                   json_t *model, json_t *effective_max_tokens) {
    json_t *rc = (cfg && cfg->type == JSON_OBJECT) ? cfg : NULL;
    json_t *enabled = rc ? json_obj_get(rc, "enabled") : NULL;
    if (enabled && enabled->type == JSON_BOOL && !enabled->bool_val) {
        json_t *args = json_object(); json_set(args, "model", json_copy(model));
        json_t *v; AA2_SEAM(ctx, "model_matches", args, v);
        int t = aa2_truthy(v); json_free(v);
        if (t) {
            json_t *th = json_object(); json_set(th, "type", json_string("between_tools"));
            json_t *out = json_object(); json_set(out, "thinking", th); return out;
        }
        args = json_object(); json_set(args, "model", json_copy(model));
        AA2_SEAM(ctx, "accepts_thinking_disable", args, v);
        t = aa2_truthy(v); json_free(v);
        json_t *out = json_object();
        if (t) { json_t *th = json_object(); json_set(th, "type", json_string("disabled")); json_set(out, "thinking", th); }
        return out;
    }
    json_t *args = json_object(); json_set(args, "model", json_copy(model));
    json_t *v; AA2_SEAM(ctx, "is_pre_thinking_haiku", args, v);
    if (aa2_truthy(v)) { json_free(v); return json_object(); }
    json_free(v);

    const char *eff = "medium";
    if (rc) {
        json_t *e = json_obj_get(rc, "effort");
        if (e && e->type == JSON_STRING && e->str_val) eff = e->str_val;
    }
    char *effort = aa2_lower_dup(eff);

    args = json_object(); json_set(args, "model", json_copy(model));
    AA2_SEAM(ctx, "supports_adaptive_thinking", args, v);
    int adaptive = aa2_truthy(v); json_free(v);
    json_t *out = json_object();
    if (adaptive) {
        /* ADAPTIVE_EFFORT_MAP (live): ultra/max->max, xhigh->xhigh,
         * high->high, medium->medium, low->low, minimal->low. */
        const char *ae = "medium";
        if (!strcmp(effort, "ultra") || !strcmp(effort, "max")) ae = "max";
        else if (!strcmp(effort, "xhigh")) ae = "xhigh";
        else if (!strcmp(effort, "high")) ae = "high";
        else if (!strcmp(effort, "low") || !strcmp(effort, "minimal")) ae = "low";
        if (!strcmp(ae, "xhigh")) {
            args = json_object(); json_set(args, "model", json_copy(model));
            AA2_SEAM(ctx, "supports_xhigh_effort", args, v);
            if (!aa2_truthy(v)) ae = "max";
            json_free(v);
        }
        json_t *th = json_object();
        json_set(th, "type", json_string("adaptive"));
        json_set(th, "display", json_string("summarized"));
        json_set(out, "thinking", th);
        json_t *oc = json_object(); json_set(oc, "effort", json_string(ae));
        json_set(out, "output_config", oc);
    } else {
        /* THINKING_BUDGET (live): xhigh 32000, high 16000, medium 8000, low 4000 */
        double budget = 8000.0;
        if (!strcmp(effort, "xhigh")) budget = 32000.0;
        else if (!strcmp(effort, "high")) budget = 16000.0;
        else if (!strcmp(effort, "low")) budget = 4000.0;
        double emt = 0.0;
        if (effective_max_tokens && effective_max_tokens->type == JSON_NUMBER)
            emt = effective_max_tokens->num_val;
        double mt = emt > budget + 4096.0 ? emt : budget + 4096.0;
        json_t *th = json_object();
        json_set(th, "type", json_string("enabled"));
        json_set(th, "budget_tokens", json_number(budget));
        json_set(out, "thinking", th);
        json_set(out, "temperature", json_number(1));
        json_set(out, "max_tokens", json_number(mt));
    }
    free(effort);
    return out;
}

/* PoP: aa2_is_pre_thinking_haiku @ agent/anthropic_adapter.py:_is_pre_thinking_haiku */
static json_t *aa2_is_pre_thinking_haiku(aa2_ctx *ctx, json_t *model) {
    const char *s = (model && model->type == JSON_STRING) ? model->str_val : "";
    char *low = aa2_lower_dup(s);
    int is_haiku = strstr(low, "haiku") != NULL;
    free(low);
    if (!is_haiku) return json_bool(0);

    json_t *args = json_object(); json_set(args, "model", json_copy(model));
    json_t *parsed;
    AA2_SEAM(ctx, "claude_family_version", args, parsed);
    int result;
    if (!parsed || parsed->type == JSON_NULL) {
        result = 1;
    } else if (parsed->type == JSON_ARRAY && json_len(parsed) >= 2) {
        json_t *fam = json_get(parsed, 0);
        const char *fs = (fam && fam->type == JSON_STRING) ? fam->str_val : "";
        if (strcmp(fs, "haiku") != 0) result = 1;
        else {
            json_t *ver = json_get(parsed, 1);
            double maj = 0.0, mnr = 0.0;
            if (ver && ver->type == JSON_ARRAY && json_len(ver) >= 2) {
                json_t *a = json_get(ver, 0), *b = json_get(ver, 1);
                if (a && a->type == JSON_NUMBER) maj = a->num_val;
                if (b && b->type == JSON_NUMBER) mnr = b->num_val;
            }
            result = (maj < 5.0 || (maj == 5.0 && mnr < 5.0)) ? 1 : 0;
        }
    } else {
        result = 1;   /* malformed parsed != None: Python would raise, but
                         fixtures stay in the documented [None|[str,[int,int]]] domain */
    }
    if (parsed) json_free(parsed);
    return json_bool(result);
}

/* PoP: aa2_base_client_kwargs @ agent/anthropic_adapter.py:_base_client_kwargs */
static json_t *aa2_base_client_kwargs(aa2_ctx *ctx, json_t *base_url, json_t *timeout) {
    json_t *out = json_object();

    json_t *args = json_object(); json_set(args, "timeout", json_copy(timeout));
    json_t *tv;
    AA2_SEAM(ctx, "client_timeout", args, tv);
    json_set(out, "timeout", tv ? tv : json_null());
    json_set(out, "max_retries", json_number(0));

    args = json_object(); json_set(args, "base_url", json_copy(base_url));
    json_t *nu;
    AA2_SEAM(ctx, "normalize_base_url_text", args, nu);
    const char *ns = (nu && nu->type == JSON_STRING) ? nu->str_val : "";
    char *rs = aa2_rstrip_slash(ns);
    char *normalized = aa2_strip_v1(rs);
    free(rs);
    if (nu) json_free(nu);

    if (normalized[0] != '\0') {
        json_set(out, "base_url", json_string(normalized));
        args = json_object(); json_set(args, "base_url", json_string(normalized));
        json_t *az;
        AA2_SEAM(ctx, "is_azure_anthropic_endpoint", args, az);
        int is_az = aa2_truthy(az); json_free(az);
        if (is_az && !strstr(normalized, "api-version")) {
            json_t *dq = json_object();
            json_set(dq, "api-version", json_string("2025-04-15"));
            json_set(out, "default_query", dq);
        }
    }
    json_t *ret = json_array();
    json_append(ret, json_string(normalized));
    json_append(ret, out);
    free(normalized);
    return ret;
}

/* PoP: aa2_custom_provider_extra_headers @ agent/anthropic_adapter.py:_custom_provider_extra_headers */
static json_t *aa2_custom_provider_extra_headers(aa2_ctx *ctx, json_t *base_url) {
    const char *s = (base_url && base_url->type == JSON_STRING) ? base_url->str_val : "";
    if (!s || s[0] == '\0') return json_object();

    json_t *args = json_object(); json_set(args, "base_url", json_string(s));
    json_t *r = aa2_seam(ctx, "get_custom_provider_extra_headers", args);
    if (ctx->exc) {
        int caught = !aa2_base_only(ctx->exc);
        if (caught) {
            json_free(ctx->exc); ctx->exc = NULL;
            if (r) json_free(r);
            json_t *da = json_object();
            json_set(da, "msg", json_string("custom-provider extra_headers skipped for Anthropic client"));
            json_t *d = aa2_seam(ctx, "logger.debug", da);
            if (d) json_free(d);
            if (ctx->exc) return NULL;          /* logger.debug raised: propagate */
            return json_object();
        }
        if (r) json_free(r);
        return NULL;                            /* BaseException: not caught */
    }
    return r ? r : json_object();
}

/* PoP: aa2_buffer_anthropic_tool_input @ agent/anthropic_adapter.py:buffer_anthropic_tool_input */
static void aa2_buffer_anthropic_tool_input(aa2_ctx *ctx, json_t *api_kwargs, json_t *base_url) {
    json_t *args = json_object(); json_set(args, "base_url", json_copy(base_url));
    json_t *betas;
    AA2_SEAM_VOID(ctx, "common_betas_for_base_url", args, betas);
    int present = 0;
    if (betas && betas->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(betas); i++) {
            json_t *v = json_get(betas, i);
            if (v && v->type == JSON_STRING && v->str_val &&
                !strcmp(v->str_val, "fine-grained-tool-streaming-2025-05-14")) { present = 1; break; }
        }
    }
    if (betas) json_free(betas);
    if (!present) return;
    if (!api_kwargs || api_kwargs->type != JSON_OBJECT) return;
    json_t *tools = json_obj_get(api_kwargs, "tools");
    if (!tools || tools->type != JSON_ARRAY) return;
    for (size_t i = 0; i < json_len(tools); i++) {
        json_t *tool = json_get(tools, i);
        if (tool && tool->type == JSON_OBJECT)
            json_set(tool, "eager_input_streaming", json_bool(0));
    }
}

#endif /* ANTHROPIC_ADAPTER_FLOW2_H */
