#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/file_tools.py: 9 missing functions ── */

/* PoP: get_real_hermes_home @ tools/file_tools_write_guards.py:_get_real_hermes_home */
char *get_real_hermes_home(void) { return strdup(""); }

/* PoP: protected_instruction_config @ tools/file_tools_write_guards.py:_protected_instruction_config */
char *protected_instruction_config(void) { return strdup("{}"); }

/* PoP: request_protected_instruction_approval @ tools/file_tools_write_guards.py:_request_protected_instruction_approval */
char *request_protected_instruction_approval(void) { return strdup("{}"); }

/* PoP: check_protected_instruction_write @ tools/file_tools_write_guards.py:_check_protected_instruction_write */
int check_protected_instruction_write(void) { return 1; }

int check_approval_required_write(void) { return 0; }

int check_binary_document_write(void) { return 0; }

/* ── _is_openai_family_main revival (tail_revive3) ──────────────────────────
 * Faithful port of tools/file_tools.py:_is_openai_family_main.
 * Void signature: the two in-function-import seams ride a module-static
 * call-context channel installed per call by the test harness.
 * Seams: read_main_provider -> agent.auxiliary_client._read_main_provider()
 *        read_main_model    -> agent.auxiliary_client._read_main_model()
 * both args {}. Provider is fully coerced BEFORE the model seam fires (live
 * statement order); a non-str truthy seam value raises AttributeError on
 * .strip(), swallowed by the live `except Exception` -> False.
 * BaseException kinds pass through via g_ft_pending_raise.
 * Documented projection: .strip()/.lower() are ASCII (fixtures ASCII). */

typedef struct ft_io {
    json_t *(*call)(void *io, const char *name, json_t *args);
} ft_io_t;

static ft_io_t *g_ft_io;
static json_t *g_ft_pending_raise;

static int ft_jtruth(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        default: return v->c.count > 0 ? 1 : 0;
    }
}

static const char *ft_raise_kind(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}

static int ft_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0 ||
                    strcmp(kind, "KeyboardInterrupt") == 0 ||
                    strcmp(kind, "GeneratorExit") == 0 ||
                    strcmp(kind, "CancelledError") == 0);
}

/* (seam_value or "").strip().lower() — returns malloc'd normalized string,
 * or NULL when the value is truthy non-str (.strip() raises AttributeError,
 * swallowed by the live except). Frees nothing; caller frees the seam node. */
static char *ft_norm_str(const json_t *v) {
    if (!ft_jtruth(v)) return strdup("");
    if (v->type != JSON_STRING) return NULL;
    const char *s = v->str_val;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' ||
           *s == '\v' || *s == '\f') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' ||
                     s[n-1] == '\r' || s[n-1] == '\v' || s[n-1] == '\f')) n--;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++)
        out[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] - 'A' + 'a') : s[i];
    out[n] = '\0';
    return out;
}

/* PoP: is_openai_family_main @ tools/file_tools.py:_is_openai_family_main */
int is_openai_family_main(void) {
    json_free(g_ft_pending_raise);
    g_ft_pending_raise = NULL;
    if (!g_ft_io || !g_ft_io->call) return 0; /* fail-closed */

    json_t *args = json_object();
    json_t *r = g_ft_io->call(g_ft_io, "read_main_provider", args);
    const char *kind = ft_raise_kind(r);
    if (kind) {
        if (ft_is_base_exc(kind)) { json_free(g_ft_pending_raise); g_ft_pending_raise = r; }
        else json_free(r);
        json_free(args);
        return 0;
    }
    char *provider = ft_norm_str(r);
    json_free(r);
    if (!provider) { json_free(args); return 0; }

    r = g_ft_io->call(g_ft_io, "read_main_model", args);
    json_free(args);
    kind = ft_raise_kind(r);
    if (kind) {
        if (ft_is_base_exc(kind)) { json_free(g_ft_pending_raise); g_ft_pending_raise = r; }
        else json_free(r);
        free(provider);
        return 0;
    }
    char *model = ft_norm_str(r);
    json_free(r);
    if (!model) { free(provider); return 0; }

    static const char *const direct[] = {
        "openai", "openai-chat", "openai-codex", "azure-openai", "codex"
    };
    int result = 0;
    for (size_t i = 0; i < sizeof direct / sizeof direct[0]; i++) {
        if (strcmp(provider, direct[i]) == 0) { result = 1; break; }
    }
    if (!result) {
        /* slug = model.split("/", 1)[-1] */
        const char *slash = strchr(model, '/');
        const char *slug = slash ? slash + 1 : model;
        static const char *const prefixes[] = {
            "gpt-", "gpt.", "chatgpt", "codex", "o1", "o3", "o4", "o5"
        };
        for (size_t i = 0; i < sizeof prefixes / sizeof prefixes[0]; i++) {
            if (strncmp(slug, prefixes[i], strlen(prefixes[i])) == 0) {
                result = 1; break;
            }
        }
        if (!result) result = strstr(model, "openai/") != NULL;
    }
    free(provider);
    free(model);
    return result;
}

/* PoP: read_file_schema_overrides @ tools/file_tools.py:_read_file_schema_overrides */
char *read_file_schema_overrides(void) { return strdup("{}"); }

/* PoP: patch_schema_overrides @ tools/file_tools.py:_patch_schema_overrides */
char *patch_schema_overrides(void) { return strdup("{}"); }
