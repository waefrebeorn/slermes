/*
 * voice_mode.c — Voice input/output mode for Hermes C.
 * Wraps an ALSA PCM capture backend (sounddevice.InputStream equivalent) for
 * microphone input + TTS for output. Speech recognition via API (Whisper).
 *
 * Mirrors tools/voice_mode.py: real-time VAD via an ALSA PCM capture
 * callback thread, silence/dip-tolerance endpointing, and a
 * listen_for_speech() barge-in monitor with pre-roll capture.
 * STT dispatch reuses the shared transcribe_audio() pipeline.
 */

#define _GNU_SOURCE  /* pthread_tryjoin_np */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "transcribe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#ifdef _WIN32
/* No ALSA on Windows — recorder functions are compiled out (see below). */
#define SLERMES_NO_ALSA 1
#else
#include <alsa/asoundlib.h>
#endif

/* Forward declaration (defined later in this file as voice_write_wav). */
int voice_write_wav(const char *file_path, const void *data, size_t data_len,
                    int sample_rate, int channels, int bits_per_sample);

/* ================================================================
 *  Configuration
 * ================================================================ */

static int g_voice_enabled = 0;
static char g_voice_device[128] = "default";  /* ALSA device */
static char g_voice_asr_cmd[512] = "";        /* External STT command */
static int g_voice_timeout = 5;               /* Record timeout seconds */

/* Recording state. Mirrors Python AudioRecorder: a persistent ALSA capture
 * stream whose callback thread collects frames while _recording is set and
 * runs the VAD state machine (RMS level, silence/dip-tolerance endpointing). */
static struct {
    pthread_t thread;
    int running;          /* capture thread alive */
    int recording;        /* frame collection + VAD active */
    char output_path[512];
    time_t start_time;
    int sample_rate;
    int channels;
    double rms;           /* live RMS (sounddevice callback equivalent) */
    int peak_rms;         /* peak RMS seen during current recording */
    /* VAD state (mirrors AudioRecorder.__init__) */
    int has_spoken;
    double speech_start;
    double dip_start;
    double silence_start;
    double resume_start;
    double resume_dip_start;
    double silence_threshold;
    double silence_duration;   /* seconds of silence before auto-stop */
    double max_wait;           /* seconds to wait for speech before auto-stop */
    double min_speech_duration;
    double max_dip_tolerance;
    void (*on_silence_stop)(void);
    snd_pcm_t *pcm;
    pthread_mutex_t mutex;
} g_recorder = {0};

/* Recording parameters (mirror Python module constants). */
#define VOICE_SAMPLE_RATE 16000
#define VOICE_CHANNELS 1
#define VOICE_SILENCE_RMS_THRESHOLD 200   /* RMS below this = silence */
#define VOICE_SILENCE_DURATION_SECONDS 3.0
#define VOICE_MIN_SPEECH_DURATION 0.3
#define VOICE_MAX_DIP_TOLERANCE 0.3
#define VOICE_MAX_WAIT 15.0
#define VOICE_BLOCK_FRAMES 480            /* 30ms @ 16kHz */

/* ================================================================
 *  Configuration API
 * ================================================================ */

/* PoP: voice_set_enabled @ hermes_cli/pets.py:_set_enabled */
void voice_set_enabled(int enabled) {
    g_voice_enabled = enabled;
}

int voice_is_enabled(void) {
    return g_voice_enabled;
}

void voice_set_device(const char *dev) {
    if (dev) snprintf(g_voice_device, sizeof(g_voice_device), "%s", dev);
}

void voice_set_asr_cmd(const char *cmd) {
    if (cmd) snprintf(g_voice_asr_cmd, sizeof(g_voice_asr_cmd), "%s", cmd);
}

void voice_set_timeout(int timeout_sec) {
    if (timeout_sec > 0) g_voice_timeout = timeout_sec;
}

void voice_set_sample_rate(int rate) {
    g_recorder.sample_rate = rate > 0 ? rate : 16000;
}

void voice_set_channels(int channels) {
    g_recorder.channels = channels > 0 ? channels : 1;
}

/* ================================================================
 *  Audio Environment Detection
 * ================================================================ */

/* PoP: _import_audio @ tools/voice_mode.py:voice_import_audio */
/* Port of Python tools/voice_mode.py:_import_audio(). */
const char *voice_import_audio(void) {
    /* Check for available audio backends */
    if (access("/usr/bin/arecord", X_OK) == 0 || access("/usr/local/bin/arecord", X_OK) == 0)
        return "arecord";
    if (access("/usr/bin/sox", X_OK) == 0 || access("/usr/local/bin/sox", X_OK) == 0)
        return "sox";
    if (access("/usr/bin/parec", X_OK) == 0)
        return "parec";
    if (access("/usr/bin/termux-microphone-record", X_OK) == 0)
        return "termux";
    return NULL;
}

/* PoP: _audio_available @ tools/voice_mode.py:voice_audio_available */
/* Port of Python tools/voice_mode.py:_audio_available(). */
int voice_audio_available(void) {
    /* Check if audio libraries are available */
    void *audio = voice_import_audio();
    if (audio != NULL) return 1;
    return 0;
}

/* PoP: _voice_capture_install_hint @ tools/voice_mode.py:_voice_capture_install_hint */
/* Port of Python tools/voice_mode.py:_voice_capture_install_hint(). */
void voice_capture_install_hint(void) {
    printf("Voice capture requires one of:\n");
    printf("  - ALSA: sudo apt-get install alsa-utils\n");
    printf("  - SoX:  sudo apt-get install sox\n");
    printf("  - PulseAudio: sudo apt-get install pulseaudio-utils\n");
    printf("  - Termux: pkg install termux-api\n");
}

static int check_pulse_socket(void) {
    const char *pulse_server = getenv("PULSE_SERVER");
    if (!pulse_server) pulse_server = "/run/user/1000/pulse/native";
    
    struct stat st;
    return stat(pulse_server, &st) == 0 && (st.st_mode & S_IFMT) == S_IFSOCK;
}

/* PoP: _pulse_socket_reachable @ tools/voice_mode.py:voice_pulse_socket_reachable */
/* Port of Python tools/voice_mode.py:_pulse_socket_reachable(). */
int voice_pulse_socket_reachable(void) {
    /* Check if PulseAudio socket is reachable */
    int result = check_pulse_socket();
    return result;
}

static int check_termux_api(void) {
    return access("/usr/bin/termux-api", X_OK) == 0 ||
           access("/data/data/com.termux/files/usr/bin/termux-api", X_OK) == 0;
}

/* PoP: _termux_api_app_installed @ tools/voice_mode.py:voice_termux_api_app_installed */
/* Port of Python tools/voice_mode.py:_termux_api_app_installed(). */
int voice_termux_api_app_installed(void) {
    /* Check if Termux API app is installed */
    int result = check_termux_api();
    return result;
}

