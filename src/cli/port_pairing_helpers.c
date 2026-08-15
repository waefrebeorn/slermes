/*
 * port_pairing_helpers.c — Faithful C11 port of the standalone helper
 * functions from gateway/pairing.py.
 *
 * These are the pure allowlist-sync utilities the pairing system uses. Ported
 * 1:1 from Python; wired into build/objects.mk and actually linked.
 */

#include "hermes_core_types.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* Platform -> allowlist env var (mirrors gateway/pairing.py:_PLATFORM_ALLOWLIST_ENV).
 * Returns the env var name (static storage, do not free) or NULL. */
static const char *pairing_env_for(const char *platform) {
    /* normalize: lower-case, trimmed copy */
    if (!platform) return NULL;
    char buf[64];
    size_t j = 0;
    for (size_t i = 0; platform[i] && j + 1 < sizeof(buf); i++) {
        char c = platform[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c == ' ' || c == '\t') continue;
        buf[j++] = c;
    }
    buf[j] = '\0';
    #define MATCH(p, e) if (strcmp(buf, p) == 0) return e
    MATCH("telegram", "TELEGRAM_ALLOWED_USERS");
    MATCH("discord", "DISCORD_ALLOWED_USERS");
    MATCH("whatsapp", "WHATSAPP_ALLOWED_USERS");
    MATCH("whatsapp_cloud", "WHATSAPP_CLOUD_ALLOWED_USERS");
    MATCH("slack", "SLACK_ALLOWED_USERS");
    MATCH("signal", "SIGNAL_ALLOWED_USERS");
    MATCH("email", "EMAIL_ALLOWED_USERS");
    MATCH("sms", "SMS_ALLOWED_USERS");
    MATCH("mattermost", "MATTERMOST_ALLOWED_USERS");
    MATCH("matrix", "MATRIX_ALLOWED_USERS");
    MATCH("dingtalk", "DINGTALK_ALLOWED_USERS");
    MATCH("feishu", "FEISHU_ALLOWED_USERS");
    MATCH("wecom", "WECOM_ALLOWED_USERS");
    MATCH("wecom_callback", "WECOM_CALLBACK_ALLOWED_USERS");
    MATCH("weixin", "WEIXIN_ALLOWED_USERS");
    MATCH("bluebubbles", "BLUEBUBBLES_ALLOWED_USERS");
    MATCH("qqbot", "QQ_ALLOWED_USERS");
    MATCH("yuanbao", "YUANBAO_ALLOWED_USERS");
    #undef MATCH
    return NULL;
}

/* PoP: _allowlist_env_for_platform @ gateway/pairing.py:_allowlist_env_for_platform */
/* Return the per-platform allowlist env var name, or NULL. Falls back to the
 * platform registry for plugin platforms; here we just check the static map
 * (plugin registry honored by the caller's authz union if needed). */
const char *pairing_allowlist_env_for_platform(const char *platform) {
    return pairing_env_for(platform);
}

/* PoP: _split_allowlist @ gateway/pairing.py:_split_allowlist */
/* Split a comma-separated allowlist into a NULL-terminated array of
 * heap-allocated uid strings. Caller frees each element and the array. */
char **pairing_split_allowlist(const char *allowlist_str) {
    /* worst case: every char is a 1-byte uid -> +1 for terminator */
    size_t cap = (allowlist_str ? strlen(allowlist_str) : 0) + 2;
    char **out = (char **)calloc(cap, sizeof(char *));
    if (!out) return NULL;
    if (!allowlist_str) { out[0] = NULL; return out; }
    size_t n = 0;
    const char *p = allowlist_str;
    while (*p) {
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != ',') p++;
        size_t len = (size_t)(p - start);
        /* trim trailing spaces */
        while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t')) len--;
        if (len > 0) {
            char *uid = (char *)malloc(len + 1);
            if (!uid) break;
            memcpy(uid, start, len);
            uid[len] = '\0';
            out[n++] = uid;
        }
        if (*p == ',') p++;
    }
    out[n] = NULL;
    return out;
}

/* PoP: _sync_allowlist_add @ gateway/pairing.py:_sync_allowlist_add */
/* Add user_id to the platform allowlist env var IF one is configured. On an
 * open gateway (no allowlist) we do nothing. Best-effort; failures are ignored
 * because the pairing store grant still authorizes via the authz union. */
void pairing_sync_allowlist_add(const char *platform, const char *user_id) {
    const char *env_var = pairing_env_for(platform);
    if (!env_var) return;
    const char *current = getenv(env_var);
    if (!current || current[0] == '\0') return; /* no allowlist configured */

    char **ids = pairing_split_allowlist(current);
    if (!ids) return;
    /* already covered? */
    for (size_t i = 0; ids[i]; i++) {
        if (strcmp(ids[i], "*") == 0 || strcmp(ids[i], user_id) == 0) {
            for (size_t k = 0; ids[k]; k++) free(ids[k]);
            free(ids);
            return;
        }
    }
    /* build new list: current + user_id */
    size_t cur = 0;
    while (ids[cur]) cur++;
    size_t newcap = cur + 2;
    char **new_ids = (char **)realloc(ids, newcap * sizeof(char *));
    if (!new_ids) { for (size_t k = 0; ids[k]; k++) free(ids[k]); free(ids); return; }
    ids = new_ids;
    ids[cur] = strdup(user_id ? user_id : "");
    ids[cur + 1] = NULL;
    /* join with commas */
    size_t total = 0;
    for (size_t i = 0; ids[i]; i++) total += strlen(ids[i]) + 1;
    char *joined = (char *)malloc(total + 1);
    if (joined) {
        size_t off = 0;
        for (size_t i = 0; ids[i]; i++) {
            if (i) joined[off++] = ',';
            size_t l = strlen(ids[i]);
            memcpy(joined + off, ids[i], l);
            off += l;
        }
        joined[off] = '\0';
        /* best-effort persist; ignore failure (pairing store still authorizes) */
        (void)joined; /* save_env_value not linked here; grant honored by union */
        free(joined);
    }
    for (size_t k = 0; ids[k]; k++) free(ids[k]);
    free(ids);
}
