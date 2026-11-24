/* noc_w19_voicemixer_6e87f2.h - public C API for plugins/platforms/discord/voice_mixer.py. */
#ifndef NOC_W19_VOICEMIXER_6E87F2_H
#define NOC_W19_VOICEMIXER_6E87F2_H
#include "hermes_json.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Discord-native frame geometry: 48 kHz, stereo, s16, 20 ms frames. */
#define W19_VOICEMIXER_6E87F2_SAMPLE_RATE     48000
#define W19_VOICEMIXER_6E87F2_CHANNELS        2
#define W19_VOICEMIXER_6E87F2_SAMPLE_WIDTH    2
#define W19_VOICEMIXER_6E87F2_FRAME_LENGTH_MS 20
#define W19_VOICEMIXER_6E87F2_SAMPLES_PER_FRAME 960
#define W19_VOICEMIXER_6E87F2_FRAME_SIZE      3840
#define W19_VOICEMIXER_6E87F2_BYTES_PER_MS    192
#define W19_VOICEMIXER_6E87F2_MAX_SPEECH      8

/* One PCM stream feeding the mixer: padded to whole frames, optional loop, gain
 * and a linear fade-in counted in frames. */
typedef struct {
    unsigned char *pcm;
    size_t len;
    size_t pos;
    int loop;
    double gain;
    int fade_frames;
    int fade_done;
    int finished;
} w19_voicemixer_6e87f2_child_t;

typedef struct {
    w19_voicemixer_6e87f2_child_t ambient;
    int has_ambient;
    w19_voicemixer_6e87f2_child_t speech[W19_VOICEMIXER_6E87F2_MAX_SPEECH];
    int speech_count;
    double ambient_gain;
    double duck_gain;
    double speech_gain;
    int duck_release_frames;
    int duck_release_left;
    int closed;
    int speech_active;
} w19_voicemixer_6e87f2_mixer_t;

/* PoP: w19_voicemixer_6e87f2_geometry @ plugins/platforms/discord/voice_mixer.py:<module> */
json_t *w19_voicemixer_6e87f2_geometry(void);

/* PoP: w19_voicemixer_6e87f2_whole_cycle_freq @ plugins/platforms/discord/voice_mixer.py:synth_ambient_pcm._whole_cycle_freq */
double w19_voicemixer_6e87f2_whole_cycle_freq(double target, double seconds);

/* PoP: w19_voicemixer_6e87f2_child_init @ plugins/platforms/discord/voice_mixer.py:MixerChild.__init__ */
void w19_voicemixer_6e87f2_child_init(w19_voicemixer_6e87f2_child_t *c, const unsigned char *pcm,
                                      size_t len, int loop, double gain, long fade_in_ms);
/* PoP: w19_voicemixer_6e87f2_child_free @ plugins/platforms/discord/voice_mixer.py:MixerChild */
void w19_voicemixer_6e87f2_child_free(w19_voicemixer_6e87f2_child_t *c);
/* PoP: w19_voicemixer_6e87f2_child_read_frame @ plugins/platforms/discord/voice_mixer.py:MixerChild.read_frame
 * 1 with *applied set when a frame was produced, 0 when the child returned None. */
int w19_voicemixer_6e87f2_child_read_frame(w19_voicemixer_6e87f2_child_t *c, double *applied);
/* PoP: w19_voicemixer_6e87f2_child_plan @ plugins/platforms/discord/voice_mixer.py:MixerChild.read_frame
 * The frame-by-frame plan of one child: gain applied and cursor after each read. */
json_t *w19_voicemixer_6e87f2_child_plan(size_t pcm_len, int loop, double gain, long fade_in_ms,
                                         int frames);

/* PoP: w19_voicemixer_6e87f2_init @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.__init__ */
void w19_voicemixer_6e87f2_init(w19_voicemixer_6e87f2_mixer_t *m, double ambient_gain,
                                double duck_gain, double speech_gain, long duck_release_ms);
void w19_voicemixer_6e87f2_free(w19_voicemixer_6e87f2_mixer_t *m);

/* PoP: w19_voicemixer_6e87f2_set_ambient @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.set_ambient */
void w19_voicemixer_6e87f2_set_ambient(w19_voicemixer_6e87f2_mixer_t *m, const unsigned char *pcm,
                                       size_t len, int has_gain, double gain);
/* PoP: w19_voicemixer_6e87f2_play_speech @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.play_speech */
void w19_voicemixer_6e87f2_play_speech(w19_voicemixer_6e87f2_mixer_t *m, const unsigned char *pcm,
                                       size_t len, int has_gain, double gain, long fade_in_ms);
/* PoP: w19_voicemixer_6e87f2_stop_speech @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.stop_speech */
void w19_voicemixer_6e87f2_stop_speech(w19_voicemixer_6e87f2_mixer_t *m);
/* PoP: w19_voicemixer_6e87f2_begin_duck_release @ plugins/platforms/discord/voice_mixer.py:VoiceMixer._begin_duck_release_locked */
void w19_voicemixer_6e87f2_begin_duck_release(w19_voicemixer_6e87f2_mixer_t *m);
/* PoP: w19_voicemixer_6e87f2_read @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.read
 * One 20 ms tick: which children produced a frame, the gain each was mixed at, the
 * ambient ramp, and whether the tick would answer SILENCE_FRAME. */
json_t *w19_voicemixer_6e87f2_read(w19_voicemixer_6e87f2_mixer_t *m);
/* PoP: w19_voicemixer_6e87f2_status @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.speech_active */
json_t *w19_voicemixer_6e87f2_status(w19_voicemixer_6e87f2_mixer_t *m);
/* PoP: w19_voicemixer_6e87f2_cleanup @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.cleanup */
void w19_voicemixer_6e87f2_cleanup(w19_voicemixer_6e87f2_mixer_t *m);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_VOICEMIXER_6E87F2_H */
