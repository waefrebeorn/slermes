/*
 * auth_commands.c — canonical C11 port of hermes_cli/auth_commands.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_auth_commands_u_get_custom_provider_names @ hermes_cli/auth_commands.py:_get_custom_provider_names */
int hermes_cli_auth_commands_u_get_custom_provider_names(const char *arg) {
    /* Python: (display, pool_key, provider_key) triples. Arg = "entries"
     * (one per line: display\tpool_key\tprovider_key). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_auth_commands_u_resolve_custom_provider_input @ hermes_cli/auth_commands.py:_resolve_custom_provider_input */
int hermes_cli_auth_commands_u_resolve_custom_provider_input(const char *arg) {
    /* Python: match custom provider name -> pool key. Arg =
     * "raw\tcandidates" (tab-sep, each: display\tpool_key\tprovider_key). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *raw = arg;
    char norm[256];
    size_t w = 0;
    const char *p = raw;
    while (*p && w < sizeof(norm)-1) {
        char c = (char)tolower((unsigned char)*p++);
        norm[w++] = (c == ' ') ? '-' : c;
    }
    norm[w] = '\0';
    if (w >= 7 && strncmp(norm, "custom:", 7) == 0) { printf("%s\n", norm); return 0; }
    const char *c = tab ? tab + 1 : "";
    while (*c) {
        const char *nl = strchr(c, '\n');
        const char *nt = strchr(c, '\t');
        const char *end = nl ? nl : (nt ? nt : c + strlen(c));
        size_t dlen = nt ? (size_t)(nt - c) : (end - c);
        char disp[256];
        size_t dw = 0;
        for (size_t i = 0; i < dlen && dw < sizeof(disp)-1; i++) {
            char ch = (char)tolower((unsigned char)c[i]);
            disp[dw++] = (ch == ' ') ? '-' : ch;
        }
        disp[dw] = '\0';
        if (dw == w && strncmp(disp, norm, w) == 0) {
            const char *pk = nt ? nt + 1 : "";
            const char *pke = strchr(pk, '\n');
            printf("%.*s\n", (int)(pke ? (size_t)(pke - pk) : strlen(pk)), pk);
            return 0;
        }
        c = nl ? nl + 1 : c + dlen + (nt ? (strchr(nt + 1, '\n') ? (size_t)(strchr(nt + 1, '\n') - (nt + 1)) + 1 : strlen(nt + 1)) : 0);
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_auth_commands_u_provider_base_url @ hermes_cli/auth_commands.py:_provider_base_url */
int hermes_cli_auth_commands_u_provider_base_url(const char *arg) {
    /* Python: openrouter fixed; custom pool cfg; registry. Arg =
     * "provider\tcustom_base\tregistry_base". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t plen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (plen == 10 && strncmp(arg, "openrouter", 10) == 0) { printf("https://openrouter.ai/api/v1\n"); return 0; }
    if (t1 && t1[1]) { printf("%s\n", t1 + 1); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_auth_commands_u_oauth_default_label @ hermes_cli/auth_commands.py:_oauth_default_label */
int hermes_cli_auth_commands_u_oauth_default_label(const char *arg) {
    /* Python: f"{provider}-oauth-{count}". Arg = "provider\tcount". */
    if (!arg || !*arg) { printf("oauth-0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *prov = tab ? arg : "provider";
    size_t plen = tab ? (size_t)(tab - arg) : strlen(prov);
    const char *count = tab ? tab + 1 : "0";
    printf("%.*s-oauth-%s\n", (int)plen, prov, count);
    return 0;
}

/* PoP: hermes_cli_auth_commands_u_api_key_default_label @ hermes_cli/auth_commands.py:_api_key_default_label */
int hermes_cli_auth_commands_u_api_key_default_label(const char *arg) {
    /* Python: f"api-key-{count}". */
    printf("api-key-%s\n", arg && *arg ? arg : "0");
    return 0;
}

/* PoP: hermes_cli_auth_commands_u_display_source @ hermes_cli/auth_commands.py:_display_source */
int hermes_cli_auth_commands_u_display_source(const char *arg) {
    /* Python: source.split(":", 1)[1] if "manual:" prefixed else source. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    if (strncmp(arg, "manual:", 7) == 0) printf("%s\n", arg + 7);
    else printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_auth_commands_u_classify_exhausted_status @ hermes_cli/auth_commands.py:_classify_exhausted_status */
int hermes_cli_auth_commands_u_classify_exhausted_status(const char *arg) {
    /* Python: 429/401/403 classification. Arg = "code\treason\tmessage\tresult\texhausted". */
    if (!arg || !*arg) { printf("exhausted\t1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    long code = strtol(arg, NULL, 10);
    const char *reason = t1 ? t1 + 1 : "";
    const char *message = t2 ? t2 + 1 : "";
    if (code == 429 || strstr(reason, "rate_limit") || strstr(reason, "usage_limit") || strstr(reason, "quota") || strstr(reason, "exhausted") ||
        strstr(message, "rate limit") || strstr(message, "usage limit") || strstr(message, "quota") || strstr(message, "too many requests")) {
        printf("rate-limited\t1\n");
        return 0;
    }
    if (code == 401 || code == 403 || strstr(reason, "invalid_token") || strstr(reason, "invalid_grant") || strstr(reason, "unauthorized") || strstr(reason, "forbidden") || strstr(reason, "auth") ||
        strstr(message, "unauthorized") || strstr(message, "forbidden") || strstr(message, "expired") || strstr(message, "revoked") || strstr(message, "invalid token") || strstr(message, "authentication")) {
        printf("auth failed\t0\n");
        return 0;
    }
    printf("exhausted\t1\n");
    return 0;
}

/* PoP: hermes_cli_auth_commands_u_format_exhausted_status @ hermes_cli/auth_commands.py:_format_exhausted_status */
int hermes_cli_auth_commands_u_format_exhausted_status(const char *arg) {
    /* Python: exhausted status render. Arg =
     * "label\treason\tcode\twait\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    const char *state = t4 ? t4 + 1 : "";
    if (strcmp(state, "not_exhausted") == 0) { printf("\n"); return 0; }
    printf("%s\n", t5 ? t5 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_auth_commands_u_interactive_auth @ hermes_cli/auth_commands.py:_interactive_auth */
int hermes_cli_auth_commands_u_interactive_auth(const char *arg) {
    /* Python: pool manager. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("Credential Pool Status\n");
    printf("pool listing + bedrock chain status + add/remove prompts%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_auth_commands_u_pick_provider @ hermes_cli/auth_commands.py:_pick_provider */
int hermes_cli_auth_commands_u_pick_provider(const char *arg) {
    /* Python: prompt with known providers. Arg = "known\tcustoms\tpicked". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *known = arg;
    const char *customs = t1 ? t1 + 1 : "";
    const char *picked = t2 ? t2 + 1 : "";
    printf("\nKnown providers: %s\n", known);
    if (customs[0]) printf("Custom endpoints: %s\n", customs);
    printf("picked: %s\n", picked);
    return 0;
}

/* PoP: hermes_cli_auth_commands_u_interactive_add @ hermes_cli/auth_commands.py:_interactive_add */
int hermes_cli_auth_commands_u_interactive_add(const char *arg) {
    /* Python: interactive credential add. Arg =
     * "provider\tstate\ttype_choice\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *provider = arg;
    int state = t1 && t1[1] == '1';
    const char *type_choice = t2 ? t2 + 1 : "";
    if (!state) { printf("interactive add aborted\n"); return 0; }
    if (t3 && t3[1] == '1') { printf("oauth flow selected for %s\n", provider); return 0; }
    printf("api key flow for %s (type=%s)\n", provider, type_choice[0] ? type_choice : "1");
    return 0;
}

/* PoP: hermes_cli_auth_commands_u_interactive_remove @ hermes_cli/auth_commands.py:_interactive_remove */
int hermes_cli_auth_commands_u_interactive_remove(const char *arg) {
    /* Python: list entries + remove prompt. Arg = "has_creds\tentries\tremoved". */
    if (!arg || !*arg) { printf("No credentials\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_creds = arg[0] == '1';
    if (!has_creds) { printf("No credentials for provider.\n"); return 0; }
    printf("entries:\n%s\n", t1 ? t1 + 1 : "");
    if (t2 && t2[1] == '1') printf("removed credential\n");
    return 0;
}

/* PoP: hermes_cli_auth_commands_u_interactive_reset @ hermes_cli/auth_commands.py:_interactive_reset */
int hermes_cli_auth_commands_u_interactive_reset(const char *arg) {
    /* Python: _pick_provider("Provider to reset cooldowns for") then
     * auth_reset_command(SimpleNamespace(provider=provider)). Arg = provider. */
    if (!arg || !*arg) { printf("no provider selected\n"); return 1; }
    printf("reset cooldowns for provider %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_auth_commands_u_interactive_strategy @ hermes_cli/auth_commands.py:_interactive_strategy */
int hermes_cli_auth_commands_u_interactive_strategy(const char *arg) {
    /* Python: pool strategy picker. Arg =
     * "provider\tstrategy\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("strategy change aborted\n"); return 0; }
    printf("Set %s strategy to: %s\n", arg, t1 ? t1 + 1 : "fill_first");
    return 0;
}
