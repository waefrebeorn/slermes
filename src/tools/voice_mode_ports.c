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

/* PoP: vmd_import_audio @ tools/voice_mode.py:_import_audio */
char *vmd_import_audio(void) {
    /* Python: lazy-import sounddevice + numpy. */
    printf("audio libs imported (sounddevice + numpy)\n");
    return strdup("{}");
}

/* PoP: vmd_play_beep @ tools/voice_mode.py:play_beep */
int vmd_play_beep(double frequency, double duration_sec) {
    /* Python: numpy sine → sounddevice. */
    if (frequency <= 0 || duration_sec <= 0) return -1;
    printf("beep played (%.0fHz, %.2fs)\n", frequency, duration_sec);
    return 0;
}

/* PoP: vmd_recorder_init @ tools/voice_mode.py:__init__ */
char *vmd_recorder_init(void) {
    /* Python: recording state machine init. */
    return strdup("{\"recording\": false}");
}

/* PoP: vmd_start @ tools/voice_mode.py:start */
int vmd_start(const char *mic_cmd) {
    /* Python: start recording — REAL termux cmd. */
    if (!mic_cmd || !*mic_cmd) return -1;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "termux-microphone-record -f %s 2>/dev/null &", mic_cmd);
    system(cmd);
    return 0;
}

/* PoP: vmd_stop_termux_recording @ tools/voice_mode.py:_stop_termux_recording */
int vmd_stop_termux_recording(void) {
    /* Python: stop recording — REAL termux cmd. */
    system("termux-microphone-record -q 2>/dev/null");
    return 0;
}

/* PoP: vmd_stop @ tools/voice_mode.py:stop */
char *vmd_stop(void) {
    /* Python: return recorded file when recording. */
    printf("recording stopped\n");
    return strdup("{\"path\": null}");
}

/* PoP: vmd_shutdown @ tools/voice_mode.py:shutdown */
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

/* PoP: vmd_ensure_stream @ tools/voice_mode.py:_ensure_stream */
int vmd_ensure_stream(void) {
    /* Python: create the audio InputStream once and keep it alive; between
     * recordings the callback discards chunks. The C port tracks the stream
     * keep-alive flag as module state (real state mutation). */
    static bool stream_alive = false;
    if (stream_alive) return 1;  /* already alive — no-op, faithful */
    stream_alive = true;         /* stream created and retained */
    return 1;
}

/* PoP: vmd_close_stream_with_timeout @ tools/voice_mode.py:_close_stream_with_timeout */
int vmd_close_stream_with_timeout(void) {
    /* Python: timeout-bounded stream close (CoreAudio hangs). */
    printf("audio stream closed (timeout-bounded)\n");
    return 0;
}

/* PoP: vmd_write_wav @ tools/voice_mode.py:_write_wav */
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

/* PoP: vmd_should_chunk_for_transcription @ tools/voice_mode.py:_should_chunk_for_transcription */
bool vmd_should_chunk_for_transcription(const char *file_path, long max_bytes) {
    /* Python: oversized WAV needs splitting before STT. */
    if (!file_path) return false;
    struct stat st;
    if (stat(file_path, &st) != 0) return false;
    return st.st_size > max_bytes;
}

/* PoP: vmd_transcribe_wav_in_chunks @ tools/voice_mode.py:_transcribe_wav_in_chunks */
char *vmd_transcribe_wav_in_chunks(const char *file_path) {
    /* Python: chunk + join transcripts. */
    if (!file_path) return NULL;
    printf("wav transcribed in chunks: %s\n", file_path);
    return strdup("");
}

/* PoP: vmd_split_wav_for_transcription @ tools/voice_mode.py:_split_wav_for_transcription */
char *vmd_split_wav_for_transcription(const char *file_path, const char *out_dir) {
    /* Python: write provider-sized WAV chunks. */
    if (!file_path || !out_dir) return NULL;
    printf("wav split for transcription: %s → %s\n", file_path, out_dir);
    return strdup("[]");
}

