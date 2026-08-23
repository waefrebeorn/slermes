/*
 * reasoning_effort.c — Faithful C11 port of agent/reasoning_effort.py.
 *
 * Canonical reasoning-effort vocabulary and wire clamping. Single source of
 * truth: clamp_effort keeps a supported level verbatim, otherwise takes the
 * NEAREST WEAKER supported level (never silently escalates cost), and only
 * when nothing weaker exists takes the weakest supported level. Declared
 * override tables carry vendor-documented translations as data.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "reasoning_effort.h"

/* ── EFFORT_LADDER: canonical low→high ordering ─────────────────────────── */
const char *const RE_EFFORT_LADDER[] = {
    "none", "minimal", "low", "medium", "high", "xhigh", "max", "ultra",
};
const size_t RE_EFFORT_LADDER_N = 8;

/* Widest OpenAI-compatible wire vocabulary (OpenRouter, Nous Portal). */
const char *const RE_OPENAI_COMPAT_WIRE_EFFORTS[] = {
    "none", "minimal", "low", "medium", "high", "xhigh", "max",
};
const size_t RE_OPENAI_COMPAT_WIRE_EFFORTS_N = 7;

/* OpenAI/Codex Responses backend — per-model vocabulary, live-verified. */
const char *const RE_CODEX_GPT56_EFFORTS[] = {
    "none", "low", "medium", "high", "xhigh", "max",
};
const size_t RE_CODEX_GPT56_EFFORTS_N = 6;
const char *const RE_CODEX_LEGACY_EFFORTS[] = {
    "none", "low", "medium", "high", "xhigh",
};
const size_t RE_CODEX_LEGACY_EFFORTS_N = 5;

/* xAI Responses — Grok 4.6+ accepts xhigh; older Grok tops out at high. */
const char *const RE_XAI_GROK46_EFFORTS[] = { "low", "medium", "high", "xhigh" };
const size_t RE_XAI_GROK46_EFFORTS_N = 4;
const char *const RE_XAI_LEGACY_EFFORTS[] = { "low", "medium", "high" };
const size_t RE_XAI_LEGACY_EFFORTS_N = 3;

/* Actual Computer relays (SGLang/vLLM). */
const char *const RE_ACTUAL_RELAY_EFFORTS[] = { "none", "low", "medium", "high", "max" };
const size_t RE_ACTUAL_RELAY_EFFORTS_N = 5;

/* Moonshot/Kimi. */
const char *const RE_KIMI_K3_EFFORTS[] = { "low", "high", "max" };
const size_t RE_KIMI_K3_EFFORTS_N = 3;
const char *const RE_KIMI_K2_EFFORTS[] = { "low", "medium", "high" };
const size_t RE_KIMI_K2_EFFORTS_N = 3;

/* OpenCode "Ox Alpha" stealth model (x-preview-f-free): always thinking. */
const char *const RE_OX_ALPHA_EFFORTS[] = { "low", "high", "max" };
const size_t RE_OX_ALPHA_EFFORTS_N = 3;

/* Tencent TokenHub. */
const char *const RE_TOKENHUB_EFFORTS[] = { "low", "medium", "high" };
const size_t RE_TOKENHUB_EFFORTS_N = 3;

/* GLM native reasoning_effort knobs. */
const char *const RE_GLM52_EFFORTS[] = { "high", "max" };
const size_t RE_GLM52_EFFORTS_N = 2;
const char *const RE_GLM53_EFFORTS[] = { "low", "medium", "high", "max" };
const size_t RE_GLM53_EFFORTS_N = 4;

/* DeepSeek V4 OpenAI-compat endpoint. */
const char *const RE_DEEPSEEK_V4_EFFORTS[] = { "low", "medium", "high", "max" };
const size_t RE_DEEPSEEK_V4_EFFORTS_N = 4;

/* Ollama Cloud /v1/chat/completions. */
const char *const RE_OLLAMA_CLOUD_EFFORTS[] = { "none", "low", "medium", "high", "max" };
const size_t RE_OLLAMA_CLOUD_EFFORTS_N = 5;

/* Meta Model API (Muse): minimal..xhigh; rejects ``none``. */
const char *const RE_META_AI_EFFORTS[] = { "minimal", "low", "medium", "high", "xhigh" };
const size_t RE_META_AI_EFFORTS_N = 5;

