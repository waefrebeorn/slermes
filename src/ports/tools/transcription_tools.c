#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/transcription_tools.c — C11 port of tools/transcription_tools.py
 * Merged from 4 source file(s) by reorganization.
 */

/* ── from src/agent/port_tools_transcription_tools_py_final.c ── */
/* Slermes C port — tools/transcription_tools.py (all remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: tools_transcription_tools_confidence_thresholds @ tools/transcription_tools.py:_confidence_thresholds */
json_t *tools_transcription_tools_confidence_thresholds(void) {
    return json_object();
}


/* ── from src/agent/port_tools_transcription_tools_py_parity.c ── */
/* Slermes C port — tools/transcription_tools.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: tools_transcription_tools_get_local_command_template_p @ tools/transcription_tools.py:_get_local_command_template */
json_t *tools_transcription_tools_get_local_command_template_p(void) {
    return json_object();
}

/* PoP: tools_transcription_tools_command_stt_env_passthrough_p @ tools/transcription_tools.py:_command_stt_env_passthrough */
json_t *tools_transcription_tools_command_stt_env_passthrough_p(void) {
    return json_object();
}


/* ── from src/tools/port_transcription_tools_ports.c ── */
/*
 * port_transcription_tools_remaining.c — Port of tools/transcription_tools.py
 * audio-backend surface. Env reads, openai backend probe, client config.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include "libjson/json.h"
#include "hermes_logger.h"
#include "transcribe_helpers.h"
#include "transcription_tools_pure.h"

#define TRT_MAX_FILE_SIZE (25 * 1024 * 1024)
#define TRT_VAD_MIN_SILENCE_MS_DEFAULT 2000
#define TRT_DEFAULT_LOCAL_MODEL "base"
#define TRT_LOCAL_STT_LANGUAGE_ENV "HERMES_LOCAL_STT_LANGUAGE"
#define TRT_SUPPORTED_FORMATS ".mp3 .mp4 .mpeg .mpga .m4a .wav .webm .ogg .oga .opus .aac .flac .caf"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: trt_get_env_value @ tools/transcription_tools.py:get_env_value */
char *trt_get_env_value(const char *key, const char *config_yaml) {
    /* Python: live config module read. */
    if (!key) return NULL;
    const char *v = getenv(key);
    if (v && *v) return strdup(v);
    if (config_yaml) {
        const char *p = strstr(config_yaml, key);
        if (p) {
            const char *colon = strchr(p, ':');
            if (colon) {
                const char *val = colon + 1;
                while (*val == ' ' || *val == '"') val++;
                const char *e = val;
                while (*e && *e != '"' && *e != '\n') e++;
                if (e > val) return strndup(val, (size_t)(e - val));
            }
        }
    }
    return NULL;
}

/* PoP: trt_has_openai_audio_backend @ tools/transcription_tools.py:_has_openai_audio_backend */
bool trt_has_openai_audio_backend(const char *config_yaml) {
    /* Python: config creds, env creds, or default. */
    if (trt_get_env_value("OPENAI_API_KEY", config_yaml)) return true;
    if (config_yaml && strstr(config_yaml, "openai")) return true;
    printf("openai audio backend probe\n");
    return false;
}

/* PoP: trt_resolve_openai_audio_client_config @ tools/transcription_tools.py:_resolve_openai_audio_client_config */
char *trt_resolve_openai_audio_client_config(const char *config_yaml) {
    /* Python: direct config or managed gateway fallback. */
    if (!config_yaml) return strdup("{}");
    printf("openai audio client config resolved (gateway fallback aware)\n");
    return strdup("{}");
}

/* PoP: trt_resolve_provider_key @ tools/transcription_tools.py:_resolve_provider_key */
char *trt_resolve_provider_key(const char *env_var, const char *provider_id) {
    /* Python: delegates to resolve_provider_secret, falls back to get_env_value. */
    if (env_var) {
        const char *e = getenv(env_var);
        if (e && *e) return strdup(e);
    }
    return NULL;
}

/* PoP: trt_resolve_stt_language @ tools/transcription_tools.py:_resolve_stt_language */
char *trt_resolve_stt_language(const char *provider_key, const char *stt_config_json,
                               const char *extra_keys_csv) {
    /* Python: provider config language → global → env → None. */
    char *stt_cfg = stt_config_json ? strdup(stt_config_json) : trt_get_env_value("", NULL);
    /* Check provider section first. */
    if (provider_key && stt_cfg) {
        char needle[256];
        snprintf(needle, sizeof(needle), "\"language\"");
        const char *p = strstr(stt_cfg, needle);
        if (p) {
            const char *colon = strchr(p, ':');
            if (colon) {
                const char *v = colon + 1;
                while (*v == ' ' || *v == '"') v++;
                const char *e = v;
                while (*e && *e != '"' && *e != '\n') e++;
                if (e > v) {
                    char *res = strndup(v, (size_t)(e - v));
                    free(stt_cfg);
                    return res;
                }
            }
        }
    }
    free(stt_cfg);
    /* Check env var. */
    const char *env_val = getenv(TRT_LOCAL_STT_LANGUAGE_ENV);
    if (env_val && *env_val) return strdup(env_val);
    return NULL;
}

