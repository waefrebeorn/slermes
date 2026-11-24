/*
 * replay.c - C11 port of upstream agent/prompt_cache_lifecycle/replay.py
 * (gate w24_replay_2d5a10).
 *
 * PoP: w24_replay_2d5a10_has_cache_markers @ agent/prompt_cache_lifecycle/replay.py:has_cache_markers
 * PoP: w24_replay_2d5a10_is_replayable @ agent/prompt_cache_lifecycle/replay.py:is_replayable
 * PoP: w24_replay_2d5a10_output_cap @ agent/prompt_cache_lifecycle/replay.py:replay_output_cap
 * PoP: w24_replay_2d5a10_bounded_chat_replay @ agent/prompt_cache_lifecycle/replay.py:bounded_chat_replay
 * PoP: w24_replay_2d5a10_cap_honoured @ agent/prompt_cache_lifecycle/replay.py:replay_cap_honoured
 * PoP: w24_replay_2d5a10_adaptive @ agent/anthropic_adapter.py:_supports_adaptive_thinking
 * PoP: w24_replay_2d5a10_constants @ agent/prompt_cache_lifecycle/replay.py:WARMABLE_API_MODES
 *
 * Ported: the whole decision surface.  The seams are the SDK objects: no
 * client is built, ``replay_cap_honoured`` reads a descriptor
 * ({"kind", "effective", "sdk"}) standing in for
 * ``isinstance(client, _sdk_client_types(api_mode))`` and for a
 * ``callable(client.effective_output_cap)``.  ``_supports_adaptive_thinking``
 * is ported outright (it is a pure model-name predicate) so the Claude +
 * reasoning branch stays comparable against the live adapter.
 *
 * Python ``dict.get`` order semantics are preserved where they matter: for the
 * non-Chat-Completions replay, ``cap`` is ``max_completion_tokens`` when the
 * caller already set it, and only then ``max_tokens`` (which the replay forces
 * to 1).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_replay_2d5a10.h"

/* _WARMABLE_API_MODES */
static const char *const WARMABLE_MODES[] = {"anthropic_messages", "chat_completions"};

/* _EXTRA_BODY_OUTPUT_CAPS */
static const char *const EXTRA_BODY_OUTPUT_CAPS[] = {
    "max_tokens", "max_completion_tokens", "max_output_tokens",
};
#define N_EXTRA_BODY_CAPS 3

/* _LEGACY_MANUAL_THINKING_CLAUDE_SUBSTRINGS */
static const char *const LEGACY_MANUAL_THINKING[] = {
    "claude-3", "claude-opus-4-0", "claude-opus-4.0", "claude-opus-4-1", "claude-opus-4.1",
    "claude-sonnet-4-0", "claude-sonnet-4.0", "claude-opus-4-2025", "claude-sonnet-4-2025",
    "claude-opus-4-5", "claude-opus-4.5", "claude-sonnet-4-5", "claude-sonnet-4.5",
    "claude-haiku-4-5", "claude-haiku-4.5",
};
#define N_LEGACY (sizeof(LEGACY_MANUAL_THINKING) / sizeof(LEGACY_MANUAL_THINKING[0]))

/* _KIMI_FAMILY_MODEL_PREFIXES / _KIMI_FAMILY_EXACT_SLUGS */
static const char *const KIMI_PREFIXES[] = {
    "kimi-", "kimi_", "moonshot-", "moonshot_", "k1.", "k1-", "k2.", "k2-",
    "k25", "k2.5", "k3.", "k3-",
};
#define N_KIMI_PREFIXES (sizeof(KIMI_PREFIXES) / sizeof(KIMI_PREFIXES[0]))

static const char *as_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static char *xdup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    if (n) memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str(v or "") for the shapes a model field or a cap can hold. */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!truthy(v)) return xdup("");
    switch (v->type) {
    case JSON_STRING: return xdup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return xdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return xdup(buf);
    default: return xdup("");   /* container repr is a seam, never used here */
    }
}

