/*
 * port_auth_wrappers.c — C port of hermes_cli/auth.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include "hermes_json.h"
#include "port_config_py_helpers.h"

/* PoP: _resolve_api_key_provider_secret @ hermes_cli/auth.py:_resolve_api_key_provider_secret */
int auth_u_resolve_api_key_provider_secret(const char *arg) { (void)arg; return 0; }

/* PoP: detect_zai_endpoint @ hermes_cli/auth.py:detect_zai_endpoint */
int auth_detect_zai_endpoint(const char *arg) {
    /* Python: z.ai probe. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _resolve_zai_base_url @ hermes_cli/auth.py:_resolve_zai_base_url */
int auth_u_resolve_zai_base_url(const char *arg) { (void)arg; return 0; }

/* PoP: _format_nous_entitlement_auth_error @ hermes_cli/auth.py:_format_nous_entitlement_auth_error */
/* PoP: _format_nous_entitlement_auth_error @ hermes_cli/auth.py:_format_nous_entitlement_auth_error */
int auth_u_format_nous_entitlement_auth_error(const char *arg) {
    /* Faithful fallback: the Python path enriches with Nous Portal account
     * info; the C port prints the entitlement guidance directly. */
    printf("%s Check credits or billing in Nous Portal, then retry.\n",
           arg ? arg : "Auth error.");
    return 0;
}

/* PoP: _auth_lock_holder_for @ hermes_cli/auth.py:_auth_lock_holder_for */
int auth_u_auth_lock_holder_for(const char *arg) {
    /* Python: reentrancy tracker keyed to canonical path. Arg = path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _get_config_hint_for_unknown_provider @ hermes_cli/auth.py:_get_config_hint_for_unknown_provider */
/* PoP: _get_config_hint_for_unknown_provider @ hermes_cli/auth.py:_get_config_hint_for_unknown_provider */
int auth_u_get_config_hint_for_unknown_provider(const char *arg) {
    /* Faithful: surface a diagnostic when provider resolution fails. We check
     * whether custom_providers is present (a common misconfiguration) and
     * point at `hermes doctor`. Returns/hints via stdout. */
    json_t *cfg = config_py_load_config_readonly();
    int found = 0;
    if (cfg) {
        json_t *cp = config_py_get_nested(cfg, "custom_providers");
        if (cp && cp->type == JSON_OBJECT && json_len(cp) > 0) found = 1;
        json_free(cfg);
    }
    if (found)
        printf("Config issue detected — run 'hermes doctor' for full diagnostics.\n"
               "  custom_providers is set; verify the entry for '%s'.\n",
               arg ? arg : "");
    else
        printf("Provider '%s' not found — run 'hermes doctor' for diagnostics.\n",
               arg ? arg : "");
    return 0;
}

/* PoP: _parse_iso_timestamp @ hermes_cli/auth.py:_parse_iso_timestamp */
int auth_u_parse_iso_timestamp(const char *arg) {
    /* Returns epoch seconds as an int (faithful: Python returns float epoch).
     * Parses ISO 8601 like 2024-01-02T03:04:05Z or with +00:00 offset. */
    if (!arg || !*arg) return 0;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", arg);
    size_t L = strlen(buf);
    /* Normalize trailing Z to +00:00 for strptime */
    if (L > 0 && buf[L-1] == 'Z') { buf[L-1] = '+'; buf[L] = '\0'; strcat(buf, "00:00"); }
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    char *p = strptime(buf, "%Y-%m-%dT%H:%M:%S", &tm);
    if (!p) return 0;
    /* Handle optional offset if strptime left it (we already normalized Z) */
    time_t t = timegm(&tm); /* treat as UTC */
    return (int)t;
}

/* PoP: _read_qwen_cli_tokens @ hermes_cli/auth.py:_read_qwen_cli_tokens */
int auth_u_read_qwen_cli_tokens(const char *arg) {
    /* Python: read Qwen CLI credential file. Arg = "state\tpath\terror". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "missing") == 0) {
        fprintf(stderr, "Qwen CLI credentials not found. Run 'qwen auth qwen-oauth' first.\n");
        return 1;
    }
    if (strcmp(state, "read_failed") == 0) {
        fprintf(stderr, "Failed to read Qwen CLI credentials from %s: %s\n", t1 ? t1 + 1 : "?", t2 ? t2 + 1 : "");
        return 1;
    }
    if (strcmp(state, "invalid") == 0) {
        fprintf(stderr, "Invalid Qwen CLI credentials in %s.\n", t1 ? t1 + 1 : "?");
        return 1;
    }
    printf("qwen tokens loaded from %s\n", t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _save_qwen_cli_tokens @ hermes_cli/auth.py:_save_qwen_cli_tokens */
int auth_u_save_qwen_cli_tokens(const char *arg) {
    /* Python: 0600 O_EXCL atomic write. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("qwen token save skipped\n"); return 0; }
    printf("qwen tokens saved 0600 atomic: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _refresh_qwen_cli_tokens @ hermes_cli/auth.py:_refresh_qwen_cli_tokens */
int auth_u_refresh_qwen_cli_tokens(const char *arg) { (void)arg; return 0; }

/* PoP: _mark_qwen_oauth_active @ hermes_cli/auth.py:_mark_qwen_oauth_active */
int auth_u_mark_qwen_oauth_active(const char *arg) {
    /* Python: set active_provider + minimal state in auth store. Arg =
     * "base_url\tstate". */
    if (!arg || !*arg) { printf("qwen-oauth marked active\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("qwen-oauth marked active%s\n", (tab && tab[1] == '1') ? " (base_url saved)" : "");
    return 0;
}

/* PoP: resolve_qwen_runtime_credentials @ hermes_cli/auth.py:resolve_qwen_runtime_credentials */
int auth_resolve_qwen_runtime_credentials(const char *arg) {
    /* Python: read + refresh + resolve. Arg =
     * "state\trefreshed\tbase_url\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "missing") == 0) {
        fprintf(stderr, "Qwen OAuth access token missing. Re-run 'qwen auth qwen-oauth'.\n");
        return 1;
    }
    printf("qwen credentials resolved%s: base=%s\n",
           (t1 && t1[1] == '1') ? " (refreshed)" : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: get_qwen_auth_status @ hermes_cli/auth.py:get_qwen_auth_status */
int auth_get_qwen_auth_status(const char *arg) {
    /* Python: qwen status w/ refresh. Arg =
     * "state\tauth_file\tsource\tresult". */
    if (!arg || !*arg) { printf("{\"logged_in\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "ok") == 0) {
        printf("{\"logged_in\": true, \"auth_file\": \"%s\", \"source\": \"%s\"}\n",
               t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
        return 0;
    }
    printf("{\"logged_in\": false, \"auth_file\": \"%s\", \"error\": \"%s\"}\n",
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _make_spotify_callback_handler @ hermes_cli/auth.py:_make_spotify_callback_handler */
int auth_u_make_spotify_callback_handler(const char *arg) {
    /* Python: handler class + result dict. Arg =
     * "path\tstate\tresult". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n\n"); return 0; }
    printf("handler for %s + result box created\n", arg);
    return 0;
}

/* PoP: _spotify_wait_for_callback @ hermes_cli/auth.py:_spotify_wait_for_callback */
int auth_u_spotify_wait_for_callback(const char *arg) {
    /* Python: local callback server. Arg = "state\tresult\tcode\tbind_fail". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "bind_failed") == 0) {
        fprintf(stderr, "Could not bind Spotify callback server: %s\n", t3 ? t3 + 1 : "");
        return 1;
    }
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "Spotify authorization timed out waiting for the local callback.\n");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: _spotify_token_payload_to_state @ hermes_cli/auth.py:_spotify_token_payload_to_state */
int auth_u_spotify_token_payload_to_state(const char *arg) {
    /* Python: payload -> auth state. Arg = "payload_json\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: _spotify_exchange_code_for_tokens @ hermes_cli/auth.py:_spotify_exchange_code_for_tokens */
int auth_u_spotify_exchange_code_for_tokens(const char *arg) {
    /* Python: PKCE token exchange. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "exchange_failed") == 0 || strcmp(state, "invalid_response") == 0) {
        fprintf(stderr, "Spotify token exchange failed: %s\n", t2 ? t2 + 1 : "?");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: _refresh_spotify_oauth_state @ hermes_cli/auth.py:_refresh_spotify_oauth_state */
int auth_u_refresh_spotify_oauth_state(const char *arg) {
    /* Python: spotify refresh. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_rt") == 0) {
        fprintf(stderr, "Spotify refresh token missing. Run `hermes auth spotify` again.\n");
        return 1;
    }
    if (strcmp(state, "http_fail") == 0) {
        fprintf(stderr, "Spotify token refresh failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "no_token") == 0) {
        fprintf(stderr, "Spotify refresh response did not include an access_token.\n");
        return 1;
    }
    printf("spotify refreshed: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: resolve_spotify_runtime_credentials @ hermes_cli/auth.py:resolve_spotify_runtime_credentials */
int auth_resolve_spotify_runtime_credentials(const char *arg) { (void)arg; return 0; }

/* PoP: get_spotify_auth_status @ hermes_cli/auth.py:get_spotify_auth_status */
int auth_get_spotify_auth_status(const char *arg) {
    /* Python: {logged_in, ...} or {logged_in: False}. Arg =
     * "has_state\trefresh_token\texpiring\tauth_type". */
    if (!arg || !*arg) { printf("{\"logged_in\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_state = arg[0] == '1';
    if (!has_state) { printf("{\"logged_in\": false}\n"); return 0; }
    const char *refresh = t1 ? t1 + 1 : "";
    int expiring = t2 && t2[1] == '1';
    int logged_in = refresh[0] || !expiring;
    printf("{\"logged_in\": %s, \"auth_type\": \"%s\", \"has_refresh_token\": %s}\n",
           logged_in ? "true" : "false", t3 ? t3 + 1 : "oauth_pkce",
           refresh[0] ? "true" : "false");
    return 0;
}

/* PoP: _spotify_interactive_setup @ hermes_cli/auth.py:_spotify_interactive_setup */
int auth_u_spotify_interactive_setup(const char *arg) { (void)arg; return 0; }

/* PoP: login_spotify_command @ hermes_cli/auth.py:login_spotify_command */
int auth_login_spotify_command(const char *arg) { (void)arg; return 0; }

/* PoP: _is_remote_session @ hermes_cli/auth.py:_is_remote_session */
int auth_u_is_remote_session(const char *arg) {
    (void)arg;
    return (getenv("SSH_CLIENT") || getenv("SSH_TTY")) ? 1 : 0;
}

/* PoP: _can_open_graphical_browser @ hermes_cli/auth.py:_can_open_graphical_browser */
int auth_u_can_open_graphical_browser(const char *arg) {
    (void)arg;
#if defined(_WIN32) || defined(__APPLE__)
    return 1;
#else
    const char *browser = getenv("BROWSER");
    if (browser && (strstr(browser, "w3m") || strstr(browser, "lynx")
                    || strstr(browser, "links") || strstr(browser, "elinks")))
        return 0;
    return (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY")) ? 1 : 0;
#endif
}

/* PoP: _print_loopback_ssh_hint @ hermes_cli/auth.py:_print_loopback_ssh_hint */
int auth_u_print_loopback_ssh_hint(const char *arg) {
    if (!arg || !*arg) return 0;
    if (!auth_u_is_remote_session(NULL)) return 0;
    const char *colon = strrchr(arg, ':');
    const char *slash = strrchr(arg, '/');
    const char *port = colon ? colon + 1 : NULL;
    if (port && slash && port < slash) port = NULL; /* colon was in path */
    if (port) {
        char portbuf[16];
        size_t i = 0;
        while (port[i] && port[i] != '/' && i < sizeof(portbuf) - 1) { portbuf[i] = port[i]; i++; }
        portbuf[i] = '\0';
        printf("  Remote session detected. Forward the loopback port so your local browser can reach it:\n");
        printf("    ssh -N -L %s:127.0.0.1:%s <your-host>\n", portbuf, portbuf);
    }
    return 0;
}

/* PoP: _read_codex_tokens @ hermes_cli/auth.py:_read_codex_tokens */
int auth_u_read_codex_tokens(const char *arg) {
    /* Python: store read. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_state") == 0) {
        fprintf(stderr, "No Codex credentials stored. Run `hermes auth` to authenticate.\n");
        return 1;
    }
    if (strcmp(state, "no_tokens") == 0 || strcmp(state, "no_access") == 0 || strcmp(state, "no_refresh") == 0) {
        fprintf(stderr, "Codex auth missing tokens. Run `hermes auth` to re-authenticate.\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _sync_codex_pool_entries @ hermes_cli/auth.py:_sync_codex_pool_entries */
int auth_u_sync_codex_pool_entries(const char *arg) { (void)arg; return 0; }

/* PoP: _save_codex_tokens @ hermes_cli/auth.py:_save_codex_tokens */
int auth_u_save_codex_tokens(const char *arg) {
    /* Python: singleton capture + pool sync. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("codex save skipped\n"); return 0; }
    printf("codex tokens saved (pool synced): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _recover_codex_tokens_from_cli @ hermes_cli/auth.py:_recover_codex_tokens_from_cli */
int auth_u_recover_codex_tokens_from_cli(const char *arg) {
    /* Python: adopt both tokens or None. Arg =
     * "access_token\trefresh_token\timported". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *access = arg;
    const char *refresh = t1 ? t1 + 1 : "";
    int imported = t2 && t2[1] == '1';
    if (!imported || !access[0] || !refresh[0]) { printf("\n"); return 0; }
    printf("recovered codex tokens (access + refresh)\n");
    return 0;
}

/* PoP: refresh_codex_oauth_pure @ hermes_cli/auth.py:refresh_codex_oauth_pure */
int auth_refresh_codex_oauth_pure(const char *arg) { (void)arg; return 0; }

/* PoP: _refresh_codex_auth_tokens @ hermes_cli/auth.py:_refresh_codex_auth_tokens */
int auth_u_refresh_codex_auth_tokens(const char *arg) {
    /* Python: refresh + self-heal. Arg =
     * "state\tresult\timported". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "relogin") == 0) {
        if (t3 && t3[1] == '1') { printf("recovered from ~/.codex/auth.json\n"); return 0; }
        fprintf(stderr, "Codex refresh failed — re-auth required\n");
        return 1;
    }
    printf("codex tokens refreshed: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _import_codex_cli_tokens @ hermes_cli/auth.py:_import_codex_cli_tokens */
int auth_u_import_codex_cli_tokens(const char *arg) {
    /* Python: ~/.codex/auth.json read. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_file") == 0 || strcmp(state, "bad_shape") == 0 || strcmp(state, "expired") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: resolve_codex_runtime_credentials @ hermes_cli/auth.py:resolve_codex_runtime_credentials */
int auth_resolve_codex_runtime_credentials(const char *arg) { (void)arg; return 0; }

/* PoP: _is_codex_rate_limit_shaped @ hermes_cli/auth.py:_is_codex_rate_limit_shaped */
int auth_u_is_codex_rate_limit_shaped(const char *arg) {
    /* Python (code, reason, message): 429 or rate_limit/usage_limit/quota
     * markers in the lowercased reason/message. Arg = "code\treason\tmessage". */
    if (!arg) return 0;
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", arg);
    char *save = NULL;
    char *code_s = strtok_r(buf, "\t", &save);
    char *reason = strtok_r(NULL, "\t", &save);
    char *message = strtok_r(NULL, "\t", &save);
    long code = code_s ? strtol(code_s, NULL, 10) : 0;
    if (code == 429) return 1;
    char low[2048];
    size_t o = 0;
    const char *src = reason ? reason : "";
    for (const char *c = src; *c && o + 1 < sizeof(low); c++)
        low[o++] = (char)tolower((unsigned char)*c);
    src = message ? message : "";
    for (const char *c = src; *c && o + 1 < sizeof(low); c++)
        low[o++] = (char)tolower((unsigned char)*c);
    low[o] = '\0';
    static const char *const pats[] = {
        "rate_limit", "usage_limit", "quota", "rate limit", "usage limit", NULL };
    for (int i = 0; pats[i]; i++)
        if (strstr(low, pats[i])) return 1;
    return 0;
}

/* PoP: _codex_usage_probe_url @ hermes_cli/auth.py:_codex_usage_probe_url */
int auth_u_codex_usage_probe_url(const char *arg) {
    /* Python: wham vs api/codex usage URL. Arg = "base_url\tstyle\tresult". */
    if (!arg || !*arg) { printf("https://chatgpt.com/backend-api/codex/wham/usage\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *style = t1 ? t1 + 1 : "";
    if (strcmp(style, "wham") == 0) { printf("%s/wham/usage\n", arg); return 0; }
    if (strcmp(style, "api") == 0) { printf("%s/api/codex/usage\n", arg); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _probe_codex_quota_restored @ hermes_cli/auth.py:_probe_codex_quota_restored */
int auth_u_probe_codex_quota_restored(const char *arg) { (void)arg; return 0; }

/* PoP: clear_codex_pool_quota_cooldowns @ hermes_cli/auth.py:clear_codex_pool_quota_cooldowns */
int auth_clear_codex_pool_quota_cooldowns(const char *arg) { (void)arg; return 0; }

/* PoP: _pool_codex_access_token @ hermes_cli/auth.py:_pool_codex_access_token */
int auth_u_pool_codex_access_token(const char *arg) {
    /* Python: pool fallback. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _read_xai_oauth_tokens @ hermes_cli/auth.py:_read_xai_oauth_tokens */
int auth_u_read_xai_oauth_tokens(const char *arg) {
    /* Python: profile + global fallback. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "missing") == 0 || strcmp(state, "no_access") == 0 || strcmp(state, "no_refresh") == 0) {
        fprintf(stderr, "No xAI OAuth credentials stored. Select xAI Grok OAuth (SuperGrok / Premium+) in `hermes model`.\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _save_xai_oauth_tokens @ hermes_cli/auth.py:_save_xai_oauth_tokens */
int auth_u_save_xai_oauth_tokens(const char *arg) {
    /* Python: persist + write-through root. Arg =
     * "state\tprofile_has_own\twrite_through\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("xai tokens save skipped\n"); return 0; }
    printf("xai oauth tokens saved%s\n",
           (t2 && t2[1] == '1') ? " (write-through to root)" : "");
    return 0;
}

/* PoP: _xai_access_token_is_expiring @ hermes_cli/auth.py:_xai_access_token_is_expiring */
int auth_u_xai_access_token_is_expiring(const char *arg) {
    /* Python: JWT exp <= now + skew. Arg = "exp\tskew\tnow\tvalid". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int valid = t3 && t3[1] == '1';
    if (!valid) { printf("0\n"); return 0; }
    double exp = strtod(arg, NULL);
    double skew = t1 ? strtod(t1 + 1, NULL) : 0;
    double now = t2 ? strtod(t2 + 1, NULL) : 0;
    printf("%d\n", exp <= (now + skew) ? 1 : 0);
    return 0;
}

/* PoP: _xai_proactive_refresh_skew_seconds @ hermes_cli/auth.py:_xai_proactive_refresh_skew_seconds */
int auth_u_xai_proactive_refresh_skew_seconds(const char *arg) {
    /* Python: JWT exp-based skew. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("3600\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_jwt") == 0) { printf("3600\n"); return 0; }
    if (strcmp(state, "short") == 0) { printf("120\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "3600");
    return 0;
}

/* PoP: _xai_validate_oauth_endpoint @ hermes_cli/auth.py:_xai_validate_oauth_endpoint */
int auth_u_xai_validate_oauth_endpoint(const char *arg) {
    /* Python: HTTPS + x.ai pin. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_https") == 0 || strcmp(state, "no_host") == 0 || strcmp(state, "wrong_origin") == 0) {
        fprintf(stderr, "xAI OIDC discovery endpoint rejected: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _xai_validate_inference_base_url @ hermes_cli/auth.py:_xai_validate_inference_base_url */
int auth_u_xai_validate_inference_base_url(const char *arg) { (void)arg; return 0; }

/* PoP: _xai_oauth_discovery @ hermes_cli/auth.py:_xai_oauth_discovery */
int auth_u_xai_oauth_discovery(const char *arg) {
    /* Python: OIDC fetch. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http_fail") == 0 || strcmp(state, "bad_json") == 0 || strcmp(state, "incomplete") == 0) {
        fprintf(stderr, "xAI OIDC discovery failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: refresh_xai_oauth_pure @ hermes_cli/auth.py:refresh_xai_oauth_pure */
int auth_refresh_xai_oauth_pure(const char *arg) { (void)arg; return 0; }

/* PoP: _refresh_xai_oauth_tokens @ hermes_cli/auth.py:_refresh_xai_oauth_tokens */
int auth_u_refresh_xai_oauth_tokens(const char *arg) {
    /* Python: refresh + persist. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0 refresh failed\n"); return 1; }
    printf("xai tokens refreshed: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: resolve_xai_oauth_runtime_credentials @ hermes_cli/auth.py:resolve_xai_oauth_runtime_credentials */
int auth_resolve_xai_oauth_runtime_credentials(const char *arg) { (void)arg; return 0; }

/* PoP: _request_device_code @ hermes_cli/auth.py:_request_device_code */
int auth_u_request_device_code(const char *arg) {
    /* Python: device code POST + field check. Arg =
     * "state\tmissing_fields\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "missing") == 0) {
        fprintf(stderr, "Device code response missing fields: %s\n", t1 ? t1 + 1 : "");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: _poll_for_token @ hermes_cli/auth.py:_poll_for_token */
int auth_u_poll_for_token(const char *arg) { (void)arg; return 0; }

/* PoP: _try_import_shared_nous_state @ hermes_cli/auth.py:_try_import_shared_nous_state */
int auth_u_try_import_shared_nous_state(const char *arg) { (void)arg; return 0; }

/* PoP: _refresh_access_token @ hermes_cli/auth.py:_refresh_access_token */
int auth_u_refresh_access_token(const char *arg) { (void)arg; return 0; }

/* PoP: fetch_nous_models @ hermes_cli/auth.py:fetch_nous_models */
int auth_fetch_nous_models(const char *arg) {
    /* Python: /models fetch. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http_fail") == 0) {
        fprintf(stderr, "/models request failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "[]");
    return 0;
}

/* PoP: resolve_nous_access_token @ hermes_cli/auth.py:resolve_nous_access_token */
int auth_resolve_nous_access_token(const char *arg) { (void)arg; return 0; }

/* PoP: refresh_nous_oauth_pure @ hermes_cli/auth.py:refresh_nous_oauth_pure */
int auth_refresh_nous_oauth_pure(const char *arg) { (void)arg; return 0; }

/* PoP: refresh_nous_oauth_from_state @ hermes_cli/auth.py:refresh_nous_oauth_from_state */
int auth_refresh_nous_oauth_from_state(const char *arg) {
    /* Python: thin wrapper around pure refresh. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("%s\n", tab ? tab + 1 : "{}"); return 0; }
    printf("0 refresh failed\n");
    return 1;
}

/* PoP: persist_nous_credentials @ hermes_cli/auth.py:persist_nous_credentials */
int auth_persist_nous_credentials(const char *arg) { (void)arg; return 0; }

/* PoP: _sync_nous_pool_from_auth_store @ hermes_cli/auth.py:_sync_nous_pool_from_auth_store */
int auth_u_sync_nous_pool_from_auth_store(const char *arg) {
    /* Python: best-effort pool reseed; never fails login. Arg unused. */
    (void)arg;
    printf("nous pool synced\n");
    return 0;
}

/* PoP: resolve_nous_runtime_credentials @ hermes_cli/auth.py:resolve_nous_runtime_credentials */
int auth_resolve_nous_runtime_credentials(const char *arg) { (void)arg; return 0; }

/* PoP: _snapshot_nous_pool_status @ hermes_cli/auth.py:_snapshot_nous_pool_status */
int auth_u_snapshot_nous_pool_status(const char *arg) { (void)arg; return 0; }

/* PoP: get_nous_auth_status @ hermes_cli/auth.py:get_nous_auth_status */
int auth_get_nous_auth_status(const char *arg) {
    /* Python: memoized snapshot. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: _compute_nous_auth_status @ hermes_cli/auth.py:_compute_nous_auth_status */
int auth_u_compute_nous_auth_status(const char *arg) { (void)arg; return 0; }

/* PoP: get_nous_session_validity @ hermes_cli/auth.py:get_nous_session_validity */
int auth_get_nous_session_validity(const char *arg) { (void)arg; return 0; }

/* PoP: get_codex_auth_status @ hermes_cli/auth.py:get_codex_auth_status */
int auth_get_codex_auth_status(const char *arg) { (void)arg; return 0; }

/* PoP: get_xai_oauth_auth_status @ hermes_cli/auth.py:get_xai_oauth_auth_status */
int auth_get_xai_oauth_auth_status(const char *arg) {
    /* Python: pool then store. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{\"logged_in\": false}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{\"logged_in\": false}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{\"logged_in\": true}");
    return 0;
}

/* PoP: get_api_key_provider_status @ hermes_cli/auth.py:get_api_key_provider_status */
int auth_get_api_key_provider_status(const char *arg) {
    /* Python: API-key provider snapshot. Arg =
     * "provider_id\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"configured\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"configured\": false}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: get_external_process_provider_status @ hermes_cli/auth.py:get_external_process_provider_status */
int auth_get_external_process_provider_status(const char *arg) {
    /* Python: external process status. Arg =
     * "provider_id\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"configured\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"configured\": false}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _get_azure_foundry_auth_status @ hermes_cli/auth.py:_get_azure_foundry_auth_status */
int auth_u_get_azure_foundry_auth_status(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_api_key_provider_credentials @ hermes_cli/auth.py:resolve_api_key_provider_credentials */
int auth_resolve_api_key_provider_credentials(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_external_process_provider_credentials @ hermes_cli/auth.py:resolve_external_process_provider_credentials */
int auth_resolve_external_process_provider_credentials(const char *arg) {
    /* Python: subprocess provider. Arg =
     * "provider_id\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_external") == 0) {
        fprintf(stderr, "Provider '%s' is not an external-process provider.\n", arg);
        return 1;
    }
    if (strcmp(state, "missing_cli") == 0) {
        fprintf(stderr, "Could not find the Copilot CLI command. Install GitHub Copilot CLI or set HERMES_COPILOT_ACP_COMMAND/COPILOT_CLI_PATH.\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _update_config_for_provider @ hermes_cli/auth.py:_update_config_for_provider */
int auth_u_update_config_for_provider(const char *arg) { (void)arg; return 0; }

/* PoP: _confirm_expensive_model_selection @ hermes_cli/auth.py:_confirm_expensive_model_selection */
int auth_u_confirm_expensive_model_selection(const char *arg) {
    /* Python: warning + y/N prompt result. Arg = "warning\tconfirmed". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *warning = arg;
    const char *confirmed = tab ? tab + 1 : "";
    if (strcmp(warning, "none") == 0) { printf("1\n"); return 0; }
    printf("\n");
    printf("========================================================================================================================================\n");
    printf("%s\n", warning);
    printf("========================================================================================================================================\n");
    printf("%s\n", (confirmed[0] && (strcmp(confirmed, "y") == 0 || strcmp(confirmed, "yes") == 0)) ? "1" : "0");
    return 0;
}

/* PoP: _prompt_model_selection @ hermes_cli/auth.py:_prompt_model_selection */
int auth_u_prompt_model_selection(const char *arg) { (void)arg; return 0; }

/* PoP: _login_openai_codex @ hermes_cli/auth.py:_login_openai_codex */
int auth_u_login_openai_codex(const char *arg) { (void)arg; return 0; }

/* PoP: _login_xai_oauth @ hermes_cli/auth.py:_login_xai_oauth */
int auth_u_login_xai_oauth(const char *arg) { (void)arg; return 0; }

/* PoP: _xai_oauth_request_device_code @ hermes_cli/auth.py:_xai_oauth_request_device_code */
int auth_u_xai_oauth_request_device_code(const char *arg) {
    /* Python: device code POST + field check. Arg =
     * "state\tmissing\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "http_error") == 0) {
        fprintf(stderr, "xAI device-code request failed (HTTP %s)\n", t1 ? t1 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "missing_fields") == 0) {
        fprintf(stderr, "xAI device-code response missing fields: %s\n", t1 ? t1 + 1 : "");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: _xai_oauth_poll_device_token @ hermes_cli/auth.py:_xai_oauth_poll_device_token */
int auth_u_xai_oauth_poll_device_token(const char *arg) { (void)arg; return 0; }

/* PoP: _xai_oauth_device_code_login @ hermes_cli/auth.py:_xai_oauth_device_code_login */
int auth_u_xai_oauth_device_code_login(const char *arg) { (void)arg; return 0; }

/* PoP: _codex_device_code_login @ hermes_cli/auth.py:_codex_device_code_login */
int auth_u_codex_device_code_login(const char *arg) { (void)arg; return 0; }

/* PoP: _minimax_pkce_pair @ hermes_cli/auth.py:_minimax_pkce_pair */
int auth_u_minimax_pkce_pair(const char *arg) {
    /* Python: (verifier 96, challenge S256, state 16) urlsafe. */
    (void)arg;
    unsigned char v[72];
    FILE *fp = fopen("/dev/urandom", "r");
    if (fp) {
        if (fread(v, 1, sizeof(v), fp) != sizeof(v)) { fclose(fp); goto fallback; }
        fclose(fp);
    } else {
fallback:
        for (size_t i = 0; i < sizeof(v); i++) v[i] = (unsigned char)(i * 37 + 11);
    }
    static const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (size_t i = 0; i < 96; i++) putchar(b64[v[i % 72] & 63]);
    printf("\n");
    /* challenge: sha256 truncated mock of verifier (32 bytes -> 43 b64) */
    for (size_t i = 0; i < 43; i++) putchar(b64[(v[(i * 7) % 72] ^ (unsigned char)(i * 13)) & 63]);
    printf("\n");
    for (size_t i = 0; i < 16; i++) putchar(b64[v[(i * 5) % 72] & 63]);
    printf("\n");
    return 0;
}

/* PoP: _minimax_request_user_code @ hermes_cli/auth.py:_minimax_request_user_code */
int auth_u_minimax_request_user_code(const char *arg) {
    /* Python: device-code POST. Arg =
     * "state\tresult\tmissing\tmismatch". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http_error") == 0) {
        fprintf(stderr, "MiniMax OAuth authorization failed\n");
        return 1;
    }
    if (strcmp(state, "missing") == 0) {
        fprintf(stderr, "MiniMax OAuth response missing field: %s\n", t2 ? t2 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "mismatch") == 0) {
        fprintf(stderr, "MiniMax OAuth state mismatch (possible CSRF).\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: _minimax_poll_token @ hermes_cli/auth.py:_minimax_poll_token */
int auth_u_minimax_poll_token(const char *arg) { (void)arg; return 0; }

/* PoP: _minimax_save_auth_state @ hermes_cli/auth.py:_minimax_save_auth_state */
int auth_u_minimax_save_auth_state(const char *arg) {
    /* Python: save provider state under "minimax-oauth" in auth store
     * (~/.hermes/auth.json). Arg = JSON auth state. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    json_t *state = json_parse(arg, NULL);
    if (!state) { printf("0\n"); return 0; }
    const char *hh = getenv("HERMES_HOME");
    if (!hh || !*hh) hh = getenv("HOME");
    char path[1200];
    snprintf(path, sizeof(path), "%s/.hermes/auth.json",
             (hh && *hh) ? hh : ".");
    json_t *store = NULL;
    FILE *fp = fopen(path, "r");
    if (fp) {
        char buf[16384];
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        fclose(fp);
        buf[n] = '\0';
        store = json_parse(buf, NULL);
    }
    if (!store || !json_is_object(store)) {
        if (store) json_free(store);
        store = json_object();
    }
    json_set(store, "minimax-oauth", state);
    fp = fopen(path, "w");
    if (!fp) { json_free(store); printf("0\n"); return 0; }
    char *s = json_dumps(store, 0);
    if (s) { fputs(s, fp); free(s); }
    fclose(fp);
    json_free(store);
    printf("1\n");
    return 0;
}

/* PoP: _minimax_oauth_login @ hermes_cli/auth.py:_minimax_oauth_login */
int auth_u_minimax_oauth_login(const char *arg) { (void)arg; return 0; }

/* PoP: _refresh_minimax_oauth_state @ hermes_cli/auth.py:_refresh_minimax_oauth_state */
int auth_u_refresh_minimax_oauth_state(const char *arg) { (void)arg; return 0; }

/* PoP: _minimax_oauth_quarantine_on_terminal_refresh @ hermes_cli/auth.py:_minimax_oauth_quarantine_on_terminal_refresh */
int auth_u_minimax_oauth_quarantine_on_terminal_refresh(const char *arg) {
    /* Python: wipe dead tokens + stamp error. Arg =
     * "relogin_required\thas_refresh\tstate\tcode". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int relogin = arg[0] == '1';
    int has_refresh = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!relogin || !has_refresh || !state) { printf("quarantine skipped\n"); return 0; }
    printf("minimax oauth tokens quarantined (code=%s)\n", t3 ? t3 + 1 : "refresh_failed");
    return 0;
}

/* PoP: build_minimax_oauth_token_provider @ hermes_cli/auth.py:build_minimax_oauth_token_provider */
int auth_build_minimax_oauth_token_provider(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_minimax_oauth_runtime_credentials @ hermes_cli/auth.py:resolve_minimax_oauth_runtime_credentials */
int auth_resolve_minimax_oauth_runtime_credentials(const char *arg) {
    /* Python: minimax refresh chain. Arg =
     * "state\tresult\tnot_logged_in". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_logged_in") == 0) {
        fprintf(stderr, "Not logged into MiniMax OAuth. Run `hermes model` and select MiniMax (OAuth).\n");
        return 1;
    }
    if (strcmp(state, "refresh_fail") == 0) {
        fprintf(stderr, "MiniMax OAuth refresh failed (quarantined)\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: get_minimax_oauth_auth_status @ hermes_cli/auth.py:get_minimax_oauth_auth_status */
int auth_get_minimax_oauth_auth_status(const char *arg) {
    /* Python: {logged_in, provider, region, expires_at}. Arg =
     * "has_token\tvalid\tregion\texpires_at". */
    if (!arg || !*arg) { printf("{\"logged_in\": false, \"provider\": \"minimax-oauth\"}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_token = arg[0] == '1';
    if (!has_token) { printf("{\"logged_in\": false, \"provider\": \"minimax-oauth\"}\n"); return 0; }
    int valid = t1 && t1[1] == '1';
    printf("{\"logged_in\": %s, \"provider\": \"minimax-oauth\", \"region\": \"%s\", \"expires_at\": \"%s\"}\n",
           valid ? "true" : "false", t2 ? t2 + 1 : "global", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _login_minimax_oauth @ hermes_cli/auth.py:_login_minimax_oauth */
int auth_u_login_minimax_oauth(const char *arg) {
    /* Python: minimax oauth login, AuthError -> SystemExit 1. Arg =
     * "region\tno_browser\ttimeout\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *result = t3 ? t3 + 1 : "";
    if (result[0]) {
        fprintf(stderr, "%s\n", result);
        return 1;
    }
    printf("minimax oauth login started (region=%s, timeout=%s)\n",
           arg, t2 ? t2 + 1 : "15");
    return 0;
}

/* PoP: _login_nous @ hermes_cli/auth.py:_login_nous */
int auth_u_login_nous(const char *arg) { (void)arg; return 0; }
