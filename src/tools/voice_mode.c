/*
 * voice_mode.c — Voice input/output mode for Hermes C.
 * Wraps arecord + sox for microphone input + TTS for output.
 * Speech recognition via API (Whisper) or external asr binary.
 *
 * Phase 131-135: Voice mode parity with Python voice_mode.py.
 * Extended with full Python voice_mode.py parity (29 additional functions).
 */

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
#include <pthread.h>

/* ================================================================
 *  Configuration
 * ================================================================ */

static int g_voice_enabled = 0;
static char g_voice_device[128] = "default";  /* ALSA device */
static char g_voice_asr_cmd[512] = "";        /* External STT command */
static int g_voice_timeout = 5;               /* Record timeout seconds */

/* Recording state for streaming support */
static struct {
    pthread_t thread;
    pid_t arecord_pid;
    char output_path[512];
    int running;
    time_t start_time;
    int sample_rate;
    int channels;
    int format;  /* 0 = S16_LE, 1 = F32_LE */
    double rms;
    pthread_mutex_t mutex;
} g_recorder = {0};

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

/* PoP: _import_audio @ src/tools/voice_mode.c:voice_import_audio
 * Port of Python tools/voice_mode.py:_import_audio(). */
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

/* PoP: _audio_available @ src/tools/voice_mode.c:voice_audio_available
 * Port of Python tools/voice_mode.py:_audio_available(). */
int voice_audio_available(void) {
    /* Check if audio libraries are available */
    void *audio = voice_import_audio();
    if (audio != NULL) return 1;
    return 0;
}

/* PoP: _voice_capture_install_hint @ src/tools/voice_mode.c:voice_capture_install_hint
 * Port of Python tools/voice_mode.py:_voice_capture_install_hint(). */
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

/* PoP: _pulse_socket_reachable @ src/tools/voice_mode.c:voice_pulse_socket_reachable
 * Port of Python tools/voice_mode.py:_pulse_socket_reachable(). */
int voice_pulse_socket_reachable(void) {
    /* Check if PulseAudio socket is reachable */
    int result = check_pulse_socket();
    return result;
}

static int check_termux_api(void) {
    return access("/usr/bin/termux-api", X_OK) == 0 ||
           access("/data/data/com.termux/files/usr/bin/termux-api", X_OK) == 0;
}

/* PoP: _termux_api_app_installed @ src/tools/voice_mode.c:voice_termux_api_app_installed
 * Port of Python tools/voice_mode.py:_termux_api_app_installed(). */
int voice_termux_api_app_installed(void) {
    /* Check if Termux API app is installed */
    int result = check_termux_api();
    return result;
}

/* PoP: _termux_microphone_command @ src/tools/voice_mode.c:voice_termux_microphone_command
 * Port of Python tools/voice_mode.py:_termux_microphone_command(). */
char *voice_termux_microphone_command(void) {
    /* Get the termux-microphone-record command path */
    if (!check_termux_api()) return NULL;
    char *cmd = strdup("termux-microphone-record");
    return cmd;
}

/* PoP: _termux_voice_capture_available @ src/tools/voice_mode.c:voice_termux_voice_capture_available
 * Port of Python tools/voice_mode.py:_termux_voice_capture_available(). */
int voice_termux_voice_capture_available(void) {
    /* Check if Termux voice capture is available */
    int api = check_termux_api();
    return api;
}

/* PoP: detect_audio_environment @ src/tools/voice_mode.c:detect_audio_environment
 * Port of Python tools/voice_mode.py:detect_audio_environment(). */
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

/* PoP: _play_beep @ src/tools/voice_mode.c:play_beep
 * Port of Python voice.py:_play_beep(). */
/* PoP: play_beep @ voice_mode:play_beep */
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

/* PoP: play_audio_file @ src/tools/voice_mode.c:play_audio_file
 * Port of Python tools/voice_mode.py:play_audio_file(). */
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

/* PoP: stop_playback @ src/tools/voice_mode.c:stop_playback
 * Port of Python tools/voice_mode.py:stop_playback(). */
void stop_playback(void) {
    system("pkill -f \"aplay|paplay|play|mpg123|ffplay\" 2>/dev/null");
}

/* ================================================================
 *  Recording State Management
 * ================================================================ */

static void *record_thread(void *arg) {
    int max_seconds = (int)(intptr_t)arg;
    if (max_seconds <= 0) max_seconds = g_voice_timeout;
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "arecord -q -D %s -f S16_LE -r %d -c %d -t wav -d %d %s 2>/dev/null",
        g_voice_device, 
        g_recorder.sample_rate ? g_recorder.sample_rate : 16000,
        g_recorder.channels ? g_recorder.channels : 1,
        max_seconds,
        g_recorder.output_path);
    
    g_recorder.arecord_pid = fork();
    if (g_recorder.arecord_pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(1);
    } else if (g_recorder.arecord_pid > 0) {
        int status;
        waitpid(g_recorder.arecord_pid, &status, 0);
        g_recorder.arecord_pid = 0;
    }
    
    pthread_mutex_lock(&g_recorder.mutex);
    g_recorder.running = 0;
    pthread_mutex_unlock(&g_recorder.mutex);
    return NULL;
}