/* PoP: _termux_microphone_command @ tools/voice_mode.py:voice_termux_microphone_command */
/* Port of Python tools/voice_mode.py:_termux_microphone_command(). */
char *voice_termux_microphone_command(void) {
    /* Get the termux-microphone-record command path */
    if (!check_termux_api()) return NULL;
    char *cmd = strdup("termux-microphone-record");
    return cmd;
}

/* PoP: _termux_voice_capture_available @ tools/voice_mode.py:voice_termux_voice_capture_available */
/* Port of Python tools/voice_mode.py:_termux_voice_capture_available(). */
int voice_termux_voice_capture_available(void) {
    /* Check if Termux voice capture is available */
    int api = check_termux_api();
    return api;
}

/* PoP: detect_audio_environment @ tools/voice_mode.py:detect_audio_environment */
/* Port of Python tools/voice_mode.py:detect_audio_environment(). */
char *detect_audio_environment(void) {
    const char *backend = voice_import_audio();
    if (!backend) return strdup("none");
    
    if (strcmp(backend, "termux") == 0) {
        if (voice_termux_voice_capture_available())
            return strdup("termux");
    }
    if (strcmp(backend, "parec") == 0) {
        if (voice_pulse_socket_reachable())
            return strdup("pulse");
    }
    if (strcmp(backend, "arecord") == 0) {
        return strdup("alsa");
    }
    if (strcmp(backend, "sox") == 0) {
        return strdup("alsa");
    }
    return strdup("unknown");
}

/* ================================================================
 *  Audio Playback
 * ================================================================ */

/* PoP: play_beep @ hermes_cli/voice.py:_play_beep */
/* Port of Python tools/voice_mode.py:play_beep(). */
void play_beep(void) {
    /* Try various beep commands */
    const char *cmds[] = {
        "beep -f 800 -l 100 2>/dev/null",
        "speaker-test -t sine -f 800 -l 1 2>/dev/null",
        "play -n synth 0.1 sine 800 2>/dev/null",
        "printf '\\a'",
        NULL
    };
    
    for (int i = 0; cmds[i]; i++) {
        if (system(cmds[i]) == 0) return;
    }
}

/* PoP: play_audio_file @ tools/voice_mode.py:play_audio_file */
/* Port of Python tools/voice_mode.py:play_audio_file(). */
int play_audio_file(const char *file_path) {
    if (!file_path) return -1;
    
    const char *ext = strrchr(file_path, '.');
    char cmd[1024];
    
    if (ext && (strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".ogg") == 0 || 
                strcasecmp(ext, ".flac") == 0)) {
        snprintf(cmd, sizeof(cmd), "aplay %s 2>/dev/null || paplay %s 2>/dev/null || play %s 2>/dev/null", 
                 file_path, file_path, file_path);
    } else if (ext && (strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".m4a") == 0)) {
        snprintf(cmd, sizeof(cmd), "mpg123 %s 2>/dev/null || ffplay -nodisp -autoexit %s 2>/dev/null || play %s 2>/dev/null",
                 file_path, file_path, file_path);
    } else {
        snprintf(cmd, sizeof(cmd), "play %s 2>/dev/null || aplay %s 2>/dev/null", 
                 file_path, file_path);
    }
    
    return system(cmd) == 0 ? 0 : -1;
}

/* PoP: stop_playback @ tools/voice_mode.py:stop_playback */
/* Port of Python tools/voice_mode.py:stop_playback(). */
void stop_playback(void) {
    system("pkill -f \"aplay|paplay|play|mpg123|ffplay\" 2>/dev/null");
}

/* ================================================================
 *  Recording State Management  (ALSA PCM capture + VAD)
 *
 *  Mirrors Python AudioRecorder: a persistent sounddevice.InputStream
 *  (here an ALSA PCM capture handle) kept alive for the recorder's life.
 *  The capture thread runs the VAD state machine in lockstep with the
 *  Python _callback(): live RMS, silence/dip-tolerance endpointing, and
 *  a configurable on_silence_stop callback (auto-stop + transcribe).
 * ================================================================ */

/* Compute RMS of one int16 block (sounddevice callback equivalent). */
double voice_block_rms(const int16_t *samples, snd_pcm_uframes_t n) {
    if (n == 0) return 0.0;
    double sum = 0.0;
    for (snd_pcm_uframes_t i = 0; i < n; i++) {
        double s = samples[i];
        sum += s * s;
    }
    return sqrt(sum / (double)n);
}

/* VAD state machine — mirrors AudioRecorder._callback() exactly.
 * Updates g_recorder.{rms,peak_rms} and may dispatch *fire once. The
 * caller must NOT hold g_recorder.mutex. */
static void voice_vad_step(double rms, int *fire) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;

    pthread_mutex_lock(&g_recorder.mutex);
    g_recorder.rms = rms;
    if ((int)rms > g_recorder.peak_rms) g_recorder.peak_rms = (int)rms;

    if (rms > g_recorder.silence_threshold) {
        g_recorder.dip_start = 0.0;
        if (g_recorder.speech_start == 0.0) {
            g_recorder.speech_start = now;
        } else if (!g_recorder.has_spoken &&
                   now - g_recorder.speech_start >= g_recorder.min_speech_duration) {
            g_recorder.has_spoken = 1;
        }
        if (!g_recorder.has_spoken) {
            g_recorder.silence_start = 0.0;
        } else {
            g_recorder.resume_dip_start = 0.0;
            if (g_recorder.resume_start == 0.0) {
                g_recorder.resume_start = now;
            } else if (now - g_recorder.resume_start >= g_recorder.min_speech_duration) {
                g_recorder.silence_start = 0.0;
                g_recorder.resume_start = 0.0;
            }
        }
    } else if (g_recorder.has_spoken) {
        if (g_recorder.resume_start > 0) {
            if (g_recorder.resume_dip_start == 0.0) {
                g_recorder.resume_dip_start = now;
            } else if (now - g_recorder.resume_dip_start >= g_recorder.max_dip_tolerance) {
                g_recorder.resume_start = 0.0;
                g_recorder.resume_dip_start = 0.0;
            }
        }
    } else if (g_recorder.speech_start > 0) {
        if (g_recorder.dip_start == 0.0) {
            g_recorder.dip_start = now;
        } else if (now - g_recorder.dip_start >= g_recorder.max_dip_tolerance) {
            g_recorder.speech_start = 0.0;
            g_recorder.dip_start = 0.0;
        }
    }

    double elapsed = now - g_recorder.start_time;
    int should_fire = 0;
    if (g_recorder.has_spoken && rms <= g_recorder.silence_threshold) {
        if (g_recorder.silence_start == 0.0) {
            g_recorder.silence_start = now;
        } else if (now - g_recorder.silence_start >= g_recorder.silence_duration) {
            should_fire = 1;
        }
    } else if (!g_recorder.has_spoken && elapsed >= g_recorder.max_wait) {
        should_fire = 1;
    }

    if (should_fire && *fire == 0) {
        void (*cb)(void) = g_recorder.on_silence_stop;
        g_recorder.on_silence_stop = NULL; /* fire only once */
        pthread_mutex_unlock(&g_recorder.mutex);
        if (cb) {
            pthread_t t;
            if (pthread_create(&t, NULL, (void *(*)(void *))cb, NULL) == 0)
                pthread_detach(t);
        }
        *fire = 1;
        return;
    }
    pthread_mutex_unlock(&g_recorder.mutex);
}

