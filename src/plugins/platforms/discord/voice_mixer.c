/*
 * voice_mixer.c - C home for plugins/platforms/discord/voice_mixer.py.
 * Faithful port of the frame geometry, MixerChild's per-frame cursor/gain plan and
 * VoiceMixer's duck/ramp state machine. The per-sample multiply and clip are the
 * numpy backend's work (np.frombuffer/astype/clip); every decision that picks which
 * samples are mixed and at what gain lives here.
 * PoP: w19_voicemixer_6e87f2_geometry @ plugins/platforms/discord/voice_mixer.py:<module>
 * PoP: w19_voicemixer_6e87f2_whole_cycle_freq @ plugins/platforms/discord/voice_mixer.py:synth_ambient_pcm._whole_cycle_freq
 * PoP: w19_voicemixer_6e87f2_child_init @ plugins/platforms/discord/voice_mixer.py:MixerChild.__init__
 * PoP: w19_voicemixer_6e87f2_child_read_frame @ plugins/platforms/discord/voice_mixer.py:MixerChild.read_frame
 * PoP: w19_voicemixer_6e87f2_child_plan @ plugins/platforms/discord/voice_mixer.py:MixerChild.read_frame
 * PoP: w19_voicemixer_6e87f2_init @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.__init__
 * PoP: w19_voicemixer_6e87f2_set_ambient @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.set_ambient
 * PoP: w19_voicemixer_6e87f2_play_speech @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.play_speech
 * PoP: w19_voicemixer_6e87f2_stop_speech @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.stop_speech
 * PoP: w19_voicemixer_6e87f2_begin_duck_release @ plugins/platforms/discord/voice_mixer.py:VoiceMixer._begin_duck_release_locked
 * PoP: w19_voicemixer_6e87f2_read @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.read
 * PoP: w19_voicemixer_6e87f2_status @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.speech_active
 * PoP: w19_voicemixer_6e87f2_cleanup @ plugins/platforms/discord/voice_mixer.py:VoiceMixer.cleanup
 */
#include "noc_w19_voicemixer_6e87f2.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_SIZE  W19_VOICEMIXER_6E87F2_FRAME_SIZE
#define FRAME_MS    W19_VOICEMIXER_6E87F2_FRAME_LENGTH_MS

json_t *w19_voicemixer_6e87f2_geometry(void) {
    json_t *o = json_object();
    json_set(o, "sample_rate", json_number(W19_VOICEMIXER_6E87F2_SAMPLE_RATE));
    json_set(o, "channels", json_number(W19_VOICEMIXER_6E87F2_CHANNELS));
    json_set(o, "sample_width", json_number(W19_VOICEMIXER_6E87F2_SAMPLE_WIDTH));
    json_set(o, "frame_length_ms", json_number(FRAME_MS));
    json_set(o, "samples_per_frame", json_number(W19_VOICEMIXER_6E87F2_SAMPLES_PER_FRAME));
    json_set(o, "frame_size", json_number(FRAME_SIZE));
    json_set(o, "bytes_per_ms", json_number(W19_VOICEMIXER_6E87F2_BYTES_PER_MS));
    json_set(o, "silence_frame_size", json_number(FRAME_SIZE));
    return o;
}

/* _whole_cycle_freq: max(1, round(target*seconds)) / seconds - round() is the
 * half-to-even rounding Python does, so rint() (default FE_TONEAREST) matches. */
double w19_voicemixer_6e87f2_whole_cycle_freq(double target, double seconds) {
    double cycles = rint(target * seconds);
    if (cycles < 1.0) cycles = 1.0;
    return cycles / seconds;
}

/* MixerChild.__init__: pad to whole frames so looping is seamless. */
void w19_voicemixer_6e87f2_child_init(w19_voicemixer_6e87f2_child_t *c, const unsigned char *pcm,
                                      size_t len, int loop, double gain, long fade_in_ms) {
    size_t pad;
    if (!c) return;
    memset(c, 0, sizeof *c);
    pad = (FRAME_SIZE - (len % FRAME_SIZE)) % FRAME_SIZE;
    c->pcm = (unsigned char *)calloc(len + pad + 1, 1);
    if (c->pcm && pcm && len) memcpy(c->pcm, pcm, len);
    c->len = len + pad;
    c->pos = 0;
    c->fade_done = 0;
    c->loop = loop ? 1 : 0;
    c->gain = (double)gain;
    c->fade_frames = fade_in_ms < 0 ? 0 : (int)(fade_in_ms / FRAME_MS);
    c->finished = 0;
}

void w19_voicemixer_6e87f2_child_free(w19_voicemixer_6e87f2_child_t *c) {
    if (!c) return;
    free(c->pcm);
    memset(c, 0, sizeof *c);
}

