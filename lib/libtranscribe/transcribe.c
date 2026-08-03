/*
 * transcribe.c — Audio transcription for Hermes C.
 * Port of Python tools/transcription_tools.py.
 * Supports groq, openai, xai, mistral, elevenlabs providers via HTTP multipart POST.
 * Also supports local_command (whisper CLI).
 *
 * MIT License — WuBu Hermes Project
 */

#include "transcribe.h"
#include "http.h"
#include "json.h"
#include "whisper_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_http.h"
#include "hermes_core_types.h"
#include "hermes_plugin.h"
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <time.h>
#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <ctype.h>
#include <fcntl.h>

/* ================================================================
 *  Supported formats (sorted for bsearch)
 * ================================================================ */

static const char *SUPPORTED_FORMATS[] = {
    ".aac", ".flac", ".m4a", ".mp3", ".mp4",
    ".mpeg", ".mpga", ".ogg", ".wav", ".webm"
};

const char *transcribe_supported_formats[] = {
    ".aac", ".flac", ".m4a", ".mp3", ".mp4",
    ".mpeg", ".mpga", ".ogg", ".wav", ".webm",
    NULL
};

const int transcribe_supported_format_count = 10;

static int strcmp_wrapper(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

bool transcribe_is_supported_format(const char *ext) {
    if (!ext || !*ext) return false;
    /* Case-insensitive comparison */
    char lower[16];
    size_t i;
    for (i = 0; ext[i] && i < sizeof(lower) - 1; i++)
        lower[i] = (ext[i] >= 'A' && ext[i] <= 'Z') ? ext[i] + 32 : ext[i];
    lower[i] = '\0';
    const char *key = lower;
    return bsearch(&key, SUPPORTED_FORMATS,
                   sizeof(SUPPORTED_FORMATS) / sizeof(SUPPORTED_FORMATS[0]),
                   sizeof(char *), strcmp_wrapper) != NULL;
}

/* ================================================================
 *  JSON result helpers
 * ================================================================ */

static char *make_result(bool success, const char *transcript,
                          const char *error, const char *provider) {
    json_t *j = json_object();
    json_set(j, "success", json_bool(success));
    json_set(j, "transcript", json_string(transcript ? transcript : ""));
    if (error && *error)
        json_set(j, "error", json_string(error));
    if (provider && *provider)
        json_set(j, "provider", json_string(provider));
    char *s = json_serialize(j);
    json_free(j);
    return s;
}

/* ================================================================
 *  Multipart form-data builder (adapted from feishu.c)
 * ================================================================ */

typedef struct {
    const char *name;
    const char *filename;
    const char *data;
    size_t      data_len;
} multipart_part_t;

static char *build_multipart_body(const char *boundary,
                                   const multipart_part_t *parts,
                                   size_t *out_len) {
    if (!boundary || !parts || !out_len) return NULL;

    size_t total = 0;
    for (const multipart_part_t *p = parts; p->name; p++) {
        total += 2 + strlen(boundary) + 2;            /* --boundary\r\n */
        total += 44 + strlen(p->name);                 /* Content-Disposition: form-data; name="..." */
        if (p->filename)
            total += 12 + strlen(p->filename);         /* ; filename="..." */
        total += 2;                                    /* \r\n */
        if (p->filename)
            total += 38;                               /* Content-Type: application/octet-stream\r\n */
        total += 2;                                    /* \r\n (blank line before data) */
        total += p->data_len;
        total += 2;                                    /* \r\n */
    }
    total += 2 + strlen(boundary) + 2 + 2;             /* --boundary--\r\n */

    char *buf = (char *)malloc(total + 1);
    if (!buf) return NULL;

    size_t pos = 0;
    for (const multipart_part_t *p = parts; p->name; p++) {
        pos += snprintf(buf + pos, total - pos + 1, "--%s\r\n", boundary);
        if (p->filename) {
            pos += snprintf(buf + pos, total - pos + 1,
                            "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"
                            "Content-Type: application/octet-stream\r\n\r\n",
                            p->name, p->filename);
        } else {
            pos += snprintf(buf + pos, total - pos + 1,
                            "Content-Disposition: form-data; name=\"%s\"\r\n\r\n",
                            p->name);
        }
        if (p->data_len > 0) {
            memcpy(buf + pos, p->data, p->data_len);
            pos += p->data_len;
        }
        pos += snprintf(buf + pos, total - pos + 1, "\r\n");
    }
    pos += snprintf(buf + pos, total - pos + 1, "--%s--\r\n", boundary);
    buf[pos] = '\0';
    *out_len = pos;
    return buf;
}

static void generate_boundary(char *buf, size_t buf_size) {
    const char *chars = "abcdefghijklmnopqrstuvwxyz0123456789";
    srand((unsigned int)(time(NULL) ^ (getpid() << 16)));
    size_t len = buf_size - 1;
    if (len > 48) len = 48;
    snprintf(buf, buf_size, "----HermesFormBoundary");
    size_t pos = strlen(buf);
    for (size_t i = 0; i < len - pos; i++)
        buf[pos + i] = chars[rand() % 36];
    buf[pos + (len - pos)] = '\0';
}

/* ================================================================
 *  File reading helper
 * ================================================================ */

static char *read_file_binary(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { *out_len = 0; return NULL; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0 || (size_t)len > TRANSCRIBE_MAX_FILE_SIZE) {
        fclose(f);
        *out_len = 0;
        return NULL;
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(f); *out_len = 0; return NULL; }
    size_t n = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[n] = '\0';
    *out_len = n;
    return buf;
}

/* ================================================================
 *  File validation
 * ================================================================ */

/* PoP: _validate_audio_file @ lib/libtranscribe/transcribe.c:transcribe_validate_file
 * Port of Python tools/transcription_tools.py:_validate_audio_file(). */
char *transcribe_validate_file(const char *file_path) {
    if (!file_path || !*file_path)
        return make_result(false, "", "No file path provided", NULL);

    struct stat st;
    if (stat(file_path, &st) != 0)
        return make_result(false, "", "Audio file not found", NULL);

    if (!S_ISREG(st.st_mode))
        return make_result(false, "", "Path is not a file", NULL);

    /* Check extension */
    const char *dot = strrchr(file_path, '.');
    if (!dot || !transcribe_is_supported_format(dot)) {
        char err[256];
        snprintf(err, sizeof(err),
                 "Unsupported format: %s. Supported: .aac, .flac, .m4a, .mp3, .mp4, .mpeg, .mpga, .ogg, .wav, .webm",
                 dot ? dot : "(none)");
        return make_result(false, "", err, NULL);
    }

    /* Check size */
    if (st.st_size > TRANSCRIBE_MAX_FILE_SIZE) {
        char err[128];
        snprintf(err, sizeof(err),
                 "File too large: %.1f MB (max 25 MB)",
                 (double)st.st_size / (1024.0 * 1024.0));
        return make_result(false, "", err, NULL);
    }

    return NULL;  /* Valid */
}

/* ================================================================
 *  Config & Environment Helpers
 *  Port of Python tools/transcription_tools.py helpers.
 * ================================================================ */

/* PoP: get_env_value @ lib/libtranscribe/transcribe.c:stt_get_env
 * Port of Python tools/transcription_tools.py:get_env_value(). */
static const char *stt_get_env(const char *name, const char *fallback) {
    const char *val = getenv(name);
    return (val && *val) ? val : fallback;
}

/* PoP: is_stt_enabled @ lib/libtranscribe/transcribe.c:stt_is_enabled
 * Port of Python tools/transcription_tools.py:is_stt_enabled(). */
static bool stt_is_enabled(void) {
    const char *disabled = getenv("HERMES_STT_DISABLED");
    if (disabled && (strcmp(disabled, "true") == 0 || strcmp(disabled, "1") == 0))
        return false;
    return true;
}

/* STT config keys using environment variables (fallback when no hermes_config_t) */
#define STT_CONFIG_PROVIDER         "HERMES_STT_PROVIDER"
#define STT_CONFIG_LOCAL_MODEL      "HERMES_STT_LOCAL_MODEL"
#define STT_CONFIG_LOCAL_LANGUAGE   "HERMES_STT_LOCAL_LANGUAGE"
#define STT_CONFIG_LOCAL_COMMAND    "HERMES_LOCAL_STT_COMMAND"
#define STT_CONFIG_DISABLED         "HERMES_STT_DISABLED"

static const char *stt_get_provider(void) {
    return getenv(STT_CONFIG_PROVIDER);
}

static const char *stt_get_local_model(void) {
    return getenv(STT_CONFIG_LOCAL_MODEL);
}

static const char *stt_get_local_language(void) {
    return getenv(STT_CONFIG_LOCAL_LANGUAGE);
}

/* ================================================================
 *  Config-based STT Helpers (using hermes_config_t)
 *  These read from parsed config.yaml instead of env vars.
 * ================================================================ */

static bool stt_config_is_enabled(const hermes_config_t *cfg) {
    if (!cfg) return true;
    return cfg->stt.enabled;
}

static const char *stt_config_get_provider(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.provider[0]) return NULL;
    return cfg->stt.provider;
}

static const char *stt_config_get_local_model(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.local_model[0]) return "base";
    return cfg->stt.local_model;
}

static const char *stt_config_get_local_language(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.local_language[0]) return "en";
    return cfg->stt.local_language;
}

static const char *stt_config_get_local_command(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.local_command[0]) return NULL;
    return cfg->stt.local_command;
}

