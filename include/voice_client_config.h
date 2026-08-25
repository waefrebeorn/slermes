#ifndef HERMES_VOICE_CLIENT_CONFIG_H
#define HERMES_VOICE_CLIENT_CONFIG_H

/*
 * voice_client_config.h — client-direct voice config resolver (pure core).
 *
 * Port of Python tools/voice_client_config.py. The LIVE module reads
 * credentials via tools.transcription_tools / tools.tts_tool resolvers;
 * those are env/key-store coupled, so the C port takes the RESOLVED inputs
 * as parameters and reproduces the decision ladder + wire-shape verdicts.
 */

#include <stdbool.h>
#include <stddef.h>

typedef struct json_t json_t;

/* Wire shapes the desktop knows how to speak. */
#define VCC_STT_WIRE_OPENAI     "openai-multipart"
#define VCC_STT_WIRE_XAI        "xai-stt"
#define VCC_STT_WIRE_ELEVENLABS "elevenlabs-stt"
#define VCC_TTS_WIRE_OPENAI     "openai-speech"
#define VCC_TTS_WIRE_ELEVENLABS "elevenlabs-tts"

/* Port of _client_direct_enabled's VALUE parsing: given the raw
 * voice.client_direct config value (may be NULL = unset → default true,
 * bool node, or string node), decide whether client-direct is on. */
bool vcc_client_direct_value(const json_t *value);

/* Port of _relay: {"mode":"relay","reason":reason}. malloc'd json_t. */
json_t *vcc_relay(const char *reason);

/* Port of resolve_client_voice_config's gate: when client-direct is
 * disabled BOTH directions report relay("voice.client_direct disabled"). */
json_t *vcc_disabled_verdict(void);

/* Port of the STT classification ladder over a pre-resolved provider name:
 * returns the relay reason for non-direct providers, or NULL when the
 * provider qualifies for a direct wire. `provider` is tt._get_provider()'s
 * output; `is_local` is tt._is_local_stt_provider()'s verdict; `in_builtin`
 * is membership in BUILTIN_STT_PROVIDERS; `has_credentials` mirrors
 * "no credentials" relay branches. */
const char *vcc_stt_relay_reason(const char *provider, bool stt_enabled,
                                 bool is_local, bool in_builtin,
                                 bool has_credentials);

#endif /* HERMES_VOICE_CLIENT_CONFIG_H */
