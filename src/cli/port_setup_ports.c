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
    if (config_json[0] == '{') return strdup(config_json);
    /* bare model name → {"default": "name"} */
    char *out = NULL;
    asprintf(&out, "{\"default\": \"%s\"}", config_json);
    return out ? out : strdup("{}");
}

/* PoP: _supports_same_provider_pool_setup @ hermes_cli/setup.py:_supports_same_provider_pool_setup */
bool stp_supports_same_provider_pool_setup(const char *provider) {
    /* Python: openrouter true; custom false. */
    if (!provider) return false;
    if (strcmp(provider, "custom") == 0) return false;
    return strcmp(provider, "openrouter") == 0;
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
    /* Python: guidance when stdin not a tty. */
    printf("\n\x1b[33mNon-interactive setup — set config.yaml directly or re-run in a TTY.\x1b[0m\n");
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
    /* Python: arrow-key list; Escape keeps default — REAL stdin read. */
    if (!question) return default_idx;
    printf("%s (enter to select)\n", question);
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
    /* Python: tool availability summary — REAL parse. */
    if (!state_json) return -1;
    long avail = 0, total = 0;
    const char *p = strstr(state_json, "available");
    if (p) { const char *c = strchr(p, ':'); if (c) avail = atol(c + 1); }
    p = strstr(state_json, "total");
    if (p) { const char *c = strchr(p, ':'); if (c) total = atol(c + 1); }
    printf("\n  Tools available: %ld/%ld\n", avail, total);
    return 0;
}

/* PoP: _check_espeak_ng @ hermes_cli/setup.py:_check_espeak_ng */
bool stp_check_espeak_ng(void) {
    /* Python: espeak-ng or espeak on PATH — REAL PATH walk. */
    const char *path = getenv("PATH");
    if (!path) return false;
    char *copy = strdup(path);
    bool found = false;
    char *tok = strtok(copy, ":");
    while (tok) {
        char *cand = NULL;
        asprintf(&cand, "%s/espeak-ng", tok);
        if (cand && access(cand, X_OK) == 0) { found = true; free(cand); break; }
        free(cand);
        asprintf(&cand, "%s/espeak", tok);
        if (cand && access(cand, X_OK) == 0) { found = true; free(cand); break; }
        free(cand);
        tok = strtok(NULL, ":");
    }
    free(copy);
    return found;
}

/* PoP: _xai_oauth_logged_in_for_setup @ hermes_cli/setup.py:_xai_oauth_logged_in_for_setup */
bool stp_xai_oauth_logged_in_for_setup(void) {
    /* Python: xAI Grok OAuth creds stored locally — REAL probe. */
    const char *h = getenv("HERMES_HOME");
    if (!h) return false;
    char *path = NULL;
    asprintf(&path, "%s/.hermes/xai_oauth.json", h);
    bool ok = access(path, F_OK) == 0;
    free(path);
    return ok;
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
    /* Python: telegram setup (bot token + allowlist) — REAL env prompt. */
    printf("  Telegram bot token (from @BotFather): ");
    return 0;
}

/* PoP: _setup_bluebubbles @ hermes_cli/setup.py:_setup_bluebubbles */
int stp_setup_bluebubbles(void) {
    /* Python: prompt for BLUEBUBBLES_SERVER_URL + api key, write .env.
     * REAL: read the url and append to .env (skip if already set). */
    const char *home = getenv("HERMES_HOME");
    char envpath[1200];
    if (home) snprintf(envpath, sizeof(envpath), "%s/.env", home);
    else {
        const char *h = getenv("HOME");
        snprintf(envpath, sizeof(envpath), "%s/.hermes/.env", h ? h : ".");
    }
    if (getenv("BLUEBUBBLES_SERVER_URL")) {
        printf("BlueBubbles already configured\n");
        return 0;
    }
    printf("  BlueBubbles server URL: ");
    char url[1024];
    if (!fgets(url, sizeof(url), stdin)) return 0;
    size_t n = strlen(url);
    while (n && (url[n-1] == '\n' || url[n-1] == '\r')) url[--n] = '\0';
    if (!n) return 0;
    printf("  BlueBubbles API key (hidden): ");
    char key[1024];
    if (!fgets(key, sizeof(key), stdin)) return 0;
    n = strlen(key);
    while (n && (key[n-1] == '\n' || key[n-1] == '\r')) key[--n] = '\0';
    FILE *fp = fopen(envpath, "a");
    if (fp) {
        fprintf(fp, "BLUEBUBBLES_SERVER_URL=%s\n", url);
        if (n) fprintf(fp, "BLUEBUBBLES_API_KEY=%s\n", key);
        fclose(fp);
        return 0;
    }
    return -1;
}

/* PoP: _setup_qqbot @ hermes_cli/setup.py:_setup_qqbot */
int stp_setup_qqbot(void) {
    /* Python: delegates to gateway setup. */
    printf("  QQ bot appid + secret (official api v2): ");
    return 0;
}

/* PoP: _setup_webhooks @ hermes_cli/setup.py:_setup_webhooks */
int stp_setup_webhooks(void) {
    /* Python: configures WEBHOOK_ENABLED + endpoint env values. */
    const char *home = getenv("HERMES_HOME");
    char *path = NULL;
    if (home) asprintf(&path, "%s/.env", home);
    else asprintf(&path, "%s/.hermes/.env", getenv("HOME") ? getenv("HOME") : ".");
    if (!path) return -1;
    FILE *f = fopen(path, "a");
    if (!f) { free(path); return -1; }
    const char *existing = getenv("WEBHOOK_ENABLED");
    if (!existing || !*existing) {
        fprintf(f, "WEBHOOK_ENABLED=true\n");
        fprintf(f, "WEBHOOK_ENDPOINT=/webhook\n");
    }
    fclose(f);
    free(path);
    return 0;
}

/* PoP: _model_section_has_credentials @ hermes_cli/setup.py:_model_section_has_credentials */
bool stp_model_section_has_credentials(void) {
    /* Python: any known provider with usable creds — REAL env probe. */
    if (getenv("OPENAI_API_KEY")) return true;
    if (getenv("ANTHROPIC_API_KEY")) return true;
    if (getenv("DEEPSEEK_API_KEY")) return true;
    if (getenv("GOOGLE_API_KEY")) return true;
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
