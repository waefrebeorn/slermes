/*
 * transcribe_helpers.c — Helpers for tools/transcription_tools.py port.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <limits.h>

#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_logger.h"
#include "transcribe_helpers.h"

/* ================================================================
 *  Config / binary helpers
 * ================================================================ */

static const char *DEFAULT_STT_CONFIG = "{}";
static const char *DEFAULT_LOCAL_CMD_TEMPLATE = "whisper {input} --output_format json";

static void ascii_lower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static char *strip_dup(const char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* PoP: transcribe_default_stt_config @ tools/transcription_tools.py:_load_stt_config */
const char *transcribe_default_stt_config(void)
{
    return DEFAULT_STT_CONFIG;
}

/* PoP: transcribe_is_stt_enabled @ tools/transcription_tools.py:is_stt_enabled */
bool transcribe_is_stt_enabled(const char *config_json)
{
    if (!config_json || !config_json[0])
        return false;
    char *err = NULL;
    json_t *cfg = json_parse(config_json, &err);
    if (!cfg) {
        free(err);
        return false;
    }
    json_t *stt = json_object_get(cfg, "stt");
    if (stt && json_is_object(stt)) {
        bool dot_enabled = false;
        json_t *enabled = json_object_get(stt, "enabled");
        if (enabled && json_is_bool(enabled))
            dot_enabled = json_object_get_bool(stt, "enabled", false);
        json_free(cfg);
        free(err);
        return dot_enabled;
    }
    /* booleans / container nodes do not mean enabled */
    json_free(cfg);
    free(err);
    return false;
}

/* PoP: transcribe_find_binary @ tools/transcription_tools.py:_find_binary */
const char *transcribe_find_binary(const char *name)
{
    char buf[256];
    const char *paths[] = {"/usr/local/bin", "/usr/bin", "/bin", NULL};
    for (int i = 0; paths[i]; i++) {
        snprintf(buf, sizeof(buf), "%s/%s", paths[i], name);
        if (access(buf, X_OK) == 0)
            return strdup(buf);
    }
    return NULL;
}

/* PoP: transcribe_find_ffmpeg_binary @ tools/transcription_tools.py:_find_ffmpeg_binary */
const char *transcribe_find_ffmpeg_binary(void)
{
    return transcribe_find_binary("ffmpeg");
}

/* PoP: transcribe_find_whisper_binary @ tools/transcription_tools.py:_find_whisper_binary */
const char *transcribe_find_whisper_binary(void)
{
    return transcribe_find_binary("whisper");
}

/* PoP: transcribe_get_local_command_template @ tools/transcription_tools.py:_get_local_command_template */
const char *transcribe_get_local_command_template(void)
{
    return DEFAULT_LOCAL_CMD_TEMPLATE;
}

/* PoP: transcribe_has_local_command @ tools/transcription_tools.py:_has_local_command */
bool transcribe_has_local_command(const char *config_json)
{
    if (!config_json) return false;
    char *err = NULL;
    json_t *cfg = json_parse(config_json, &err);
    free(err);
    return cfg != NULL && json_object_get_string(cfg, "command", NULL) != NULL;
}

/* PoP: transcribe_resolve_command_stt_provider_config @ tools/transcription_tools.py:_resolve_command_stt_provider_config */
char *transcribe_resolve_command_stt_provider_config(const char *config_json, const char *provider_name)
{
    (void)provider_name;
    return config_json ? strdup(config_json) : NULL;
}

/* PoP: transcribe_get_named_stt_provider_config @ tools/transcription_tools.py:_get_named_stt_provider_config */
char *transcribe_get_named_stt_provider_config(const char *config_json, const char *name)
{
    if (!config_json || !name) return NULL;
    char *err = NULL;
    json_t *cfg = json_parse(config_json, &err);
    if (!cfg) { free(err); return NULL; }
    json_t *stt = json_object_get(cfg, "stt");
    char *result = NULL;
    if (stt && json_is_object(stt)) {
        json_t *prov = json_object_get(stt, name);
        if (prov) {
            char *s = json_dumps(prov, 0);
            result = s ? strdup(s) : NULL;
            free(s);
        }
    }
    json_free(cfg);
    free(err);
    return result;
}

/* ================================================================
 *  Text extraction / error classification
 * ================================================================ */

/* PoP: transcribe_extract_transcript_text @ tools/transcription_tools.py:_extract_transcript_text */
const char *transcribe_extract_transcript_text(const char *json_text)
{
    if (!json_text || !json_text[0])
        return "";
    char *err = NULL;
    json_t *root = json_parse(json_text, &err);
    if (!root)
        return "";
    const char *text = json_object_get_string(root, "text", "");
    if (!text || !text[0])
        text = json_object_get_string(root, "transcript", "");
    if (!text || !text[0])
        text = "";
    json_free(root);
    free(err);
    return text ? text : "";
}

/* PoP: transcribe_looks_like_cuda_lib_error @ tools/transcription_tools.py:_looks_like_cuda_lib_error */
bool transcribe_looks_like_cuda_lib_error(const char *msg)
{
    if (!msg) return false;
    const char *patterns[] = {
        "cudaErrorNoDevice",
        "CUDA driver version is insufficient",
        "libcuda.so",
        "nvidia-smi",
        "out of memory",
        "CUBLAS",
        NULL
    };
    for (int i = 0; patterns[i]; i++) {
        if (strcasestr(msg, patterns[i]))
            return true;
    }
    return false;
}

/* ================================================================
 *  OpenAI-compatible STT transcription helper
 * ================================================================ */

static char *build_multipart(const char *file_path,
                             const char *model, const char *lang,
                             char **out_content_type, size_t *out_len)
{
    if (!file_path || !out_content_type || !out_len) return NULL;
    FILE *f = fopen(file_path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_size <= 0) { fclose(f); return NULL; }

    unsigned int r;
    if (read(STDIN_FILENO, &r, sizeof(r)) != sizeof(r)) {
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
            r = (unsigned int)(ts.tv_nsec ^ (unsigned long)ts.tv_sec);
        else
            r = (unsigned int)getpid();
    }

    char boundary[64];
    snprintf(boundary, sizeof(boundary),
             "----HermesBoundary%08x", r % 0xffffffffU);

    size_t hdr1 = (size_t)snprintf(NULL, 0,
        "--%s\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n%s\r\n",
        boundary, model ? model : "");
    size_t hdr2 = 0;
    if (lang && lang[0])
        hdr2 = (size_t)snprintf(NULL, 0,
            "--%s\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\n%s\r\n",
            boundary, lang);
    const char *file_part_fmt =
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio\"\r\n"
        "Content-Type: application/octet-stream\r\n\r\n";
    size_t hdr3 = (size_t)snprintf(NULL, 0, file_part_fmt, boundary);
    const char *closing = "--\r\n";

    size_t total = hdr1 + hdr2 + hdr3 + (size_t)file_size + strlen(closing);
    char *buf = malloc(total + 1);
    if (!buf) { fclose(f); return NULL; }
    char *p = buf;
    p += snprintf(p, (size_t)(buf + total - p + 1),
                  "--%s\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n%s\r\n",
                  boundary, model ? model : "");
    if (lang && lang[0])
        p += snprintf(p, (size_t)(buf + total - p + 1),
                      "--%s\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\n%s\r\n",
                      boundary, lang);
    p += snprintf(p, (size_t)(buf + total - p + 1), file_part_fmt, boundary);
    size_t read_bytes = fread(p, 1, (size_t)file_size, f);
    p += read_bytes;
    fclose(f);
    memcpy(p, closing, strlen(closing));
    p[strlen(closing)] = '\0';

    size_t ct_len = (size_t)snprintf(NULL, 0,
                         "multipart/form-data; boundary=%s", boundary);
    *out_content_type = malloc(ct_len + 1);
    if (*out_content_type)
        snprintf(*out_content_type, ct_len + 1,
                 "multipart/form-data; boundary=%s", boundary);
    *out_len = (size_t)(p - buf) + strlen(closing);
    return buf;
}

/* PoP: transcribe_openai_compatible @ tools/transcription_tools.py:_transcribe_openai */
char *transcribe_openai_compatible(const char *url,
                                   const char *api_key,
                                   const char *file_path,
                                   const char *model,
                                   const char *language)
{
    if (!url || !api_key || !file_path)
        return NULL;

    char *payload = NULL;
    size_t payload_len = 0;
    char *content_type = NULL;
    payload = build_multipart(file_path,
                              model ? model : "whisper-1",
                              language,
                              &content_type,
                              &payload_len);
    if (!payload) {
        hermes_log(LOG_WARNING, "transcribe", "build multipart failed for %s", file_path);
        return NULL;
    }

    char auth[512];
    snprintf(auth, sizeof(auth),
             "Authorization: Bearer %s", api_key);

    http_client_t *client = http_client_new(120);
    if (!client) {
        free(payload);
        free(content_type);
        return NULL;
    }

    http_response_t *resp = NULL;
    if (content_type) {
        resp = http_request(client, HTTP_POST, url, auth, payload, payload_len);
    }
    free(payload);
    free(content_type);
    if (!client) {
        http_client_free(client);
        free(payload);
        free(content_type);
        return NULL;
    }
    if (!resp) {
        http_client_free(client);
        return NULL;
    }
    if (resp->status != 200) {
        char *err_json = NULL;
        if (resp->body && resp->body_len > 0) {
            err_json = malloc(resp->body_len + 1);
            if (err_json) {
                memcpy(err_json, resp->body, resp->body_len);
                err_json[resp->body_len] = '\0';
            }
        }
        http_response_free(resp);
        http_client_free(client);
        hermes_log(LOG_WARNING, "transcribe",
                   "openai-compatible STT failed: status=%d body=%s",
                   resp->status,
                   err_json ? err_json : "");
        free(err_json);
        return NULL;
    }

    char *out = NULL;
    if (resp->body && resp->body_len > 0) {
        out = malloc(resp->body_len + 1);
        if (out) {
            memcpy(out, resp->body, resp->body_len);
            out[resp->body_len] = '\0';
        }
    }
    http_response_free(resp);
    http_client_free(client);
    return out;
}

/* ================================================================
 *  Provider wrappers
 * ================================================================ */

/* PoP: transcribe_transcribe_groq @ tools/transcription_tools.py:_transcribe_groq */
char *transcribe_transcribe_groq(const char *file_path, const char *model)
{
    const char *api_key = getenv("GROQ_API_KEY");
    if (!api_key)
        return strdup("{\"success\":false,\"error\":\"GROQ_API_KEY not set\"}");
    return transcribe_openai_compatible(
        "https://api.groq.com/openai/v1/audio/transcriptions",
        api_key, file_path, model ? model : "whisper-large-v3-turbo", NULL);
}

/* PoP: transcribe_transcribe_mistral @ tools/transcription_tools.py:_transcribe_mistral */
char *transcribe_transcribe_mistral(const char *file_path, const char *model)
{
    const char *api_key = getenv("MISTRAL_API_KEY");
    if (!api_key)
        return strdup("{\"success\":false,\"error\":\"MISTRAL_API_KEY not set\"}");
    return transcribe_openai_compatible(
        "https://api.mistral.ai/v1/audio/transcriptions",
        api_key, file_path, model ? model : "voxtral-mini-latest", NULL);
}

/* PoP: transcribe_transcribe_xai @ tools/transcription_tools.py:_transcribe_xai */
char *transcribe_transcribe_xai(const char *file_path, const char *model)
{
    const char *api_key = getenv("XAI_API_KEY");
    if (!api_key)
        return strdup("{\"success\":false,\"error\":\"XAI_API_KEY not set\"}");
    return transcribe_openai_compatible(
        "https://api.x.ai/v1/audio/transcriptions",
        api_key, file_path, model ? model : "grok-stt", NULL);
}

/* PoP: transcribe_transcribe_elevenlabs @ tools/transcription_tools.py:_transcribe_elevenlabs */
char *transcribe_transcribe_elevenlabs(const char *file_path, const char *model)
{
    const char *api_key = getenv("ELEVENLABS_API_KEY");
    if (!api_key)
        return strdup("{\"success\":false,\"error\":\"ELEVENLABS_API_KEY not set\"}");
    (void)model;
    return transcribe_openai_compatible(
        "https://api.elevenlabs.io/v1/speech-to-text",
        api_key, file_path, "eleven_multilingual_v2", NULL);
}

/* PoP: transcribe_transcribe_deepinfra @ tools/transcription_tools.py:_transcribe_deepinfra */
char *transcribe_transcribe_deepinfra(const char *file_path, const char *model)
{
    const char *api_key = getenv("DEEPINFRA_API_KEY");
    if (!api_key)
        return strdup("{\"success\":false,\"error\":\"DEEPINFRA_API_KEY not set\"}");
    return transcribe_openai_compatible(
        "https://api.deepinfra.com/v1/audio/transcriptions",
        api_key, file_path, model ? model : "whisper-large-v3", NULL);
}

/* ================================================================
 *  Command-STT helpers
 * ================================================================ */

/* PoP: transcribe_validate_audio_file @ tools/transcription_tools.py:_validate_audio_file */
char *transcribe_validate_audio_file(const char *file_path)
{
    if (!file_path || !file_path[0])
        return strdup("{\"success\":false,\"error\":\"file_path is required\"}");
    struct stat st;
    if (stat(file_path, &st) != 0)
        return strdup("{\"success\":false,\"error\":\"File not found\"}");
    if (st.st_size == 0)
        return strdup("{\"success\":false,\"error\":\"File is empty\"}");
    return NULL; /* valid */
}

/* PoP: transcribe_get_stt_section @ tools/transcription_tools.py:_get_stt_section */
json_t *transcribe_get_stt_section(const char *config_json, const char *name)
{
    if (!config_json || !config_json[0])
        config_json = transcribe_default_stt_config();
    char *err = NULL;
    json_t *cfg = json_parse(config_json, &err);
    if (!cfg) { free(err); return NULL; }
    json_t *stt = json_object_get(cfg, "stt");
    if (!stt || !json_is_object(stt)) {
        json_free(cfg);
        free(err);
        return NULL;
    }
    json_t *section = json_object_get(stt, name);
    json_free(cfg);
    free(err);
    return section ? json_copy(section) : NULL;
}

/* PoP: transcribe_is_command_stt_provider_config @ tools/transcription_tools.py:_is_command_stt_provider_config */
bool transcribe_is_command_stt_provider_config(const char *config_json)
{
    if (!config_json)
        return false;
    char *err = NULL;
    json_t *cfg = json_parse(config_json, &err);
    if (!cfg) { free(err); return false; }
    bool result = false;
    json_t *cmd = json_object_get(cfg, "command");
    if (cmd && json_is_string(cmd))
        result = true;
    json_free(cfg);
    free(err);
    return result;
}

/* PoP: transcribe_get_command_stt_timeout @ tools/transcription_tools.py:_get_command_stt_timeout */
int transcribe_get_command_stt_timeout(const char *config_json)
{
    if (!config_json)
        return 300;
    char *err = NULL;
    json_t *cfg = json_parse(config_json, &err);
    if (!cfg) { free(err); return 300; }
    int result = json_object_get_number(cfg, "timeout", 300);
    json_free(cfg);
    free(err);
    return result;
}

/* PoP: transcribe_get_command_stt_output_format @ tools/transcription_tools.py:_get_command_stt_output_format */
const char *transcribe_get_command_stt_output_format(const char *config_json)
{
    if (!config_json)
        return "json";
    char *err = NULL;
    json_t *cfg = json_parse(config_json, &err);
    if (!cfg) { free(err); return "json"; }
    const char *fmt = json_object_get_string(cfg, "output_format", "json");
    json_free(cfg);
    free(err);
    return fmt;
}

/* PoP: transcribe_quote_command_placeholder @ tools/transcription_tools.py:_quote_command_stt_placeholder */
char *transcribe_quote_command_placeholder(const char *text)
{
    if (!text) return NULL;
    size_t need = strlen(text) * 2 + 3;
    char *out = malloc(need);
    if (!out) return NULL;
    snprintf(out, need, "\"%s\"", text);
    return out;
}

/* PoP: transcribe_terminate_process_tree @ tools/transcription_tools.py:_terminate_command_stt_process_tree */
bool transcribe_terminate_process_tree(int pid)
{
    if (pid <= 0) return true;
    if (kill(pid, SIGTERM) != 0) {
        if (kill(pid, SIGKILL) != 0)
            return false;
    }
    return true;
}

static char *make_cmd_temp_file(void)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/tmp/slermes-stt-XXXXXX");
    int fd = mkstemp(path);
    if (fd < 0) return NULL;
    close(fd);
    return strdup(path);
}