static char *lower_dup(const char *s) {
    char *o = xdup(s ? s : "");
    size_t i;
    if (!o) return NULL;
    for (i = 0; o[i]; i++)
        if (o[i] >= 'A' && o[i] <= 'Z') o[i] = (char)(o[i] - 'A' + 'a');
    return o;
}

/* ------------------------------------------------------- cache markers */

static int walk_markers(const json_t *v) {
    size_t i, n;
    if (!v) return 0;
    if (v->type == JSON_OBJECT) {
        if (json_obj_get(v, "cache_control") != NULL) return 1;
        n = json_object_size(v);
        for (i = 0; i < n; i++)
            if (walk_markers(json_object_get_at(v, i))) return 1;
        return 0;
    }
    if (v->type == JSON_ARRAY) {
        n = json_len(v);
        for (i = 0; i < n; i++)
            if (walk_markers(json_get(v, i))) return 1;
    }
    return 0;
}

json_t *w24_replay_2d5a10_has_cache_markers(const json_t *api_kwargs) {
    static const char *const KEYS[3] = {"system", "tools", "messages"};
    size_t i;
    if (!api_kwargs || api_kwargs->type != JSON_OBJECT) return json_string("ERROR:attribute");
    for (i = 0; i < 3; i++)
        if (walk_markers(json_obj_get(api_kwargs, KEYS[i]))) return json_bool(1);
    return json_bool(0);
}

/* ------------------------------------------- adaptive thinking predicate */

static int model_matches(const char *lower, const char *const *subs, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        if (strstr(lower, subs[i])) return 1;
    return 0;
}

static int kimi_family(const char *model) {
    char *m, *s;
    const char *tail;
    size_t i;
    int hit = 0;
    if (!model) return 0;
    m = lower_dup(model);
    if (!m) return 0;
    /* .strip() */
    s = m;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\v' || *s == '\f') s++;
    {
        size_t l = strlen(s);
        while (l && (s[l - 1] == ' ' || s[l - 1] == '\t' || s[l - 1] == '\n' ||
                     s[l - 1] == '\r' || s[l - 1] == '\v' || s[l - 1] == '\f')) s[--l] = '\0';
    }
    tail = strrchr(s, '/');           /* rsplit("/", 1)[-1] */
    tail = tail ? tail + 1 : s;
    if (*tail) {
        if (!strcmp(tail, "k3")) hit = 1;
        for (i = 0; !hit && i < N_KIMI_PREFIXES; i++)
            if (strncmp(tail, KIMI_PREFIXES[i], strlen(KIMI_PREFIXES[i])) == 0) hit = 1;
    }
    free(m);
    return hit;
}

static int supports_adaptive(const char *model) {
    char *lower;
    int claude, legacy;
    if (!model) return 0;
    lower = lower_dup(model);
    if (!lower) return 0;
    claude = strstr(lower, "claude") != NULL;
    legacy = model_matches(lower, LEGACY_MANUAL_THINKING, N_LEGACY);
    free(lower);
    return kimi_family(model) || (claude && !legacy);
}

json_t *w24_replay_2d5a10_adaptive(const json_t *model) {
    if (model && model->type != JSON_NULL && model->type != JSON_STRING) return json_bool(0);
    return json_bool(supports_adaptive(as_str(model)));
}

/* -------------------------------------------------------- is_replayable */

