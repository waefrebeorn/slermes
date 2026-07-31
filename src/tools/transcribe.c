/*
 * transcribe.c — Audio transcription tool for Hermes C.
 * Wraps lib/libtranscribe/ as a registered tool.
 * Port of Python tools/transcription_tools.py tool registration.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_transcription.h"
#include "transcribe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"file_path\":{\"type\":\"string\",\"description\":\"Absolute path to audio file (mp3, wav, ogg, m4a, etc.)\"},"
      "\"model\":{\"type\":\"string\",\"description\":\"Provider model name, e.g. 'whisper-large-v3-turbo' (groq), 'whisper-1' (openai), 'grok-stt' (xai). Omit for auto-detect.\"},"
      "\"language\":{\"type\":\"string\",\"description\":\"Language code (e.g., 'en', 'fr', 'de', 'zh'). Improves accuracy for known language.\"}"
    "},"
    "\"required\":[\"file_path\"]"
"}";

char *transcribe_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"success\":false,\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) {
        free(err);
        return strdup("{\"success\":false,\"error\":\"JSON parse error\"}");
    }

    const char *file_path = json_object_get_string(args, "file_path", NULL);
    if (!file_path || !*file_path) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"file_path is required\"}");
    }

    const char *model = json_object_get_string(args, "model", NULL);
    /* Validate file first */
    char *validation = transcribe_validate_file(file_path);
    if (validation) {
        json_free(args);
        return validation; /* already JSON */
    }

    /* Transcribe */
/* PoP: transcribe_audio @ tools/transcription_tools.py:transcribe_audio */
    char *result = transcribe_audio(file_path, model);
    json_free(args);

    if (!result) {
        return strdup("{\"success\":false,\"error\":\"Transcription failed (no result)\"}");
    }

    return result;
}

__attribute__((constructor))
static void init_transcribe(void) {
    registry_register("transcribe", "Transcribe audio to text using Whisper (groq/openai/xai).",
                       SCHEMA, transcribe_handler);
}

/* Called from tool_init.c — registers the transcribe tool name.
 * The constructor above handles the actual registration, but tool_init.c
 * also calls registry_init_transcribe() which needs a definition. */
void registry_init_transcribe_impl(void) {
    /* Already registered via constructor — nothing to do here.
     * This function exists so tool_init.c can link. */
}

/* ================================================================
 *  Transcription Provider Registry (port of Python transcription_registry.py)
 * ================================================================ */

#define MAX_TRANSCRIPTION_PROVIDERS 32

/* Built-in provider names that plugins cannot override */
static const char *BUILTIN_STT_NAMES[] = {
    "local", "local_command", "groq", "openai", "mistral", "xai", NULL
};

static struct {
    char name[64];
    char provider_type[64];
} g_trans_providers[MAX_TRANSCRIPTION_PROVIDERS];
static int g_trans_provider_count = 0;

/* Register a transcription provider. Returns true on success.
 * Rejects built-in names (logs warning, returns false).
 * Port of Python transcription_registry.py:register_provider(). */
bool transcription_register_provider(const char *name, const char *provider_type) {
    if (!name || !name[0]) return false;

    /* Normalize: lowercase, strip */
    char key[64];
    size_t j = 0;
    for (const char *p = name; *p && j < sizeof(key) - 1; p++) {
        if (*p >= 'A' && *p <= 'Z')
            key[j++] = (char)(*p + 32);
        else if (*p != ' ')
            key[j++] = *p;
    }
    key[j] = '\0';
    if (!key[0]) return false;

    /* Check built-in names */
    for (int i = 0; BUILTIN_STT_NAMES[i]; i++) {
        if (strcmp(key, BUILTIN_STT_NAMES[i]) == 0) {
            fprintf(stderr, "[transcribe] Warning: provider '%s' shadows built-in name '%s' — ignored\n",
                    name, BUILTIN_STT_NAMES[i]);
            return false;
        }
    }

    /* Check for existing and overwrite */
    for (int i = 0; i < g_trans_provider_count; i++) {
        if (strcmp(g_trans_providers[i].name, key) == 0) {
            snprintf(g_trans_providers[i].provider_type, sizeof(g_trans_providers[i].provider_type),
                     "%s", provider_type ? provider_type : "");
            return true;
        }
    }

    /* Add new entry */
    if (g_trans_provider_count >= MAX_TRANSCRIPTION_PROVIDERS) return false;
    snprintf(g_trans_providers[g_trans_provider_count].name, sizeof(g_trans_providers[g_trans_provider_count].name), "%s", key);
    snprintf(g_trans_providers[g_trans_provider_count].provider_type, sizeof(g_trans_providers[g_trans_provider_count].provider_type),
             "%s", provider_type ? provider_type : "");
    g_trans_provider_count++;
    return true;
}

/* List registered provider names. Returns comma-separated malloc'd string (caller free). */
/* Port of Python agent/transcription_registry.py:list_providers(). */
char *transcription_list_providers(void) {
    if (g_trans_provider_count == 0) return strdup("");
    size_t total = 0;
    for (int i = 0; i < g_trans_provider_count; i++)
        total += strlen(g_trans_providers[i].name) + 2;
    char *buf = (char *)malloc(total + 1);
    if (!buf) return strdup("");
    buf[0] = '\0';
    for (int i = 0; i < g_trans_provider_count; i++) {
        if (i > 0) strcat(buf, ", ");
        strcat(buf, g_trans_providers[i].name);
    }
    return buf;
}

/* Get a registered provider by name (case-insensitive). Returns provider type or NULL. */
/* Port of Python agent/transcription_registry.py:get_provider(). */
const char *transcription_get_provider(const char *name) {
    if (!name) return NULL;
    char key[64];
    size_t j = 0;
    for (const char *p = name; *p && j < sizeof(key) - 1; p++) {
        if (*p >= 'A' && *p <= 'Z')
            key[j++] = (char)(*p + 32);
        else if (*p != ' ')
            key[j++] = *p;
    }
    key[j] = '\0';
    for (int i = 0; i < g_trans_provider_count; i++) {
        if (strcmp(g_trans_providers[i].name, key) == 0)
            return g_trans_providers[i].provider_type;
    }
    return NULL;
}

/* Clear all registered providers. Test-only. */
/* Port of Python agent/transcription_registry.py:_reset_for_tests(). */
void transcription_reset_for_tests(void) {
    g_trans_provider_count = 0;
}