/* Upstage Solar Pro/Open. */
const char *const RE_SOLAR_EFFORTS[] = { "low", "medium", "high" };
const size_t RE_SOLAR_EFFORTS_N = 3;

/* ── Declared translation overrides ({from,to}) ─────────────────────────── */
const re_override_t RE_OX_ALPHA_OVERRIDES[] = { { "xhigh", "max" } };
const size_t RE_OX_ALPHA_OVERRIDES_N = 1;

/* Kimi K3 vendor-documented quirks: medium rounds to high (positional middle
 * + server default); xhigh rounds up to max. */
const re_override_t RE_KIMI_K3_OVERRIDES[] = { { "medium", "high" }, { "xhigh", "max" } };
const size_t RE_KIMI_K3_OVERRIDES_N = 2;

const re_override_t RE_GLM52_OVERRIDES[] = { { "xhigh", "max" } };
const size_t RE_GLM52_OVERRIDES_N = 1;
const re_override_t RE_GLM53_OVERRIDES[] = { { "xhigh", "max" } };
const size_t RE_GLM53_OVERRIDES_N = 1;
const re_override_t RE_DEEPSEEK_V4_OVERRIDES[] = { { "xhigh", "max" } };
const size_t RE_DEEPSEEK_V4_OVERRIDES_N = 1;
const re_override_t RE_OLLAMA_CLOUD_OVERRIDES[] = { { "xhigh", "max" } };
const size_t RE_OLLAMA_CLOUD_OVERRIDES_N = 1;

/* ── helpers ─────────────────────────────────────────────────────────────── */

static int ladder_index(const char *level)
{
    if (!level) return -1;
    for (size_t i = 0; i < RE_EFFORT_LADDER_N; i++)
        if (strcmp(RE_EFFORT_LADDER[i], level) == 0) return (int)i;
    return -1;
}

/* str.strip().lower() into a fixed buffer; returns NULL when empty result. */
static char *strip_lower(const char *s, char *out, size_t cap)
{
    if (!s) return NULL;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t o = 0;
    for (; *s && o + 1 < cap; s++)
        out[o++] = (char)tolower((unsigned char)*s);
    /* right-strip */
    while (o > 0 && (out[o-1] == ' ' || out[o-1] == '\t' ||
                     out[o-1] == '\n' || out[o-1] == '\r'))
        o--;
    out[o] = '\0';
    if (!o) return NULL;
    return out;
}

/*
 * PoP: _KIMI_K3_SLUG_RE.search @ agent/reasoning_effort.py:_KIMI_K3_SLUG_RE
 *
 * K3 slug detector — matches ``k3`` as a delimited token (``k3``,
 * ``k3-256k``, ``kimi-k3``, ``kimi-k3-cot``) without matching K2-era names
 * (``kimi-k2.6``). Regex: (?:^|[^a-z0-9])k3(?:[^a-z0-9]|$)
 */
static int kimi_k3_slug_match(const char *m)
{
    if (!m) return 0;
    size_t n = strlen(m);
    for (size_t i = 0; i + 2 <= n; i++) {
        if (strncmp(m + i, "k3", 2) != 0)
            continue;
        int left_ok  = (i == 0) || !(isalnum((unsigned char)m[i-1]));
        int right_ok = (i + 2 >= n) || !(isalnum((unsigned char)m[i+2]));
        if (left_ok && right_ok)
            return 1;
    }
    return 0;
}

/* ── public API ─────────────────────────────────────────────────────────── */

/* PoP: codex_supported_efforts @ agent/reasoning_effort.py:codex_supported_efforts */
const char *const *codex_supported_efforts(const char *model, size_t *out_n)
{
    const char *lo = model ? model : "";
    if (strstr(lo, "gpt-5.6") || strstr(lo, "GPT-5.6")) {
        /* Python lowercases first; do the same case-insensitive containment. */
    }
    char buf[256];
    if (model && strip_lower(model, buf, sizeof buf) && strstr(buf, "gpt-5.6")) {
        if (out_n) *out_n = RE_CODEX_GPT56_EFFORTS_N;
        return RE_CODEX_GPT56_EFFORTS;
    }
    if (out_n) *out_n = RE_CODEX_LEGACY_EFFORTS_N;
    return RE_CODEX_LEGACY_EFFORTS;
}

