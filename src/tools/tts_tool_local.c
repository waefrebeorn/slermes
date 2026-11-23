/*
 * tts_tool_local.c — C home for upstream Hermes tools/tts_tool_local.py.
 * PoP: w7_ttstoolloc_4467f6_constants @ tools/tts_tool_local.py:<module constants>
 * PoP: w7_ttstoolloc_4467f6_voice_default @ tools/tts_tool_local.py:_resolve_piper_voice_path
 * PoP: w7_ttstoolloc_4467f6_cache_get_or_load @ tools/tts_tool_local.py:_tts_cache_get_or_load
 * PoP: w7_ttstoolloc_4467f6_piper_cache_key @ tools/tts_tool_local.py:_load_piper_voice_for_config
 * PoP: w7_ttstoolloc_4467f6_piper_speaker_id @ tools/tts_tool_local.py:_generate_piper_tts
 * PoP: w7_ttstoolloc_4467f6_has_advanced_knobs @ tools/tts_tool_local.py:_generate_piper_tts
 *
 * Cache state and model/voice paths come from the fixture (the live module uses
 * process-global dicts and the filesystem); only deterministic logic is ported.
 */
#include "noc_w7_ttstoolloc_4467f6.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const KNOBS[] = {
    "length_scale", "noise_scale", "noise_w_scale", "volume", "normalize_audio", "speaker_id"};

static int in_list(const char *const *list, size_t n, const char *s) {
    size_t i;
    if (!s) return 0;
    for (i = 0; i < n; i++) if (!strcmp(list[i], s)) return 1;
    return 0;
}
static const char *jstr(json_t *v) { return (v && v->type == JSON_STRING) ? v->str_val : NULL; }

json_t *w7_ttstoolloc_4467f6_constants(void) {
    json_t *o = json_object();
    json_t *a = json_array();
    size_t i;
    json_set(o, "DEFAULT_KITTENTTS_MODEL", json_string("KittenML/kitten-tts-nano-0.8-int8"));
    json_set(o, "DEFAULT_KITTENTTS_VOICE", json_string("Jasper"));
    json_set(o, "DEFAULT_PIPER_VOICE", json_string("en_US-lessac-medium"));
    json_set(o, "_TTS_MODEL_CACHE_MAX", json_number(3));
    for (i = 0; i < sizeof(KNOBS)/sizeof(KNOBS[0]); i++) json_append(a, json_string(KNOBS[i]));
    json_set(o, "_PIPER_ADVANCED_KNOBS", a);
    return o;
}
json_t *w7_ttstoolloc_4467f6_voice_default(json_t *voice) {
    const char *v = jstr(voice);
    return json_string((v && *v) ? v : "en_US-lessac-medium");
}
json_t *w7_ttstoolloc_4467f6_cache_get_or_load(json_t *entries, const char *key,
                                              json_t *load_value, long max_entries) {
    const char **keys;
    json_t **vals;
    size_t n = 0, cap, i, found = (size_t)-1;
    int loaded = 0;
    json_t *result, *cache_arr, *out;
    if (!key) key = "";
    cap = (entries && entries->type == JSON_ARRAY) ? json_len(entries) + 2 : 2;
    keys = (const char **)malloc(sizeof(char *) * cap);
    vals = (json_t **)malloc(sizeof(json_t *) * cap);
    if (!keys || !vals) { free(keys); free(vals); return json_null(); }
    if (entries && entries->type == JSON_ARRAY) {
        for (i = 0; i < json_len(entries); i++) {
            json_t *e = json_get(entries, i);
            const char *k;
            if (!e || e->type != JSON_ARRAY || json_len(e) < 2) continue;
            k = jstr(json_get(e, 0));
            if (!k) continue;
            keys[n] = k;
            vals[n] = json_get(e, 1);
            n++;
        }
    }
    for (i = 0; i < n; i++) if (!strcmp(keys[i], key)) { found = i; break; }
    if (found != (size_t)-1) {
        result = json_copy(vals[found]);
        /* hit refreshes recency: pop + reinsert at the end */
        for (i = found; i + 1 < n; i++) { keys[i] = keys[i+1]; vals[i] = vals[i+1]; }
        n--;
        keys[n] = key;
        vals[n] = result;
        n++;
    } else {
        loaded = 1;
        result = json_copy(load_value);
        keys[n] = key;
        vals[n] = result;
        n++;
        while ((long)n > max_entries && n > 0) {
            for (i = 0; i + 1 < n; i++) { keys[i] = keys[i+1]; vals[i] = vals[i+1]; }
            n--;
        }
    }
    cache_arr = json_array();
    for (i = 0; i < n; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(keys[i]));
        json_append(pair, json_copy(vals[i]));
        json_append(cache_arr, pair);
    }
    out = json_object();
    json_set(out, "value", json_copy(result));
    json_set(out, "cache", cache_arr);
    json_set(out, "loaded", json_bool(loaded ? true : false));
    json_free(result);
    free(keys); free(vals);
    return out;
}
json_t *w7_ttstoolloc_4467f6_piper_cache_key(const char *model_path, int use_cuda) {
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s::cuda=%s", model_path ? model_path : "",
             use_cuda ? "True" : "False");
    return json_string(buf);
}
json_t *w7_ttstoolloc_4467f6_piper_speaker_id(json_t *raw) {
    long long v = 0;
    if (raw && raw->type == JSON_STRING && raw->str_val) v = strtoll(raw->str_val, NULL, 10);
    return json_number((double)v);
}
json_t *w7_ttstoolloc_4467f6_has_advanced_knobs(json_t *keys) {
    size_t i;
    if (!keys || keys->type != JSON_ARRAY) return json_bool(false);
    for (i = 0; i < json_len(keys); i++)
        if (in_list(KNOBS, sizeof(KNOBS)/sizeof(KNOBS[0]), jstr(json_get(keys, i)))) return json_bool(true);
    return json_bool(false);
}
