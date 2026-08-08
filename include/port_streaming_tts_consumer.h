#ifndef PORT_STREAMING_TTS_CONSUMER_H
#define PORT_STREAMING_TTS_CONSUMER_H

#include <stdbool.h>
#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Types mirrored from gateway/platforms/base.py ────────────────────── */

/* PoP: AudioFormat @ gateway/platforms/base.py:AudioFormat */
typedef struct {
    int sample_rate;    /* Hz, default 24000 */
    int channels;       /* default 1 (mono) */
    int sample_width;   /* bytes/sample, default 2 (int16) */
} stts_audio_format_t;

/* PoP: StreamingTTSHandle @ gateway/platforms/base.py:StreamingTTSHandle */
typedef struct {
    bool audible;
    bool aborted;
    void *adapter_handle;
} stts_tts_handle_t;

/* ── Adapter vtable (platform base abstract methods) ──────────────────── */
typedef struct {
    bool (*supports_streaming_tts)(void *adapter, const char *chat_id,
                                   const stts_audio_format_t *fmt);
    stts_tts_handle_t *(*begin_streaming_tts)(void *adapter, const char *chat_id,
                                               const stts_audio_format_t *fmt,
                                               json_t *metadata);
    int  (*write_streaming_tts)(void *adapter, stts_tts_handle_t *handle,
                                const void *pcm_data, size_t len);
    void (*finish_streaming_tts)(void *adapter, stts_tts_handle_t *handle,
                                 bool interrupted);
    void (*abort_streaming_tts)(void *adapter, stts_tts_handle_t *handle,
                                const char *error);
    const char *name;
} stts_adapter_vtable_t;

/* ── Opaque consumer ──────────────────────────────────────────────────── */
typedef struct stts_consumer stts_consumer_t;

/* ── Constructor / destructor ──────────────────────────────────────────── */
/* PoP: __init__ @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.__init__ */
stts_consumer_t *stts_consumer_new(
    void *adapter,
    stts_adapter_vtable_t *vtable,
    const char *chat_id,
    json_t *tts_config,
    json_t *metadata,
    stts_audio_format_t *audio_format_override);
void stts_consumer_free(stts_consumer_t *c);

/* ── Public properties ────────────────────────────────────────────────── */
/* PoP: active @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.active */
bool stts_consumer_active(const stts_consumer_t *c);
/* PoP: completed @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.completed */
bool stts_consumer_completed(const stts_consumer_t *c);
/* PoP: partial @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.partial */
bool stts_consumer_partial(const stts_consumer_t *c);
/* PoP: started @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.started */
bool stts_consumer_started(const stts_consumer_t *c);
/* PoP: audible @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.audible */
bool stts_consumer_audible(const stts_consumer_t *c);
/* PoP: dropped @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.dropped */
bool stts_consumer_dropped(const stts_consumer_t *c);
/* PoP: suppress_whole_file @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.suppress_whole_file */
bool stts_consumer_suppress_whole_file(const stts_consumer_t *c);
/* PoP: done @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.done */
bool stts_consumer_done(const stts_consumer_t *c);

/* ── Sync callback (agent worker thread) ──────────────────────────────── */
/* PoP: on_delta @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.on_delta */
void stts_consumer_on_delta(stts_consumer_t *c, const char *text);

/* PoP: finish @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.finish */
void stts_consumer_finish(stts_consumer_t *c);

/* PoP: _enqueue_done @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer._enqueue_done */
void stts_consumer_enqueue_done(stts_consumer_t *c);

/* ── Async lifecycle (drain thread) ───────────────────────────────────── */
/* PoP: start @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.start */
void stts_consumer_start(stts_consumer_t *c);
void *stts_drain_thread(void *arg);

/* PoP: _synthesise_and_write @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer._synthesise_and_write */
void stts_synthesise_and_write(stts_consumer_t *c, const char *clause);

/* PoP: abort @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.abort */
void stts_consumer_abort(stts_consumer_t *c, const char *reason);

/* PoP: wait_complete @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.wait_complete */
bool stts_consumer_wait_complete(stts_consumer_t *c, double timeout_secs);

/* ── Platform base.py methods ──────────────────────────────────────────── */
/* PoP: supports_streaming_tts @ gateway/platforms/base.py:supports_streaming_tts */
bool stts_supports_streaming_tts(stts_consumer_t *c, const char *chat_id,
                                  const stts_audio_format_t *fmt);
/* PoP: begin_streaming_tts @ gateway/platforms/base.py:begin_streaming_tts */
stts_tts_handle_t *stts_begin_streaming_tts(stts_consumer_t *c, const char *chat_id,
                                             const stts_audio_format_t *fmt,
                                             json_t *metadata);
/* PoP: write_streaming_tts @ gateway/platforms/base.py:write_streaming_tts */
int stts_write_streaming_tts(stts_consumer_t *c, stts_tts_handle_t *handle,
                              const void *pcm_data, size_t len);
/* PoP: finish_streaming_tts @ gateway/platforms/base.py:finish_streaming_tts */
void stts_finish_streaming_tts(stts_consumer_t *c, stts_tts_handle_t *handle,
                                bool interrupted);
/* PoP: abort_streaming_tts @ gateway/platforms/base.py:abort_streaming_tts */
void stts_abort_streaming_tts(stts_consumer_t *c, stts_tts_handle_t *handle,
                               const char *error);

/* PoP: streaming_tts_turn_key @ gateway/platforms/base.py:streaming_tts_turn_key */
char *stts_streaming_tts_turn_key(const char *chat_id, const char *session_id);
/* PoP: streaming_tts_should_skip_whole_file @ gateway/platforms/base.py:streaming_tts_should_skip_whole_file */
bool stts_streaming_tts_should_skip_whole_file(stts_consumer_t *c, const char *chat_id,
                                               const char *session_id, json_t *msg);
/* PoP: _mark_streaming_tts_completed_turn @ gateway/platforms/base.py:_mark_streaming_tts_completed_turn */
void stts_mark_streaming_tts_completed_turn(stts_consumer_t *c, const char *chat_id,
                                             const char *session_id);
/* PoP: _streaming_tts_turn_completed @ gateway/platforms/base.py:_streaming_tts_turn_completed */
bool stts_streaming_tts_turn_completed(stts_consumer_t *c, const char *chat_id,
                                        const char *session_id);
/* PoP: _notify_media_delivery_failure @ gateway/platforms/base.py:_notify_media_delivery_failure */
void stts_notify_media_delivery_failure(stts_consumer_t *c, const char *chat_id,
                                         const char *error);

/* PoP: _streaming_tts_turn_key @ gateway/platforms/base.py:_streaming_tts_turn_key */
char *stts_streaming_tts_turn_key_private(const char *chat_id, const char *session_id);

#ifdef __cplusplus
}
#endif

#endif /* PORT_STREAMING_TTS_CONSUMER_H */