static const char *stt_config_get_groq_model(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.groq_model[0]) return "whisper-large-v3-turbo";
    return cfg->stt.groq_model;
}

static const char *stt_config_get_openai_model(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.openai_model[0]) return "whisper-1";
    return cfg->stt.openai_model;
}

static const char *stt_config_get_mistral_model(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.mistral_model[0]) return "voxtral-mini-latest";
    return cfg->stt.mistral_model;
}

static const char *stt_config_get_xai_model(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.xai_model[0]) return "grok-stt";
    return cfg->stt.xai_model;
}

static const char *stt_config_get_xai_language(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.xai_language[0]) return "en";
    return cfg->stt.xai_language;
}

static bool stt_config_get_xai_format(const hermes_config_t *cfg) {
    if (!cfg) return true;
    return cfg->stt.xai_format;
}

static bool stt_config_get_xai_diarize(const hermes_config_t *cfg) {
    if (!cfg) return false;
    return cfg->stt.xai_diarize;
}

static const char *stt_config_get_elevenlabs_model(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.elevenlabs_model[0]) return "scribe_v2";
    return cfg->stt.elevenlabs_model;
}

static const char *stt_config_get_elevenlabs_language(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.elevenlabs_language[0]) return "";
    return cfg->stt.elevenlabs_language;
}

static bool stt_config_get_elevenlabs_tag_audio_events(const hermes_config_t *cfg) {
    if (!cfg) return false;
    return cfg->stt.elevenlabs_tag_audio_events;
}

static bool stt_config_get_elevenlabs_diarize(const hermes_config_t *cfg) {
    if (!cfg) return false;
    return cfg->stt.elevenlabs_diarize;
}

static const char *stt_config_get_deepgram_model(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.deepgram_model[0]) return "nova-2";
    return cfg->stt.deepgram_model;
}

static const char *stt_config_get_command_timeout(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.command_timeout[0]) return "300";
    return cfg->stt.command_timeout;
}

static const char *stt_config_get_command_format(const hermes_config_t *cfg) {
    if (!cfg || !cfg->stt.command_format[0]) return "txt";
    return cfg->stt.command_format;
}

/* Portable which(1) implementation - forward declaration */
static char *shutil_which(const char *binary_name);

/* PoP: _find_binary @ lib/libtranscribe/transcribe.c:find_binary
 * Port of Python tools/transcription_tools.py:_find_binary(). */
static const char *find_binary(const char *binary_name) {
    /* Check common Homebrew/local prefixes */
    static const char *common_dirs[] = {
        "/opt/homebrew/bin",
        "/usr/local/bin",
        "/usr/bin",
        NULL
    };
    for (int i = 0; common_dirs[i]; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", common_dirs[i], binary_name);
        if (access(path, X_OK) == 0)
            return strdup(path);
    }
    /* Fall back to PATH */
    return shutil_which(binary_name);
}

/* Portable which(1) implementation */
static char *shutil_which(const char *binary_name) {
    const char *path_env = getenv("PATH");
    if (!path_env) return NULL;
    char *path_copy = strdup(path_env);
    char *saveptr;
    char *dir = strtok_r(path_copy, ":", &saveptr);
    while (dir) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, binary_name);
        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return strdup(full_path);
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }
    free(path_copy);
    return NULL;
}

/* PoP: _find_ffmpeg_binary @ lib/libtranscribe/transcribe.c:find_ffmpeg_binary
 * Port of Python tools/transcription_tools.py:_find_ffmpeg_binary(). */
static const char *find_ffmpeg_binary(void) {
    return find_binary("ffmpeg");
}

/* PoP: _find_whisper_binary @ lib/libtranscribe/transcribe.c:find_whisper_binary
 * Port of Python tools/transcription_tools.py:_find_whisper_binary(). */
static const char *find_whisper_binary(void) {
    return find_binary("whisper");
}

/* PoP: _get_local_command_template @ lib/libtranscribe/transcribe.c:get_local_command_template
 * Port of Python tools/transcription_tools.py:_get_local_command_template(). */
static const char *get_local_command_template(void) {
    /* Check explicit HERMES_LOCAL_STT_COMMAND */
    const char *cmd = getenv(STT_CONFIG_LOCAL_COMMAND);
    if (cmd && *cmd)
        return cmd;
    /* Auto-detect whisper binary */
    const char *whisper_bin = find_whisper_binary();
    if (whisper_bin) {
        static char template[1024];
        snprintf(template, sizeof(template),
                 "%s {input_path} --model {model} --output_format txt "
                 "--output_dir {output_dir} --language {language}",
                 whisper_bin);
        free((void *)whisper_bin);
        return template;
    }
    return NULL;
}

/* PoP: _has_local_command @ lib/libtranscribe/transcribe.c:has_local_command
 * Port of Python tools/transcription_tools.py:_has_local_command(). */
static bool has_local_command(void) {
    return get_local_command_template() != NULL;
}

/* ================================================================
 *  Model Normalization for Local Providers
 * ================================================================ */

/* PoP: _normalize_local_model @ lib/libtranscribe/transcribe.c:normalize_local_model
 * Port of Python tools/transcription_tools.py:_normalize_local_model(). */
static const char *normalize_local_model(const char *model) {
    /* Map cloud-only model names to valid faster-whisper sizes */
    static const char *cloud_models[] = {
        "whisper-1", "gpt-4o-mini-transcribe", "gpt-4o-transcribe",
        "whisper-large-v3", "whisper-large-v3-turbo", "distil-whisper-large-v3-en",
        NULL
    };
    if (!model)
        return "base";  /* DEFAULT_LOCAL_MODEL */
    for (int i = 0; cloud_models[i]; i++) {
        if (strcmp(model, cloud_models[i]) == 0)
            return "base";
    }
    return model;
}

/* PoP: _normalize_local_command_model @ lib/libtranscribe/transcribe.c:normalize_local_command_model
 * Port of Python tools/transcription_tools.py:_normalize_local_command_model(). */
static const char *normalize_local_command_model(const char *model) {
    return normalize_local_model(model);
}

/* ================================================================
 *  Lazy Install Stub (C has no pip - log warning only)
 * ================================================================ */

/* PoP: _try_lazy_install_stt @ lib/libtranscribe/transcribe.c:try_lazy_install_stt
 * Port of Python tools/transcription_tools.py:_try_lazy_install_stt(). */
static bool try_lazy_install_stt(void) {
    /* C implementation has no lazy pip install.
     * Log a hint for the user. */
    fprintf(stderr,
            "[transcribe] faster-whisper not available in C. "
            "Install faster-whisper in Python environment or use local_command provider.\n");
    return false;
}

/* ================================================================
 *  Command Provider Infrastructure
 *  Port of Python tools/transcription_tools.py command STT helpers.
 * ================================================================ */

#define DEFAULT_COMMAND_STT_TIMEOUT_SECONDS 300
#define DEFAULT_COMMAND_STT_LANGUAGE        "en"
#define DEFAULT_COMMAND_STT_OUTPUT_FORMAT   "txt"

/* Supported output formats for command providers */
static const char *COMMAND_STT_OUTPUT_FORMATS[] = {
    "txt", "json", "srt", "vtt", NULL
};

static bool is_valid_output_format(const char *fmt) {
    if (!fmt) return false;
    for (int i = 0; COMMAND_STT_OUTPUT_FORMATS[i]; i++) {
        if (strcmp(fmt, COMMAND_STT_OUTPUT_FORMATS[i]) == 0)
            return true;
    }
    return false;
}

/* PoP: _get_command_stt_timeout @ lib/libtranscribe/transcribe.c:get_command_stt_timeout
 * Port of Python tools/transcription_tools.py:_get_command_stt_timeout(). */
static double get_command_stt_timeout(const char *timeout_str, const char *timeout_seconds_str) {
    const char *raw = timeout_str ? timeout_str : timeout_seconds_str;
    if (!raw) raw = getenv("HERMES_COMMAND_STT_TIMEOUT");
    if (!raw) raw = getenv("HERMES_COMMAND_STT_TIMEOUT_SECONDS");
    if (!raw) return DEFAULT_COMMAND_STT_TIMEOUT_SECONDS;

    char *endptr;
    double value = strtod(raw, &endptr);
    if (endptr == raw || value <= 0)
        return DEFAULT_COMMAND_STT_TIMEOUT_SECONDS;
    return value;
}

/* PoP: _get_command_stt_output_format @ lib/libtranscribe/transcribe.c:get_command_stt_output_format
 * Port of Python tools/transcription_tools.py:_get_command_stt_output_format(). */
static const char *get_command_stt_output_format(const char *format_str, const char *output_format_str) {
    const char *raw = format_str ? format_str : output_format_str;
    if (!raw) raw = getenv("HERMES_COMMAND_STT_FORMAT");
    if (!raw) raw = getenv("HERMES_COMMAND_STT_OUTPUT_FORMAT");
    if (!raw) return DEFAULT_COMMAND_STT_OUTPUT_FORMAT;

    /* Normalize to lowercase */
    static char fmt[32];
    size_t i;
    for (i = 0; raw[i] && i < sizeof(fmt) - 1; i++)
        fmt[i] = (raw[i] >= 'A' && raw[i] <= 'Z') ? raw[i] + 32 : raw[i];
    fmt[i] = '\0';

    /* Strip leading dot if present */
    if (fmt[0] == '.')
        return is_valid_output_format(fmt + 1) ? fmt + 1 : DEFAULT_COMMAND_STT_OUTPUT_FORMAT;

    return is_valid_output_format(fmt) ? fmt : DEFAULT_COMMAND_STT_OUTPUT_FORMAT;
}