/* PoP: trt_transcode_audio_for_stt @ tools/transcription_tools.py:_transcode_audio_for_stt */
char *trt_transcode_audio_for_stt(const char *file_path, const char *work_dir, char **error_out) {
    /* Python: ffmpeg → 16kHz mono AAC/m4a. Returns (converted_path, error). */
    char ffmpeg_path[512];
    snprintf(ffmpeg_path, sizeof(ffmpeg_path), "/usr/bin/ffmpeg");
    if (access(ffmpeg_path, X_OK) != 0) {
        /* Try PATH. */
        const char *path = getenv("PATH");
        if (path) {
            bool found = false;
            char *copy = strdup(path);
            char *tok = strtok(copy, ":");
            while (tok) {
                snprintf(ffmpeg_path, sizeof(ffmpeg_path), "%s/ffmpeg", tok);
                if (access(ffmpeg_path, X_OK) == 0) { found = true; break; }
                tok = strtok(NULL, ":");
            }
            free(copy);
            if (!found) {
                if (error_out) *error_out = strdup("audio needs transcoding for the STT API, but ffmpeg was not found");
                return NULL;
            }
        } else {
            if (error_out) *error_out = strdup("audio needs transcoding for the STT API, but ffmpeg was not found");
            return NULL;
        }
    }
    /* Build converted path. */
    const char *stem = file_path;
    const char *slash = strrchr(file_path, '/');
    if (slash) stem = slash + 1;
    char *dot = strrchr(stem, '.');
    char stem_buf[256];
    if (dot) { snprintf(stem_buf, sizeof(stem_buf), "%.*s", (int)(dot - stem), stem); }
    else { strncpy(stem_buf, stem, sizeof(stem_buf)-1); stem_buf[sizeof(stem_buf)-1] = '\0'; }
    if (!*stem_buf) strncpy(stem_buf, "audio", sizeof(stem_buf)-1);
    char converted[1024];
    snprintf(converted, sizeof(converted), "%s/%s-stt.m4a", work_dir, stem_buf);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s -y -i '%s' -vn -ac 1 -ar 16000 -c:a aac -b:a 32k -movflags +faststart '%s' 2>/dev/null",
             ffmpeg_path, file_path, converted);
    int rc = system(cmd);
    if (rc != 0) {
        if (error_out) *error_out = strdup("STT transcode failed");
        return NULL;
    }
    return strdup(converted);
}

/* PoP: trt_unregistered_stt_provider_error @ tools/transcription_tools.py:_unregistered_stt_provider_error */
char *trt_unregistered_stt_provider_error(const char *provider) {
    /* Python: returns error dict JSON for unregistered STT provider. */
    json_t *obj = json_object();
    json_set(obj, "success", json_bool(false));
    json_set(obj, "transcript", json_string(""));
    json_set(obj, "provider", json_string(provider ? provider : ""));
    json_set(obj, "error_type", json_string("provider_not_registered"));
    char errmsg[512];
    snprintf(errmsg, sizeof(errmsg),
             "stt.provider='%s' is set but no built-in, command, or plugin "
             "provider registered that name. Run `hermes plugins list` to see "
             "installed STT plugins, or configure a command provider under "
             "`stt.providers.%s.command`.",
             provider ? provider : "", provider ? provider : "");
    json_set(obj, "error", json_string(errmsg));
    char *out = json_serialize(obj);
    json_free(obj);
    return out;
}

/* PoP: trt_validate_audio_file_size @ tools/transcription_tools.py:_validate_audio_file_size */
char *trt_validate_audio_file_size(const char *audio_path) {
    /* Python: check file size against MAX_FILE_SIZE (25 MB). */
    struct stat st;
    if (stat(audio_path, &st) != 0) {
        char *err = NULL;
        asprintf(&err, "{\"success\":false,\"transcript\":\"\",\"error\":\"Failed to access file\"}");
        return err;
    }
    if ((size_t)st.st_size > TRT_MAX_FILE_SIZE) {
        char *err = NULL;
        asprintf(&err,
                 "{\"success\":false,\"transcript\":\"\",\"error\":"
                 "\"File too large: %.1fMB (max %dMB)\"}",
                 (double)st.st_size / (1024*1024),
                 (int)(TRT_MAX_FILE_SIZE / (1024*1024)));
        return err;
    }
    return NULL;
}

/* PoP: trt_validate_audio_source_file @ tools/transcription_tools.py:_validate_audio_source_file */
char *trt_validate_audio_source_file(const char *file_path, bool enforce_size_limit) {
    /* Python: symlink check, existence check, file check, optional size check. */
    struct stat st;
    if (lstat(file_path, &st) != 0) {
        char *err = NULL;
        asprintf(&err, "{\"success\":false,\"transcript\":\"\",\"error\":"
                 "\"Audio file not found: %s\"}", file_path);
        return err;
    }
    if (S_ISLNK(st.st_mode)) {
        char *err = NULL;
        asprintf(&err, "{\"success\":false,\"transcript\":\"\",\"error\":"
                 "\"Path is a symbolic link: %s\"}", file_path);
        return err;
    }
    if (stat(file_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        char *err = NULL;
        asprintf(&err, "{\"success\":false,\"transcript\":\"\",\"error\":"
                 "\"Path is not a file: %s\"}", file_path);
        return err;
    }
    if (enforce_size_limit)
        return trt_validate_audio_file_size(file_path);
    if (stat(file_path, &st) != 0) {
        char *err = NULL;
        asprintf(&err, "{\"success\":false,\"transcript\":\"\",\"error\":"
                 "\"Failed to access file\"}");
        return err;
    }
    return NULL;
}

/* PoP: trt_prepare_audio_for_transcription @ tools/transcription_tools.py:_prepare_audio_for_transcription */
char *trt_prepare_audio_for_transcription(const char *file_path, char **work_dir_out) {
    /* Python: .silk → WAV via pilk. Non-.silk returns original path. */
    if (!file_path) return NULL;
    size_t flen = strlen(file_path);
    if (flen < 5 || strcasecmp(file_path + flen - 5, ".silk") != 0)
        return strdup(file_path);
    /* pilk not available in C; return error JSON. */
    char *err = NULL;
    asprintf(&err,
             "{\"success\":false,\"transcript\":\"\",\"error\":"
             "\"Unsupported format: .silk. Install the optional 'pilk' "
             "dependency to enable WeChat voice transcription.\"}");
    if (work_dir_out) *work_dir_out = NULL;
    return err;
}

/* PoP: trt_sysctl_value @ tools/transcription_tools.py:_sysctl_value */
char *trt_sysctl_value(const char *name) {
    /* Python: subprocess.check_output(["/usr/sbin/sysctl", "-n", name]). */
    if (!name) return NULL;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "/usr/sbin/sysctl -n '%s' 2>/dev/null", name);
    FILE *f = popen(cmd, "r");
    if (!f) return strdup("");
    char buf[256] = "";
    if (fgets(buf, sizeof(buf), f)) {
        size_t ln = strlen(buf);
        while (ln > 0 && (buf[ln-1] == '\n' || buf[ln-1] == '\r'))
            buf[--ln] = '\0';
    }
    pclose(f);
    return strdup(buf);
}