/* MixerChild.read_frame: 1 and the applied gain when a frame came out, else 0. */
int w19_voicemixer_6e87f2_child_read_frame(w19_voicemixer_6e87f2_child_t *c, double *applied) {
    double gain;
    if (!c || c->finished) return 0;
    if (c->pos >= c->len) {
        if (c->loop && c->len) c->pos = 0;
        else {
            c->finished = 1;
            return 0;
        }
    }
    c->pos += FRAME_SIZE; /* chunk = pcm[pos:pos+FRAME_SIZE]; cursor advances anyway */
    gain = c->gain;
    if (c->fade_frames && c->fade_done < c->fade_frames) {
        c->fade_done += 1;
        gain *= (double)c->fade_done / (double)c->fade_frames;
    }
    if (applied) *applied = gain;
    return 1;
}

json_t *w19_voicemixer_6e87f2_child_plan(size_t pcm_len, int loop, double gain, long fade_in_ms,
                                         int frames) {
    w19_voicemixer_6e87f2_child_t c;
    json_t *out = json_array();
    int i;
    w19_voicemixer_6e87f2_child_init(&c, NULL, pcm_len, loop, gain, fade_in_ms);
    for (i = 0; i < frames; i++) {
        double applied = 0.0;
        json_t *step = json_object();
        int ok = w19_voicemixer_6e87f2_child_read_frame(&c, &applied);
        json_set(step, "frame", json_bool(ok ? true : false));
        json_set(step, "gain", ok ? json_number(applied) : json_null());
        json_set(step, "pos", json_number((double)c.pos));
        json_set(step, "fade_done", json_number(c.fade_done));
        json_set(step, "finished", json_bool(c.finished ? true : false));
        json_append(out, step);
    }
    w19_voicemixer_6e87f2_child_free(&c);
    return out;
}

void w19_voicemixer_6e87f2_init(w19_voicemixer_6e87f2_mixer_t *m, double ambient_gain,
                                double duck_gain, double speech_gain, long duck_release_ms) {
    if (!m) return;
    memset(m, 0, sizeof *m);
    m->ambient_gain = ambient_gain;
    m->duck_gain = duck_gain;
    m->speech_gain = speech_gain;
    m->duck_release_frames = duck_release_ms / FRAME_MS;
    if (m->duck_release_frames < 1) m->duck_release_frames = 1;
    m->duck_release_left = 0;
    m->closed = m->speech_active = 0;
}

void w19_voicemixer_6e87f2_free(w19_voicemixer_6e87f2_mixer_t *m) {
    int i;
    if (!m) return;
    if (m->has_ambient) w19_voicemixer_6e87f2_child_free(&m->ambient);
    for (i = 0; i < m->speech_count; i++) w19_voicemixer_6e87f2_child_free(&m->speech[i]);
    memset(m, 0, sizeof *m);
}

void w19_voicemixer_6e87f2_set_ambient(w19_voicemixer_6e87f2_mixer_t *m, const unsigned char *pcm,
                                       size_t len, int has_gain, double gain) {
    double gain_now;
    if (!m) return;
    if (has_gain) m->ambient_gain = (double)gain;
    if (!pcm || !len) {
        if (m->has_ambient) {
            w19_voicemixer_6e87f2_child_free(&m->ambient);
            m->has_ambient = 0;
        }
        return;
    }
    if (m->has_ambient) w19_voicemixer_6e87f2_child_free(&m->ambient);
    gain_now = m->speech_active ? m->duck_gain : m->ambient_gain;
    w19_voicemixer_6e87f2_child_init(&m->ambient, pcm, len, 1, gain_now, 200);
    m->has_ambient = 1;
}

void w19_voicemixer_6e87f2_play_speech(w19_voicemixer_6e87f2_mixer_t *m, const unsigned char *pcm,
                                       size_t len, int has_gain, double gain, long fade_in_ms) {
    double g;
    if (!m || !pcm || !len) return;
    if (m->speech_count >= W19_VOICEMIXER_6E87F2_MAX_SPEECH) return;
    g = has_gain ? (double)gain : m->speech_gain;
    w19_voicemixer_6e87f2_child_init(&m->speech[m->speech_count], pcm, len, 0, g, fade_in_ms);
    m->speech_count++;
    m->speech_active = 1;
    m->duck_release_left = 0;
    if (m->has_ambient) m->ambient.gain = m->duck_gain;
}

void w19_voicemixer_6e87f2_begin_duck_release(w19_voicemixer_6e87f2_mixer_t *m) {
    if (!m) return;
    m->speech_active = 0;
    m->duck_release_left = m->duck_release_frames;
}

void w19_voicemixer_6e87f2_stop_speech(w19_voicemixer_6e87f2_mixer_t *m) {
    int i;
    if (!m) return;
    for (i = 0; i < m->speech_count; i++) w19_voicemixer_6e87f2_child_free(&m->speech[i]);
    m->speech_count = 0;
    w19_voicemixer_6e87f2_begin_duck_release(m);
}