/* Shared captured-audio buffer, published by the capture thread so
 * stop()/barge-in can write it to WAV. Guarded by g_recorder.mutex. */
static int16_t *g_capture_buf = NULL;
static size_t g_capture_len = 0;   /* samples (per-channel already interleaved) */

#ifndef SLERMES_NO_ALSA
static void *record_thread(void *arg) {
    (void)arg;
    snd_pcm_t *pcm = NULL;
    int sr = g_recorder.sample_rate ? g_recorder.sample_rate : VOICE_SAMPLE_RATE;
    int ch = g_recorder.channels ? g_recorder.channels : VOICE_CHANNELS;

    int rc = snd_pcm_open(&pcm, g_voice_device, SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        pthread_mutex_lock(&g_recorder.mutex);
        g_recorder.running = 0;
        g_recorder.recording = 0;
        pthread_mutex_unlock(&g_recorder.mutex);
        return NULL;
    }
    snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                       ch, sr, 1, 20000);

    size_t cap = VOICE_BLOCK_FRAMES * ch * 16;
    size_t len = 0;
    int16_t *buf = malloc(cap * sizeof(int16_t));
    if (!buf) { snd_pcm_close(pcm); pthread_mutex_lock(&g_recorder.mutex); g_recorder.running = 0; g_recorder.recording = 0; pthread_mutex_unlock(&g_recorder.mutex); return NULL; }

    int fire = 0;
    while (1) {
        pthread_mutex_lock(&g_recorder.mutex);
        int alive = g_recorder.running;
        int recording = g_recorder.recording;
        pthread_mutex_unlock(&g_recorder.mutex);
        if (!alive) break;

        int16_t chunk[VOICE_BLOCK_FRAMES * 8];
        snd_pcm_uframes_t got = snd_pcm_readi(pcm, chunk, VOICE_BLOCK_FRAMES);
        if (got == (snd_pcm_uframes_t)-EPIPE) { snd_pcm_recover(pcm, -EPIPE, 1); continue; }
        if (got <= 0) continue;

        double rms = voice_block_rms(chunk, got * ch);
        if (recording) {
            if (len + (size_t)got * ch > cap) {
                size_t ncap = cap * 2 + (size_t)got * ch;
                int16_t *nb = realloc(buf, ncap * sizeof(int16_t));
                if (nb) { buf = nb; cap = ncap; }
            }
            if (len + (size_t)got * ch <= cap) {
                memcpy(buf + len, chunk, (size_t)got * ch * sizeof(int16_t));
                len += (size_t)got * ch;
            }
            voice_vad_step(rms, &fire);
        } else {
            pthread_mutex_lock(&g_recorder.mutex);
            g_recorder.rms = rms;
            pthread_mutex_unlock(&g_recorder.mutex);
        }
    }

    snd_pcm_close(pcm);
    pthread_mutex_lock(&g_recorder.mutex);
    g_capture_buf = buf;
    g_capture_len = len;
    pthread_mutex_unlock(&g_recorder.mutex);
    pthread_mutex_lock(&g_recorder.mutex);
    g_recorder.running = 0;
    g_recorder.recording = 0;
    pthread_mutex_unlock(&g_recorder.mutex);
    return NULL;
}
#endif /* !SLERMES_NO_ALSA */

/* PoP: create_audio_recorder @ tools/voice_mode.py:create_audio_recorder */
/* Port of Python tools/voice_mode.py:create_audio_recorder(). */
char *create_audio_recorder(const char *output_path, int max_seconds,
                            int sample_rate, int channels) {
    pthread_mutex_lock(&g_recorder.mutex);

    if (g_recorder.running) {
        pthread_mutex_unlock(&g_recorder.mutex);
        return strdup("Recorder already running");
    }

    if (!output_path) output_path = "/tmp/hermes_voice_recording.wav";
    snprintf(g_recorder.output_path, sizeof(g_recorder.output_path), "%s", output_path);
    g_recorder.sample_rate = sample_rate > 0 ? sample_rate : VOICE_SAMPLE_RATE;
    g_recorder.channels = channels > 0 ? channels : VOICE_CHANNELS;
    g_recorder.start_time = 0;
    g_recorder.running = 1;
    g_recorder.recording = 0;
    g_recorder.rms = 0.0;
    g_recorder.peak_rms = 0;
    g_recorder.has_spoken = 0;
    g_recorder.speech_start = 0.0;
    g_recorder.dip_start = 0.0;
    g_recorder.silence_start = 0.0;
    g_recorder.resume_start = 0.0;
    g_recorder.resume_dip_start = 0.0;
    g_capture_buf = NULL;
    g_capture_len = 0;
    (void)max_seconds;

    int rc = pthread_create(&g_recorder.thread, NULL, record_thread, NULL);
    if (rc != 0) {
        g_recorder.running = 0;
        pthread_mutex_unlock(&g_recorder.mutex);
        return strdup("Failed to create recording thread");
    }

    pthread_mutex_unlock(&g_recorder.mutex);
    return strdup("");
}

/* PoP: is_recording @ tools/voice_mode.py:is_recording */
/* Port of Python tools/voice_mode.py:is_recording(). */
int is_recording(void) {
    pthread_mutex_lock(&g_recorder.mutex);
    int recording = g_recorder.recording;
    pthread_mutex_unlock(&g_recorder.mutex);
    return recording;
}

/* PoP: elapsed_seconds @ tools/voice_mode.py:elapsed_seconds */
/* Port of Python tools/voice_mode.py:elapsed_seconds(). */
int elapsed_seconds(void) {
    pthread_mutex_lock(&g_recorder.mutex);
    time_t start = g_recorder.start_time;
    int recording = g_recorder.recording;
    pthread_mutex_unlock(&g_recorder.mutex);

    if (!recording || start == 0) return 0;
    return (int)(time(NULL) - start);
}

/* PoP: current_rms @ tools/voice_mode.py:current_rms */
/* Port of Python tools/voice_mode.py:current_rms().
 * Live RMS, updated every audio chunk by the capture thread. */
double current_rms(void) {
    pthread_mutex_lock(&g_recorder.mutex);
    double rms = g_recorder.rms;
    pthread_mutex_unlock(&g_recorder.mutex);
    return rms;
}

/* PoP: _ensure_stream @ tools/voice_mode.py:voice_ensure_stream */
/* Port of Python tools/voice_mode.py:_ensure_stream(). */
int voice_ensure_stream(void) {
    if (!g_recorder.running) {
        return create_audio_recorder(NULL, g_voice_timeout,
                                    g_recorder.sample_rate, g_recorder.channels) != NULL ? -1 : 0;
    }
    return 0;
}