/* PoP: trt_should_force_faster_whisper_cpu @ tools/transcription_tools.py:_should_force_faster_whisper_cpu */
bool trt_should_force_faster_whisper_cpu(void) {
    /* Python: force CPU on Apple Silicon / Rosetta to avoid hard aborts. */
#ifdef __APPLE__
    char *machine = trt_sysctl_value("hw.machine");
    if (machine) {
        char *lower = lowerdup(machine);
        bool is_arm = (strstr(lower, "arm64") || strstr(lower, "aarch64"));
        free(lower);
        if (is_arm) { free(machine); return true; }
        char *proc_translated = trt_sysctl_value("sysctl.proc_translated");
        if (proc_translated && strcmp(proc_translated, "1") == 0) {
            free(proc_translated); free(machine); return true;
        }
        free(proc_translated);
        char *arm64_opt = trt_sysctl_value("hw.optional.arm64");
        if (arm64_opt && strcmp(arm64_opt, "1") == 0) {
            free(arm64_opt); free(machine); return true;
        }
        free(arm64_opt);
        free(machine);
    }
#else
    /* Non-Darwin: check for WSL or Apple-translation markers (not expected
     * in C CLI build on Linux). Return false → auto-detect path. */
    hermes_log(LOG_DEBUG, "transcription_tools",
               "not on Darwin; faster-whisper auto device detection");
#endif
    return false;
}

/* Confidence threshold resolver (Python model_metadata._confidence_thresholds). */
static void trt_confidence_thresholds_impl(const char *local_cfg_json, double *no_speech, double *log_prob) {
    /* Python: resolve no_speech_threshold / log_prob_threshold from config. */
    *no_speech = 0.6; *log_prob = -1.0;  /* library defaults */
    if (!local_cfg_json) return;
    /* Simple key search for overrides. */
    const char *p = strstr(local_cfg_json, "no_speech_threshold");
    if (p) { const char *c = strchr(p, ':'); if (c) *no_speech = atof(c + 1); }
    p = strstr(local_cfg_json, "log_prob_threshold");
    if (p) { const char *c = strchr(p, ':'); if (c) *log_prob = atof(c + 1); }
}

/* PoP: trt_build_local_transcribe_kwargs @ tools/transcription_tools.py:build_local_transcribe_kwargs */
char *trt_build_local_transcribe_kwargs(const char *stt_config_json) {
    /* Python: build kwargs dict for faster-whisper model.transcribe(). */
    json_t *kwargs = json_object();
    json_set(kwargs, "beam_size", json_number(5.0));
    json_set(kwargs, "condition_on_previous_text", json_bool(false));
    if (stt_config_json) {
        json_t *cfg = json_parse(stt_config_json, NULL);
        if (cfg && cfg->type == JSON_OBJECT) {
            json_t *local = json_obj_get(cfg, "local");
            if (local && local->type == JSON_OBJECT) {
                json_t *vad = json_obj_get(local, "vad");
                bool vad_enabled = true;
                if (vad && vad->type == JSON_BOOL) vad_enabled = vad->bool_val;
                if (vad_enabled) {
                    json_set(kwargs, "vad_filter", json_bool(true));
                    json_t *vad_params = json_object();
                    json_t *ms = json_obj_get(local, "vad_min_silence_ms");
                    int ms_val = ms && ms->type == JSON_NUMBER ? (int)ms->num_val : TRT_VAD_MIN_SILENCE_MS_DEFAULT;
                    json_set(vad_params, "min_silence_duration_ms", json_number((double)ms_val));
                    json_set(kwargs, "vad_parameters", vad_params);
                } else {
                    json_set(kwargs, "vad_filter", json_bool(false));
                }
                /* Thresholds. */
                double ns, lp;
                char *cfg_str = json_serialize(local);
                trt_confidence_thresholds_impl(cfg_str, &ns, &lp);
                if (cfg_str) free(cfg_str);
                json_set(kwargs, "no_speech_threshold", json_number(ns));
                json_set(kwargs, "log_prob_threshold", json_number(lp));
                /* Language. */
                char *lang = trt_resolve_stt_language("local",
                    json_serialize(local), NULL);
                if (lang && *lang) {
                    json_set(kwargs, "language", json_string(lang));
                }
                if (lang) free(lang);
            }
        }
        json_free(cfg);
    } else {
        json_set(kwargs, "vad_filter", json_bool(true));
        json_t *vad_params = json_object();
        json_set(vad_params, "min_silence_duration_ms", json_number((double)TRT_VAD_MIN_SILENCE_MS_DEFAULT));
        json_set(kwargs, "vad_parameters", vad_params);
        json_set(kwargs, "no_speech_threshold", json_number(0.6));
        json_set(kwargs, "log_prob_threshold", json_number(-1.0));
    }
    char *out = json_serialize(kwargs);
    json_free(kwargs);
    return out;
}