/* ================================================================
 *  Shell Quote Context Analysis
 *  Mirrors tools.tts_tool._shell_quote_context
 * ================================================================ */

/* PoP: _shell_quote_context_stt @ lib/libtranscribe/transcribe.c:shell_quote_context
 * Port of Python tools/transcription_tools.py:_shell_quote_context_stt(). */
/* PoP: shell_quote_context @ fuzzy_match:shell_quote_context */
/* PoP: shell_quote_context @ tts_tool:_shell_quote_context */
static const char *shell_quote_context(const char *command_template, size_t position) {
    char quote = 0;  /* 0 = none, '\'' = single, '\"' = double */
    bool escaped = false;

    for (size_t i = 0; i < position && command_template[i]; i++) {
        char c = command_template[i];

        if (quote == '\'') {
            if (c == '\'') quote = 0;
        } else if (quote == '\"') {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '\"') {
                quote = 0;
            }
        } else if (c == '\'') {
            quote = '\'';
        } else if (c == '\"') {
            quote = '\"';
        } else if (c == '\\') {
            i++;  /* Skip next char in unquoted context */
        }
    }

    static char result[2];
    if (quote)
        sprintf(result, "%c", quote), result[1] = '\0';
    else
        result[0] = '\0';

    return result[0] ? result : NULL;
}

/* ================================================================
 *  Placeholder Quoting
 *  Mirrors tools.tts_tool._quote_command_tts_placeholder
 * ================================================================ */

/* PoP: _quote_command_stt_placeholder @ lib/libtranscribe/transcribe.c:quote_placeholder
 * Port of Python tools/transcription_tools.py:_quote_command_stt_placeholder(). */
static char *quote_placeholder(const char *value, const char *quote_context) {
    if (!value) return strdup("");

    if (quote_context && *quote_context == '\'') {
        /* Single-quote safe: escape single quotes as '\'' */
        size_t len = strlen(value);
        char *result = malloc(len * 4 + 3);  /* Worst case: every char is ' */
        if (!result) return NULL;
        char *p = result;
        *p++ = '\'';
        for (size_t i = 0; i < len; i++) {
            if (value[i] == '\'') {
                strcpy(p, "'\\''");
                p += 4;
            } else {
                *p++ = value[i];
            }
        }
        *p++ = '\'';
        *p = '\0';
        return result;
    }

    if (quote_context && *quote_context == '\"') {
        /* Double-quote safe: escape \, ", $, ` */
        size_t len = strlen(value);
        char *result = malloc(len * 2 + 3);
        if (!result) return NULL;
        char *p = result;
        *p++ = '\"';
        for (size_t i = 0; i < len; i++) {
            switch (value[i]) {
                case '\\': strcpy(p, "\\\\"); p += 2; break;
                case '\"': strcpy(p, "\\\""); p += 2; break;
                case '$':  strcpy(p, "\\$");  p += 2; break;
                case '`':  strcpy(p, "\\`");  p += 2; break;
                default: *p++ = value[i];
            }
        }
        *p++ = '\"';
        *p = '\0';
        return result;
    }

    /* Bare context: use shell quoting */
    /* Simple implementation: if contains shell metacharacters, single-quote with escaping */
    bool needs_quote = false;
    for (const char *c = value; *c; c++) {
        if (!((*c >= 'a' && *c <= 'z') ||
              (*c >= 'A' && *c <= 'Z') ||
              (*c >= '0' && *c <= '9') ||
              *c == '_' || *c == '-' || *c == '.' || *c == '/' || *c == ':')) {
            needs_quote = true;
            break;
        }
    }

    if (!needs_quote)
        return strdup(value);

    /* Single-quote with escaping */
    size_t len = strlen(value);
    char *result = malloc(len * 4 + 3);
    if (!result) return NULL;
    char *p = result;
    *p++ = '\'';
    for (size_t i = 0; i < len; i++) {
        if (value[i] == '\'') {
            strcpy(p, "'\\''");
            p += 4;
        } else {
            *p++ = value[i];
        }
    }
    *p++ = '\'';
    *p = '\0';
    return result;
}

/* ================================================================
 *  Template Rendering
 *  Mirrors tools.tts_tool._render_command_tts_template
 * ================================================================ */

/* PoP: _render_command_stt_template @ lib/libtranscribe/transcribe.c:render_command_template
 * Port of Python tools/transcription_tools.py:_render_command_stt_template(). */
static char *render_command_template(const char *command_template,
                                      const char *input_path,
                                      const char *output_path,
                                      const char *output_dir,
                                      const char *format,
                                      const char *language,
                                      const char *model) {
    if (!command_template) return NULL;

    /* Build placeholders dict */
    struct {
        const char *name;
        const char *value;
    } placeholders[] = {
        {"input_path",  input_path},
        {"output_path", output_path},
        {"output_dir",  output_dir},
        {"format",      format},
        {"language",    language},
        {"model",       model},
        {NULL, NULL}
    };

    /* First pass: calculate needed size */
    size_t result_size = strlen(command_template) + 1;
    for (int i = 0; placeholders[i].name; i++) {
        const char *name = placeholders[i].name;
        const char *value = placeholders[i].value;
        if (!value) continue;
        size_t name_len = strlen(name);
        /* For each {name} or {{name}} occurrence */
        const char *p = command_template;
        while ((p = strstr(p, name))) {
            if (p > command_template && p[-1] == '$') {
                p += name_len;
                continue;  /* Escaped with $ */
            }
            bool is_double = (p > command_template && p[-1] == '{' && p[-2] == '{');
            bool is_single = (p > command_template && p[-1] == '{');
            if (!is_double && !is_single) {
                p += name_len;
                continue;
            }
            const char *q = p + name_len;
            if (is_double && q[0] == '}' && q[1] == '}') {
                result_size += strlen(value) - (name_len + 2);
            } else if (is_single && *q == '}') {
                result_size += strlen(value) - (name_len + 1);
            }
            p += name_len;
        }
    }

    char *result = malloc(result_size);
    if (!result) return NULL;
    result[0] = '\0';

    /* Second pass: actual replacement */
    const char *src = command_template;
    char *dst = result;

    while (*src) {
        bool matched = false;
        for (int i = 0; placeholders[i].name && !matched; i++) {
            const char *name = placeholders[i].name;
            const char *value = placeholders[i].value;
            if (!value) continue;
            size_t name_len = strlen(name);

            /* Check for {{name}} */
            if (strncmp(src, "{{", 2) == 0 &&
                strncmp(src + 2, name, name_len) == 0 &&
                strncmp(src + 2 + name_len, "}}", 2) == 0) {
                char *quoted = quote_placeholder(value, shell_quote_context(command_template, src - command_template));
                if (quoted) {
                    strcpy(dst, quoted);
                    dst += strlen(quoted);
                    free(quoted);
                }
                src += 2 + name_len + 2;
                matched = true;
            }
            /* Check for {name} */
            else if (*src == '{' &&
                     strncmp(src + 1, name, name_len) == 0 &&
                     src[1 + name_len] == '}') {
                char *quoted = quote_placeholder(value, shell_quote_context(command_template, src - command_template));
                if (quoted) {
                    strcpy(dst, quoted);
                    dst += strlen(quoted);
                    free(quoted);
                }
                src += 1 + name_len + 1;
                matched = true;
            }
        }
        if (!matched) {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    /* Preserve doubled braces as literal braces */
    /* Replace {{ with { and }} with } */
    char *final_result = malloc(strlen(result) + 1);
    if (!final_result) { free(result); return NULL; }
    char *p = final_result;
    for (char *r = result; *r; r++) {
        if (*r == '{' && r[1] == '{') {
            *p++ = '{';
            r++;
        } else if (*r == '}' && r[1] == '}') {
            *p++ = '}';
            r++;
        } else {
            *p++ = *r;
        }
    }
    *p = '\0';
    free(result);
    return final_result;
}

/* ================================================================
 *  Process Tree Termination
 *  Mirrors tools.tts_tool._terminate_command_tts_process_tree
 * ================================================================ */

/* PoP: _terminate_command_stt_process_tree @ lib/libtranscribe/transcribe.c:terminate_command_process_tree
 * Port of Python tools/transcription_tools.py:_terminate_command_stt_process_tree(). */
static void terminate_command_process_tree(int pid) {
    if (pid <= 0) return;

#ifdef _WIN32
    /* Windows: use taskkill /T /F /PID */
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "taskkill /F /T /PID %d", pid);
    system(cmd);
#else
    /* Unix: try psutil-style recursive kill, fallback to killpg */
    /* First try to kill the process group (the shell started with setsid) */
    if (kill(-pid, SIGTERM) == 0) {
        /* Wait a bit */
        sleep(1);
        /* Check if still alive */
        if (kill(-pid, 0) != 0)
            return;  /* All dead */
        /* Force kill */
        kill(-pid, SIGKILL);
        return;
    }

    /* Fallback: single process */
    kill(pid, SIGTERM);
    sleep(1);
    if (kill(pid, 0) == 0)
        kill(pid, SIGKILL);
#endif
}

/* ================================================================
 *  Command Execution with Timeout
 * ================================================================ */

#define CMD_STT_MAX_OUTPUT (1024 * 1024)  /* 1MB max output */

/* PoP: _run_command_stt @ lib/libtranscribe/transcribe.c:run_command_stt
 * Port of Python tools/transcription_tools.py:_run_command_stt(). */
static bool run_command_stt(const char *command, double timeout_seconds,
                             char **out_stdout, char **out_stderr,
                             int *out_exit_code) {
    if (out_stdout) *out_stdout = NULL;
    if (out_stderr) *out_stderr = NULL;

    char *stdout_buf = malloc(CMD_STT_MAX_OUTPUT);
    char *stderr_buf = malloc(CMD_STT_MAX_OUTPUT);
    if (!stdout_buf || !stderr_buf) {
        free(stdout_buf);
        free(stderr_buf);
        return false;
    }
    stdout_buf[0] = '\0';
    stderr_buf[0] = '\0';

#ifdef _WIN32
    /* Windows: use _popen with timeout polling */
    FILE *fp = _popen(command, "r");
    if (!fp) {
        free(stdout_buf);
        free(stderr_buf);
        return false;
    }

    /* Read output with timeout */
    time_t start = time(NULL);
    int exit_code = -1;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        if (time(NULL) - start > (time_t)timeout_seconds) {
            /* Timeout - terminate */
            _pclose(fp);  /* Note: this doesn't kill children properly on Windows */
            exit_code = -1;
            break;
        }
        if (strlen(stdout_buf) + strlen(buf) < CMD_STT_MAX_OUTPUT - 1)
            strcat(stdout_buf, buf);
    }
    exit_code = _pclose(fp);

    if (out_stdout) *out_stdout = stdout_buf;
    else free(stdout_buf);
    if (out_stderr) *out_stderr = stderr_buf;
    else free(stderr_buf);
    if (out_exit_code) *out_exit_code = exit_code;
    return exit_code == 0;
#else
    /* Unix: fork + exec with process group for clean termination */
    int stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        free(stdout_buf);
        free(stderr_buf);
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        free(stdout_buf);
        free(stderr_buf);
        return false;
    }

    if (pid == 0) {
        /* Child */
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        /* Create new process group for clean termination */
        setsid();

        /* Execute via shell */
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(127);
    }

    /* Parent */
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    /* Read output with timeout */
    time_t start = time(NULL);
    bool timed_out = false;
    fd_set read_fds;
    int max_fd = (stdout_pipe[0] > stderr_pipe[0]) ? stdout_pipe[0] : stderr_pipe[0];

    while (time(NULL) - start < (time_t)timeout_seconds) {
        FD_ZERO(&read_fds);
        FD_SET(stdout_pipe[0], &read_fds);
        FD_SET(stderr_pipe[0], &read_fds);

        struct timeval tv = {1, 0};  /* 1 second intervals */
        int ret = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) break;

        if (FD_ISSET(stdout_pipe[0], &read_fds)) {
            ssize_t n = read(stdout_pipe[0], stdout_buf + strlen(stdout_buf),
                             CMD_STT_MAX_OUTPUT - strlen(stdout_buf) - 1);
            if (n <= 0) FD_CLR(stdout_pipe[0], &read_fds);
            else stdout_buf[strlen(stdout_buf) + n] = '\0';
        }
        if (FD_ISSET(stderr_pipe[0], &read_fds)) {
            ssize_t n = read(stderr_pipe[0], stderr_buf + strlen(stderr_buf),
                             CMD_STT_MAX_OUTPUT - strlen(stderr_buf) - 1);
            if (n <= 0) FD_CLR(stderr_pipe[0], &read_fds);
            else stderr_buf[strlen(stderr_buf) + n] = '\0';
        }

        /* Check if process exited */
        int status;
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            if (out_stdout) *out_stdout = stdout_buf; else free(stdout_buf);
            if (out_stderr) *out_stderr = stderr_buf; else free(stderr_buf);
            if (out_exit_code) *out_exit_code = exit_code;
            return exit_code == 0;
        }
    }

    /* Timeout - kill process group */
    timed_out = true;
    terminate_command_process_tree(pid);

    /* Drain remaining output */
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);
    waitpid(pid, NULL, 0);

    if (out_stdout) *out_stdout = stdout_buf; else free(stdout_buf);
    if (out_stderr) *out_stderr = stderr_buf; else free(stderr_buf);
    if (out_exit_code) *out_exit_code = timed_out ? -2 : -1;
    return false;
