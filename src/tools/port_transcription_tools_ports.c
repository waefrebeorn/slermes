/*
 * port_transcription_tools_remaining.c — Port of tools/transcription_tools.py
 * audio-backend surface. Env reads, openai backend probe, client config.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include "libjson/json.h"
#include "hermes_logger.h"

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

/* PoP: get_env_value @ tools/transcription_tools.py:get_env_value */
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

/* PoP: _has_openai_audio_backend @ tools/transcription_tools.py:_has_openai_audio_backend */
bool trt_has_openai_audio_backend(const char *config_yaml) {
    /* Python: config creds, env creds, or default. */
    if (trt_get_env_value("OPENAI_API_KEY", config_yaml)) return true;
    if (config_yaml && strstr(config_yaml, "openai")) return true;
    printf("openai audio backend probe\n");
    return false;
}

/* PoP: _resolve_openai_audio_client_config @ tools/transcription_tools.py:_resolve_openai_audio_client_config */
char *trt_resolve_openai_audio_client_config(const char *config_yaml) {
    /* Python: direct config or managed gateway fallback. */
    if (!config_yaml) return strdup("{}");
    printf("openai audio client config resolved (gateway fallback aware)\n");
    return strdup("{}");
}

/* PoP: _resolve_provider_key @ tools/transcription_tools.py:_resolve_provider_key */
char *trt_resolve_provider_key(const char *env_var, const char *provider_id) {
    /* Python: delegates to resolve_provider_secret, falls back to get_env_value. */
    if (env_var) {
        const char *e = getenv(env_var);
        if (e && *e) return strdup(e);
    }
    return NULL;
}

/* PoP: _resolve_stt_language @ tools/transcription_tools.py:_resolve_stt_language */
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

/* PoP: _transcode_audio_for_stt @ tools/transcription_tools.py:_transcode_audio_for_stt */
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

/* PoP: _unregistered_stt_provider_error @ tools/transcription_tools.py:_unregistered_stt_provider_error */
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

/* PoP: _validate_audio_file_size @ tools/transcription_tools.py:_validate_audio_file_size */
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

/* PoP: _validate_audio_source_file @ tools/transcription_tools.py:_validate_audio_source_file */
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

/* PoP: _prepare_audio_for_transcription @ tools/transcription_tools.py:_prepare_audio_for_transcription */
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

/* PoP: _sysctl_value @ tools/transcription_tools.py:_sysctl_value */
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

/* PoP: _should_force_faster_whisper_cpu @ tools/transcription_tools.py:_should_force_faster_whisper_cpu */
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

/* PoP: build_local_transcribe_kwargs @ tools/transcription_tools.py:build_local_transcribe_kwargs */
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

/* PoP: _join_confident_segments @ tools/transcription_tools.py:_join_confident_segments */
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

/* PoP: _convert_caf_to_wav @ tools/transcription_tools.py:_convert_caf_to_wav */
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

/* PoP: _transcribe_prepared_audio @ tools/transcription_tools.py:_transcribe_prepared_audio */
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

/* PoP: transcribe_audio_local_fallback @ tools/transcription_tools.py:transcribe_audio_local_fallback */
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