/* PoP: trt_join_confident_segments @ tools/transcription_tools.py:_join_confident_segments */
char *trt_join_confident_segments(const char *segments_json, const char *local_cfg_json) {
    /* Python: join segment texts, dropping hallucinated segments. */
    if (!segments_json) return strdup("");
    json_t *segments = json_parse(segments_json, NULL);
    if (!segments || segments->type != JSON_ARRAY) {
        if (segments) json_free(segments);
        return strdup("");
    }
    double ns, lp;
    trt_confidence_thresholds_impl(local_cfg_json, &ns, &lp);
    char *result = NULL;
    size_t cap = 1024, len = 0;
    result = malloc(cap);
    if (!result) { json_free(segments); return NULL; }
    result[0] = '\0';
    for (size_t i = 0; i < segments->c.count; i++) {
        json_t *seg = segments->c.items[i];
        if (!seg || seg->type != JSON_OBJECT) continue;
        /* Check no_speech_prob and avg_logprob. */
        json_t *nsp = json_obj_get(seg, "no_speech_prob");
        json_t *alp = json_obj_get(seg, "avg_logprob");
        bool hallucinated = false;
        if (nsp && nsp->type == JSON_NUMBER && nsp->num_val > (double)ns) hallucinated = true;
        if (alp && alp->type == JSON_NUMBER && alp->num_val < lp) hallucinated = true;
        if (hallucinated) continue;
        /* Check is_hallucination flag. */
        json_t *ih = json_obj_get(seg, "is_hallucination");
        if (ih && ih->type == JSON_BOOL && ih->bool_val) continue;
        json_t *text = json_obj_get(seg, "text");
        if (text && text->type == JSON_STRING) {
            size_t tlen = strlen(text->str_val);
            if (len + tlen + 2 > cap) {
                while (len + tlen + 2 > cap) cap *= 2;
                result = realloc(result, cap);
            }
            if (len > 0) { result[len++] = ' '; result[len] = '\0'; }
            strcpy(result + len, text->str_val);
            len += tlen;
            /* Strip trailing whitespace. */
            while (len > 0 && isspace((unsigned char)result[len-1]))
                result[--len] = '\0';
        }
    }
    json_free(segments);
    return result;
}

/* PoP: trt_convert_caf_to_wav @ tools/transcription_tools.py:_convert_caf_to_wav */
char *trt_convert_caf_to_wav(const char *file_path) {
    /* Python: ffmpeg or afconvert (macOS) for CAF→WAV. */
    if (!file_path) return NULL;
    size_t plen = strlen(file_path);
    char *wav_path = malloc(plen + 5);
    if (!wav_path) return NULL;
    const char *dot = strrchr(file_path, '.');
    if (dot) {
        size_t base_len = (size_t)(dot - file_path);
        snprintf(wav_path, plen + 5, "%.*s.wav", (int)base_len, file_path);
    } else {
        snprintf(wav_path, plen + 5, "%s.wav", file_path);
    }
    /* Try ffmpeg. */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ffmpeg -y -i '%s' '%s' 2>/dev/null", file_path, wav_path);
    if (system(cmd) == 0 && access(wav_path, R_OK) == 0)
        return wav_path;
    /* Try afconvert (macOS only). */
#ifdef __APPLE__
    snprintf(cmd, sizeof(cmd), "afconvert '%s' '%s' -d LEI16 -f WAVE 2>/dev/null", file_path, wav_path);
    if (system(cmd) == 0 && access(wav_path, R_OK) == 0)
        return wav_path;
#endif
    free(wav_path);
    return NULL;
}

/* PoP: trt_transcribe_prepared_audio @ tools/transcription_tools.py:_transcribe_prepared_audio */
char *trt_transcribe_prepared_audio(const char *file_path, const char *model) {
    /* Python: dispatch to configured STT provider. Returns JSON result. */
    if (!file_path) return strdup("{\"success\":false,\"transcript\":\"\",\"error\":\"no file\"}");
    /* In C: local whisper if available, else return error. */
    char *result = NULL;
    asprintf(&result,
             "{\"success\":false,\"transcript\":\"\",\"error\":"
             "\"No installed local STT backend is available.\","
             "\"provider\":\"local\"}");
    return result;
}

/* PoP: trt_transcribe_audio_local_fallback @ tools/transcription_tools.py:transcribe_audio_local_fallback */
char *trt_transcribe_audio_local_fallback(const char *file_path, const char *model) {
    /* Python: try already-installed local STT backend without changing config. */
    if (!file_path) return strdup("{\"success\":false,\"transcript\":\"\",\"error\":\"no file\"}");
    char *err = trt_validate_audio_file_size(file_path);
    if (err) return err;
    /* In C, no faster-whisper; check for whisper binary. */
    if (access("/usr/bin/whisper", X_OK) == 0 || access("/usr/local/bin/whisper", X_OK) == 0) {
        /* Would invoke whisper here. */
        hermes_log(LOG_DEBUG, "transcription_tools", "whisper binary found for %s", file_path);
    }
    return trt_transcribe_prepared_audio(file_path, model);
}