#endif
}

/* ================================================================
 *  Read Command STT Output
 * ================================================================ */

/* PoP: _read_command_stt_output @ lib/libtranscribe/transcribe.c:read_command_stt_output
 * Port of Python tools/transcription_tools.py:_read_command_stt_output(). */
static char *read_command_stt_output(const char *output_path, const char *stdout_str, const char *format) {
    (void)format;  /* Not used for raw text reading */

    /* 1. Check output file */
    if (output_path) {
        FILE *f = fopen(output_path, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (len > 0 && len < CMD_STT_MAX_OUTPUT) {
                char *content = malloc(len + 1);
                if (content) {
                    size_t n = fread(content, 1, len, f);
                    content[n] = '\0';
                    fclose(f);
                    /* Trim whitespace */
                    char *start = content;
                    while (*start && (*start == ' ' || *start == '\n' || *start == '\r' || *start == '\t'))
                        start++;
                    char *end = content + n - 1;
                    while (end > start && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t'))
                        *end-- = '\0';
                    if (*start) {
                        char *trimmed = strdup(start);
                        free(content);
                        return trimmed;
                    }
                    free(content);
                }
            }
            fclose(f);
        }
    }

    /* 2. Fall back to stdout */
    if (stdout_str && *stdout_str) {
        char *trimmed = strdup(stdout_str);
        if (trimmed) {
            char *start = trimmed;
            while (*start && (*start == ' ' || *start == '\n' || *start == '\r' || *start == '\t'))
                start++;
            char *end = trimmed + strlen(trimmed) - 1;
            while (end > start && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t'))
                *end-- = '\0';
            if (start != trimmed)
                memmove(trimmed, start, strlen(start) + 1);
            if (*trimmed) return trimmed;
            free(trimmed);
        }
    }

    return NULL;  /* No usable output */
}

/* ================================================================
 *  Transcribe via Command Provider
 *  Port of Python tools/transcription_tools.py:_transcribe_command_stt()
 * ================================================================ */

/* PoP: _transcribe_command_stt @ lib/libtranscribe/transcribe.c:transcribe_command_stt
 * Port of Python tools/transcription_tools.py:_transcribe_command_stt(). */
static char *transcribe_command_stt(const char *file_path,
                                     const char *provider_name,
                                     const char *command_template,
                                     const char *timeout_str,
                                     const char *timeout_seconds_str,
                                     const char *format_str,
                                     const char *output_format_str,
                                     const char *language,
                                     const char *model_override) {
    if (!command_template || !*command_template)
        return make_result(false, "", "Command template not configured", provider_name);

    /* Check file exists */
    struct stat st;
    if (stat(file_path, &st) != 0)
        return make_result(false, "", "Audio file not found", provider_name);

    double timeout = get_command_stt_timeout(timeout_str, timeout_seconds_str);
    const char *output_format = get_command_stt_output_format(format_str, output_format_str);
    const char *lang = language ? language : DEFAULT_COMMAND_STT_LANGUAGE;
    const char *model = model_override ? model_override : "";

    /* Create temp directory */
    char tmpdir[] = "/tmp/hermes-cmd-stt-XXXXXX";
    if (!mkdtemp(tmpdir))
        return make_result(false, "", "Failed to create temp directory", provider_name);

    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/transcript.%s", tmpdir, output_format);

    /* Render command template */
    char *command = render_command_template(command_template,
                                             file_path, output_path, tmpdir,
                                             output_format, lang, model);
    if (!command) {
        rmdir(tmpdir);
        return make_result(false, "", "Failed to render command template", provider_name);
    }

    /* Execute command */
    char *stdout_buf = NULL;
    char *stderr_buf = NULL;
    int exit_code = 0;
    bool ok = run_command_stt(command, timeout, &stdout_buf, &stderr_buf, &exit_code);
    free(command);

    char *result = NULL;
    if (!ok) {
        if (exit_code == -2) {
            result = make_result(false, "", "Command timed out", provider_name);
        } else {
            char err[1024];
            snprintf(err, sizeof(err), "Command failed (exit %d): %s%s%s",
                     exit_code,
                     stderr_buf ? stderr_buf : "",
                     (stderr_buf && stdout_buf) ? "; " : "",
                     stdout_buf ? stdout_buf : "");
            result = make_result(false, "", err, provider_name);
        }
    } else {
        char *transcript = read_command_stt_output(output_path, stdout_buf, output_format);
        if (transcript) {
            result = make_result(true, transcript, NULL, provider_name);
            free(transcript);
        } else {
            result = make_result(false, "", "Command completed but produced no transcript", provider_name);
        }
    }

    free(stdout_buf);
    free(stderr_buf);

    /* Clean up temp directory */
    char rm_cmd[1024];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", tmpdir);
    system(rm_cmd);

    return result;
}

/* ================================================================
 *  Local (faster-whisper) Provider using whisper.cpp
 *  Port of Python tools/transcription_tools.py:_transcribe_local().
 *  Now uses whisper.cpp C API for native local transcription.
 * ================================================================ */

/* PoP: _transcribe_local @ lib/libtranscribe/transcribe.c:transcribe_local
 * Port of Python tools/transcription_tools.py:_transcribe_local(). */
static char *transcribe_local(const char *file_path, const char *model) {
    /* Get model path from config or use default */
    const char *model_path = NULL;
    
    /* Try to get from hermes config if available */
    /* For now, check standard locations */
    static const char *search_paths[] = {
        "./models/ggml-base.en.bin",
        "./models/ggml-base.bin",
        "./models/ggml-small.en.bin",
        "./models/ggml-small.bin",
        "./models/ggml-medium.en.bin",
        "./models/ggml-medium.bin",
        "/usr/local/share/whisper.cpp/models/ggml-base.en.bin",
        "/usr/share/whisper.cpp/models/ggml-base.en.bin",
        "~/.cache/whisper.cpp/ggml-base.en.bin",
        NULL
    };
    
    for (int i = 0; search_paths[i]; i++) {
        if (whisperc_model_exists(search_paths[i])) {
            model_path = search_paths[i];
            break;
        }
    }
    
    /* If model specified, try to find it */
    if (!model_path && model) {
        char custom_path[512];
        snprintf(custom_path, sizeof(custom_path), "./models/%s", model);
        if (whisperc_model_exists(custom_path)) {
            model_path = custom_path;
        } else if (whisperc_model_exists(model)) {
            model_path = model;
        }
    }
    
    /* Fallback to default */
    if (!model_path) {
        model_path = whisperc_default_model_path();
    }
    
    /* Initialize whisper context */
    whisper_context_t *ctx = whisperc_init_from_file_with_params(model_path, 4, 0);
    if (!ctx) {
        return make_result(false, "", 
                           "Failed to initialize whisper.cpp model. "
                           "Download a model (e.g., ggml-base.en.bin) to ./models/ or specify path.",
                           "local");
    }
    
    /* Load audio file */
    float *samples = NULL;
    int n_samples = 0;
    int audio_ret = whisperc_load_audio(file_path, &samples, &n_samples);
    if (audio_ret != 0 || !samples || n_samples <= 0) {
        whisperc_free(ctx);
        return make_result(false, "", "Failed to load audio file", "local");
    }
    
    /* Transcribe */
    whisper_full_result_t result;
    int ret = whisperc_full(ctx, samples, n_samples, "auto", 0, 4, &result);
    
    /* Cleanup */
    free(samples);
    whisperc_free(ctx);
    
    if (ret != 0) {
        return make_result(false, "", "Transcription failed", "local");
    }
    
    /* Return result */
    char *json = make_result(true, result.text, "", "local");
    
    /* Free whisper result */
    whisperc_full_result_free(&result);
    
    return json;
}

/* ================================================================
 *  Extract Transcript Text Helper
 *  Port of Python tools/transcription_tools.py:_extract_transcript_text().
 * ================================================================ */

/* PoP: _extract_transcript_text @ lib/libtranscribe/transcribe.c:extract_transcript_text
 * Port of Python tools/transcription_tools.py:_extract_transcript_text(). */
static const char *extract_transcript_text(const char *json_text) {
    if (!json_text) return "";

    /* Try to parse as JSON and extract "text" field */
    json_t *j = json_parse(json_text, NULL);
    if (j) {
        const char *text = json_get_str(j, "text", "");
        if (text && *text) {
            const char *result = strdup(text);
            json_free(j);
            return result;
        }
        json_free(j);
    }

    /* Check if it's plain text (not JSON) */
    const char *p = json_text;
    while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t'))
        p++;
    if (*p && *p != '{' && *p != '[')
        return json_text;

    return "";
}

/* ================================================================
 *  Provider: Groq Whisper API
 * ================================================================ */

static const char *normalize_model_for_groq(const char *model) {
    /* OpenAI models aren't available on Groq — map to default */
    if (!model) return TRANSCRIBE_DEFAULT_MODEL_GROQ;
    if (strcmp(model, "whisper-1") == 0 ||
        strcmp(model, "gpt-4o-mini-transcribe") == 0 ||
        strcmp(model, "gpt-4o-transcribe") == 0)
        return TRANSCRIBE_DEFAULT_MODEL_GROQ;
    return model;
}

static const char *normalize_model_for_openai(const char *model) {
    /* Groq-only models aren't available on OpenAI */
    if (!model) return TRANSCRIBE_DEFAULT_MODEL_OPENAI;
    static const char *groq_only[] = {"whisper-large-v3", "whisper-large-v3-turbo", "distil-whisper-large-v3-en"};
    for (size_t i = 0; i < sizeof(groq_only)/sizeof(groq_only[0]); i++) {
        if (strcmp(model, groq_only[i]) == 0)
            return TRANSCRIBE_DEFAULT_MODEL_OPENAI;
    }
    return model;
}

/* PoP: _transcribe_groq @ lib/libtranscribe/transcribe.c:transcribe_groq
 * Port of Python tools/transcription_tools.py:_transcribe_groq(). */
static char *transcribe_groq(const char *file_path, const char *model) {
    const char *api_key = getenv("GROQ_API_KEY");
    if (!api_key || !*api_key)
        return make_result(false, "", "GROQ_API_KEY not set", TRANSCRIBE_PROVIDER_GROQ);

    const char *model_name = normalize_model_for_groq(model);
    const char *base_url = getenv("GROQ_BASE_URL");
    if (!base_url || !*base_url)
        base_url = "https://api.groq.com/openai/v1";

    /* Read audio file */
    size_t file_len;
    char *file_data = read_file_binary(file_path, &file_len);
    if (!file_data)
        return make_result(false, "", "Failed to read audio file", TRANSCRIBE_PROVIDER_GROQ);

    /* Get filename from path */
    const char *filename = strrchr(file_path, '/');
    filename = filename ? filename + 1 : file_path;

    /* Build multipart body */
    char boundary[64];
    generate_boundary(boundary, sizeof(boundary));

    /* Get model string length for size estimation */
    size_t model_len = strlen(model_name);

    /* model part */
    multipart_part_t parts[] = {
        {"model",    NULL, model_name, model_len},
        {"file",     filename, file_data, file_len},
        {NULL, NULL, NULL, 0}
    };

    size_t body_len;
    char *body = build_multipart_body(boundary, parts, &body_len);
    free(file_data);
    if (!body)
        return make_result(false, "", "Failed to build multipart body", TRANSCRIBE_PROVIDER_GROQ);

    /* Build URL */
    char url[512];
    snprintf(url, sizeof(url), "%s/audio/transcriptions", base_url);

    /* Build Content-Type header with boundary */
    char content_type[128];
    snprintf(content_type, sizeof(content_type),
             "Content-Type: multipart/form-data; boundary=%s", boundary);

    /* Build auth header */
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s\r\n%s", api_key, content_type);

    /* Do HTTP POST */
    http_t *h = http_new(60);  /* 60s timeout for audio upload */
    if (!h) {
        free(body);
        return make_result(false, "", "Failed to create HTTP client", TRANSCRIBE_PROVIDER_GROQ);
    }

    http_resp_t *resp = http_request(h, HTTP_POST, url, auth_header, body, body_len);
    free(body);
    if (!resp) { http_free(h); return make_result(false, "", "HTTP request failed", TRANSCRIBE_PROVIDER_GROQ); }

    char *result;
    if (resp->status != 200) {
        char err[512];
        /* Try to extract error from JSON response */
        json_t *j = json_parse(resp->body, NULL);
        if (j) {
            json_t *err_obj = json_obj_get(j, "error");
            const char *msg = err_obj ? json_get_str(err_obj, "message", "") : NULL;
            snprintf(err, sizeof(err), "Groq API error (HTTP %d): %s",
                     resp->status, msg ? msg : resp->body);
            json_free(j);
        } else {
            snprintf(err, sizeof(err), "Groq API error (HTTP %d): %s",
                     resp->status, resp->body);
        }
        result = make_result(false, "", err, TRANSCRIBE_PROVIDER_GROQ);
    } else {
        /* Parse JSON response */
        json_t *j = json_parse(resp->body, NULL);
        if (!j) {
            /* Response may be plain text (response_format=text) */
            const char *text = resp->body;
            while (*text == ' ' || *text == '\n' || *text == '\r') text++;
            if (*text && *text != '{') {
                result = make_result(true, text, NULL, TRANSCRIBE_PROVIDER_GROQ);
            } else {
                result = make_result(false, "", "Failed to parse Groq response",
                                     TRANSCRIBE_PROVIDER_GROQ);
            }
        } else {
            const char *text = json_get_str(j, "text", "");
            if (text && *text)
                result = make_result(true, text, NULL, TRANSCRIBE_PROVIDER_GROQ);
            else
                result = make_result(false, "", "Groq returned empty transcript",
                                     TRANSCRIBE_PROVIDER_GROQ);
            json_free(j);
        }
    }

    http_resp_free(resp);
    http_free(h);
    return result;
}

/* ================================================================
 *  Provider: OpenAI Whisper API
 *  PoP: _transcribe_openai @ lib/libtranscribe/transcribe.c:transcribe_openai
 * Port of Python tools/transcription_tools.py:_transcribe_openai().
 * ================================================================ */

static char *transcribe_openai(const char *file_path, const char *model) {
    const char *api_key = getenv("VOICE_TOOLS_OPENAI_KEY");
    if (!api_key || !*api_key)
        api_key = getenv("OPENAI_API_KEY");
    if (!api_key || !*api_key)
        return make_result(false, "", "No OpenAI API key (set VOICE_TOOLS_OPENAI_KEY or OPENAI_API_KEY)",
                           TRANSCRIBE_PROVIDER_OPENAI);

    const char *model_name = normalize_model_for_openai(model);
    const char *base_url = getenv("STT_OPENAI_BASE_URL");
    if (!base_url || !*base_url)
        base_url = "https://api.openai.com/v1";

    /* Read audio file */
    size_t file_len;
    char *file_data = read_file_binary(file_path, &file_len);
    if (!file_data)
        return make_result(false, "", "Failed to read audio file", TRANSCRIBE_PROVIDER_OPENAI);

    const char *filename = strrchr(file_path, '/');
    filename = filename ? filename + 1 : file_path;

    /* Build multipart */
    char boundary[64];
    generate_boundary(boundary, sizeof(boundary));

    size_t model_len = strlen(model_name);
    multipart_part_t parts[] = {
        {"model",           NULL, model_name, model_len},
        {"file",            filename, file_data, file_len},
        {"response_format", NULL, "json", 4},
        {NULL, NULL, NULL, 0}
    };

    size_t body_len;
    char *body = build_multipart_body(boundary, parts, &body_len);
    free(file_data);
    if (!body)
        return make_result(false, "", "Failed to build multipart body", TRANSCRIBE_PROVIDER_OPENAI);

    char url[512];
    snprintf(url, sizeof(url), "%s/audio/transcriptions", base_url);

    char content_type[128];
    snprintf(content_type, sizeof(content_type),
             "Content-Type: multipart/form-data; boundary=%s", boundary);
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s\r\n%s", api_key, content_type);

    http_t *h = http_new(60);
    if (!h) { free(body); return make_result(false, "", "Failed to create HTTP client", TRANSCRIBE_PROVIDER_OPENAI); }

    http_resp_t *resp = http_request(h, HTTP_POST, url, auth_header, body, body_len);
    free(body);
    if (!resp) { http_free(h); return make_result(false, "", "HTTP request failed", TRANSCRIBE_PROVIDER_OPENAI); }

    char *result;
    if (resp->status != 200) {
        char err[512];
        json_t *j = json_parse(resp->body, NULL);
        if (j) {
            json_t *err_obj = json_obj_get(j, "error");
            const char *msg = err_obj ? json_get_str(err_obj, "message", "") : NULL;
            snprintf(err, sizeof(err), "OpenAI API error (HTTP %d): %s",
                     resp->status, msg ? msg : resp->body);
            json_free(j);
        } else {
            snprintf(err, sizeof(err), "OpenAI API error (HTTP %d): %s",
                     resp->status, resp->body);
        }
        result = make_result(false, "", err, TRANSCRIBE_PROVIDER_OPENAI);
    } else {
        json_t *j = json_parse(resp->body, NULL);
        if (!j) {
            result = make_result(false, "", "Failed to parse OpenAI response",
                                 TRANSCRIBE_PROVIDER_OPENAI);
        } else {
            /* OpenAI returns {"text": "..."} when response_format=json */
            const char *text = json_get_str(j, "text", "");
            if (!text || !*text)
                text = resp->body;  /* fallback to raw */
            result = make_result(true, text && *text ? text : resp->body, NULL,
                                 TRANSCRIBE_PROVIDER_OPENAI);
            json_free(j);
        }
    }

    http_resp_free(resp);
    http_free(h);
    return result;
}

/* ================================================================
 *  Provider: xAI Grok STT API
 *  PoP: _transcribe_xai @ lib/libtranscribe/transcribe.c:transcribe_xai
 * Port of Python tools/transcription_tools.py:_transcribe_xai().
 * ================================================================ */

static char *transcribe_xai(const char *file_path, const char *model) {
    (void)model;  /* xAI doesn't use a model parameter */

    const char *api_key = getenv("XAI_API_KEY");
    if (!api_key || !*api_key)
        return make_result(false, "", "XAI_API_KEY not set", TRANSCRIBE_PROVIDER_XAI);

    const char *base_url = getenv("XAI_STT_BASE_URL");
    if (!base_url || !*base_url)
        base_url = "https://api.x.ai/v1";

    /* Read audio file */
    size_t file_len;
    char *file_data = read_file_binary(file_path, &file_len);
    if (!file_data)
        return make_result(false, "", "Failed to read audio file", TRANSCRIBE_PROVIDER_XAI);

    const char *filename = strrchr(file_path, '/');
    filename = filename ? filename + 1 : file_path;

    /* xAI STT uses POST /v1/stt with multipart, optional parameters */
    char boundary[64];
    generate_boundary(boundary, sizeof(boundary));

    const char *language = getenv("HERMES_LOCAL_STT_LANGUAGE");
    if (!language || !*language) language = "en";

    multipart_part_t text_parts[] = {
        {"file",     filename, file_data, file_len},
        {"language", NULL,     language,  strlen(language)},
        {NULL, NULL, NULL, 0}
    };

    size_t body_len;
    char *body = build_multipart_body(boundary, text_parts, &body_len);
    free(file_data);
    if (!body)
        return make_result(false, "", "Failed to build multipart body", TRANSCRIBE_PROVIDER_XAI);

    char url[512];
    snprintf(url, sizeof(url), "%s/stt", base_url);

    char content_type[128];
    snprintf(content_type, sizeof(content_type),
             "Content-Type: multipart/form-data; boundary=%s", boundary);
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s\r\n%s", api_key, content_type);

    /* Do HTTP POST */
    http_t *h = http_new(120);  /* 120s timeout — xAI STT can be slow */
    if (!h) { free(body); return make_result(false, "", "Failed to create HTTP client", TRANSCRIBE_PROVIDER_XAI); }

    http_resp_t *resp = http_request(h, HTTP_POST, url, auth_header, body, body_len);
    free(body);
    if (!resp) { http_free(h); return make_result(false, "", "HTTP request failed", TRANSCRIBE_PROVIDER_XAI); }

    char *result;
    if (resp->status != 200) {
        char err[512];
        /* Try to extract error from JSON response */
        json_t *j = json_parse(resp->body, NULL);
        if (j) {
            json_t *err_obj = json_obj_get(j, "error");
            const char *msg = err_obj ? json_get_str(err_obj, "message", "") : NULL;
            snprintf(err, sizeof(err), "xAI STT error (HTTP %d): %s",
                     resp->status, msg ? msg : resp->body);
            json_free(j);
        } else {
            snprintf(err, sizeof(err), "xAI STT error (HTTP %d): %s",
                     resp->status, resp->body);
        }
        result = make_result(false, "", err, TRANSCRIBE_PROVIDER_XAI);
    } else {
        /* Parse JSON response */
        json_t *j = json_parse(resp->body, NULL);
        if (!j) {
            result = make_result(false, "", "Failed to parse xAI STT response",
                                 TRANSCRIBE_PROVIDER_XAI);
        } else {
            const char *text = json_get_str(j, "text", "");
            if (text && *text)
                result = make_result(true, text, NULL, TRANSCRIBE_PROVIDER_XAI);
            else
                result = make_result(false, "", "xAI STT returned empty transcript",
                                     TRANSCRIBE_PROVIDER_XAI);
            json_free(j);
        }
    }

    http_resp_free(resp);
    http_free(h);
    return result;
}

/* ================================================================
 *  Provider: Mistral Voxtral Transcribe API
 * ================================================================ */

/* PoP: _transcribe_mistral @ lib/libtranscribe/transcribe.c:transcribe_mistral
 * Port of Python tools/transcription_tools.py:_transcribe_mistral(). */
static char *transcribe_mistral(const char *file_path, const char *model) {
    const char *api_key = getenv("MISTRAL_API_KEY");
    if (!api_key || !*api_key)
        return make_result(false, "", "MISTRAL_API_KEY not set", TRANSCRIBE_PROVIDER_MISTRAL);

    const char *base_url = getenv("MISTRAL_BASE_URL");
    if (!base_url || !*base_url)
        base_url = "https://api.mistral.ai/v1";

    const char *model_name = model ? model : TRANSCRIBE_DEFAULT_MODEL_MISTRAL;

    /* Read audio file */
    size_t file_len;
    char *file_data = read_file_binary(file_path, &file_len);
    if (!file_data)
        return make_result(false, "", "Failed to read audio file", TRANSCRIBE_PROVIDER_MISTRAL);

    const char *filename = strrchr(file_path, '/');
    filename = filename ? filename + 1 : file_path;

    /* Mistral Voxtral uses POST /v1/audio/transcriptions with multipart */
    char boundary[64];
    generate_boundary(boundary, sizeof(boundary));

    size_t model_len = strlen(model_name);

    multipart_part_t parts[] = {
        {"model", NULL, model_name, model_len},
        {"file",  filename, file_data, file_len},
        {NULL, NULL, NULL, 0}
    };

    size_t body_len;
    char *body = build_multipart_body(boundary, parts, &body_len);
    free(file_data);
    if (!body)
        return make_result(false, "", "Failed to build multipart body", TRANSCRIBE_PROVIDER_MISTRAL);

    char url[512];
    snprintf(url, sizeof(url), "%s/audio/transcriptions", base_url);

    char content_type[128];
    snprintf(content_type, sizeof(content_type),
             "Content-Type: multipart/form-data; boundary=%s", boundary);
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s\r\n%s", api_key, content_type);

    http_t *h = http_new(120);  /* 120s timeout */
    if (!h) { free(body); return make_result(false, "", "Failed to create HTTP client", TRANSCRIBE_PROVIDER_MISTRAL); }

    http_resp_t *resp = http_request(h, HTTP_POST, url, auth_header, body, body_len);
    free(body);
    if (!resp) { http_free(h); return make_result(false, "", "HTTP request failed", TRANSCRIBE_PROVIDER_MISTRAL); }

    char *result;
    if (resp->status != 200) {
        char err[512];
        json_t *j = json_parse(resp->body, NULL);
        if (j) {
            json_t *err_obj = json_obj_get(j, "error");
            const char *msg = err_obj ? json_get_str(err_obj, "message", "") : NULL;
            snprintf(err, sizeof(err), "Mistral API error (HTTP %d): %s",
                     resp->status, msg ? msg : resp->body);
            json_free(j);
        } else {
            snprintf(err, sizeof(err), "Mistral API error (HTTP %d): %s",
                     resp->status, resp->body);
        }
        result = make_result(false, "", err, TRANSCRIBE_PROVIDER_MISTRAL);
    } else {
        json_t *j = json_parse(resp->body, NULL);
        if (!j) {
            /* Response may be plain text */
            const char *text = resp->body;
            while (*text == ' ' || *text == '\n' || *text == '\r') text++;
            if (*text && *text != '{') {
                result = make_result(true, text, NULL, TRANSCRIBE_PROVIDER_MISTRAL);
            } else {
                result = make_result(false, "", "Failed to parse Mistral response",
                                     TRANSCRIBE_PROVIDER_MISTRAL);
            }
        } else {
            const char *text = json_get_str(j, "text", "");
            if (text && *text)
                result = make_result(true, text, NULL, TRANSCRIBE_PROVIDER_MISTRAL);
            else
                result = make_result(false, "", "Mistral returned empty transcript",
                                     TRANSCRIBE_PROVIDER_MISTRAL);
            json_free(j);
        }
    }

    http_resp_free(resp);
    http_free(h);
    return result;
}

/* ================================================================
 *  Provider: ElevenLabs Scribe STT API
 * ================================================================ */

/* PoP: _transcribe_elevenlabs @ lib/libtranscribe/transcribe.c:transcribe_elevenlabs
 * Port of Python tools/transcription_tools.py:_transcribe_elevenlabs(). */
static char *transcribe_elevenlabs(const char *file_path, const char *model) {
    const char *api_key = getenv("ELEVENLABS_API_KEY");
    if (!api_key || !*api_key)
        return make_result(false, "", "ELEVENLABS_API_KEY not set", TRANSCRIBE_PROVIDER_ELEVENLABS);

    const char *base_url = getenv("ELEVENLABS_STT_BASE_URL");
    if (!base_url || !*base_url)
        base_url = "https://api.elevenlabs.io/v1";

    const char *model_name = model ? model : TRANSCRIBE_DEFAULT_MODEL_ELEVENLABS;

    /* Read audio file */
    size_t file_len;
    char *file_data = read_file_binary(file_path, &file_len);
    if (!file_data)
        return make_result(false, "", "Failed to read audio file", TRANSCRIBE_PROVIDER_ELEVENLABS);

    const char *filename = strrchr(file_path, '/');
    filename = filename ? filename + 1 : file_path;

    /* ElevenLabs Scribe uses POST /v1/speech-to-text with multipart */
    char boundary[64];
    generate_boundary(boundary, sizeof(boundary));

    size_t model_len = strlen(model_name);

    multipart_part_t parts[] = {
        {"model_id", NULL, model_name, model_len},
        {"file",     filename, file_data, file_len},
        {NULL, NULL, NULL, 0}
    };

    size_t body_len;
    char *body = build_multipart_body(boundary, parts, &body_len);
    free(file_data);
    if (!body)
        return make_result(false, "", "Failed to build multipart body", TRANSCRIBE_PROVIDER_ELEVENLABS);

    char url[512];
    snprintf(url, sizeof(url), "%s/speech-to-text", base_url);

    char content_type[128];
    snprintf(content_type, sizeof(content_type),
             "Content-Type: multipart/form-data; boundary=%s", boundary);
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "xi-api-key: %s\r\n%s", api_key, content_type);

    http_t *h = http_new(120);  /* 120s timeout */
    if (!h) { free(body); return make_result(false, "", "Failed to create HTTP client", TRANSCRIBE_PROVIDER_ELEVENLABS); }

    http_resp_t *resp = http_request(h, HTTP_POST, url, auth_header, body, body_len);
    free(body);
    if (!resp) { http_free(h); return make_result(false, "", "HTTP request failed", TRANSCRIBE_PROVIDER_ELEVENLABS); }

    char *result;
    if (resp->status != 200) {
        char err[512];
        json_t *j = json_parse(resp->body, NULL);
        if (j) {
            json_t *err_obj = json_obj_get(j, "detail");
            const char *msg = err_obj ? json_get_str(err_obj, "", "") : NULL;
            if (!msg || !*msg) {
                err_obj = json_obj_get(j, "error");
                msg = err_obj ? json_get_str(err_obj, "message", "") : NULL;
            }
            snprintf(err, sizeof(err), "ElevenLabs API error (HTTP %d): %s",
                     resp->status, msg ? msg : resp->body);
            json_free(j);
        } else {
            snprintf(err, sizeof(err), "ElevenLabs API error (HTTP %d): %s",
                     resp->status, resp->body);
        }
        result = make_result(false, "", err, TRANSCRIBE_PROVIDER_ELEVENLABS);
    } else {
        json_t *j = json_parse(resp->body, NULL);
        if (!j) {
            result = make_result(false, "", "Failed to parse ElevenLabs response",
                                 TRANSCRIBE_PROVIDER_ELEVENLABS);
        } else {
            const char *text = json_get_str(j, "text", "");
            if (text && *text)
                result = make_result(true, text, NULL, TRANSCRIBE_PROVIDER_ELEVENLABS);
            else
                result = make_result(false, "", "ElevenLabs returned empty transcript",
                                     TRANSCRIBE_PROVIDER_ELEVENLABS);
            json_free(j);
        }
    }

    http_resp_free(resp);
    http_free(h);
    return result;
}

/* ================================================================
 *  Provider: Local Command (whisper CLI)
 * ================================================================ */

/* PoP: _transcribe_local_command @ lib/libtranscribe/transcribe.c:transcribe_local_command
 * Port of Python tools/transcription_tools.py:_transcribe_local_command(). */
static char *transcribe_local_command(const char *file_path, const char *model) {
    /* Check if local command is configured */
    const char *cmd_template = getenv("HERMES_LOCAL_STT_COMMAND");
    if (!cmd_template || !*cmd_template) {
        /* Try to auto-detect whisper binary */
        const char *whisper_bin = getenv("WHISPER_BINARY");
        if (!whisper_bin || !*whisper_bin) {
            /* Check common locations */
            static const char *common_paths[] = {
                "/usr/local/bin/whisper",
                "/opt/homebrew/bin/whisper",
                "/usr/bin/whisper",
                NULL
            };
            for (int i = 0; common_paths[i]; i++) {
                if (access(common_paths[i], X_OK) == 0) {
                    whisper_bin = common_paths[i];
                    break;
                }
            }
        }
        if (!whisper_bin || !*whisper_bin)
            return make_result(false, "", "No local STT command configured (set HERMES_LOCAL_STT_COMMAND or ensure 'whisper' is in PATH)", TRANSCRIBE_PROVIDER_LOCAL_CMD);
        
        /* Build default command template */
        static char default_template[1024];
        snprintf(default_template, sizeof(default_template),
                 "%s {input_path} --model {model} --output_format txt --output_dir {output_dir} --language {language}",
                 whisper_bin);
        cmd_template = default_template;
    }

    const char *language = getenv("HERMES_LOCAL_STT_LANGUAGE");
    if (!language || !*language) language = "en";

    const char *model_name = model ? model : TRANSCRIBE_DEFAULT_MODEL_GROQ;  /* fallback to a valid model */

    /* Create temp directory for output */
    char tmpdir[] = "/tmp/hermes_stt_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        return make_result(false, "", "Failed to create temp directory", TRANSCRIBE_PROVIDER_LOCAL_CMD);
    }

    /* Build command */
    char cmd[4096];
    char *p = cmd;
    const char *t = cmd_template;
    size_t remaining = sizeof(cmd) - 1;

    while (*t && remaining > 1) {
        if (strncmp(t, "{input_path}", 12) == 0) {
            size_t len = snprintf(p, remaining, "%s", file_path);
            if (len >= remaining) break;
            p += len; remaining -= len; t += 12;
        } else if (strncmp(t, "{model}", 7) == 0) {
            size_t len = snprintf(p, remaining, "%s", model_name);
            if (len >= remaining) break;
            p += len; remaining -= len; t += 7;
        } else if (strncmp(t, "{output_dir}", 12) == 0) {
            size_t len = snprintf(p, remaining, "%s", tmpdir);
            if (len >= remaining) break;
            p += len; remaining -= len; t += 12;
        } else if (strncmp(t, "{language}", 10) == 0) {
            size_t len = snprintf(p, remaining, "%s", language);
            if (len >= remaining) break;
            p += len; remaining -= len; t += 10;
        } else {
            *p++ = *t++; remaining--;
        }
    }
    *p = '\0';

    /* Execute command */
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        rmdir(tmpdir);
        return make_result(false, "", "Failed to execute local STT command", TRANSCRIBE_PROVIDER_LOCAL_CMD);
    }

    /* Read stdout (ignored, output is file) */
    char buf[1024];
    while (fgets(buf, sizeof(buf), fp)) {}
    pclose(fp);

    /* Find .txt file in output directory */
    DIR *dir = opendir(tmpdir);
    if (!dir) {
        rmdir(tmpdir);
        return make_result(false, "", "Local STT command completed but no output directory", TRANSCRIBE_PROVIDER_LOCAL_CMD);
    }

    char transcript[16384] = {0};
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len >= 4 && strcmp(name + len - 4, ".txt") == 0) {
            char txt_path[512];
            snprintf(txt_path, sizeof(txt_path), "%s/%s", tmpdir, name);
            FILE *tf = fopen(txt_path, "r");
            if (tf) {
                size_t n = fread(transcript, 1, sizeof(transcript) - 1, tf);
                transcript[n] = '\0';
                fclose(tf);
            }
            break;
        }
    }
    closedir(dir);

    /* Clean up temp directory */
    char rm_cmd[1024];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", tmpdir);
    system(rm_cmd);

    if (!transcript[0]) {
        return make_result(false, "", "Local STT command completed but did not produce a transcript", TRANSCRIBE_PROVIDER_LOCAL_CMD);
    }

    return make_result(true, transcript, NULL, TRANSCRIBE_PROVIDER_LOCAL_CMD);
}

