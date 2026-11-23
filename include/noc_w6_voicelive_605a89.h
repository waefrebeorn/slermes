#ifndef NOC_W6_VOICELIVE_605A89_H
#define NOC_W6_VOICELIVE_605A89_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_voicelive_605a89_voice_live_turn_note @ tools/voice_live.py:voice_live_turn_note
 * Caller frees the returned string. */
char *w6_voicelive_605a89_voice_live_turn_note(const char *context);

/* PoP: w6_voicelive_605a89_voice_chat_mode @ tools/voice_live.py:voice_chat_mode
 * Returns "gpt-live" or "chained" (static, do not free). */
const char *w6_voicelive_605a89_voice_chat_mode(const struct json_t *voice);

/* PoP: w6_voicelive_605a89_live_instructions @ tools/voice_live.py:live_instructions
 * Caller frees the returned string. */
char *w6_voicelive_605a89_live_instructions(const struct json_t *live);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W6_VOICELIVE_605A89_H */