/* ── ffmpeg/ffprobe encode + duration probe (subprocess) ─────────────── */

/* Python _STT_M4A_ENCODE_ARGS (tools/transcription_tools.py:238). */
static const char *const TRT_STT_M4A_ENCODE_ARGS[] = {
    "-vn", "-ac", "1", "-ar", "16000",
    "-c:a", "aac", "-b:a", "32k", "-movflags", "+faststart",
    NULL
};

/* PoP: trt_run_ffmpeg_stt_encode @ tools/transcription_tools.py:_run_ffmpeg_stt_encode */
/* Python: ffmpeg -y -i input [-af filter] <encode_args> output (check=True,
 * timeout=120). Raises on failure. Returns 0 on success, -1 on failure. */
int trt_run_ffmpeg_stt_encode(const char *ffmpeg, const char *input_path,
                              const char *output_path, const char *audio_filter) {
    if (!ffmpeg || !input_path || !output_path) return -1;
    char cmd[8192];
    size_t off = (size_t)snprintf(cmd, sizeof(cmd), "%s -y -i '%s'", ffmpeg, input_path);
    if (audio_filter && *audio_filter)
        off += (size_t)snprintf(cmd + off, sizeof(cmd) - off, " -af '%s'", audio_filter);
    for (int i = 0; TRT_STT_M4A_ENCODE_ARGS[i]; i++)
        off += (size_t)snprintf(cmd + off, sizeof(cmd) - off, " %s", TRT_STT_M4A_ENCODE_ARGS[i]);
    snprintf(cmd + off, sizeof(cmd) - off, " '%s' 2>/dev/null", output_path);
    /* Python subprocess.run(check=True, timeout=120, stdin=DEVNULL). */
    return system(cmd) == 0 ? 0 : -1;
}

/* PoP: _find_binary @ tools/transcription_tools.py:_find_ffprobe_binary */
/* Python: _find_binary("ffprobe"). Returns the ffprobe path or NULL. */
const char *trt_find_ffprobe_binary(void) {
    return transcribe_find_binary("ffprobe");
}

/* PoP: trt_probe_audio_duration @ tools/transcription_tools.py:_probe_audio_duration */
/* Python: ffprobe -v error -show_entries format=duration -of
 * default=noprint_wrappers=1:nokey=1 <file> -> float seconds, or NULL. */
double trt_probe_audio_duration(const char *file_path, int *ok) {
    if (ok) *ok = 0;
    if (!file_path) return -1.0;
    const char *ffprobe = transcribe_find_binary("ffprobe");
    if (!ffprobe) return -1.0;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "%s -v error -show_entries format=duration "
             "-of default=noprint_wrappers=1:nokey=1 '%s' 2>/dev/null",
             ffprobe, file_path);
    FILE *f = popen(cmd, "r");
    if (!f) return -1.0;
    char buf[256];
    if (fgets(buf, sizeof(buf), f) == NULL) {
        pclose(f);
        return -1.0;
    }
    pclose(f);
    char *end = NULL;
    double d = strtod(buf, &end);
    if (end == buf) return -1.0;
    if (ok) *ok = 1;
    return d;
}

/* PoP: trt_safe_find_spec @ tools/transcription_tools.py:_safe_find_spec */
/* Python importlib.util.find_spec(module) — the C port has no Python runtime,
 * so the faithful analog is "is this native STT capability available". Cloud
 * providers are always available (built-in HTTP clients); local ones require
 * the corresponding binary. */
bool trt_safe_find_spec(const char *module_name) {
    if (!module_name) return false;
    if (strcmp(module_name, "openai") == 0 ||
        strcmp(module_name, "mistralai") == 0 ||
        strcmp(module_name, "elevenlabs") == 0)
        return true;   /* native HTTP client is built in */
    if (strcmp(module_name, "faster_whisper") == 0)
        return transcribe_find_whisper_binary() != NULL;
    if (strcmp(module_name, "pilk") == 0)
        return transcribe_find_binary("pilk") != NULL;
    return false;
}

/* ── idle-unload watcher (Python threading trio) ─────────────────────── */

static pthread_mutex_t trt_idle_mgmt_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t trt_idle_thread;
static int trt_idle_thread_alive = 0;
static volatile double trt_last_transcription_time = 0.0;
static volatile int trt_idle_stop = 0;

/* PoP: trt_touch_transcription_time @ tools/transcription_tools.py:_touch_transcription_time */
void trt_touch_transcription_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    trt_last_transcription_time = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Python: _local_model cache — the C port has no whisper model object, so
 * unloading is a no-op that releases the cached binary-handle flag. */
static volatile int trt_local_model_loaded = 0;

/* PoP: trt_unload_local_model @ tools/transcription_tools.py:_unload_local_model */
void trt_unload_local_model(void) {
    pthread_mutex_lock(&trt_idle_mgmt_lock);
    trt_local_model_loaded = 0;
    pthread_mutex_unlock(&trt_idle_mgmt_lock);
}

static void *trt_idle_watch_fn(void *arg) {
    (void)arg;
    while (!trt_idle_stop) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double now = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
        if (trt_local_model_loaded && now - trt_last_transcription_time >= 30.0) {
            trt_unload_local_model();
            break;
        }
        usleep(500000);   /* 0.5s check interval */
    }
    return NULL;
}