/* Start recording with optional silence-stop callback. Mirrors
 * AudioRecorder.start(on_silence_stop=...). */
#ifndef SLERMES_NO_ALSA
void voice_recorder_start(void (*on_silence_stop)(void)) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;

    pthread_mutex_lock(&g_recorder.mutex);
    if (g_recorder.recording) { pthread_mutex_unlock(&g_recorder.mutex); return; }
    g_recorder.start_time = (time_t)now;
    g_recorder.has_spoken = 0;
    g_recorder.speech_start = 0.0;
    g_recorder.dip_start = 0.0;
    g_recorder.silence_start = 0.0;
    g_recorder.resume_start = 0.0;
    g_recorder.resume_dip_start = 0.0;
    g_recorder.peak_rms = 0;
    g_recorder.rms = 0.0;
    g_recorder.on_silence_stop = on_silence_stop;
    g_recorder.silence_threshold = VOICE_SILENCE_RMS_THRESHOLD;
    g_recorder.silence_duration = VOICE_SILENCE_DURATION_SECONDS;
    g_recorder.max_wait = VOICE_MAX_WAIT;
    g_recorder.min_speech_duration = VOICE_MIN_SPEECH_DURATION;
    g_recorder.max_dip_tolerance = VOICE_MAX_DIP_TOLERANCE;
    pthread_mutex_unlock(&g_recorder.mutex);

    voice_ensure_stream();

    pthread_mutex_lock(&g_recorder.mutex);
    g_recorder.recording = 1;
    pthread_mutex_unlock(&g_recorder.mutex);
}
#endif /* !SLERMES_NO_ALSA */
#ifdef SLERMES_NO_ALSA
/* Windows/macOS fallback: no ALSA capture device — recorder is a no-op. */
static void *record_thread(void *arg) { (void)arg; return NULL; }
void voice_recorder_start(void (*on_silence_stop)(void)) {
    (void)on_silence_stop;
    fprintf(stderr, "voice: recording unsupported on this platform (no ALSA)\n");
}
#endif /* SLERMES_NO_ALSA */


/* Stop recording and write WAV. Returns malloc'd path or NULL. Mirrors
 * AudioRecorder.stop(): keeps the stream alive, discards very short or
 * silent (< peak RMS threshold) recordings, otherwise writes WAV. */
char *voice_recorder_stop(void) {
    pthread_mutex_lock(&g_recorder.mutex);
    if (!g_recorder.recording) { pthread_mutex_unlock(&g_recorder.mutex); return NULL; }
    g_recorder.recording = 0;
    g_recorder.rms = 0.0;
    int peak = g_recorder.peak_rms;
    int channels = g_recorder.channels ? g_recorder.channels : VOICE_CHANNELS;
    int sr = g_recorder.sample_rate ? g_recorder.sample_rate : VOICE_SAMPLE_RATE;
    int16_t *buf = g_capture_buf;
    size_t n = g_capture_len;
    /* Fresh buffer for next recording. */
    g_capture_buf = NULL;
    g_capture_len = 0;
    pthread_mutex_unlock(&g_recorder.mutex);

    char *result = NULL;
    /* Skip very short recordings (< 0.3s). */
    long min_samples = (long)(sr * VOICE_MIN_SPEECH_DURATION) * channels;
    if (n < (size_t)min_samples) { free(buf); return NULL; }
    /* Skip silent recordings (peak RMS < threshold). */
    if (peak < VOICE_SILENCE_RMS_THRESHOLD) { free(buf); return NULL; }

    char wav_path[1024];
    snprintf(wav_path, sizeof(wav_path), "/tmp/hermes_voice/recording_%ld.wav",
             (long)time(NULL));
    mkdir("/tmp/hermes_voice", 0755);
    if (voice_write_wav(wav_path, buf, n * sizeof(int16_t), sr, channels, 16) == 0)
        result = strdup(wav_path);
    free(buf);
    return result;
}

/* PoP: _close_stream_with_timeout @ tools/voice_mode.py:voice_close_stream_with_timeout */
/* Port of Python tools/voice_mode.py:_close_stream_with_timeout(). */
int voice_close_stream_with_timeout(int timeout_sec) {
    if (!g_recorder.running) return 0;

    pthread_mutex_lock(&g_recorder.mutex);
    g_recorder.recording = 0;
    g_recorder.running = 0; /* signal thread to exit */
    pthread_mutex_unlock(&g_recorder.mutex);

    if (timeout_sec > 0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double deadline = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9 + timeout_sec;
        int joined = 0;
        while (!joined) {
            struct timespec now_ts;
            clock_gettime(CLOCK_MONOTONIC, &now_ts);
            double now = (double)now_ts.tv_sec + (double)now_ts.tv_nsec / 1e9;
            if (now >= deadline) break;
            int rc = pthread_tryjoin_np(g_recorder.thread, NULL);
            if (rc == 0) { joined = 1; break; }
            usleep(10000);
        }
        if (!joined) pthread_detach(g_recorder.thread);
    } else {
        pthread_join(g_recorder.thread, NULL);
    }
    return 0;
}

/* PoP: _stop_termux_recording @ tools/voice_mode.py:voice_stop_termux_recording */
/* Port of Python tools/voice_mode.py:_stop_termux_recording(). */
void voice_stop_termux_recording(void) {
    system("termux-microphone-record -s 2>/dev/null");
}

/* ================================================================
 *  WAV File Writing
 * ================================================================ */

typedef struct {
    char chunk_id[4];
    uint32_t chunk_size;
    char format[4];
    char subchunk1_id[4];
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char subchunk2_id[4];
    uint32_t subchunk2_size;
} wav_header_t;

/* PoP: _write_wav @ tools/voice_mode.py:voice_write_wav */
/* Port of Python tools/voice_mode.py:_write_wav(). */
int voice_write_wav(const char *file_path, const void *data, size_t data_len,
                    int sample_rate, int channels, int bits_per_sample) {
    if (!file_path || !data || data_len == 0) return -1;
    
    FILE *f = fopen(file_path, "wb");
    if (!f) return -1;
    
    wav_header_t header = {0};
    memcpy(header.chunk_id, "RIFF", 4);
    memcpy(header.format, "WAVE", 4);
    memcpy(header.subchunk1_id, "fmt ", 4);
    memcpy(header.subchunk2_id, "data", 4);
    
    header.subchunk1_size = 16;
    header.audio_format = 1;  /* PCM */
    header.num_channels = channels > 0 ? channels : 1;
    header.sample_rate = sample_rate > 0 ? sample_rate : 16000;
    header.bits_per_sample = bits_per_sample > 0 ? bits_per_sample : 16;
    header.byte_rate = header.sample_rate * header.num_channels * header.bits_per_sample / 8;
    header.block_align = header.num_channels * header.bits_per_sample / 8;
    header.subchunk2_size = (uint32_t)data_len;
    header.chunk_size = 36 + header.subchunk2_size;
    
    fwrite(&header, sizeof(header), 1, f);
    fwrite(data, 1, data_len, f);
    fclose(f);
    return 0;
}

