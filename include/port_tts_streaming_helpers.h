/*
 * port_tts_streaming_helpers.h — declarations for the C port of
 * tools/tts_streaming.py: SentenceChunker + StreamingTTSProvider resolution.
 */
#ifndef PORT_TTS_STREAMING_HELPERS_H
#define PORT_TTS_STREAMING_HELPERS_H

#include <stdbool.h>
#include "libjson/json.h"

/* ── StreamingTTSProvider vtable (mirror tts_streaming.py:StreamingTTSProvider) ─ */
typedef struct {
    int sample_rate;
    int channels;
    int sample_width;
    /* Stream PCM chunks for text. Returns number of chunks yielded into
     * out_chunks[] (each a malloc'd buffer + length), or -1 on error.
     * Caller frees each chunk buffer. */
    int (*stream)(void *provider_ctx, const char *text,
                  void **out_chunks, size_t *out_lens, int max_chunks);
    void *provider_ctx;  /* provider-specific state */
} stts_provider_t;

/* ── Stateful SentenceChunker ──────────────────────────────────────────── */
/* PoP: __init__ @ tools/tts_streaming.py:SentenceChunker.__init__ */
typedef struct stts_chunker stts_chunker_t;
stts_chunker_t *stts_chunker_new(long min_len);
void stts_chunker_free(stts_chunker_t *c);

/* feed: absorb delta, return \n-separated complete sentences.
 * Sets *out_buf_state to the chunker's internal buffer (borrowed).
 * Returns malloc'd string; caller frees. */
char *stts_chunker_feed(stts_chunker_t *c, const char *delta, char **out_buf_state);
/* flush: drain the tail. Returns malloc'd stripped text; caller frees. */
char *stts_chunker_flush(stts_chunker_t *c);

/* ── Stateless shim (backward compat — used by older ports) ────────────── */
char *stts_buf_init(long min_len);
char *stts_buf_feed(const char *delta, long min_len);
char *stts_buf_flush(const char *buf);

/* ── StreamingTTSProvider resolution ──────────────────────────────────── */
/* PoP: resolve_streaming_provider @ tools/tts_streaming.py:resolve_streaming_provider */
stts_provider_t *resolve_streaming_provider_c(json_t *tts_config);
/* PoP: _try_instantiate @ tools/tts_streaming.py:_try_instantiate */
stts_provider_t *resolve_streaming_provider_c_named(const char *name,
                                                      json_t *tts_config);

/* Provider name resolution */
/* PoP: _get_provider @ tools/tts_tool.py:_get_provider */
const char *tts_tool_resolve_provider_name(json_t *tts_config);

/* Provider availability probes */
/* PoP: available @ tools/tts_streaming.py:ElevenLabsStreamer.available */
bool stts_provider_elevenlabs_available(void);
/* PoP: available @ tools/tts_streaming.py:OpenAIStreamer.available */
bool stts_provider_openai_available(void);
/* PoP: available @ tools/tts_streaming.py:GeminiStreamer.available */
bool stts_provider_gemini_available(void);
/* PoP: available @ tools/tts_streaming.py:XAIStreamer.available */
bool stts_provider_xai_available(void);

/* ── TTS markdown stripper ────────────────────────────────────────────── */
/* PoP: _strip_markdown_for_tts @ tools/tts_tool.py:_strip_markdown_for_tts */
char *tts_strip_markdown_simple(const char *text);
char *(*resolve_tts_strip_markdown(void))(const char *text);

#endif /* PORT_TTS_STREAMING_HELPERS_H */