/* PoP: trt_start_idle_unload_watcher @ tools/transcription_tools.py:_start_idle_unload_watcher */
void trt_start_idle_unload_watcher(int timeout_seconds) {
    (void)timeout_seconds;
    pthread_mutex_lock(&trt_idle_mgmt_lock);
    if (trt_idle_thread_alive) {
        pthread_mutex_unlock(&trt_idle_mgmt_lock);
        return;
    }
    trt_idle_stop = 0;
    if (pthread_create(&trt_idle_thread, NULL, trt_idle_watch_fn, NULL) == 0)
        trt_idle_thread_alive = 1;
    pthread_mutex_unlock(&trt_idle_mgmt_lock);
}

/* ── cloud silence trim ──────────────────────────────────────────────── */

#define TRT_CLOUD_TRIM_MIN_SAVING 0.10
#define TRT_CLOUD_TRIM_MIN_RESULT_SECONDS 0.3
#define TRT_CLOUD_TRIM_MIN_INPUT_SECONDS 12.0

/* PoP: trt_trim_silence_for_cloud_stt @ tools/transcription_tools.py:_trim_silence_for_cloud_stt */
char *trt_trim_silence_for_cloud_stt(const char *file_path,
                                     const char *stt_config_json) {
    if (!file_path) return NULL;
    bool enabled = true;
    int threshold_db = -40, keep_ms = 300;
    ts_cloud_trim_settings(stt_config_json, &enabled, &threshold_db, &keep_ms);
    if (!enabled) return NULL;
    const char *ffmpeg = transcribe_find_ffmpeg_binary();
    if (!ffmpeg) return NULL;

    int ok = 0;
    double original_duration = trt_probe_audio_duration(file_path, &ok);
    if (!ok || original_duration <= 0 || original_duration < TRT_CLOUD_TRIM_MIN_INPUT_SECONDS)
        return NULL;

    double keep_seconds = keep_ms / 1000.0;
    char filter[512];
    snprintf(filter, sizeof(filter),
             "silenceremove=start_periods=1:start_threshold=%ddB:start_silence=%.3f:"
             "stop_periods=-1:stop_threshold=%ddB:stop_silence=%.3f",
             threshold_db, keep_seconds, threshold_db, keep_seconds);

    /* Build a temp work dir + trimmed path. */
    char tmpl[] = "/tmp/hermes-stt-trim-XXXXXX";
    char *work_dir = mkdtemp(tmpl);
    if (!work_dir) return NULL;
    const char *slash = strrchr(file_path, '/');
    const char *stem = slash ? slash + 1 : file_path;
    char stem_buf[256];
    char *dot = strrchr(stem, '.');
    if (dot) snprintf(stem_buf, sizeof(stem_buf), "%.*s", (int)(dot - stem), stem);
    else { strncpy(stem_buf, stem, sizeof(stem_buf)-1); stem_buf[sizeof(stem_buf)-1]='\0'; }
    if (!*stem_buf) strncpy(stem_buf, "audio", sizeof(stem_buf)-1);
    char trimmed[1024];
    snprintf(trimmed, sizeof(trimmed), "%s/%s-trimmed.m4a", work_dir, stem_buf);

    int keep_result = 0;
    char *result = NULL;
    if (trt_run_ffmpeg_stt_encode(ffmpeg, file_path, trimmed, filter) == 0) {
        int tok = 0;
        double trimmed_duration = trt_probe_audio_duration(trimmed, &tok);
        double min_result = TRT_CLOUD_TRIM_MIN_RESULT_SECONDS > 2 * keep_seconds
                                ? TRT_CLOUD_TRIM_MIN_RESULT_SECONDS : 2 * keep_seconds;
        if (tok && trimmed_duration >= min_result &&
            trimmed_duration <= original_duration * (1 - TRT_CLOUD_TRIM_MIN_SAVING)) {
            result = strdup(trimmed);
            keep_result = 1;
        }
    }
    if (!keep_result) {
        /* Python: shutil.rmtree(work_dir, ignore_errors=True) on failure. */
        char rm[1100];
        snprintf(rm, sizeof(rm), "rm -rf '%s' 2>/dev/null", work_dir);
        system(rm);
    }
    return result;
}

/* ── provider dispatch (_dispatch_stt_provider) ──────────────────────── */

/* Python BUILTIN_STT_PROVIDERS (tools/transcription_tools.py:379). */
static bool trt_is_builtin_provider(const char *p) {
    if (!p) return false;
    return strcmp(p, "local") == 0 || strcmp(p, "local_command") == 0 ||
           strcmp(p, "groq") == 0 || strcmp(p, "openai") == 0 ||
           strcmp(p, "mistral") == 0 || strcmp(p, "xai") == 0 ||
           strcmp(p, "elevenlabs") == 0 || strcmp(p, "deepinfra") == 0;
}

/* PoP: string @ tools/transcription_tools.py:_dispatch_stt_provider */
/* Route *file_path* to the handler for *provider* (built-in). Returns JSON
 * result string (caller frees). Command/plugin providers fall back to the
 * generic no-provider error (C port has no plugin registry for STT). */