static int cp_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { fclose(in); fclose(out); return -1; }
    }
    fclose(in);
    fclose(out);
    return 0;
}

/* PoP: transcribe_transcribe_command_stt @ tools/transcription_tools.py:_transcribe_command_stt */
char *transcribe_transcribe_command_stt(const char *cmd_template,
                                        const char *file_path,
                                        const char *language)
{
    if (!cmd_template || !cmd_template[0])
        return strdup("{\"success\":false,\"error\":\"command_template is required for command STT\"}");
    if (!file_path)
        return strdup("{\"success\":false,\"error\":\"file_path is required\"}");

    char *temp_out = NULL;
    const char *input_arg = file_path;
    const char *lang_arg = language && language[0] ? language : NULL;

    if (!strstr(cmd_template, "{input}")) {
        temp_out = make_cmd_temp_file();
        if (!temp_out)
            return strdup("{\"success\":false,\"error\":\"failed to create temp output path\"}");
        if (cp_file(file_path, temp_out) != 0) {
            free(temp_out);
            return strdup("{\"success\":false,\"error\":\"failed to stage temp audio file\"}");
        }
        input_arg = temp_out;
    }

    size_t cmd_len = strlen(cmd_template) + 256;
    char *cmdline = malloc(cmd_len);
    if (!cmdline) { free(temp_out); return NULL; }
    snprintf(cmdline, cmd_len, cmd_template,
             input_arg, lang_arg ? lang_arg : "");

    int argc = 1;
    for (char *p = cmdline; *p; p++) if (*p == ' ') argc++;
    char **argv = malloc(sizeof(char*) * (argc + 1));
    if (!argv) { free(cmdline); free(temp_out); return NULL; }
    argc = 0;
    char *save = NULL;
    for (char *tok = strtok_r(cmdline, " ", &save); tok; tok = strtok_r(NULL, " ", &save))
        argv[argc++] = tok;
    argv[argc] = NULL;

    int out_pipe[2];
    if (pipe(out_pipe) != 0) { free(cmdline); free(argv); free(temp_out); return NULL; }
    int pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        free(cmdline); free(argv); free(temp_out);
        return strdup("{\"success\":false,\"error\":\"fork failed\"}");
    }
    if (pid == 0) {
        close(out_pipe[0]);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(out_pipe[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(out_pipe[1]);
    free(cmdline);
    free(argv);
    free(temp_out);
    char buf[4096];
    ssize_t got = 0, total = 0;
    char *out = malloc(65536);
    if (!out) { close(out_pipe[0]); return strdup("{\"success\":false,\"error\":\"oom\"}"); }
    while ((got = read(out_pipe[0], buf + total, sizeof(buf) - (size_t)total - 1)) > 0) total += got;
    buf[total] = '\0';
    memcpy(out, buf, (size_t)total + 1);
    close(out_pipe[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        if (!memchr(out, '{', (size_t)total)) {
            char buf2[65536];
            snprintf(buf2, sizeof(buf2), "{\"success\":false,\"error\":\"%s\",\"details\":\"%s\"}",
                     WIFSIGNALED(status) ? "process killed" : "process failed",
                     out);
            free(out);
            out = strdup(buf2);
        }
    }
    return out;
}