/* ================================================================
 *  Plugin STT Provider Dispatch
 *  Port of Python tools/transcription_tools.py:_dispatch_to_plugin_provider().
 * ================================================================ */

static char *transcribe_plugin_provider(const hermes_config_t *cfg,
                                         const char *file_path,
                                         const char *provider_name,
                                         const char *model,
                                         const char *language) {
    if (!provider_name || !provider_name[0])
        return NULL;

    /* Built-in names should never reach here - caller handles them */
    const char *builtins[] = {
        "local", "local_command", "groq", "openai", "xai", "mistral", "elevenlabs", "deepgram", NULL
    };
    for (int i = 0; builtins[i]; i++) {
        if (strcasecmp(provider_name, builtins[i]) == 0)
            return NULL;  /* Built-ins always win */
    }

    /* Get plugin registry - for now we'll use a simple approach
     * In a full implementation, this would come from the agent state */
    /* The plugin registry should be passed in or retrieved from global state */
    extern plugin_registry_t *g_plugin_registry;
    if (!g_plugin_registry) {
        return NULL;
    }

    /* Find the transcription plugin by name */
    plugin_t *plug = plugin_registry_find(g_plugin_registry, provider_name);
    if (!plug) {
        return NULL;
    }

    /* Check it's a transcription plugin */
    if (plugin_type(plug) != PLUGIN_TRANSCRIPTION) {
        return make_result(false, "",
                          "Plugin is not a transcription provider",
                          provider_name);
    }

    /* Get the interface */
    void *(*get_iface)(void) = (void *(*)(void))plugin_symbol(plug, "plugin_get_interface");
    if (!get_iface) {
        return make_result(false, "",
                          "Plugin missing plugin_get_interface symbol",
                          provider_name);
    }

    plugin_interface_t *iface = (plugin_interface_t *)get_iface();
    if (!iface || !iface->transcription_transcribe) {
        return make_result(false, "",
                          "Plugin missing transcription_transcribe function",
                          provider_name);
    }

    /* Build extra JSON for future extensibility */
    char extra_json[1024];
    snprintf(extra_json, sizeof(extra_json),
             "{\"provider\":\"%s\"}", provider_name);

    /* Call the plugin's transcribe function */
    char *result_json = iface->transcription_transcribe(
        file_path,
        model ? model : "",
        language ? language : "",
        extra_json
    );

    if (!result_json) {
        return make_result(false, "",
                          "Plugin returned NULL result",
                          provider_name);
    }

    return result_json;
}