/* PoP: create_audio_recorder @ src/tools/voice_mode.c:create_audio_recorder
 * Port of Python tools/voice_mode.py:create_audio_recorder(). */
char *create_audio_recorder(const char *output_path, int max_seconds, 
                            int sample_rate, int channels) {
    pthread_mutex_lock(&g_recorder.mutex);
    
    if (g_recorder.running) {
        pthread_mutex_unlock(&g_recorder.mutex);
        return strdup("Recorder already running");
    }
    
    if (!output_path) output_path = "/tmp/hermes_voice_recording.wav";
    snprintf(g_recorder.output_path, sizeof(g_recorder.output_path), "%s", output_path);
    g_recorder.sample_rate = sample_rate > 0 ? sample_rate : 16000;
    g_recorder.channels = channels > 0 ? channels : 1;
    g_recorder.start_time = time(NULL);
    g_recorder.running = 1;
    g_recorder.rms = 0.0;
    
    int rc = pthread_create(&g_recorder.thread, NULL, record_thread, (void *)(intptr_t)max_seconds);
    if (rc != 0) {
        g_recorder.running = 0;
        pthread_mutex_unlock(&g_recorder.mutex);
        return strdup("Failed to create recording thread");
    }
    
    pthread_mutex_unlock(&g_recorder.mutex);
    return strdup("");
}

/* PoP: is_recording @ src/tools/voice_mode.c:is_recording
 * Port of Python tools/voice_mode.py:is_recording(). */
int is_recording(void) {
    pthread_mutex_lock(&g_recorder.mutex);
    int running = g_recorder.running;
    pthread_mutex_unlock(&g_recorder.mutex);
    return running;
}

/* PoP: elapsed_seconds @ src/tools/voice_mode.c:elapsed_seconds
 * Port of Python tools/voice_mode.py:elapsed_seconds(). */
int elapsed_seconds(void) {
    pthread_mutex_lock(&g_recorder.mutex);
    time_t start = g_recorder.start_time;
    int running = g_recorder.running;
    pthread_mutex_unlock(&g_recorder.mutex);
    
    if (!running || start == 0) return 0;
    return (int)(time(NULL) - start);
}

/* PoP: current_rms @ src/tools/voice_mode.c:current_rms
 * Port of Python tools/voice_mode.py:current_rms(). */
double current_rms(void) {
    pthread_mutex_lock(&g_recorder.mutex);
    double rms = g_recorder.rms;
    pthread_mutex_unlock(&g_recorder.mutex);
    return rms;
}

/* PoP: _ensure_stream @ src/tools/voice_mode.c:voice_ensure_stream
 * Port of Python tools/voice_mode.py:_ensure_stream(). */
int voice_ensure_stream(void) {
    if (!g_recorder.running) {
        return create_audio_recorder(NULL, g_voice_timeout, 
                                    g_recorder.sample_rate, g_recorder.channels) != NULL ? -1 : 0;
    }
    return 0;
}

/* PoP: _close_stream_with_timeout @ src/tools/voice_mode.c:voice_close_stream_with_timeout
 * Port of Python tools/voice_mode.py:_close_stream_with_timeout(). */
int voice_close_stream_with_timeout(int timeout_sec) {
    if (!g_recorder.running) return 0;
    
    pthread_mutex_lock(&g_recorder.mutex);
    if (g_recorder.arecord_pid > 0) {
        kill(g_recorder.arecord_pid, SIGTERM);
    }
    pthread_mutex_unlock(&g_recorder.mutex);
    
    if (timeout_sec > 0) {
        time_t deadline = time(NULL) + timeout_sec;
        int joined = 0;
        while (!joined && time(NULL) < deadline) {
            /* Try to join without blocking - pthread_tryjoin_np is GNU extension */
            int rc = pthread_join(g_recorder.thread, NULL);
            if (rc == 0) joined = 1;
            else if (rc == EBUSY || rc == EINVAL) {
                usleep(10000); /* 10ms */
                continue;
            } else {
                joined = 1; /* Some other error */
            }
        }
        if (!joined) pthread_detach(g_recorder.thread);
    } else {
        pthread_join(g_recorder.thread, NULL);
    }
    
    pthread_mutex_lock(&g_recorder.mutex);
    g_recorder.running = 0;
    g_recorder.arecord_pid = 0;
    pthread_mutex_unlock(&g_recorder.mutex);
    return 0;
}

/* PoP: _stop_termux_recording @ src/tools/voice_mode.c:voice_stop_termux_recording
 * Port of Python tools/voice_mode.py:_stop_termux_recording(). */
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

/* PoP: _write_wav @ src/tools/voice_mode.c:voice_write_wav
 * Port of Python tools/voice_mode.py:_write_wav(). */
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

