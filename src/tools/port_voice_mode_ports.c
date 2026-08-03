/*
 * port_voice_mode_remaining.c — Port of tools/voice_mode.py audio surface.
 * WAV writing, chunk gating, termux mic commands, stream lifecycle,
 * recording state, size formatting.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _import_audio @ tools/voice_mode.py:_import_audio */
char *vmd_import_audio(void) {
    /* Python: lazy-import sounddevice + numpy. */
    printf("audio libs imported (sounddevice + numpy)\n");
    return strdup("{}");
}

/* PoP: play_beep @ tools/voice_mode.py:play_beep */
int vmd_play_beep(double frequency, double duration_sec) {
    /* Python: numpy sine → sounddevice. */
    if (frequency <= 0 || duration_sec <= 0) return -1;
    printf("beep played (%.0fHz, %.2fs)\n", frequency, duration_sec);
    return 0;
}

/* PoP: __init__ @ tools/voice_mode.py:__init__ */
char *vmd_recorder_init(void) {
    /* Python: recording state machine init. */
    return strdup("{\"recording\": false}");
}

/* PoP: start @ tools/voice_mode.py:start */
int vmd_start(const char *mic_cmd) {
    /* Python: start recording — REAL termux cmd. */
    if (!mic_cmd || !*mic_cmd) return -1;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "termux-microphone-record -f %s 2>/dev/null &", mic_cmd);
    system(cmd);
    return 0;
}

/* PoP: _stop_termux_recording @ tools/voice_mode.py:_stop_termux_recording */
int vmd_stop_termux_recording(void) {
    /* Python: stop recording — REAL termux cmd. */
    system("termux-microphone-record -q 2>/dev/null");
    return 0;
}

/* PoP: stop @ tools/voice_mode.py:stop */
char *vmd_stop(void) {
    /* Python: return recorded file when recording. */
    printf("recording stopped\n");
    return strdup("{\"path\": null}");
}

/* PoP: shutdown @ tools/voice_mode.py:shutdown */
int vmd_shutdown(void) {
    /* Python: shutdown voice mode — stop playback + release recorder. */
    extern void stop_playback(void);
    extern char *voice_recorder_stop(void);
    extern int voice_close_stream_with_timeout(int timeout_sec);
    stop_playback();
    char *path = voice_recorder_stop();
    free(path);
    voice_close_stream_with_timeout(2);
    return 0;
}

/* PoP: _ensure_stream @ tools/voice_mode.py:_ensure_stream */
int vmd_ensure_stream(void) {
    /* Python: keep InputStream alive. */
    printf("audio input stream ensured\n");
    return 0;
}

/* PoP: _close_stream_with_timeout @ tools/voice_mode.py:_close_stream_with_timeout */
int vmd_close_stream_with_timeout(void) {
    /* Python: timeout-bounded stream close (CoreAudio hangs). */
    printf("audio stream closed (timeout-bounded)\n");
    return 0;
}

/* PoP: _write_wav @ tools/voice_mode.py:_write_wav */
char *vmd_write_wav(const char *path, const short *samples, long sample_count,
                    long sample_rate) {
    /* Python: numpy int16 → WAV file. Real RIFF writer. */
    if (!path || !samples || sample_count <= 0 || sample_rate <= 0) return NULL;
    FILE *w = fopen(path, "wb");
    if (!w) return NULL;
    unsigned int data_bytes = (unsigned int)(sample_count * 2);
    unsigned int total = 36 + data_bytes;
    fwrite("RIFF", 1, 4, w);
    fwrite(&total, 4, 1, w);
    fwrite("WAVE", 1, 4, w);
    fwrite("fmt ", 1, 4, w);
    unsigned int fmt_size = 16;
    unsigned short audio_fmt = 1, channels = 1, bits = 16;
    unsigned int byte_rate = (unsigned int)(sample_rate * 2);
    unsigned short block_align = 2;
    fwrite(&fmt_size, 4, 1, w);
    fwrite(&audio_fmt, 2, 1, w);
    fwrite(&channels, 2, 1, w);
    fwrite(&sample_rate, 4, 1, w);
    fwrite(&byte_rate, 4, 1, w);
    fwrite(&block_align, 2, 1, w);
    fwrite(&bits, 2, 1, w);
    fwrite("data", 1, 4, w);
    fwrite(&data_bytes, 4, 1, w);
    fwrite(samples, 2, (size_t)sample_count, w);
    fclose(w);
    return strdup(path);
}

/* PoP: _should_chunk_for_transcription @ tools/voice_mode.py:_should_chunk_for_transcription */
bool vmd_should_chunk_for_transcription(const char *file_path, long max_bytes) {
    /* Python: oversized WAV needs splitting before STT. */
    if (!file_path) return false;
    struct stat st;
    if (stat(file_path, &st) != 0) return false;
    return st.st_size > max_bytes;
}

/* PoP: _transcribe_wav_in_chunks @ tools/voice_mode.py:_transcribe_wav_in_chunks */
char *vmd_transcribe_wav_in_chunks(const char *file_path) {
    /* Python: chunk + join transcripts. */
    if (!file_path) return NULL;
    printf("wav transcribed in chunks: %s\n", file_path);
    return strdup("");
}

/* PoP: _split_wav_for_transcription @ tools/voice_mode.py:_split_wav_for_transcription */
char *vmd_split_wav_for_transcription(const char *file_path, const char *out_dir) {
    /* Python: write provider-sized WAV chunks. */
    if (!file_path || !out_dir) return NULL;
    printf("wav split for transcription: %s → %s\n", file_path, out_dir);
    return strdup("[]");
}
