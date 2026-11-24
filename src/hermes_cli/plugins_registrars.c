/*
 * plugins_registrars.c — C11 port of upstream Hermes hermes_cli/plugins_registrars.py.
 *
 * PoP: w17_pluginsreg_e9d267_normalize @ hermes_cli/plugins_registrars.py:_NAME_NORMALIZERS
 * PoP: w17_pluginsreg_e9d267_row       @ hermes_cli/plugins_registrars.py:_ROWS_BY_METHOD
 *
 * Ports the pure, data-driven surface: the three name normalizers
 * (strip / strip+lower / raw) and the scoped-provider registrar table keyed by
 * method name. The static table mirrors _SCOPED_PROVIDER_REGISTRARS exactly for
 * the fields the shared body reads (kind, registry module, base ref, label,
 * normalize mode and article). All symbols are prefixed with the gate key.
 */
#include "noc_w17_pluginsreg_e9d267.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

struct json_t *w17_pluginsreg_e9d267_normalize(const char *name, const char *mode) {
    if (!name) return json_null();
    if (!strcmp(mode, "none")) return json_string(name);  /* _NAME_NORMALIZERS[None] is None -> raw */
    if (strcmp(mode, "strip") && strcmp(mode, "lower")) return json_string(name);

    char *s = dupstr(name);
    if (!s) return json_null();
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) *--end = '\0';

    if (!strcmp(mode, "lower")) {
        for (char *p = start; *p; p++) *p = (char)tolower((unsigned char)*p);
    }
    json_t *out = json_string(start);
    free(s);
    return out;
}

typedef struct {
    const char *method;
    const char *kind;
    const char *registry_mod;
    const char *base_ref;
    const char *label;
    const char *normalize;   /* NULL == raw (options normalize=None) */
    const char *article;
} RegRow;

/* Mirrors _SCOPED_PROVIDER_REGISTRARS, in declaration order. */
static const RegRow ROWS[] = {
    {"register_login_backend", "login_backend", "agent.vault_backend_registry",
     "agent.vault_backends.base:LoginBackend", "login backend", NULL, "a"},
    {"register_browser_page_backend", "browser_page_backend", "agent.browser_page_registry",
     "agent.browser_page_backend:BrowserPageBackend", "browser page backend", "strip", "a"},
    {"register_image_gen_provider", "image_gen_provider", "agent.image_gen_registry",
     "agent.image_gen_provider:ImageGenProvider", "image_gen provider", "strip", "an"},
    {"register_video_gen_provider", "video_gen_provider", "agent.video_gen_registry",
     "agent.video_gen_provider:VideoGenProvider", "video_gen provider", "strip", "a"},
    {"register_web_search_provider", "web_search_provider", "agent.web_search_registry",
     "agent.web_search_provider:WebSearchProvider", "web provider", "strip", "a"},
    {"register_browser_provider", "browser_provider", "agent.browser_registry",
     "agent.browser_provider:BrowserProvider", "browser provider", "strip", "a"},
    {"register_terminal_environment_provider", "terminal_environment_provider", "agent.terminal_env_registry",
     "agent.terminal_env_provider:TerminalEnvironmentProvider", "terminal environment provider", "lower", "a"},
    {"register_secret_source", "secret_source", "agent.secret_sources.registry",
     "agent.secret_sources.base:SecretSource", "secret source", NULL, "a"},
    {"register_tts_provider", "tts_provider", "agent.tts_registry",
     "agent.tts_provider:TTSProvider", "TTS provider", "lower", "a"},
    {"register_transcription_provider", "transcription_provider", "agent.transcription_registry",
     "agent.transcription_provider:TranscriptionProvider", "transcription provider", "lower", "a"},
    {"register_delegation_provider", "delegation_provider", "agent.delegation_registry",
     "agent.delegation_provider:DelegationProvider", "delegation provider", "lower", "a"},
};
static const size_t NROWS = sizeof(ROWS) / sizeof(ROWS[0]);

struct json_t *w17_pluginsreg_e9d267_row(const char *method) {
    if (!method) return json_null();
    for (size_t i = 0; i < NROWS; i++) {
        if (strcmp(ROWS[i].method, method) == 0) {
            json_t *o = json_object();
            json_set(o, "kind", json_string(ROWS[i].kind));
            json_set(o, "registry_mod", json_string(ROWS[i].registry_mod));
            json_set(o, "base_ref", json_string(ROWS[i].base_ref));
            json_set(o, "label", json_string(ROWS[i].label));
            json_set(o, "normalize", ROWS[i].normalize ? json_string(ROWS[i].normalize) : json_null());
            json_set(o, "article", json_string(ROWS[i].article));
            return o;
        }
    }
    return json_null();
}