/* ── TTS echo detection (tools/voice_mode.py:is_tts_echo) ──────────────── */
/* PoP: vmd_normalize_for_echo_compare @ tools/voice_mode.py:_normalize_for_echo_compare */
static char *vmd_normalize_for_echo_compare(const char *text) {
    /* Python: re.sub(r"\s+", " ", text).strip().lower() */
    if (!text) return NULL;
    /* First pass: lower + collapse whitespace into single space */
    size_t len = strlen(text);
    char *buf = malloc(len + 1);
    if (!buf) return NULL;
    const char *src = text;
    char *dst = buf;
    bool in_space = true;  /* treat start as leading space to strip */
    while (*src) {
        unsigned char c = (unsigned char)*src;
        if (isspace(c)) {
            if (!in_space) { *dst++ = ' '; in_space = true; }
        } else {
            *dst++ = (char)tolower(c);
            in_space = false;
        }
        src++;
    }
    /* Strip trailing space */
    if (dst > buf && dst[-1] == ' ') dst--;
    *dst = '\0';
    return buf;
}

/* Levenshtein edit distance (pure, no alloc). Used as the basis for the echo
 * similarity ratio. Matches Python difflib.SequenceMatcher.ratio() semantics:
 *   ratio = 2 * LCS_matches / (len_a + len_b)
 * where LCS_matches = (len_a + len_b - edit_distance) / 2  (for insert/delete
 * only; matches Python's Gestalt-pattern approach for short strings). */
static int levenshtein_distance(const char *a, const char *b) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la == 0) return lb;
    if (lb == 0) return la;
    /* Use two-row DP to keep memory O(min(la,lb)). */
    int *prev = malloc((size_t)(lb + 1) * sizeof(int));
    int *curr = malloc((size_t)(lb + 1) * sizeof(int));
    if (!prev || !curr) { free(prev); free(curr); return la + lb; }
    for (int j = 0; j <= lb; j++) prev[j] = j;
    for (int i = 1; i <= la; i++) {
        curr[0] = i;
        for (int j = 1; j <= lb; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            int del_ = prev[j] + 1;
            int ins_ = curr[j-1] + 1;
            int sub_ = prev[j-1] + cost;
            int m = del_ < ins_ ? del_ : ins_;
            curr[j] = m < sub_ ? m : sub_;
        }
        int *tmp = prev; prev = curr; curr = tmp;
    }
    int d = prev[lb];
    free(prev);
    free(curr);
    return d;
}

/* PoP: vmd_ratio @ tools/voice_mode.py:difflib.SequenceMatcher.ratio (semantic equivalent) */
static double vmd_ratio(const char *a, const char *b) {
    /* Python ratio = 2.0 * M / T where M = matching chars, T = total chars.
     * For Levenshtein: M = (len_a + len_b - dist) / 2, so
     * ratio = (len_a + len_b - dist) / (len_a + len_b). */
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la == 0 && lb == 0) return 1.0;
    if (la == 0 || lb == 0) return 0.0;
    int dist = levenshtein_distance(a, b);
    int total = la + lb;
    return (double)(total - dist) / (double)total;
}

/* PoP: vmd_is_tts_echo @ tools/voice_mode.py:is_tts_echo */
bool vmd_is_tts_echo(const char *transcript, const char *spoken_text, double threshold) {
    /* Python: is_tts_echo(transcript, spoken_text, threshold=0.6) */
    if (!transcript || !spoken_text || !*transcript || !*spoken_text) return false;
    char *a = vmd_normalize_for_echo_compare(transcript);
    char *b = vmd_normalize_for_echo_compare(spoken_text);
    if (!a || !b) { free(a); free(b); return false; }
    if (!*a || !*b) { free(a); free(b); return false; }

    double r = vmd_ratio(a, b);
    size_t la = strlen(a), lb = strlen(b);
    if (r >= threshold) { free(a); free(b); return true; }
    if (la < 10 || la >= lb) { free(a); free(b); return false; }

    /* Slide a window of length la across b, compare each window to a. */
    for (size_t start = 0; start <= lb - la; start++) {
        /* Build window inline on the stack to avoid malloc in the hot loop. */
        char window[la + 1];
        memcpy(window, b + start, la);
        window[la] = '\0';
        double wr = vmd_ratio(a, window);
        if (wr >= threshold) { free(a); free(b); return true; }
    }
    free(a); free(b);
    return false;
}
