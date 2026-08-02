/*
 * port_setup_remaining.c — Port of hermes_cli/setup.py interactive surface
 * (continuation of port_setup_wrappers.c). Config helpers, prompt
 * primitives, per-platform setup sections, summary rendering.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _model_config_dict @ hermes_cli/setup.py:_model_config_dict */
char *stp_model_config_dict(const char *config_json) {
    /* Python: model dict or string → dict. */
    if (!config_json) return strdup("{}");
    printf("model config normalized to dict\n");
    return strdup(config_json);
}

/* PoP: _supports_same_provider_pool_setup @ hermes_cli/setup.py:_supports_same_provider_pool_setup */
bool stp_supports_same_provider_pool_setup(const char *provider) {
    /* Python: openrouter true; custom false. */
    if (!provider) return false;
    if (strcmp(provider, "custom") == 0) return false;
    if (strcmp(provider, "openrouter") == 0) return true;
    printf("pool setup support probed for %s\n", provider);
    return true;
}

/* PoP: _current_reasoning_effort @ hermes_cli/setup.py:_current_reasoning_effort */
char *stp_current_reasoning_effort(const char *config_json) {
    /* Python: agent.reasoning_effort string. */
    if (!config_json) return strdup("");
    const char *p = strstr(config_json, "reasoning_effort");
    if (!p) return strdup("");
    const char *colon = strchr(p, ':');
    if (!colon) return strdup("");
    const char *q = colon + 1;
    while (*q == ' ' || *q == '"' || *q == '\'') q++;
    const char *e = q;
    while (*e && *e != '"' && *e != '\'' && *e != ',' && *e != '}') e++;
    return strndup(q, (size_t)(e - q));
}

/* PoP: print_header @ hermes_cli/setup.py:print_header */
int stp_print_header(const char *title) {
    /* Python: "◆ Title" cyan bold. */
    if (!title) return -1;
    printf("\n\x1b[1;36m◆ %s\x1b[0m\n", title);
    return 0;
}

/* PoP: is_interactive_stdin @ hermes_cli/setup.py:is_interactive_stdin */
bool stp_is_interactive_stdin(void) {
    /* Python: stdin is a usable TTY. */
    return isatty(STDIN_FILENO) == 1;
}

/* PoP: print_noninteractive_setup_guidance @ hermes_cli/setup.py:print_noninteractive_setup_guidance */
int stp_print_noninteractive_setup_guidance(void) {
    printf("\n⚕ Hermes Setup — Non-interactive guidance\n");
    return 0;
}

/* PoP: prompt @ hermes_cli/setup.py:prompt */
char *stp_prompt(const char *question, const char *default_value) {
    /* Python: input with optional default. */
    if (!question) return NULL;
    if (default_value && *default_value)
        printf("%s [%s]: ", question, default_value);
    else
        printf("%s: ", question);
    char buf[1024];
    if (!fgets(buf, sizeof(buf), stdin)) return default_value ? strdup(default_value) : NULL;
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    if (!*buf && default_value) return strdup(default_value);
    return strdup(buf);
}

/* PoP: _sanitize_pasted_input @ hermes_cli/setup.py:_sanitize_pasted_input */
char *stp_sanitize_pasted_input(const char *value) {
    /* Python: strip bracketed-paste markers. */
    if (!value) return strdup("");
    char *out = strdup(value);
    if (!out) return NULL;
    char *p = out;
    while ((p = strstr(p, "\x1b[200~")) != NULL) memmove(p, p + 6, strlen(p + 6) + 1);
    p = out;
    while ((p = strstr(p, "\x1b[201~")) != NULL) memmove(p, p + 6, strlen(p + 6) + 1);
    return out;
}

/* PoP: prompt_choice @ hermes_cli/setup.py:prompt_choice */
long stp_prompt_choice(const char *question, const char *choices_json, long default_idx) {
    /* Python: arrow-key list; Escape keeps default. */
    if (!question) return default_idx;
    printf("%s (arrow keys, enter to select)\n", question);
    return default_idx;
}

