/* setup_pure.c — Faithful C11 ports of the small, pure, standalone helpers
 * from hermes_cli/setup.py that have no curses / I/O / async dependencies.
 *
 * PoP (Python -> C):
 *   _get_credential_pool_strategies  -> setup_get_credential_pool_strategies
 *   _set_credential_pool_strategy    -> setup_set_credential_pool_strategy
 *   _set_reasoning_effort             -> setup_set_reasoning_effort
 *   is_noninteractive                 -> setup_is_noninteractive
 *   _is_valid_telegram_bot_token     -> setup_is_valid_telegram_bot_token
 *   _profile_name_from_hermes_home   -> setup_profile_name_from_hermes_home
 *
 * Callers receive freshly malloc'd strings (free with free()) or NULL.
 * JSON dict helpers operate on json_t* (libjson) — matching Python's
 * config-dict semantics: missing keys -> NULL/empty, scalar coercion, etc.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "setup_pure.h"

/* ==========================================================================
 * _get_credential_pool_strategies
 * ========================================================================== */
/* PoP: stp_get_credential_pool_strategies @ hermes_cli/setup.py:_get_credential_pool_strategies */
json_t *stp_get_credential_pool_strategies(const json_t *config) {
    if (!config) return json_object();
    const json_t *strategies = json_obj_get(config, "credential_pool_strategies");
    if (!strategies || !json_is_object(strategies)) {
        return json_object();
    }
    return json_copy(strategies);
}

/* ==========================================================================
 * _set_credential_pool_strategy
 * ========================================================================== */
/* PoP: stp_set_credential_pool_strategy @ hermes_cli/setup.py:_set_credential_pool_strategy */
void stp_set_credential_pool_strategy(json_t *config,
                                        const char *provider,
                                        const char *strategy) {
    if (!config || !provider || !provider[0]) return;
    json_t *strategies = json_obj_get(config, "credential_pool_strategies");
    if (!strategies || !json_is_object(strategies)) {
        strategies = json_object();
        json_set(config, "credential_pool_strategies", strategies);
    }
    if (strategy && strategy[0]) {
        json_set(strategies, provider, json_string(strategy));
    } else {
        json_obj_del(strategies, provider);
    }
}

/* ==========================================================================
 * _set_reasoning_effort
 * ========================================================================== */
/* PoP: stp_set_reasoning_effort @ hermes_cli/setup.py:_set_reasoning_effort */
void stp_set_reasoning_effort(json_t *config, const char *effort) {
    if (!config) return;
    json_t *agent = json_obj_get(config, "agent");
    if (!agent || !json_is_object(agent)) {
        agent = json_object();
        json_set(config, "agent", agent);
    }
    if (effort && effort[0]) {
        json_set(agent, "reasoning_effort", json_string(effort));
    } else {
        json_obj_del(agent, "reasoning_effort");
    }
}

/* ==========================================================================
 * is_noninteractive
 * ========================================================================== */
/* PoP: stp_is_noninteractive @ hermes_cli/setup.py:is_noninteractive */
bool stp_is_noninteractive(void) {
    const char *val = getenv("HERMES_NONINTERACTIVE");
    if (!val) return false;
    /* strip and lowercase — matching Python: .strip().lower() in {1,true,yes,on} */
    char buf[256];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    /* trim leading whitespace */
    char *p = buf;
    while (*p && isspace((unsigned char)*p)) p++;
    /* trim trailing whitespace */
    size_t len = strlen(p);
    while (len > 0 && isspace((unsigned char)p[len - 1])) len--;
    p[len] = '\0';
    /* lowercase */
    for (size_t i = 0; i < len; i++) {
        p[i] = (char)tolower((unsigned char)p[i]);
    }
    return (strcmp(p, "1") == 0 || strcmp(p, "true") == 0 ||
            strcmp(p, "yes") == 0 || strcmp(p, "on") == 0);
}

/* ==========================================================================
 * _is_valid_telegram_bot_token
 * ========================================================================== */
/* PoP: stp_is_valid_telegram_bot_token @ hermes_cli/setup.py:_is_valid_telegram_bot_token */
bool stp_is_valid_telegram_bot_token(const char *token) {
    if (!token || !token[0]) return false;
    /* Python: _TELEGRAM_BOT_TOKEN_RE.match(token)
     * Pattern: 8-10 digits then colon then AA then 34+ base64 chars. */
    size_t tlen = strlen(token);
    /* numeric prefix: 8-10 digits */
    if (tlen < 8 + 1 + 34 + 1) return false;  /* min: 8+1+35 = 44 */
    size_t i = 0;
    int digit_count = 0;
    while (i < tlen && token[i] >= '0' && token[i] <= '9') {
        digit_count++;
        i++;
    }
    if (digit_count < 8 || digit_count > 10) return false;
    /* colon */
    if (token[i] != ':') return false;
    i++;
    /* AA prefix */
    if (tlen - i < 35) return false;  /* AA + 34+ chars */
    if (token[i] != 'A' || token[i + 1] != 'A') return false;
    i += 2;
    /* remaining 34+ chars: [A0-9a-zA-Z_-] */
    if (tlen - i < 34) return false;
    for (size_t j = i; j < tlen; j++) {
        char c = token[j];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}

/* ==========================================================================
 * _profile_name_from_hermes_home
 * ========================================================================== */
/* PoP: stp_profile_name_from_hermes_home @ hermes_cli/setup.py:_profile_name_from_hermes_home */
char *stp_profile_name_from_hermes_home(const char *hermes_home_path) {
    if (!hermes_home_path) return NULL;
    /* Python: Path(hermes_home).parent.name == "profiles" -> return hermes_home.name
     * C: check if the parent directory of hermes_home_path is named "profiles" */
    char *resolved = realpath(hermes_home_path, NULL);
    if (!resolved) {
        /* fall back to the raw path if realpath fails */
        resolved = strdup(hermes_home_path);
        if (!resolved) return NULL;
    }
    /* Find the last '/' to get the profile name */
    char *last_slash = strrchr(resolved, '/');
    if (!last_slash) {
        free(resolved);
        return NULL;
    }
    /* Parent dir is everything before the last slash */
    char *parent_end = last_slash;
    char *parent_start = strrchr(resolved, '/');
    /* Find the parent's parent: go back from parent_end */
    if (parent_start == resolved) {
        /* Path is like "/profiles" — parent is "/" */
        free(resolved);
        return NULL;
    }
    /* Get parent directory name */
    *parent_end = '\0';  /* temporarily terminate at the profile name */
    char *parent_name_start = strrchr(resolved, '/');
    if (!parent_name_start) {
        free(resolved);
        return NULL;
    }
    parent_name_start++;  /* skip the '/' */
    bool is_profiles = (strcmp(parent_name_start, "profiles") == 0);
    *parent_end = '/';  /* restore */
    if (!is_profiles) {
        free(resolved);
        return NULL;
    }
    /* Return the profile name (the basename) */
    char *profile_name = strdup(last_slash + 1);
    free(resolved);
    return profile_name;
}
