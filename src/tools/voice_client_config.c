/* voice_client_config.c — client-direct voice config resolver (pure core).
 *
 * Port of Python tools/voice_client_config.py decision ladders. The LIVE
 * module's credential resolvers are env/key-store coupled; this port takes
 * the resolved inputs as parameters and reproduces the verdict logic:
 *   _client_direct_enabled value parsing (bool / string truthiness)
 *   _relay verdict shape  {"mode":"relay","reason":...}
 *   STT relay ladder: disabled → local → command/plugin → credentials
 */

#include "voice_client_config.h"
#include "hermes_json.h"

#include <strings.h>
#include <stdlib.h>

/* PoP: _client_direct_enabled (value parse) @ tools/voice_client_config.py:_client_direct_enabled */
bool vcc_client_direct_value(const json_t *value)
{
    if (!value || value->type == JSON_NULL) return true;
    if (value->type == JSON_BOOL) return value->bool_val != 0;
    if (value->type == JSON_STRING && value->str_val) {
        /* value.strip().lower() not in {"0","false","no","off"} */
        const char *s = value->str_val;
        return !(strcasecmp(s, "0") == 0 || strcasecmp(s, "false") == 0 ||
                 strcasecmp(s, "no") == 0 || strcasecmp(s, "off") == 0);
    }
    /* numbers/other truthy types follow Python bool() semantics: 0 is false */
    if (value->type == JSON_NUMBER) return value->num_val != 0.0;
    return true;
}

/* PoP: _relay @ tools/voice_client_config.py:_relay */
json_t *vcc_relay(const char *reason)
{
    json_t *o = json_object();
    if (!o) return NULL;
    json_set(o, "mode", json_string("relay"));
    json_set(o, "reason", json_string(reason ? reason : ""));
    return o;
}

json_t *vcc_disabled_verdict(void)
{
    return vcc_relay("voice.client_direct disabled");
}

/* PoP: _resolve_stt_client_config relay ladder @ tools/voice_client_config.py:_resolve_stt_client_config */
const char *vcc_stt_relay_reason(const char *provider, bool stt_enabled,
                                 bool is_local, bool in_builtin,
                                 bool has_credentials)
{
    if (!stt_enabled) return "stt disabled";
    if (is_local) return "local provider";
    if (!in_builtin) return "command/plugin provider";
    /* Cloud providers with a key requirement: groq/openai/mistral/xai/
     * elevenlabs/deepinfra all relay on missing credentials. openai's
     * resolver failure also maps here via its own message upstream. */
    if (!has_credentials) return "no credentials";
    (void)provider;
    return NULL; /* qualifies for direct wire */
}
