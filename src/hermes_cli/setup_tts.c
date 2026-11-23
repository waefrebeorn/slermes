/*
 * setup_tts.c — C port of upstream Hermes hermes_cli/setup_tts.py.
 *
 * PoP: w7_setuptts_e5dee4_provider_label @ hermes_cli/setup_tts.py:_TTS_PROVIDER_LABELS
 * PoP: w7_setuptts_e5dee4_all_labels @ hermes_cli/setup_tts.py:_TTS_PROVIDER_LABELS
 * PoP: w7_setuptts_e5dee4_tts_api_key_step @ hermes_cli/setup_tts.py:_tts_api_key_step
 *
 * The interactive wizard surface (_setup.*, pm, OAuth) is injected through the
 * env / prompt_value seams; this port covers the deterministic decision logic.
 */
#include "noc_w7_setuptts_e5dee4.h"
#include "hermes_json.h"
#include <string.h>

struct Provider { const char *key; const char *label; };

static const struct Provider PROVIDERS[] = {
    {"edge", "Edge TTS"},
    {"elevenlabs", "ElevenLabs"},
    {"openai", "OpenAI TTS"},
    {"xai", "xAI TTS"},
    {"minimax", "MiniMax TTS"},
    {"mistral", "Mistral Voxtral TTS"},
    {"gemini", "Google Gemini TTS"},
    {"neutts", "NeuTTS"},
    {"kittentts", "KittenTTS"},
};
#define NPROV ((int)(sizeof(PROVIDERS) / sizeof(PROVIDERS[0])))

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

struct ApiKeyProvider {
    const char *key;
    const char *const *env_vars;
    int n_env_vars;
};
static const char *const ENV_ELEVENLABS[] = {"ELEVENLABS_API_KEY"};
static const char *const ENV_OPENAI[] = {"VOICE_TOOLS_OPENAI_KEY", "OPENAI_API_KEY"};
static const char *const ENV_MINIMAX[] = {"MINIMAX_API_KEY"};
static const char *const ENV_MISTRAL[] = {"MISTRAL_API_KEY"};
static const char *const ENV_GEMINI[] = {"GEMINI_API_KEY", "GOOGLE_API_KEY"};
static const struct ApiKeyProvider API_KEY_PROVIDERS[] = {
    {"elevenlabs", ENV_ELEVENLABS, 1},
    {"openai", ENV_OPENAI, 2},
    {"minimax", ENV_MINIMAX, 1},
    {"mistral", ENV_MISTRAL, 1},
    {"gemini", ENV_GEMINI, 2},
};

struct json_t *w7_setuptts_e5dee4_provider_label(const char *key) {
    if (!key) return json_null();
    for (int i = 0; i < NPROV; i++)
        if (!strcmp(PROVIDERS[i].key, key)) return json_string(PROVIDERS[i].label);
    return json_null();
}

struct json_t *w7_setuptts_e5dee4_all_labels(void) {
    json_t *obj = json_object();
    for (int i = 0; i < NPROV; i++)
        json_set(obj, PROVIDERS[i].key, json_string(PROVIDERS[i].label));
    return obj;
}

struct json_t *w7_setuptts_e5dee4_tts_api_key_step(const char *selected,
                                                  const json_t *env,
                                                  const json_t *prompt_value) {
    const struct ApiKeyProvider *p = NULL;
    if (selected) {
        for (size_t i = 0; i < sizeof(API_KEY_PROVIDERS) / sizeof(API_KEY_PROVIDERS[0]); i++)
            if (!strcmp(API_KEY_PROVIDERS[i].key, selected)) { p = &API_KEY_PROVIDERS[i]; break; }
    }
    if (!p) return json_string("edge");
    /* if any(_setup.get_env_value(v) for v in env_vars): return selected */
    for (int i = 0; i < p->n_env_vars; i++)
        if (truthy(json_obj_get(env, p->env_vars[i]))) return json_string(selected);
    /* api_key = _setup.prompt(...); if api_key: return selected */
    if (truthy(prompt_value)) return json_string(selected);
    return json_string("edge");
}