/* ================================================================
 *  Provider detection
 * ================================================================ */

/* Check if an env var is set and non-empty */
/* PoP: has_env @ environments:has_env */
/* PoP: has_env @ web_tools:_has_env */
static bool has_env(const char *name) {
    const char *val = getenv(name);
    return val && *val;
}

static const char *detect_provider(void) {
    /* Check explicit config via env */
    const char *explicit = getenv("HERMES_STT_PROVIDER");
    if (explicit && *explicit) {
        if (strcmp(explicit, "groq") == 0)
            return has_env("GROQ_API_KEY") ? TRANSCRIBE_PROVIDER_GROQ : NULL;
        if (strcmp(explicit, "openai") == 0) {
            if (has_env("VOICE_TOOLS_OPENAI_KEY") || has_env("OPENAI_API_KEY"))
                return TRANSCRIBE_PROVIDER_OPENAI;
            return NULL;
        }
        if (strcmp(explicit, "xai") == 0)
            return has_env("XAI_API_KEY") ? TRANSCRIBE_PROVIDER_XAI : NULL;
        if (strcmp(explicit, "deepgram") == 0)
            return has_env("DEEPGRAM_API_KEY") ? TRANSCRIBE_PROVIDER_DEEPGRAM : NULL;
        if (strcmp(explicit, "mistral") == 0)
            return has_env("MISTRAL_API_KEY") ? TRANSCRIBE_PROVIDER_MISTRAL : NULL;
        if (strcmp(explicit, "elevenlabs") == 0)
            return has_env("ELEVENLABS_API_KEY") ? TRANSCRIBE_PROVIDER_ELEVENLABS : NULL;
        if (strcmp(explicit, "local") == 0)
            return TRANSCRIBE_PROVIDER_LOCAL_CMD;  /* local -> local_command in C */
        if (strcmp(explicit, "local_command") == 0)
            return TRANSCRIBE_PROVIDER_LOCAL_CMD;
        return explicit;  /* Unknown — let it fail downstream */
    }

    /* Auto-detect: groq (free) > openai (paid) > xai > mistral > elevenlabs > local_command */
    if (has_env("GROQ_API_KEY"))
        return TRANSCRIBE_PROVIDER_GROQ;
    if (has_env("VOICE_TOOLS_OPENAI_KEY") || has_env("OPENAI_API_KEY"))
        return TRANSCRIBE_PROVIDER_OPENAI;
    if (has_env("XAI_API_KEY"))
        return TRANSCRIBE_PROVIDER_XAI;
    if (has_env("DEEPGRAM_API_KEY"))
        return TRANSCRIBE_PROVIDER_DEEPGRAM;
    if (has_env("MISTRAL_API_KEY"))
        return TRANSCRIBE_PROVIDER_MISTRAL;
    if (has_env("ELEVENLABS_API_KEY"))
        return TRANSCRIBE_PROVIDER_ELEVENLABS;

    /* Check for local command (whisper CLI) */
    if (has_local_command())
        return TRANSCRIBE_PROVIDER_LOCAL_CMD;

    return NULL;
}