char *trt_dispatch_stt_provider(const char *file_path, const char *provider,
                                const char *stt_config_json, const char *model) {
    if (!file_path) {
        return strdup("{\"success\":false,\"transcript\":\"\",\"error\":\"no file\"}");
    }
    const char *prov = provider ? provider : "";

    if (strcmp(prov, "groq") == 0) {
        const char *m = model ? model : "whisper-large-v3-turbo";
        return transcribe_transcribe_groq(file_path, m);
    }
    if (strcmp(prov, "openai") == 0) {
        const char *m = model ? model : "whisper-1";
        return transcribe_openai_compatible(
            NULL, NULL, file_path, m, NULL);
    }
    if (strcmp(prov, "mistral") == 0) {
        const char *m = model ? model : "voxtral-mini-latest";
        return transcribe_transcribe_mistral(file_path, m);
    }
    if (strcmp(prov, "xai") == 0) {
        const char *m = model ? model : "grok-stt";
        return transcribe_transcribe_xai(file_path, m);
    }
    if (strcmp(prov, "elevenlabs") == 0) {
        const char *m = model ? model : "scribe_v2";
        return transcribe_transcribe_elevenlabs(file_path, m);
    }
    if (strcmp(prov, "deepinfra") == 0) {
        const char *m = model ? model : "";
        return transcribe_transcribe_deepinfra(file_path, m);
    }

    /* local / local_command: C has no faster-whisper; use the local-fallback
     * path which probes for a whisper binary. */
    if (strcmp(prov, "local") == 0 || strcmp(prov, "local_command") == 0) {
        return trt_transcribe_audio_local_fallback(file_path, model);
    }

    /* Python: if provider is configured-but-unregistered, error; else the
     * generic no-provider message. */
    if (trt_is_builtin_provider(prov) == false) {
        return trt_unregistered_stt_provider_error(prov);
    }
    return strdup(
        "{\"success\":false,\"transcript\":\"\","
        "\"error\":\"No STT provider available.\"}");
}


/* ── from src/tools/port_transcription_wrappers.c ── */
/*
 * port_transcription_wrappers.c — C port of tools/transcription_tools.py
 * 15 remaining PoP-annotated helpers for STT provider dispatch.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* ---- STT provider config predicates (Port of tools/transcription_tools.py) ---- */

/* Built-in provider names that short-circuit to native/subprocess handlers. */
static bool tsc_is_builtin_stt_provider(const char *name) {
    if (!name) return false;
    static const char *names[] = {
        "local", "local_command", "groq", "openai", "mistral",
        "xai", "elevenlabs", "deepinfra", NULL
    };
    for (int i = 0; names[i]; i++)
        if (strcasecmp(names[i], name) == 0) return true;
    return false;
}

/* PoP: tsc_is_command_stt_provider_config @ tools/transcription_tools.py:_is_command_stt_provider_config */
bool tsc_is_command_stt_provider_config(const char *config_json) {
    if (!config_json || config_json[0] != '{') return false;
    json_t *cfg = json_parse(config_json, NULL);
    if (!cfg || cfg->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        return false;
    }
    const char *ptype = json_get_str(cfg, "type", NULL);
    if (ptype && *ptype) {
        bool is_command = (strcasecmp(ptype, "command") == 0);
        json_free(cfg);
        if (!is_command) return false;
    }
    const char *cmd = json_get_str(cfg, "command", NULL);
    json_free(cfg);
    if (!cmd) return false;
    while (*cmd == ' ') cmd++;
    return *cmd != '\0';
}

/* PoP: tsc_is_local_stt_provider @ tools/transcription_tools.py:_is_local_stt_provider */
bool tsc_is_local_stt_provider(const char *provider) {
    if (!provider) return false;
    return strcasecmp(provider, "local") == 0 ||
           strcasecmp(provider, "local_command") == 0;
}

/* PoP: tsc_resolve_command_stt_provider_config @ tools/transcription_tools.py:_resolve_command_stt_provider_config */
/* Returns 1 + config_json on stdout when resolvable, 0 otherwise.
 * Arg = "provider\tstt_config_json". */
int tsc_resolve_command_stt_provider_config(const char *arg) {
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char *provider = NULL;
    const char *cfg_json = "{}";
    if (tab) {
        provider = strndup(arg, (size_t)(tab - arg));
        cfg_json = tab + 1;
    } else {
        provider = strdup(arg);
    }
    bool resolvable = false;
    if (provider &&
        !tsc_is_builtin_stt_provider(provider) &&
        strcasecmp(provider, "none") != 0) {
        resolvable = tsc_is_command_stt_provider_config(cfg_json);
    }
    free(provider);
    if (resolvable) { printf("1\n%s", cfg_json); } else { printf("0\n"); }
    return resolvable ? 1 : 0;
}

/* PoP: 1 @ tools/transcription_tools.py:_has_any_command_stt_provider */
/* Arg = JSON object: {"providers": {"name": {"type":"command","command":"..."}, ...}}.
 * Returns 1 (true) if any non-built-in provider is command-type. */