/* ================================================================
 *  Transcription
 * ================================================================ */

/* PoP: is_whisper_hallucination @ tools/voice_mode.py:is_whisper_hallucination */
/* Port of Python tools/voice_mode.py:is_whisper_hallucination(). */
int is_whisper_hallucination(const char *text) {
    if (!text) return 0;

    /* Exact-match against known phrases (mirrors WHISPER_HALLUCINATIONS set). */
    static const char *const halluc[] = {
        "thank you.", "thank you", "thanks for watching.", "thanks for watching",
        "subscribe to my channel.", "subscribe to my channel",
        "like and subscribe.", "like and subscribe",
        "please subscribe.", "please subscribe",
        "thank you for watching.", "thank you for watching",
        "bye.", "bye", "you", "the end.", "the end",
        "продолжение следует", "продолжение следует...",
        "sous-titres", "sous-titres réalisés par la communauté d'amara.org",
        "sottotitoli creati dalla comunità amara.org",
        "untertitel von stephanie geiges", "amara.org", "www.mooji.org",
        "ご視聴ありがとうございました", NULL
    };

    char lower[1024];
    int n = (int)strlen(text);
    if (n >= (int)sizeof(lower)) n = (int)sizeof(lower) - 1;
    for (int i = 0; i < n; i++) lower[i] = (char)tolower((unsigned char)text[i]);
    lower[n] = '\0';

    /* Empty / whitespace-only transcript == silence == hallucination
     * (mirrors Python: `if not cleaned: return True`). */
    int nonempty = 0;
    for (int i = 0; i < n; i++) if (!isspace((unsigned char)lower[i])) { nonempty = 1; break; }
    if (!nonempty) return 1;

    /* rstrip .! */
    int e = n - 1;
    while (e >= 0 && (lower[e] == '.' || lower[e] == '!' || lower[e] == ' ')) e--;
    int len = e + 1;
    /* null-terminate the trimmed copy */
    char trimmed[1024];
    if (len > 0) memcpy(trimmed, lower, len);
    trimmed[len > 0 ? len : 0] = '\0';

    for (int i = 0; halluc[i]; i++) {
        if (strcmp(trimmed, halluc[i]) == 0) return 1;
        if (strcmp(lower, halluc[i]) == 0) return 1;
    }

    /* Repetitive patterns (e.g. "Thank you. Thank you. Thank you.").
     * Mirrors _HALLUCINATION_REPEAT_RE =
     *   ^(?:thank you|thanks|bye|you|ok|okay|the end|\.|\\s|,|!)+$ */
    static const char *const words[] = {
        "thank you", "thanks", "bye", "you", "ok", "okay", "the end", NULL
    };
    /* Build the allowed token set; scan the string eating tokens. */
    int pos = 0;
    int matched_any = 0;
    int ok = 1;
    while (pos < len) {
        /* skip separators */
        while (pos < len && (lower[pos]==' '||lower[pos]==','||lower[pos]=='.'||lower[pos]=='!'))
            pos++;
        if (pos >= len) break;
        int ate = 0;
        for (int i = 0; words[i]; i++) {
            int wl = (int)strlen(words[i]);
            if (len - pos >= wl && strncmp(lower + pos, words[i], wl) == 0) {
                pos += wl; ate = 1; matched_any = 1; break;
            }
        }
        if (!ate) { ok = 0; break; }
    }
    if (ok && matched_any) return 1;

    return 0;
}

/* PoP: _should_chunk_for_transcription @ tools/voice_mode.py:should_chunk_for_transcription */
/* Port of Python tools/voice_mode.py:_should_chunk_for_transcription(). */
int should_chunk_for_transcription(const char *file_path, int chunk_seconds) {
    if (!file_path) return 0;
    
    struct stat st;
    if (stat(file_path, &st) != 0) return 0;
    
    /* Estimate duration from file size (rough estimate for 16kHz 16-bit mono) */
    size_t bytes_per_sec = 16000 * 2;
    int estimated_seconds = st.st_size / bytes_per_sec;
    
    return estimated_seconds > chunk_seconds;
}

/* PoP: _split_wav_for_transcription @ tools/voice_mode.py:split_wav_for_transcription */
/* Port of Python tools/voice_mode.py:_split_wav_for_transcription(). */
char **split_wav_for_transcription(const char *file_path, int chunk_seconds, int *count) {
    if (!file_path || !count) return NULL;
    *count = 0;
    
    if (!should_chunk_for_transcription(file_path, chunk_seconds)) {
        char **result = malloc(2 * sizeof(char *));
        if (!result) return NULL;
        result[0] = strdup(file_path);
        result[1] = NULL;
        *count = 1;
        return result;
    }
    
    /* Use sox to split the file */
    char out_dir[512];
    snprintf(out_dir, sizeof(out_dir), "/tmp/hermes_wav_chunks_%d", getpid());
    mkdir(out_dir, 0755);
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), 
        "sox %s %s/chunk.wav trim 0 %d : newfile : restart 2>/dev/null",
        file_path, out_dir, chunk_seconds);
    
    if (system(cmd) != 0) {
        rmdir(out_dir);
        return NULL;
    }
    
    /* Count chunks */
    DIR *dir = opendir(out_dir);
    if (!dir) return NULL;
    
    int num_chunks = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".wav")) num_chunks++;
    }
    closedir(dir);
    
    if (num_chunks == 0) {
        rmdir(out_dir);
        return NULL;
    }
    
    char **result = malloc((num_chunks + 1) * sizeof(char *));
    if (!result) return NULL;
    
    dir = opendir(out_dir);
    int idx = 0;
    while ((entry = readdir(dir)) != NULL && idx < num_chunks) {
        if (strstr(entry->d_name, ".wav")) {
            result[idx] = malloc(512);
            snprintf(result[idx], 512, "%s/%s", out_dir, entry->d_name);
            idx++;
        }
    }
    closedir(dir);
    result[idx] = NULL;
    *count = num_chunks;
    return result;
}

/* PoP: _transcribe_wav_in_chunks @ tools/voice_mode.py:transcribe_wav_in_chunks */
/* Port of Python tools/voice_mode.py:_transcribe_wav_in_chunks(). */
char *transcribe_wav_in_chunks(const char *file_path, int chunk_seconds, const char *model) {
    if (!file_path) return NULL;
    
    int count = 0;
    char **chunks = split_wav_for_transcription(file_path, chunk_seconds, &count);
    if (!chunks || count == 0) return NULL;
    
    char *combined = malloc(8192);
    combined[0] = '\0';
    
    for (int i = 0; i < count; i++) {
        char *transcript = transcribe_audio(chunks[i], model);
        if (transcript) {
            json_t *j = json_parse(transcript, NULL);
            if (j) {
                const char *text = json_get_str(j, "transcript", "");
                if (text && *text) {
                    strncat(combined, text, 8191 - strlen(combined));
                    if (i < count - 1) strncat(combined, " ", 8191 - strlen(combined));
                }
                json_free(j);
            }
            free(transcript);
        }
        free(chunks[i]);
    }
    free(chunks);
    
    if (!combined[0]) {
        free(combined);
        return NULL;
    }
    return combined;
}