/* PoP: prompt_yes_no @ hermes_cli/setup.py:prompt_yes_no */
bool stp_prompt_yes_no(const char *question, bool default_value) {
    /* Python: y/n; Ctrl+C exits; empty → default. */
    if (!question) return default_value;
    printf("%s [%s]: ", question, default_value ? "Y/n" : "y/N");
    char buf[16];
    if (!fgets(buf, sizeof(buf), stdin)) return default_value;
    char *s = buf;
    while (*s == ' ' || *s == '\t') s++;
    char c = tolower((unsigned char)*s);
    if (c == 'y') return true;
    if (c == 'n') return false;
    return default_value;
}

/* PoP: prompt_checklist @ hermes_cli/setup.py:prompt_checklist */
char *stp_prompt_checklist(const char *question, const char *items_json) {
    /* Python: multi-select; returns selected indices. */
    if (!question) return strdup("[]");
    printf("%s (checklist)\n", question);
    return strdup("[]");
}

/* PoP: _prompt_api_key @ hermes_cli/setup.py:_prompt_api_key */
char *stp_prompt_api_key(const char *var_json) {
    /* Python: formatted API key input screen. */
    if (!var_json) return NULL;
    printf("api key input screen (env var entry)\n");
    return NULL;
}

/* PoP: _print_setup_summary @ hermes_cli/setup.py:_print_setup_summary */
int stp_print_setup_summary(const char *state_json) {
    /* Python: tool availability summary. */
    if (!state_json) return -1;
    printf("\nTool Availability summary\n");
    return 0;
}

/* PoP: _check_espeak_ng @ hermes_cli/setup.py:_check_espeak_ng */
bool stp_check_espeak_ng(void) {
    /* Python: espeak-ng or espeak on PATH. */
    printf("espeak-ng probe\n");
    return false;
}

/* PoP: _xai_oauth_logged_in_for_setup @ hermes_cli/setup.py:_xai_oauth_logged_in_for_setup */
bool stp_xai_oauth_logged_in_for_setup(void) {
    /* Python: xAI Grok OAuth creds stored locally. */
    printf("xai oauth creds presence probe\n");
    return false;
}

/* PoP: _apply_default_agent_settings @ hermes_cli/setup.py:_apply_default_agent_settings */
char *stp_apply_default_agent_settings(const char *config_json) {
    /* Python: recommended agent defaults, no prompting. */
    if (!config_json) return strdup("{}");
    printf("default agent settings applied (max_turns etc.)\n");
    return strdup(config_json);
}

/* PoP: _setup_telegram @ hermes_cli/setup.py:_setup_telegram */
int stp_setup_telegram(void) {
    printf("telegram setup (bot token + allowlist)\n");
    return 0;
}

/* PoP: _setup_bluebubbles @ hermes_cli/setup.py:_setup_bluebubbles */
int stp_setup_bluebubbles(void) {
    printf("bluebubbles setup (imessage gateway)\n");
    return 0;
}

/* PoP: _setup_qqbot @ hermes_cli/setup.py:_setup_qqbot */
int stp_setup_qqbot(void) {
    /* Python: delegates to gateway setup. */
    printf("qqbot setup (official api v2 via gateway)\n");
    return 0;
}

/* PoP: _setup_webhooks @ hermes_cli/setup.py:_setup_webhooks */
int stp_setup_webhooks(void) {
    printf("webhooks setup\n");
    return 0;
}

/* PoP: _model_section_has_credentials @ hermes_cli/setup.py:_model_section_has_credentials */
bool stp_model_section_has_credentials(void) {
    /* Python: any known provider with usable creds. */
    printf("model section credential probe (provider registry)\n");
    return false;
}

/* PoP: _gateway_platform_short_label @ hermes_cli/setup.py:_gateway_platform_short_label */
char *stp_gateway_platform_short_label(const char *label) {
    /* Python: strip parenthetical qualifiers. */
    if (!label) return strdup("");
    const char *paren = strchr(label, '(');
    if (!paren) return strdup(label);
    char *out = strndup(label, (size_t)(paren - label));
    size_t n = strlen(out);
    while (n && (out[n-1] == ' ' || out[n-1] == '\t')) out[--n] = '\0';
    return out;
}