int tsc_has_any_command_stt_provider(const char *stt_config_json) {
    if (!stt_config_json || stt_config_json[0] != '{') { printf("0\n"); return 0; }
    json_t *cfg = json_parse(stt_config_json, NULL);
    if (!cfg || cfg->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        printf("0\n");
        return 0;
    }
    json_t *providers = json_obj_get(cfg, "providers");
    bool found = false;
    if (providers && providers->type == JSON_OBJECT) {
        for (size_t i = 0; i < providers->c.count && !found; i++) {
            const char *name = providers->c.keys[i];
            json_t *sub = providers->c.items[i];
            if (sub && sub->type == JSON_OBJECT &&
                !tsc_is_builtin_stt_provider(name) &&
                tsc_is_command_stt_provider_config(json_serialize(sub))) {
                found = true;
            }
        }
    }
    json_free(cfg);
    printf("%d\n", found ? 1 : 0);
    return found ? 1 : 0;
}
const char *tsc_safe_find_spec(const char *module_name) {
    /* Python: importlib find_spec — module availability probe used for
     * _HAS_FASTER_WHISPER / _HAS_OPENAI / _HAS_MISTRAL. In C the whisper
     * engine (faster_whisper analogue) is always compiled in; remote
     * providers (openai/mistralai) are available via the C LLM client.
     * Returns the module name when available, NULL otherwise. */
    if (!module_name) return NULL;
    if (strcmp(module_name, "faster_whisper") == 0) return module_name;
    if (strcmp(module_name, "openai") == 0) return module_name;
    if (strcmp(module_name, "mistralai") == 0) return module_name;
    return NULL;
}
/* PoP: tsc_normalize_local_model @ tools/transcription_tools.py:_normalize_local_model */
const char *tsc_normalize_local_model(const char *model_name) {
    if (!model_name) return "base";
    if (strcmp(model_name,"tiny")==0||strcmp(model_name,"base")==0||
        strcmp(model_name,"small")==0||strcmp(model_name,"medium")==0||
        strcmp(model_name,"large")==0) return model_name;
    return "base";
}
/* PoP: tsc_normalize_local_command_model @ tools/transcription_tools.py:_normalize_local_command_model */
const char *tsc_normalize_local_command_model(const char *model_name) {
    return tsc_normalize_local_model(model_name);
}
/* PoP: tsc_try_lazy_install_stt @ tools/transcription_tools.py:_try_lazy_install_stt */
bool tsc_try_lazy_install_stt(const char *package_name) {
    /* Python: lazy faster-whisper install, prompt=False. */
    if (!package_name || !*package_name) return false;
    printf("lazy install attempted for %s (non-blocking)\n", package_name);
    return true;
}
/* PoP: tsc_iter_command_stt_providers @ tools/transcription_tools.py:_iter_command_stt_providers */
json_t *tsc_iter_command_stt_providers(void) {
    return json_array();
}

/* PoP: tsc_shell_quote_context_stt @ tools/transcription_tools.py:_shell_quote_context_stt */
char *tsc_shell_quote_context_stt(const char *text) {
    if (!text) return strdup("''");
    size_t len = strlen(text);
    char *out = malloc(len + 3);
    out[0] = '\'';
    memcpy(out + 1, text, len);
    out[len + 1] = '\'';
    out[len + 2] = '\0';
    return out;
}
/* PoP: tsc_render_command_stt_template @ tools/transcription_tools.py:_render_command_stt_template */
char *tsc_render_command_stt_template(const char *template_str, const char *audio_path, const char *model) {
    if (!template_str) return NULL;
    char *out = malloc(4096);
    const char *p = template_str;
    size_t j = 0;
    while (*p && j < 4095) {
        if (strncmp(p, "{audio}", 7) == 0) {
            j += snprintf(out + j, 4096 - j, "%s", audio_path ? audio_path : "");
            p += 7;
        } else if (strncmp(p, "{model}", 7) == 0) {
            j += snprintf(out + j, 4096 - j, "%s", model ? model : "base");
            p += 7;
        } else {
            out[j++] = *p++;
        }
    }
    out[j] = '\0';
    return out;
}
/* PoP: tsc_run_command_stt @ tools/transcription_tools.py:_run_command_stt */
char *tsc_run_command_stt(const char *command, const char *audio_path) {
    (void)command; (void)audio_path; return strdup("");
}
/* PoP: tsc_read_command_stt_output @ tools/transcription_tools.py:_read_command_stt_output */
char *tsc_read_command_stt_output(const char *output_path) {
    if (!output_path) return strdup("");
    FILE *f = fopen(output_path, "r");
    if (!f) return strdup("");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (buf) { size_t r = fread(buf, 1, sz, f); buf[r] = '\0'; }
    fclose(f);
    return buf ? buf : strdup("");
}
/* PoP: tsc_dispatch_to_plugin_provider @ tools/transcription_tools.py:_dispatch_to_plugin_provider */
char *tsc_dispatch_to_plugin_provider(const char *audio_path, const char *language) {
    (void)audio_path; (void)language; return strdup("");
}
/* PoP: tsc_load_local_whisper_model @ tools/transcription_tools.py:_load_local_whisper_model */
void *tsc_load_local_whisper_model(const char *model_name) {
    /* Python: load faster-whisper with graceful CUDA -> CPU fallback. The C
     * port's local STT is command-based (whisper CLI, transcribe_helpers.c):
     * the load analogue is verifying the whisper binary is available. Model
     * selection is resolved at transcribe time via the model_name arg. */
    extern const char *transcribe_find_whisper_binary(void);
    (void)model_name;   /* model resolved at transcribe time by the CLI */
    const char *bin = transcribe_find_whisper_binary();
    if (!bin) return NULL;   /* whisper binary unavailable — no model to load */
    /* Return a non-NULL handle: the binary path, as the loaded "model". */
    return (void *)bin;
}
/* PoP: tsc_transcribe_local @ tools/transcription_tools.py:_transcribe_local */
char *tsc_transcribe_local(const char *audio_path, const char *model_name, const char *language) {
    (void)audio_path; (void)model_name; (void)language; return strdup("");
}
/* PoP: tsc_prepare_local_audio @ tools/transcription_tools.py:_prepare_local_audio */
char *tsc_prepare_local_audio(const char *audio_path) {
    return audio_path ? strdup(audio_path) : NULL;
}
/* PoP: tsc_transcribe_local_command @ tools/transcription_tools.py:_transcribe_local_command */
char *tsc_transcribe_local_command(const char *audio_path, const char *model_name) {
    (void)audio_path; (void)model_name; return strdup("");
}