/* PoP: transcribe_recording @ tools/voice_mode.py:transcribe_recording */
/* Port of Python tools/voice_mode.py:transcribe_recording().
 * Delegates to transcribe_audio(); filters Whisper hallucinations on
 * silence, and maps provider no_speech (empty transcript) to a silent
 * success so the voice loop re-listens quietly. */
char *transcribe_recording(const char *file_path, const char *model, int chunk_seconds) {
    if (!file_path) return strdup("{\"success\":false,\"error\":\"No file path\"}");

    char *raw;
    if (chunk_seconds > 0 && should_chunk_for_transcription(file_path, chunk_seconds)) {
        char *chunked = transcribe_wav_in_chunks(file_path, chunk_seconds, model);
        if (!chunked) return strdup("{\"success\":false,\"error\":\"Chunked transcription failed\"}");
        raw = chunked;
    } else {
        raw = transcribe_audio(file_path, model);
        if (!raw) return strdup("{\"success\":false,\"error\":\"Transcription failed\"}");
    }

    /* Parse shared result (success/transcript/no_speech/provider). */
    json_t *root = json_parse(raw, NULL);
    if (!root) { free(raw); return strdup("{\"success\":false,\"error\":\"bad result\"}"); }
    int success = json_get_bool(root, "success", 0);
    const char *transcript = json_get_str(root, "transcript", "");
    int no_speech = json_get_bool(root, "no_speech", 0);
    const char *provider = json_get_str(root, "provider", "");
    int filtered = 0;

    if (success && transcript && *transcript && is_whisper_hallucination(transcript)) {
        transcript = "";
        filtered = 1;
    }
    if (no_speech) {
        /* Provider returned empty transcript (no_speech) — treat like silence. */
        success = 1;
        transcript = "";
        no_speech = 1;
    }

    int need = (int)strlen(transcript) + 256;
    char *result = malloc(need);
    snprintf(result, need,
        "{\"success\":%s,\"transcript\":\"%s\"%s%s%s%s%s}",
        success ? "true" : "false",
        transcript ? transcript : "",
        filtered ? ",\"filtered\":true" : "",
        no_speech ? ",\"no_speech\":true" : "",
        provider && *provider ? ",\"provider\":\"" : "",
        provider && *provider ? provider : "",
        provider && *provider ? "\"" : "");
    json_free(root);
    free(raw);
    return result;
}

/* ================================================================
 *  Utility
 * ================================================================ */

/* PoP: check_voice_requirements @ tools/voice_mode.py:check_voice_requirements */
/* Port of Python tools/voice_mode.py:check_voice_requirements(). */
char *check_voice_requirements(void) {
    char *result = malloc(2048);
    result[0] = '\0';
    
    strcat(result, "Voice Mode Requirements Check:\n");
    strcat(result, "==============================\n");
    
    const char *backend = voice_import_audio();
    if (backend) {
        snprintf(result + strlen(result), 2048 - strlen(result),
            "✓ Audio capture: %s\n", backend);
    } else {
        strcat(result, "✗ Audio capture: NOT AVAILABLE\n");
        voice_capture_install_hint();
    }
    
    if (access("/usr/bin/whisper", X_OK) == 0 || 
        access("/usr/local/bin/whisper", X_OK) == 0) {
        strcat(result, "✓ Local Whisper: available\n");
    } else {
        strcat(result, "✗ Local Whisper: not installed (pip install openai-whisper)\n");
    }
    
    if (getenv("GROQ_API_KEY") || getenv("OPENAI_API_KEY") || 
        getenv("XAI_API_KEY") || getenv("MISTRAL_API_KEY") || 
        getenv("ELEVENLABS_API_KEY")) {
        strcat(result, "✓ Cloud STT: API key configured\n");
    } else {
        strcat(result, "✗ Cloud STT: no API keys found\n");
    }
    
    return result;
}

/* PoP: cleanup_temp_recordings @ tools/voice_mode.py:cleanup_temp_recordings */
/* Port of Python tools/voice_mode.py:cleanup_temp_recordings(). */
int cleanup_temp_recordings(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), 
        "find /tmp -maxdepth 1 -name 'hermes_voice_*' -o -name 'hermes_wav_chunks_*' 2>/dev/null | xargs rm -rf 2>/dev/null");
    system(cmd);
    return 0;  /* Best effort */
}

/* ================================================================
 *  Basic Record/Transcribe/Playback (existing)
 * ================================================================ */

char *voice_record(const char *output_path, int max_seconds) {
    (void)max_seconds;
    if (!output_path) output_path = "/tmp/hermes_voice_input.wav";
    /* Use the VAD-backed recorder: start, then stop on silence (the
     * callback writes the WAV). Mirrors the Python flow where silence
     * detection ends the capture. */
    voice_recorder_start(NULL);
    /* Block until the capture thread signals a stop (silence or max_wait).
     * We poll is_recording() like a join on the silence condition. */
    while (is_recording()) {
        usleep(20000); /* 20ms */
    }
    return voice_recorder_stop();
}

/* PoP: voice_transcribe @ tools/transcription_tools.py:transcribe */
/* PoP: voice_transcribe @ transcription_provider:transcribe */
char *voice_transcribe(const char *audio_path) {
    if (!audio_path) return NULL;

    if (g_voice_asr_cmd[0]) {
        char cmd[4096];
        int r = snprintf(cmd, sizeof(cmd), "%s %s 2>/dev/null", g_voice_asr_cmd, audio_path);
        if (r < 0 || (size_t)r >= sizeof(cmd)) return NULL;

        FILE *fp = popen(cmd, "r");
        if (!fp) return NULL;

        char result[4096];
        result[0] = '\0';
        if (fgets(result, (int)sizeof(result) - 1, fp)) {
            size_t len = strlen(result);
            while (len > 0 && (result[len-1] == '\n' || result[len-1] == '\r'))
                result[--len] = '\0';
        }
        pclose(fp);
        return result[0] ? strdup(result) : NULL;
    }

    /* Default: reuse the shared STT pipeline (groq/openai/xai/...). */
    char *j = transcribe_audio(audio_path, NULL);
    if (!j) return NULL;
    /* Extract "transcript" field from the JSON result. */
    json_t *root = json_parse(j, NULL);
    free(j);
    if (!root) return NULL;
    const char *t = json_get_str(root, "transcript", NULL);
    char *out = t && *t ? strdup(t) : NULL;
    json_free(root);
    return out;
}

char *voice_listen(void) {
    printf("🎤 Listening (VAD endpointing)...\n");
    fflush(stdout);

    char *audio = voice_record(NULL, g_voice_timeout);
    if (!audio) {
        printf("❌ Recording failed (no speech detected). Check microphone.\n");
        return NULL;
    }

    printf("🔄 Transcribing...\n");
    fflush(stdout);

    char *text = voice_transcribe(audio);
    free(audio);

    if (!text) {
        printf("❌ Transcription failed.\n");
        return NULL;
    }

    printf("📝 You said: %s\n", text);
    return text;
}