json_t *w24_replay_2d5a10_is_replayable(const json_t *api_kwargs) {
    const json_t *thinking, *extra, *reasoning, *model_node, *enabled_node, *effort_node;
    char *model;
    json_t *out;

    if (!api_kwargs || api_kwargs->type != JSON_OBJECT) return json_string("ERROR:attribute");

    thinking = json_obj_get(api_kwargs, "thinking");
    if (thinking && thinking->type == JSON_OBJECT) {
        const char *t = as_str(json_obj_get(thinking, "type"));
        if (t && !strcmp(t, "enabled") && json_obj_get(thinking, "budget_tokens") != NULL)
            return json_bool(0);
    }

    extra = json_obj_get(api_kwargs, "extra_body");
    if (truthy(extra) && extra->type != JSON_OBJECT) return json_string("ERROR:attribute");
    reasoning = truthy(extra) ? json_obj_get(extra, "reasoning") : NULL;

    model_node = json_obj_get(api_kwargs, "model");
    model = py_str(model_node);
    if (!model) return json_null();

    {
        char *lower = lower_dup(model);
        int claude = lower && strstr(lower, "claude") != NULL;
        free(lower);
        if (claude && reasoning && reasoning->type == JSON_OBJECT) {
            enabled_node = json_obj_get(reasoning, "enabled");
            effort_node = json_obj_get(reasoning, "effort");
            {
                int enabled = enabled_node ? truthy(enabled_node) : 1;      /* .get("enabled", True) */
                const char *effort = as_str(effort_node);
                int effort_is_none = effort && !strcmp(effort, "none");
                if (enabled && !effort_is_none) {
                    out = json_bool(supports_adaptive(model));
                    free(model);
                    return out;
                }
            }
        }
    }
    free(model);
    return json_bool(1);
}

/* --------------------------------------------------------- output caps */

/* _OPENAI_REASONING_MODEL = (^|/)(gpt-[5-9]|o[1-9]) */
static int openai_reasoning_model(const char *s) {
    size_t i, n = strlen(s);
    for (i = 0; i < n; i++) {
        if (i != 0 && s[i - 1] != '/') continue;
        if (i + 4 <= n && !strncmp(s + i, "gpt-", 4) && s[i + 4] >= '5' && s[i + 4] <= '9')
            return 1;
        if (i + 1 <= n && s[i] == 'o' && s[i + 1] >= '1' && s[i + 1] <= '9') return 1;
    }
    return 0;
}

json_t *w24_replay_2d5a10_output_cap(const json_t *model) {
    char *m, *lower;
    int cap;
    if (model && model->type != JSON_NULL && model->type != JSON_STRING) return json_string("ERROR:attribute");
    m = xdup(as_str(model) ? as_str(model) : "");
    if (!m) return json_null();
    lower = lower_dup(m);
    cap = (lower && openai_reasoning_model(lower)) ? 16 : 1;
    free(m);
    free(lower);
    return json_number((double)cap);
}

json_t *w24_replay_2d5a10_bounded_chat_replay(const json_t *api_kwargs, const json_t *model) {
    json_t *out, *extra, *new_extra, *cap;
    size_t i, n;
    int has_mct = 0;

    if (!api_kwargs || api_kwargs->type != JSON_OBJECT) return json_string("ERROR:attribute");
    cap = w24_replay_2d5a10_output_cap(model);
    if (cap && cap->type == JSON_STRING) return cap;     /* ERROR:attribute passthrough */

    out = json_object();
    n = json_object_size(api_kwargs);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(api_kwargs, i);
        if (!k) continue;
        if (!strcmp(k, "stream") || !strcmp(k, "stream_options")) continue;
        if (!strcmp(k, "max_completion_tokens")) has_mct = 1;
        json_set(out, k, json_copy(json_object_get_at(api_kwargs, i)));
    }
    json_set(out, has_mct ? "max_completion_tokens" : "max_tokens", cap);

    extra = json_obj_get(out, "extra_body");
    if (extra && extra->type == JSON_OBJECT) {
        int lift = 0;
        for (i = 0; i < N_EXTRA_BODY_CAPS; i++)
            if (json_obj_get(extra, EXTRA_BODY_OUTPUT_CAPS[i])) lift = 1;
        if (lift) {
            new_extra = json_object();
            n = json_object_size(extra);
            for (i = 0; i < n; i++) {
                const char *k = json_object_get_key_at(extra, i);
                size_t j;
                int drop = 0;
                if (!k) continue;
                for (j = 0; j < N_EXTRA_BODY_CAPS; j++)
                    if (!strcmp(k, EXTRA_BODY_OUTPUT_CAPS[j])) drop = 1;
                if (!drop) json_set(new_extra, k, json_copy(json_object_get_at(extra, i)));
            }
            json_set(out, "extra_body", new_extra);   /* frees the old value */
        }
    }
    return out;
}

