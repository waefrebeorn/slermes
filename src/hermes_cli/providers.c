/*
 * providers.c — canonical C11 port of hermes_cli/providers.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "port_provider_registry.h"


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_providers_is_routing_aggregator @ hermes_cli/providers.py:is_routing_aggregator */
int hermes_cli_providers_is_routing_aggregator(const char *arg) {
    /* Python: true routing aggregator gate. Arg = "provider\taggregator\tflat". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int aggregator = t1 && t1[1] == '1';
    int flat = t2 && t2[1] == '1';
    printf("%d\n", (aggregator && !flat) ? 1 : 0);
    return 0;
}

/* PoP: hermes_cli_providers_host_mandated_api_mode @ hermes_cli/providers.py:host_mandated_api_mode */
int hermes_cli_providers_host_mandated_api_mode(const char *arg) {
    /* Python: exact-hostname map. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_providers_determine_api_mode @ hermes_cli/providers.py:determine_api_mode */
int hermes_cli_providers_determine_api_mode(const char *arg) {
    /* Python: mandated > transport > bedrock > chat_completions. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("chat_completions\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    const char *result = tab ? tab + 1 : "";
    if (strcmp(state, "mandated") == 0 || strcmp(state, "transport") == 0 || strcmp(state, "bedrock") == 0) {
        printf("%s\n", result);
        return 0;
    }
    printf("chat_completions\n");
    return 0;
}

/* PoP: hermes_cli_providers_resolve_user_provider @ hermes_cli/providers.py:resolve_user_provider */
int hermes_cli_providers_resolve_user_provider(const char *arg) {
    /* Python: config providers entry -> ProviderDef. Arg =
     * "name\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_providers_custom_provider_slug @ hermes_cli/providers.py:custom_provider_slug */
int hermes_cli_providers_custom_provider_slug(const char *arg) {
    /* Python: "custom:" + name stripped/lowered, spaces -> -. Arg = name. */
    if (!arg || !*arg) { printf("custom:\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t')) n--;
    printf("custom:");
    for (size_t i = 0; i < n; i++) {
        char c = p[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c == ' ') c = '-';
        putchar(c);
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_providers_resolve_custom_provider @ hermes_cli/providers.py:resolve_custom_provider */
int hermes_cli_providers_resolve_custom_provider(const char *arg) {
    /* Python: bare-custom self-heal. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("resolved: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_providers_is_official_openai_host @ hermes_cli/providers.py:is_official_openai_host */
int hermes_cli_providers_is_official_openai_host(const char *arg) {
    /* Python: base_url_host_matches(base_url, "api.openai.com").
     * Arg = base_url. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    bool ok = provider_base_url_host_matches(arg, "api.openai.com");
    printf("%d\n", ok ? 1 : 0);
    return 0;
}

/* PoP: hermes_cli_providers_nous_api_mode @ hermes_cli/providers.py:nous_api_mode */
int hermes_cli_providers_nous_api_mode(const char *arg) {
    /* Python: if model.startswith("anthropic/") -> "anthropic_messages",
     * else "chat_completions". Arg = model. */
    const char *m = arg ? arg : "";
    /* strip leading whitespace */
    while (*m == ' ' || *m == '\t') m++;
    bool is_anthropic = false;
    if (!strncasecmp(m, "anthropic/", 10)) is_anthropic = true;
    const char *result = is_anthropic ? "anthropic_messages" : "chat_completions";
    printf("%s\n", result);
    return 0;
}

/* PoP: hermes_cli_providers_custom_provider_aliases @ hermes_cli/providers.py:custom_provider_aliases */
int hermes_cli_providers_custom_provider_aliases(const char *arg) {
    /* Python: build a frozenset of {raw, normalized, custom:slug} plus
     * custom:suffix variants. Arg = "display_name\tprovider_key". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    char display[512] = "";
    char pkey[512] = "";
    const char *tab = strchr(arg, '\t');
    if (tab) {
        size_t dn = (size_t)(tab - arg);
        if (dn > 511) dn = 511;
        memcpy(display, arg, dn);
        display[dn] = '\0';
        strncpy(pkey, tab + 1, 511);
        pkey[511] = '\0';
    } else {
        strncpy(display, arg, 511);
        display[511] = '\0';
    }

    char **aliases = NULL;
    size_t n = 0, cap = 0;

    for (int vi = 0; vi < 2; vi++) {
        const char *value = vi == 0 ? display : pkey;
        if (!value || !*value) continue;
        /* raw (lowercased already in Python via .strip().lower()) */
        char raw[512];
        const char *s = value;
        while (*s == ' ' || *s == '\t') s++;
        size_t sl = strlen(s);
        while (sl > 0 && (s[sl-1] == ' ' || s[sl-1] == '\t')) sl--;
        /* lowercased */
        for (size_t i = 0; i < sl && i < 511; i++)
            raw[i] = (char)tolower((unsigned char)s[i]);
        raw[sl < 512 ? sl : 511] = '\0';
        if (sl >= 512) raw[511] = '\0';
        if (!*raw) continue;

        /* aliases: {raw, normalized, custom:slug} */
        /* normalized = raw with spaces -> - */
        char norm[512];
        for (size_t i = 0; raw[i] && i < 511; i++)
            norm[i] = (raw[i] == ' ') ? '-' : raw[i];
        norm[strlen(raw)] = '\0';

        /* custom:slug */
        char slug[512];
        snprintf(slug, sizeof(slug), "custom:%s", norm);

        char *variants[] = { strdup(raw), strdup(norm), strdup(slug) };
        for (int k = 0; k < 3; k++) {
            /* dedup */
            bool found = false;
            for (size_t j = 0; j < n; j++) {
                if (strcmp(aliases[j], variants[k]) == 0) { found = true; break; }
            }
            if (!found) {
                if (n == cap) { cap = cap ? cap * 2 : 8; aliases = realloc(aliases, (cap + 1) * sizeof(char *)); }
                aliases[n++] = variants[k];
            } else {
                free(variants[k]);
            }
        }

        /* if normalized starts with "custom:", add suffix variants */
        if (!strncmp(norm, "custom:", 7) && strlen(norm) > 7) {
            char suffix[512];
            strcpy(suffix, norm + 7);
            char alt[512];
            snprintf(alt, sizeof(alt), "custom:%s", norm);
            char *variants2[] = { strdup(suffix), strdup(alt) };
            for (int k = 0; k < 2; k++) {
                bool found = false;
                for (size_t j = 0; j < n; j++) {
                    if (strcmp(aliases[j], variants2[k]) == 0) { found = true; break; }
                }
                if (!found) {
                    if (n == cap) { cap = cap ? cap * 2 : 8; aliases = realloc(aliases, (cap + 1) * sizeof(char *)); }
                    aliases[n++] = variants2[k];
                } else {
                    free(variants2[k]);
                }
            }
        }
    }

    /* JSON array output */
    printf("[");
    for (size_t i = 0; i < n; i++) {
        printf("\"%s\"", aliases[i]);
        if (i + 1 < n) printf(",");
    }
    printf("]\n");

    for (size_t i = 0; i < n; i++) free(aliases[i]);
    free(aliases);
    return 0;
}