/* Public wrapper: true when any STT provider is configured. */
bool transcription_is_available(void) {
    return detect_provider() != NULL;
}

/* ================================================================
 *  Public API
 *  ================================================================ */

/* PoP: transcribe_audio @ lib/libtranscribe/transcribe.c:transcribe_audio */
/* Port of Python tools/transcription_tools.py:transcribe_audio(). */
char *transcribe_audio(const char *file_path, const char *model) {
    /* Validate input */
    char *validation_err = transcribe_validate_file(file_path);
    if (validation_err)
        return validation_err;  /* Already a JSON result string */

    /* Check if STT is disabled */
    const char *disabled = getenv("HERMES_STT_DISABLED");
    if (disabled && (strcmp(disabled, "true") == 0 || strcmp(disabled, "1") == 0))
        return make_result(false, "", "STT is disabled (HERMES_STT_DISABLED=true)", NULL);

    /* Detect provider */
    const char *provider = detect_provider();
    if (!provider) {
        return make_result(false, "", "No STT provider available. Set GROQ_API_KEY (free), "
                           "VOICE_TOOLS_OPENAI_KEY or OPENAI_API_KEY (paid), XAI_API_KEY, "
                           "MISTRAL_API_KEY, ELEVENLABS_API_KEY, or configure local whisper.",
                           NULL);
    }

    /* Dispatch */
    if (strcmp(provider, TRANSCRIBE_PROVIDER_GROQ) == 0)
        return transcribe_groq(file_path, model);
    if (strcmp(provider, TRANSCRIBE_PROVIDER_OPENAI) == 0)
        return transcribe_openai(file_path, model);
    if (strcmp(provider, TRANSCRIBE_PROVIDER_XAI) == 0)
        return transcribe_xai(file_path, model);
    if (strcmp(provider, TRANSCRIBE_PROVIDER_DEEPGRAM) == 0)
        return make_result(false, "", "Deepgram provider not yet implemented", TRANSCRIBE_PROVIDER_DEEPGRAM);
    if (strcmp(provider, TRANSCRIBE_PROVIDER_MISTRAL) == 0)
        return transcribe_mistral(file_path, model);
    if (strcmp(provider, TRANSCRIBE_PROVIDER_ELEVENLABS) == 0)
        return transcribe_elevenlabs(file_path, model);
    if (strcmp(provider, TRANSCRIBE_PROVIDER_LOCAL_CMD) == 0) {
        /* Check if explicit "local" was requested (faster-whisper stub) */
        const char *explicit = getenv("HERMES_STT_PROVIDER");
        if (explicit && strcmp(explicit, "local") == 0)
            return transcribe_local(file_path, model);
        return transcribe_local_command(file_path, model);
    }

    /* Try plugin provider (for custom transcription backends) */
    extern plugin_registry_t *g_plugin_registry;
    char *plugin_result = transcribe_plugin_provider(NULL, file_path, provider, model, NULL);
    if (plugin_result)
        return plugin_result;

    return make_result(false, "", "Unknown STT provider", provider);
}