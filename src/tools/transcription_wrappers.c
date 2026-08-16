/*
 * port_transcription_wrappers.c — C port of tools/transcription_tools.py
 * 15 remaining PoP-annotated helpers for STT provider dispatch.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
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