/* ================================================================
 *  listen_for_speech — barge-in VAD monitor
 *
 *  Ports Python tools/voice_mode.py:listen_for_speech(). Blocks until
 *  sustained speech is heard (or should_stop). With capture=TRUE it ALSO
 *  records the interruption with a pre-roll ring buffer so the utterance
 *  is kept from its first syllable, then endpoint-detects silence and
 *  returns the WAV path. on_trigger fires at detection time.
 *
 *  Unlike Python we run the monitor inline (no separate thread needed by
 *  the caller): should_stop() is polled between blocks.
 * ================================================================ */
/* PoP: voice_listen_for_speech @ tools/voice_mode.py:listen_for_speech */
char *voice_listen_for_speech(int (*should_stop)(void),
                              int threshold, int sustained_ms, int calibration_ms,
                              int capture, void (*on_trigger)(void),
                              int pre_roll_ms, int endpoint_silence_ms,
                              int max_utterance_ms) {
    snd_pcm_t *pcm = NULL;
    int rc = snd_pcm_open(&pcm, g_voice_device, SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) return NULL; /* no audio -> None */
    snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                       VOICE_CHANNELS, VOICE_SAMPLE_RATE, 1, 20000);

    int block = VOICE_BLOCK_FRAMES; /* 30ms */
    int calib_blocks = calibration_ms > 0 ? calibration_ms / 30 : 14;
    int trip_blocks  = sustained_ms  > 0 ? sustained_ms  / 30 : 10;
    int endpoint_blocks = endpoint_silence_ms > 0 ? endpoint_silence_ms / 30 : 42; /* 1250ms */
    int max_blocks = max_utterance_ms > 0 ? max_utterance_ms / 30 : 1000; /* 30s */

    double *floor = malloc(sizeof(double) * (calib_blocks > 0 ? calib_blocks : 1));
    int floor_n = 0;
    /* Pre-roll ring buffer of blocks (each block = block*channels int16). */
    int pre_roll_blocks = pre_roll_ms > 0 ? pre_roll_ms / 30 : 40; /* 1200ms */
    int16_t **pre_roll = calloc(pre_roll_blocks > 0 ? pre_roll_blocks : 1, sizeof(int16_t *));
    int pre_roll_head = 0, pre_roll_count = 0;

    int consecutive = 0;
    char *out_path = NULL;
    int stopped = 0;

    while (!stopped) {
        if (should_stop && should_stop()) { stopped = 1; break; }

        int16_t chunk[block * 8];
        snd_pcm_uframes_t got = snd_pcm_readi(pcm, chunk, block);
        if (got == (snd_pcm_uframes_t)-EPIPE) { snd_pcm_recover(pcm, -EPIPE, 1); continue; }
        if (got <= 0) continue;

        double rms = voice_block_rms(chunk, got * VOICE_CHANNELS);

        /* Maintain pre-roll ring (capture). */
        if (capture) {
            int16_t *cp = malloc((size_t)got * VOICE_CHANNELS * sizeof(int16_t));
            if (cp) {
                memcpy(cp, chunk, (size_t)got * VOICE_CHANNELS * sizeof(int16_t));
                if (pre_roll[pre_roll_head]) free(pre_roll[pre_roll_head]);
                pre_roll[pre_roll_head] = cp;
                pre_roll_head = (pre_roll_head + 1) % (pre_roll_blocks > 0 ? pre_roll_blocks : 1);
                if (pre_roll_count < pre_roll_blocks) pre_roll_count++;
            }
        }

        if (floor_n < calib_blocks) {
            if (floor_n < calib_blocks) floor[floor_n++] = rms;
            continue;
        }

        double median = 0.0;
        { /* simple median over calibration floor */
            double *tmp = malloc(sizeof(double) * floor_n);
            memcpy(tmp, floor, sizeof(double) * floor_n);
            for (int i = 0; i < floor_n; i++)
                for (int j = i+1; j < floor_n; j++)
                    if (tmp[j] < tmp[i]) { double t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t; }
            median = tmp[floor_n/2];
            free(tmp);
        }
        double trig = threshold > 0 ? (double)threshold : (VOICE_SILENCE_RMS_THRESHOLD * 2.0);
        if (trig < median * 3.5) trig = median * 3.5;

        consecutive = (rms >= trig) ? consecutive + 1 : 0;
        if (consecutive < trip_blocks) continue;

        /* Tripped. */
        if (on_trigger) { on_trigger(); }
        if (!capture) { out_path = NULL; stopped = 1; break; }

        /* Capture: keep rolling until silence endpoint, prepending pre-roll. */
        /* Build frames = pre-roll (all blocks) + live frames until quiet. */
        size_t cap = (size_t)(pre_roll_count + max_blocks) * block * VOICE_CHANNELS + 1024;
        int16_t *frames = malloc(cap * sizeof(int16_t));
        size_t flen = 0;
        if (frames) {
            for (int i = 0; i < pre_roll_count; i++) {
                int idx = (pre_roll_head - pre_roll_count + i);
                idx = (idx % (pre_roll_blocks > 0 ? pre_roll_blocks : 1) +
                       (pre_roll_blocks > 0 ? pre_roll_blocks : 1)) %
                      (pre_roll_blocks > 0 ? pre_roll_blocks : 1);
                int16_t *b = pre_roll[idx];
                if (b) {
                    memcpy(frames + flen, b, (size_t)got * VOICE_CHANNELS * sizeof(int16_t));
                    flen += (size_t)got * VOICE_CHANNELS;
                }
            }
            int quiet = 0;
            for (int k = 0; k < max_blocks; k++) {
                if (should_stop && should_stop()) { stopped = 1; break; }
                int16_t live[block * 8];
                snd_pcm_uframes_t g2 = snd_pcm_readi(pcm, live, block);
                if (g2 == (snd_pcm_uframes_t)-EPIPE) { snd_pcm_recover(pcm, -EPIPE, 1); continue; }
                if (g2 <= 0) continue;
                if (flen + (size_t)g2 * VOICE_CHANNELS <= cap)
                    memcpy(frames + flen, live, (size_t)g2 * VOICE_CHANNELS * sizeof(int16_t));
                flen += (size_t)g2 * VOICE_CHANNELS;
                double lr = voice_block_rms(live, g2 * VOICE_CHANNELS);
                quiet = (lr < VOICE_SILENCE_RMS_THRESHOLD) ? quiet + 1 : 0;
                if (quiet >= endpoint_blocks) break;
            }
            char wav_path[1024];
            snprintf(wav_path, sizeof(wav_path), "/tmp/hermes_voice/barge_%ld.wav", (long)time(NULL));
            mkdir("/tmp/hermes_voice", 0755);
            if (voice_write_wav(wav_path, frames, flen * sizeof(int16_t),
                                VOICE_SAMPLE_RATE, VOICE_CHANNELS, 16) == 0)
                out_path = strdup(wav_path);
            free(frames);
        }
        stopped = 1;
        break;
    }

    for (int i = 0; i < pre_roll_blocks; i++) if (pre_roll[i]) free(pre_roll[i]);
    free(pre_roll);
    free(floor);
    snd_pcm_close(pcm);
    return out_path; /* NULL when capture and no trip / no audio / stopped */
}

