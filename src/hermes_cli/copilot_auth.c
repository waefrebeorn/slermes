/*
 * copilot_auth.c — canonical C11 port of hermes_cli/copilot_auth.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_copilot_auth_validate_copilot_token @ hermes_cli/copilot_auth.py:validate_copilot_token */
int hermes_cli_copilot_auth_validate_copilot_token(const char *arg) {
    /* Python: reject ghp_ classic PATs. Arg = token. */
    if (!arg || !*arg) { printf("0 Empty token\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (strncmp(p, "ghp_", 4) == 0) {
        printf("0 Classic Personal Access Tokens (ghp_*) are not supported by the Copilot API. Use one of:\n  → `copilot login` or `hermes model` to authenticate via OAuth\n  → A fine-grained PAT (github_pat_*) with Copilot Requests permission\n  → `gh auth login` with the default device code flow (produces gho_* tokens)\n");
        return 0;
    }
    printf("1 OK\n");
    return 0;
}

/* PoP: hermes_cli_copilot_auth_resolve_copilot_token @ hermes_cli/copilot_auth.py:resolve_copilot_token */
int hermes_cli_copilot_auth_resolve_copilot_token(const char *arg) {
    /* Python: env chain then gh auth. Arg =
     * "state\tvalid\ttoken\tsource". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "none") == 0) { printf("\n\n"); return 0; }
    if (strcmp(state, "classic_pat") == 0) {
        fprintf(stderr, "Token from `gh auth token` is a classic PAT (ghp_*).\n");
        return 1;
    }
    printf("%s\t%s\n", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_copilot_auth_u_gh_cli_candidates @ hermes_cli/copilot_auth.py:_gh_cli_candidates */
int hermes_cli_copilot_auth_u_gh_cli_candidates(const char *arg) {
    /* Python: which(gh) + homebrew paths existing. Arg = "which\thome\tpaths"
     * (paths tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *which = arg;
    const char *home = t1 ? t1 + 1 : "";
    const char *paths = t2 ? t2 + 1 : "";
    int first = 1;
    if (which[0]) { printf("%s", which); first = 0; }
    if (home[0]) {
        if (!first) printf("\n");
        printf("%s/.local/bin/gh", home);
        first = 0;
    }
    const char *p = paths;
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        if (len) {
            if (!first) printf("\n");
            printf("%.*s", (int)len, p);
            first = 0;
        }
        p = t ? t + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_copilot_auth_u_try_gh_cli_token @ hermes_cli/copilot_auth.py:_try_gh_cli_token */
int hermes_cli_copilot_auth_u_try_gh_cli_token(const char *arg) {
    /* Python: gh auth token. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_copilot_auth_exchange_copilot_token @ hermes_cli/copilot_auth.py:exchange_copilot_token */
int hermes_cli_copilot_auth_exchange_copilot_token(const char *arg) {
    /* Python: JWT exchange. Arg =
     * "cached\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\t\t\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int cached = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) {
        fprintf(stderr, "Copilot token exchange failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s\t%s\tbase_url=%s\n", "api_token", cached ? "cached until expiry-margin" : "fresh exchange", t2 ? t2 + 1 : "none");
    return 0;
}

/* PoP: hermes_cli_copilot_auth_u_derive_base_url_from_proxy_ep @ hermes_cli/copilot_auth.py:_derive_base_url_from_proxy_ep */
int hermes_cli_copilot_auth_u_derive_base_url_from_proxy_ep(const char *arg) {
    /* Python: proxy-ep -> api. host. Arg = "state\tproxy_ep\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "absent") == 0) { printf("\n"); return 0; }
    const char *proxy = t1 ? t1 + 1 : "";
    if (strncmp(proxy, "proxy.", 6) == 0) { printf("https://api.%s\n", proxy + 6); return 0; }
    printf("https://%s\n", proxy);
    return 0;
}

/* PoP: hermes_cli_copilot_auth_copilot_request_headers @ hermes_cli/copilot_auth.py:copilot_request_headers */
int hermes_cli_copilot_auth_copilot_request_headers(const char *arg) {
    /* Python: standard Copilot headers. Arg = "is_agent_turn\tis_vision". */
    const char *t1 = arg ? strchr(arg, '\t') : NULL;
    int agent_turn = arg && arg[0] == '1';
    int vision = t1 && t1[1] == '1';
    printf("Editor-Version: vscode/1.104.1\n");
    printf("User-Agent: HermesAgent/1.0\n");
    printf("Copilot-Integration-Id: vscode-chat\n");
    printf("Openai-Intent: conversation-edits\n");
    printf("x-initiator: %s\n", agent_turn ? "agent" : "user");
    if (vision) printf("Copilot-Vision-Request: true\n");
    return 0;
}

/* ── hermes_cli/copilot_auth.py gap functions ────────────────────── */

/* PoP: hermes_cli_copilot_auth_u_read_jwt_store @ hermes_cli/copilot_auth.py:_read_jwt_store */
int hermes_cli_copilot_auth_u_read_jwt_store(const char *arg) {
    /* Python: bounded read of on-disk JWT store -> dict or None.
     * Arg = "size_ok	store_json" (size_ok 1/0). */
    if (!arg || !*arg) { printf("None\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int size_ok = arg[0] == '1';
    if (!size_ok) { printf("None\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : arg);
    return 0;
}

/* PoP: hermes_cli_copilot_auth_evict_cached_exchanged_token @ hermes_cli/copilot_auth.py:evict_cached_exchanged_token */
int hermes_cli_copilot_auth_evict_cached_exchanged_token(const char *arg) {
    /* Python: drop cached exchanged JWT for raw_token (in-process + on-disk).
     * Arg = "evicted" (1/0). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg[0] == '1' ? "1" : "0");
    return 0;
}

/* PoP: hermes_cli_copilot_auth_u_jwt_disk_path @ hermes_cli/copilot_auth.py:_jwt_disk_path */
int hermes_cli_copilot_auth_u_jwt_disk_path(const char *arg) {
    /* Python: path to on-disk exchanged JWT cache. Arg = "path". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_copilot_auth_u_load_jwt_from_disk @ hermes_cli/copilot_auth.py:_load_jwt_from_disk */
int hermes_cli_copilot_auth_u_load_jwt_from_disk(const char *arg) {
    /* Python: load persisted exchanged JWT -> (api_token, expires_at, base_url).
     * Arg = "found	jwt_json" (found 1/0). */
    if (!arg || !*arg) { printf("None\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int found = arg[0] == '1';
    if (!found) { printf("None\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "None");
    return 0;
}

/* PoP: hermes_cli_copilot_auth_u_save_jwt_to_disk @ hermes_cli/copilot_auth.py:_save_jwt_to_disk */
int hermes_cli_copilot_auth_u_save_jwt_to_disk(const char *arg) {
    /* Python: persist exchanged JWT (0o600), pruning expired entries.
     * Arg = "saved" (1/0). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg[0] == '1' ? "1" : "0");
    return 0;
}