/* VoiceMixer.read, minus the numpy add/clip: the tick's plan and SILENCE_FRAME answer. */
json_t *w19_voicemixer_6e87f2_read(w19_voicemixer_6e87f2_mixer_t *m) {
    json_t *o, *speech;
    double applied[W19_VOICEMIXER_6E87F2_MAX_SPEECH];
    int mixed = 0;
    int i, live = 0;
    int amb_had = 0;
    double amb_applied = 0.0;
    if (!m) return json_null();
    if (m->closed) {
        o = json_object();
        json_set(o, "closed", json_bool(true));
        json_set(o, "silence", json_bool(true));
        return o;
    }
    memset(applied, 0, sizeof applied);
    if (m->speech_count) {
        w19_voicemixer_6e87f2_child_t keep[W19_VOICEMIXER_6E87F2_MAX_SPEECH];
        double keep_gain[W19_VOICEMIXER_6E87F2_MAX_SPEECH];
        memset(keep, 0, sizeof keep);
        memset(keep_gain, 0, sizeof keep_gain);
        for (i = 0; i < m->speech_count; i++) {
            double g = 0.0;
            if (!w19_voicemixer_6e87f2_child_read_frame(&m->speech[i], &g)) {
                w19_voicemixer_6e87f2_child_free(&m->speech[i]);
                continue;
            }
            mixed = 1;
            keep[live] = m->speech[i];
            keep_gain[live] = g;
            live++;
        }
        memset(m->speech, 0, sizeof m->speech);
        for (i = 0; i < live; i++) {
            m->speech[i] = keep[i];
            applied[i] = keep_gain[i];
        }
        m->speech_count = live;
        if (!m->speech_count && m->speech_active) w19_voicemixer_6e87f2_begin_duck_release(m);
    }
    if (m->has_ambient) {
        if (m->duck_release_left > 0 && !m->speech_active) {
            double frac;
            m->duck_release_left -= 1;
            frac = 1.0 - ((double)m->duck_release_left / (double)m->duck_release_frames);
            m->ambient.gain = m->duck_gain + (m->ambient_gain - m->duck_gain) * frac;
        } else if (!m->speech_active && m->duck_release_left == 0) {
            m->ambient.gain = m->ambient_gain;
        }
        amb_had = w19_voicemixer_6e87f2_child_read_frame(&m->ambient, &amb_applied);
        if (amb_had) mixed = 1;
    }

    o = json_object();
    speech = json_array();
    json_set(o, "closed", json_bool(false));
    json_set(o, "silence", json_bool(mixed ? false : true));
    json_set(o, "speech_live", json_number(m->speech_count));
    for (i = 0; i < m->speech_count; i++) {
        json_t *rep = json_object();
        json_set(rep, "gain", json_number(m->speech[i].gain));
        json_set(rep, "applied", json_number(applied[i]));
        json_set(rep, "pos", json_number((double)m->speech[i].pos));
        json_set(rep, "fade_done", json_number(m->speech[i].fade_done));
        json_set(rep, "finished", json_bool(m->speech[i].finished ? true : false));
        json_append(speech, rep);
    }
    json_set(o, "speech", speech);
    if (m->has_ambient) {
        json_t *a = json_object();
        json_set(a, "present", json_bool(true));
        json_set(a, "gain", json_number(m->ambient.gain));
        json_set(a, "applied", amb_had ? json_number(amb_applied) : json_null());
        json_set(a, "pos", json_number((double)m->ambient.pos));
        json_set(a, "fade_done", json_number(m->ambient.fade_done));
        json_set(a, "finished", json_bool(m->ambient.finished ? true : false));
        json_set(o, "ambient", a);
    } else {
        json_set(o, "ambient", json_null());
    }
    json_set(o, "duck_release_left", json_number(m->duck_release_left));
    json_set(o, "speech_active", json_bool(m->speech_active ? true : false));
    json_set(o, "ambient_gain", json_number(m->ambient_gain));
    return o;
}

json_t *w19_voicemixer_6e87f2_status(w19_voicemixer_6e87f2_mixer_t *m) {
    json_t *o = json_object();
    if (!m) return o;
    json_set(o, "speech_active", json_bool(m->speech_active ? true : false));
    json_set(o, "speech_count", json_number(m->speech_count));
    json_set(o, "ambient_present", json_bool(m->has_ambient ? true : false));
    json_set(o, "closed", json_bool(m->closed ? true : false));
    json_set(o, "duck_release_left", json_number(m->duck_release_left));
    json_set(o, "duck_release_frames", json_number(m->duck_release_frames));
    return o;
}

void w19_voicemixer_6e87f2_cleanup(w19_voicemixer_6e87f2_mixer_t *m) {
    int i;
    if (!m) return;
    m->closed = 1;
    if (m->has_ambient) {
        w19_voicemixer_6e87f2_child_free(&m->ambient);
        m->has_ambient = 0;
    }
    for (i = 0; i < m->speech_count; i++) w19_voicemixer_6e87f2_child_free(&m->speech[i]);
    m->speech_count = 0;
}