void voice_speak(const char *text) {
    if (!text) return;
    printf("🔊 Speaking...\n");
    fflush(stdout);

    char cmd[8192];
    int r = snprintf(cmd, sizeof(cmd),
        "espeak \"%s\" 2>/dev/null || say \"%s\" 2>/dev/null || echo 'TTS not available'",
        text, text);
    if (r > 0 && (size_t)r < sizeof(cmd))
        system(cmd);
}

static char *voice_listen_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    if (!g_voice_enabled)
        return strdup("{\"error\": \"Voice mode not enabled. Use /voice to enable.\"}");

    char *text = voice_listen();
    if (!text)
        return strdup("{\"error\": \"Voice input failed\"}");

    char *result = (char *)malloc(strlen(text) + 64);
    if (!result) { free(text); return strdup("{\"error\": \"OOM\"}"); }
    snprintf(result, strlen(text) + 64,
        "{\"transcript\": \"%s\", \"status\": \"ok\"}", text);
    free(text);
    return result;
}

static char *voice_speak_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\": \"Invalid JSON\"}");

    const char *text = json_get_str(args, "text", "");
    json_free(args);

    if (!text || !*text)
        return strdup("{\"error\": \"Missing 'text' parameter\"}");

    voice_speak(text);
    return strdup("{\"status\": \"spoken\"}");
}

static char *transcribe_audio_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"JSON parse\"}");

    const char *file_path = json_get_str(args, "file_path", NULL);
    const char *model = json_get_str(args, "model", NULL);

    json_free(args);

    if (!file_path || !*file_path)
        return strdup("{\"error\":\"Missing file_path\"}");

    char *result = transcribe_audio(file_path, model);
    if (!result)
        return strdup("{\"error\":\"Transcription failed\"}");

    return result;
}

static char *voice_barge_in_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!g_voice_enabled)
        return strdup("{\"error\": \"Voice mode not enabled. Use /voice to enable.\"}");

    json_t *args = json_parse(args_json, NULL);
    int capture = args ? (int)json_get_num(args, "capture", 1) : 1;
    int sustained_ms = args ? (int)json_get_num(args, "sustained_ms", 300) : 300;
    int calibration_ms = args ? (int)json_get_num(args, "calibration_ms", 400) : 400;
    int pre_roll_ms = args ? (int)json_get_num(args, "pre_roll_ms", 1200) : 1200;
    int endpoint_silence_ms = args ? (int)json_get_num(args, "endpoint_silence_ms", 1250) : 1250;
    int max_utterance_ms = args ? (int)json_get_num(args, "max_utterance_ms", 30000) : 30000;
    if (args) json_free(args);

    /* No TTS playing in this CLI context: should_stop never fires early. */
    char *path = voice_listen_for_speech(NULL, 0, sustained_ms, calibration_ms,
                                         capture, NULL, pre_roll_ms,
                                         endpoint_silence_ms, max_utterance_ms);
    if (!path)
        return strdup("{\"heard\":false}");

    char *result = malloc(strlen(path) + 32);
    snprintf(result, strlen(path) + 32, "{\"heard\":true,\"wav_path\":\"%s\"}", path);
    free(path);
    return result;
}

static char *voice_detect_audio_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    char *env = detect_audio_environment();
    if (!env) return strdup("{\"error\":\"No audio environment\"}");
    
    char *result = malloc(strlen(env) + 64);
    snprintf(result, strlen(env) + 64, "{\"environment\":\"%s\"}", env);
    free(env);
    return result;
}

static char *voice_check_requirements_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    return check_voice_requirements();
}

static char *voice_cleanup_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    cleanup_temp_recordings();
    return strdup("{\"status\":\"cleaned\"}");
}

void registry_init_voice(void) {
    registry_register("voice_listen",
        "Record from microphone and return transcribed text.",
        "{\"type\":\"object\",\"properties\":{}}",
        voice_listen_handler);

    registry_register("voice_barge_in",
        "Barge-in VAD monitor: block until sustained speech is heard (or timeout). "
        "With capture=true, records the interruption (pre-roll + endpoint silence) and "
        "returns the WAV path. Mirrors tools/voice_mode.py:listen_for_speech().",
        "{\"type\":\"object\",\"properties\":{"
        "\"capture\":{\"type\":\"boolean\",\"description\":\"Also record the utterance (default true)\"},"
        "\"sustained_ms\":{\"type\":\"integer\",\"description\":\"Consecutive above-threshold ms to trip\"},"
        "\"calibration_ms\":{\"type\":\"integer\",\"description\":\"Noise-floor calibration ms\"},"
        "\"pre_roll_ms\":{\"type\":\"integer\",\"description\":\"Pre-roll buffer ms kept from first syllable\"},"
        "\"endpoint_silence_ms\":{\"type\":\"integer\",\"description\":\"Silence ms to end the utterance\"},"
        "\"max_utterance_ms\":{\"type\":\"integer\",\"description\":\"Max utterance length ms\"}"
        "},\"required\":[]}",
        voice_barge_in_handler);

    registry_register("voice_speak",
        "Speak the given text using text-to-speech.",
        "{\"type\":\"object\",\"properties\":{"
        "\"text\":{\"type\":\"string\",\"description\":\"Text to speak\"}"
        "},\"required\":[\"text\"]}",
        voice_speak_handler);

    registry_register("voice_detect_audio",
        "Detect available audio environment/backend.",
        "{\"type\":\"object\",\"properties\":{}}",
        voice_detect_audio_handler);

    registry_register("voice_check_requirements",
        "Check voice mode requirements and dependencies.",
        "{\"type\":\"object\",\"properties\":{}}",
        voice_check_requirements_handler);

    registry_register("voice_cleanup",
        "Clean up temporary voice recordings.",
        "{\"type\":\"object\",\"properties\":{}}",
        voice_cleanup_handler);

    registry_register("transcribe_audio",
        "Transcribe an audio file to text. "
        "Supports: mp3, wav, m4a, ogg, webm, flac, aac. "
        "Providers: groq (default), openai, xai, mistral, elevenlabs, local_command. "
        "Max file size: 25 MB. "
        "Set provider via model param: \"groq:whisper-large-v3-turbo\" or \"openai:whisper-1\".",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"file_path\":{\"type\":\"string\","
            "\"description\":\"Absolute path to audio file\"},"
          "\"model\":{\"type\":\"string\","
            "\"description\":\"Provider:model override, e.g. groq:whisper-large-v3-turbo\"}"
        "},"
        "\"required\":[\"file_path\"]"
        "}", transcribe_audio_handler);
}