/* PoP: kimi_supported_efforts @ agent/reasoning_effort.py:kimi_supported_efforts */
const char *const *kimi_supported_efforts(const char *model, size_t *out_n)
{
    char lo[512];
    const char *slug = "";
    if (model && strip_lower(model, lo, sizeof lo)) {
        slug = strrchr(lo, '/');
        slug = slug ? slug + 1 : lo;
    }
    if (kimi_k3_slug_match(slug)) {
        if (out_n) *out_n = RE_KIMI_K3_EFFORTS_N;
        return RE_KIMI_K3_EFFORTS;
    }
    if (out_n) *out_n = RE_KIMI_K2_EFFORTS_N;
    return RE_KIMI_K2_EFFORTS;
}

/* PoP: clamp_effort @ agent/reasoning_effort.py:clamp_effort */
const char *clamp_effort(const char *effort,
                         const char *const *supported, size_t supported_n,
                         const re_override_t *overrides, size_t overrides_n)
{
    char requested_buf[64];
    char *requested = strip_lower(effort ? effort : "", requested_buf,
                                  sizeof requested_buf);
    if (!requested || !supported || supported_n == 0)
        return effort; /* unset stays unset / unknown wire passes through */

    /* Normalize + filter the supported set onto known ladder levels. */
    const char *supported_norm[64];
    size_t snorm = 0;
    for (size_t i = 0; i < supported_n && snorm < 64; i++) {
        char nb[64];
        char *lvl = strip_lower(supported[i] ? supported[i] : "", nb, sizeof nb);
        if (lvl && ladder_index(lvl) >= 0)
            supported_norm[snorm++] = RE_EFFORT_LADDER[ladder_index(lvl)];
    }
    if (snorm == 0)
        return effort;
    for (size_t i = 0; i < snorm; i++)
        if (strcmp(supported_norm[i], requested) == 0)
            return effort; /* already supported verbatim */

    if (overrides) {
        for (size_t i = 0; i < overrides_n; i++) {
            if (overrides[i].from && strcmp(overrides[i].from, requested) == 0 &&
                overrides[i].to) {
                for (size_t j = 0; j < snorm; j++)
                    if (strcmp(supported_norm[j], overrides[i].to) == 0)
                        return overrides[i].to; /* mapped value is supported */
            }
        }
    }

    int ridx = ladder_index(requested);
    if (ridx < 0)
        return effort; /* bespoke provider name — pass through rather than guess */

    /* "none" disables reasoning — never a degradation target for an enabled
     * ask; it still passes through verbatim when requested (checked above). */
    const char *candidates[64];
    size_t ncand = 0;
    for (size_t i = 0; i < snorm; i++)
        if (strcmp(supported_norm[i], "none") != 0)
            candidates[ncand++] = supported_norm[i];
    if (ncand == 0)
        return effort;

    /* nearest weaker supported level; fallback = weakest supported level. */
    int best_below = -1, best_below_idx = -1;
    int min_idx = -1;
    for (size_t i = 0; i < ncand; i++) {
        int idx = ladder_index(candidates[i]);
        if (min_idx < 0 || idx < min_idx)
            min_idx = idx;
        if (idx < ridx && idx > best_below_idx) {
            best_below_idx = idx;
            best_below = (int)i;
        }
    }
    if (best_below >= 0)
        return candidates[best_below];
    for (size_t i = 0; i < ncand; i++)
        if (ladder_index(candidates[i]) == min_idx)
            return candidates[i];
    return effort; /* unreachable */
}

/* PoP: requested_effort @ agent/reasoning_effort.py:requested_effort */
char *requested_effort(const void *reasoning_config_v)
{
    const json_t *cfg = (const json_t *)reasoning_config_v;
    if (!cfg || cfg->type != JSON_OBJECT)
        return NULL;
    json_t *enabled = json_obj_get((json_t *)cfg, "enabled");
    if (enabled && enabled->type == JSON_BOOL && enabled->bool_val == 0)
        return NULL; /* reasoning explicitly disabled */
    json_t *eff = json_obj_get((json_t *)cfg, "effort");
    const char *raw = (eff && eff->type == JSON_STRING) ? eff->str_val : "";
    char buf[64];
    char *e = strip_lower(raw ? raw : "", buf, sizeof buf);
    if (!e)
        return NULL;
    return strdup(e);
}