/* PoP: is_whisper_hallucination @ src/tools/voice_mode.c:is_whisper_hallucination
 * Port of Python tools/voice_mode.py:is_whisper_hallucination(). */
int is_whisper_hallucination(const char *text) {
    if (!text) return 0;
    
    /* Common hallucination patterns in Whisper output */
    const char *patterns[] = {
        "thanks for watching",
        "subscribe",
        "like and subscribe",
        "thanks for listening",
        "don't forget to",
        "hit the bell",
        "turn on notifications",
        "follow me on",
        "check out my",
        "visit my website",
        "link in description",
        "social media",
        "patreon",
        "discord server",
        "join our community",
        "music playing",
        "[music]",
        "[applause]",
        "[silence]",
        NULL
    };
    
    char lower[1024];
    snprintf(lower, sizeof(lower), "%s", text);
    for (char *p = lower; *p; p++) *p = tolower((unsigned char)*p);
    
    for (int i = 0; patterns[i]; i++) {
        if (strstr(lower, patterns[i])) return 1;
    }
    return 0;
}

/* PoP: _should_chunk_for_transcription @ src/tools/voice_mode.c:should_chunk_for_transcription
 * Port of Python tools/voice_mode.py:_should_chunk_for_transcription(). */
int should_chunk_for_transcription(const char *file_path, int chunk_seconds) {
    if (!file_path) return 0;
    
    struct stat st;
    if (stat(file_path, &st) != 0) return 0;
    
    /* Estimate duration from file size (rough estimate for 16kHz 16-bit mono) */
    size_t bytes_per_sec = 16000 * 2;
    int estimated_seconds = st.st_size / bytes_per_sec;
    
    return estimated_seconds > chunk_seconds;
}

/* PoP: _split_wav_for_transcription @ src/tools/voice_mode.c:split_wav_for_transcription
 * Port of Python tools/voice_mode.py:_split_wav_for_transcription(). */
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

/* PoP: _transcribe_wav_in_chunks @ src/tools/voice_mode.c:transcribe_wav_in_chunks
 * Port of Python tools/voice_mode.py:_transcribe_wav_in_chunks(). */
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

/* PoP: transcribe_recording @ src/tools/voice_mode.c:transcribe_recording
 * Port of Python tools/voice_mode.py:transcribe_recording(). */
char *transcribe_recording(const char *file_path, const char *model, int chunk_seconds) {
    if (!file_path) return strdup("{\"success\":false,\"error\":\"No file path\"}");
    
    if (chunk_seconds > 0 && should_chunk_for_transcription(file_path, chunk_seconds)) {
        char *chunked = transcribe_wav_in_chunks(file_path, chunk_seconds, model);
        if (chunked) {
            char *result = malloc(strlen(chunked) + 128);
            snprintf(result, strlen(chunked) + 128, 
                "{\"success\":true,\"transcript\":\"%s\",\"provider\":\"chunked\"}", chunked);
            free(chunked);
            return result;
        }
    }
    
    return transcribe_audio(file_path, model);
}

/* ================================================================
 *  Utility
 * ================================================================ */

/* PoP: check_voice_requirements @ src/tools/voice_mode.c:check_voice_requirements
 * Port of Python tools/voice_mode.py:check_voice_requirements(). */
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

/* PoP: cleanup_temp_recordings @ src/tools/voice_mode.c:cleanup_temp_recordings
 * Port of Python tools/voice_mode.py:cleanup_temp_recordings(). */
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
    if (max_seconds <= 0) max_seconds = g_voice_timeout;
    if (!output_path) output_path = "/tmp/hermes_voice_input.wav";

    char cmd[1024];
    int r = snprintf(cmd, sizeof(cmd),
        "arecord -q -D %s -f cd -t wav -d %d %s 2>/dev/null || "
        "sox -q -d -t wav %s trim 0 %d 2>/dev/null",
        g_voice_device, max_seconds, output_path,
        output_path, max_seconds);

    if (r < 0 || (size_t)r >= sizeof(cmd)) return NULL;

    int rc = system(cmd);
    if (rc != 0) return NULL;

    if (access(output_path, F_OK) != 0) return NULL;

    return strdup(output_path);
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

    if (access("whisper", X_OK) == 0 || access("/usr/local/bin/whisper", X_OK) == 0) {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "whisper %s --language en --output-txt -o /tmp 2>/dev/null "
                 "&& cat /tmp/$(basename %s .wav).txt 2>/dev/null",
                 audio_path, audio_path);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char result[4096]; result[0] = '\0';
            fgets(result, (int)sizeof(result) - 1, fp);
            pclose(fp);
            if (result[0]) return strdup(result);
        }
    }

    return NULL;
}

char *voice_listen(void) {
    printf("🎤 Listening (timeout: %ds)...\n", g_voice_timeout);
    fflush(stdout);

    char *audio = voice_record(NULL, g_voice_timeout);
    if (!audio) {
        printf("❌ Recording failed. Check microphone.\n");
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