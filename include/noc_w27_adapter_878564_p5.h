/*
 * noc_w27_adapter_878564_p5.h - C surface for upstream
 * plugins/platforms/photon/adapter.py (gate w27_adapter_878564_p5).
 *
 * Gate 5 of 5: the MENTION and PRESENCE gates — wake-word pattern
 * normalization/matching, the leading-mention strip, the gate-time STT probe,
 * the boolean env ladders and the rich-link preview window.  The compiled regex
 * objects and PHOTON_* env reads are seams; the shipped default wake-word
 * grammar (see adapter.c) is what is matched here.
 */
#ifndef NOC_W27_ADAPTER_878564_P5_H
#define NOC_W27_ADAPTER_878564_P5_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PhotonAdapter._compile_mention_patterns' normalization ladder: the pattern
 * TEXTS re.compile would receive (defaults when raw is null). */
json_t *w27_adapter_878564_p5_mention_pattern_texts(const json_t *raw);

/* _message_matches_mention_patterns: any pattern.search hits.  A null/empty
 * `patterns` array means the shipped defaults. */
int w27_adapter_878564_p5_mention_match(const char *text, const json_t *patterns);

/* _clean_mention_text: strip a leading wake word only, then leading " ,:-";
 * falls back to the original text when nothing usable remains.  Caller frees. */
char *w27_adapter_878564_p5_clean_mention_text(const char *text, const json_t *patterns);

/* _gate_stt_chat: {"chat", "transcribes"} — group + require_mention + a voice
 * payload + the voice gate enabled. */
json_t *w27_adapter_878564_p5_gate_stt_chat(const char *space_id, const char *space_type,
                                            const json_t *content, int require_mention,
                                            int voice_gate_enabled);

/* __init__: extra["require_mention"], else the PHOTON_REQUIRE_MENTION value;
 * str(x).strip().lower() in {true,1,yes,on}. */
int w27_adapter_878564_p5_require_mention_flag(const json_t *value, const json_t *env);

/* _markdown_enabled: default "true"; off for {false,0,no}. */
int w27_adapter_878564_p5_markdown_enabled(const json_t *value);

/* _reactions_enabled: default "false"; on for {true,1,yes,on}. */
int w27_adapter_878564_p5_reactions_enabled(const json_t *value);

/* __init__: default "true"; off for exactly {0,false,no} — no strip(). */
int w27_adapter_878564_p5_autostart_enabled(const json_t *value);

/* _is_recent_richlink_preview: {"hit","popped"} — the 30 s window over a
 * preview-art payload, popping the stale entry. */
json_t *w27_adapter_878564_p5_is_recent_richlink_preview(const char *chat_id,
                                                         const json_t *content,
                                                         const json_t *last_ts, double now);

/* _record_recent_richlink's predicate: {"recorded","key"} — only a URL-only
 * text in a truthy chat records. */
json_t *w27_adapter_878564_p5_record_recent_richlink(const char *chat_id, const char *text);

#ifdef __cplusplus
}
#endif
#endif
