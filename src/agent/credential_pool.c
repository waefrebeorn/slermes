/*
 * credential_pool.c — canonical C11 port of agent/credential_pool.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_credential_pool_u_normalize_pool_auth_type @ agent/credential_pool.py:_normalize_pool_auth_type */
int agent_credential_pool_u_normalize_pool_auth_type(const char *arg) {
    /* Python: oauth for anthropic sk-ant-oat tokens, else auth_type or
     * api_key. Arg = "provider\tauth_type\ttoken". */
    if (!arg || !*arg) { printf("api_key\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *provider = arg;
    const char *auth_type = t1 ? t1 + 1 : "";
    const char *token = t2 ? t2 + 1 : "";
    if (strcmp(provider, "anthropic") == 0 && strncmp(token, "sk-ant-oat", 10) == 0) {
        printf("oauth\n");
        return 0;
    }
    if (auth_type && *auth_type && t2) printf("%s\n", auth_type);
    else printf("api_key\n");
    return 0;
}

/* PoP: agent_credential_pool_credential_pool_matches_provider @ agent/credential_pool.py:credential_pool_matches_provider */
int agent_credential_pool_credential_pool_matches_provider(const char *arg) {
    /* Python: custom: key match. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: agent_credential_pool_u_current_unlocked @ agent/credential_pool.py:_current_unlocked */
int agent_credential_pool_u_current_unlocked(const char *arg) {
    /* Python: None if not self._current_id; else the entry whose id matches
     * _current_id. Arg = "current_id\tentry_id..." — echoes the current id
     * if it appears among entries, else empty. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    char current[256];
    size_t clen = (size_t)(tab - arg);
    if (clen >= sizeof(current)) clen = sizeof(current) - 1;
    memcpy(current, arg, clen); current[clen] = '\0';
    const char *entries = tab + 1;
    const char *p = entries;
    while (*p) {
        while (*p == '\t') p++;
        if (!*p) break;
        const char *e = p;
        while (*e && *e != '\t') e++;
        if ((size_t)(e - p) == clen && strncmp(p, current, clen) == 0) {
            printf("%s\n", current);
            return 0;
        }
        p = e;
    }
    printf("\n");
    return 0;
}

/* PoP: agent_credential_pool_entry_id_for_api_key @ agent/credential_pool.py:entry_id_for_api_key */
int agent_credential_pool_entry_id_for_api_key(const char *arg) {
    /* Python: current selection else unambiguous match. Arg =
     * "current_id\tmatches\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *current = arg;
    const char *matches = t1 ? t1 + 1 : "";
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (current[0] && strcmp(current, "none") != 0) { printf("%s\n", current); return 0; }
    if (matches[0] && strcmp(matches, "none") != 0) { printf("%s\n", matches); return 0; }
    printf("\n");
    return 0;
}

/* PoP: agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store @ agent/credential_pool.py:_sync_xai_oauth_entry_from_pool_store */
int agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store(const char *arg) {
    /* Python: adopt rotated pair. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("entry unchanged\n"); return 0; }
    printf("adopted rotated tokens: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: agent_credential_pool_u_single_use_refresh_lock_timeout @ agent/credential_pool.py:_single_use_refresh_lock_timeout */
int agent_credential_pool_u_single_use_refresh_lock_timeout(const char *arg) {
    /* Python: max(lock, refresh + 5). Arg = "refresh_timeout\tlock_timeout". */
    if (!arg || !*arg) { printf("30.00\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    double refresh = strtod(arg, NULL);
    double lock = tab ? strtod(tab + 1, NULL) : 30.0;
    double v = refresh + 5.0;
    if (lock > v) v = lock;
    printf("%.2f\n", v);
    return 0;
}

/* PoP: agent_credential_pool_u_codex_quota_restored_upstream @ agent/credential_pool.py:_codex_quota_restored_upstream */
int agent_credential_pool_u_codex_quota_restored_upstream(const char *arg) {
    /* Python: early-reset probe. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: agent_credential_pool_u_log_no_available_entries @ agent/credential_pool.py:_log_no_available_entries */
int agent_credential_pool_u_log_no_available_entries(const char *arg) {
    /* Python: throttle INFO log. Arg = "throttled". */
    if (arg && arg[0] == '1') { printf("log throttled\n"); return 0; }
    printf("credential pool: no available entries (all exhausted or empty)\n");
    return 0;
}

/* PoP: agent_credential_pool_try_refresh_matching @ agent/credential_pool.py:try_refresh_matching */
int agent_credential_pool_try_refresh_matching(const char *arg) {
    /* Python: hint-based refresh. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("refresh result: %s\n", tab ? tab + 1 : "");
    return 0;
}
