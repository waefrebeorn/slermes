/*
 * config_defaults_voice.c — real C port of upstream Hermes hermes_cli/config_defaults_voice.py constant tables.
 * PoP: cn_640c50c0d5_constants @ hermes_cli/config_defaults_voice.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_640c50c0d5.h"

json_t *cn_640c50c0d5_constants(void) {
    json_t *out = json_object();
    json_set(out, "VOICE_DEFAULTS", ({ json_t *__o = json_object();
        json_set(__o, "auto_tts", json_bool(false));
        json_set(__o, "auto_tts_mode", json_string("all"));
        json_set(__o, "barge_in", json_bool(true));
        json_set(__o, "barge_in_grace_seconds", json_number(0.5));
        json_set(__o, "barge_in_threshold_multiplier", json_number(3));
        json_set(__o, "beep_enabled", json_bool(true));
        json_set(__o, "beep_volume", json_number(0.29999999999999999));
        json_set(__o, "client_direct", json_bool(true));
        json_set(__o, "gpt_live", ({ json_t *__o = json_object();
            json_set(__o, "instructions", json_string(""));
            json_set(__o, "model", json_string("gpt-live-1"));
            json_set(__o, "voice", json_string("marin"));
          __o; }));
        json_set(__o, "max_recording_seconds", json_number(120));
        json_set(__o, "record_key", json_string("ctrl+b"));
        json_set(__o, "silence_duration", json_number(3));
        json_set(__o, "silence_threshold", json_number(200));
        json_set(__o, "stop_phrases", ({ json_t *__a = json_array();
            json_append(__a, json_string("stop"));
          __a; }));
        json_set(__o, "submit_mode", json_string("direct"));
        json_set(__o, "thinking_sound", json_bool(true));
        json_set(__o, "voice_chat_mode", json_string("chained"));
      __o; }));
    return out;
}