/* ------------------------------------------------------ cap honoured */

json_t *w24_replay_2d5a10_cap_honoured(const json_t *client, const json_t *api_kwargs,
                                       const json_t *model, const json_t *api_mode) {
    const char *mode = as_str(api_mode);
    const char *kind;
    json_t *replay, *cap_node;
    double cap;
    const json_t *eff;
    const json_t *sdk;

    if (!api_kwargs || api_kwargs->type != JSON_OBJECT) return json_string("ERROR:attribute");
    if (mode && !strcmp(mode, "chat_completions")) {
        replay = w24_replay_2d5a10_bounded_chat_replay(api_kwargs, model);
    } else {
        /* {**api_kwargs, "max_tokens": 1} */
        size_t i, n = json_object_size(api_kwargs);
        replay = json_object();
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(api_kwargs, i);
            if (!k) continue;
            json_set(replay, k, json_copy(json_object_get_at(api_kwargs, i)));
        }
        json_set(replay, "max_tokens", json_number(1));
    }
    if (replay && replay->type == JSON_STRING) return replay;   /* ERROR passthrough */

    cap_node = json_obj_get(replay, "max_completion_tokens");
    if (!cap_node) cap_node = json_obj_get(replay, "max_tokens");
    if (!cap_node || (cap_node->type != JSON_NUMBER && cap_node->type != JSON_BOOL)) {
        json_free(replay);
        return json_string("ERROR:type");
    }
    cap = cap_node->type == JSON_BOOL ? (cap_node->bool_val ? 1.0 : 0.0) : cap_node->num_val;

    eff = json_obj_get(client, "effective");
    if (eff && (eff->type == JSON_NUMBER || eff->type == JSON_BOOL)) {
        double v = eff->type == JSON_BOOL ? (eff->bool_val ? 1.0 : 0.0) : eff->num_val;
        json_free(replay);
        return json_bool(v <= cap);
    }

    kind = as_str(json_obj_get(client, "kind"));
    sdk = json_obj_get(client, "sdk");
    if (!kind) kind = "other";
    {
        int sdk_openai = 1, sdk_anthropic = 1, ok = 0;
        if (sdk && sdk->type == JSON_OBJECT) {
            const json_t *a = json_obj_get(sdk, "openai");
            const json_t *b = json_obj_get(sdk, "anthropic");
            if (a) sdk_openai = truthy(a);
            if (b) sdk_anthropic = truthy(b);
        }
        if (mode && !strcmp(mode, "chat_completions")) {
            ok = !strcmp(kind, "openai") && sdk_openai;
        } else {
            ok = sdk_anthropic &&
                 (!strcmp(kind, "anthropic") || !strcmp(kind, "anthropic_bedrock") ||
                  !strcmp(kind, "anthropic_vertex"));
        }
        json_free(replay);
        return json_bool(ok);
    }
}

json_t *w24_replay_2d5a10_constants(void) {
    json_t *o = json_object(), *modes = json_array(), *caps = json_array();
    size_t i;
    for (i = 0; i < 2; i++) json_append(modes, json_string(WARMABLE_MODES[i]));
    for (i = 0; i < N_EXTRA_BODY_CAPS; i++) json_append(caps, json_string(EXTRA_BODY_OUTPUT_CAPS[i]));
    json_set(o, "warmable_api_modes", modes);
    json_set(o, "extra_body_output_caps", caps);
    json_set(o, "openai_reasoning_pattern", json_string("(^|/)(gpt-[5-9]|o[1-9])"));
    return o;
}
