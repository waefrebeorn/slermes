/*
 * setup.c -- real C11 port of plugins/platforms/signal/setup.py.
 *
 * PoP: w22_setup_424f2e_run @ plugins/platforms/signal/setup.py:interactive_setup
 *
 * Terminal prompts, prints, `shutil.which` and the httpx probe are seams; the
 * wizard flow (defaults, cancel points, retry questions, the four env writes)
 * is pure and reproduced here over a scripted answer list.
 */
#include "hermes_json.h"
#include "noc_w22_setup_424f2e.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ST08013_DEFAULT_URL "http://127.0.0.1:8080"
#define ST08013_MAX_SAVES 8

typedef struct {
    const char *key;
    char *value;
} st08013_save;

typedef struct {
    const json_t *inputs;
    size_t input_i;
    const json_t *yes;
    size_t yes_i;
} st08013_script;

static char *st08013_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python str.strip(): ASCII whitespace on both ends. */
static char *st08013_strip_dup(const char *s) {
    size_t b = 0, e;
    if (!s) s = "";
    e = strlen(s);
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n' ||
                     s[b] == '\v' || s[b] == '\f')) b++;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' ||
                     s[e - 1] == '\n' || s[e - 1] == '\v' || s[e - 1] == '\f')) e--;
    {
        size_t n = e - b;
        char *out = (char *)malloc(n + 1);
        if (!out) return NULL;
        memcpy(out, s + b, n);
        out[n] = '\0';
        return out;
    }
}

/* None on EOF (a JSON null answer) or a non-string answer; the caller cancels. */
static char *st08013_next_input(st08013_script *sc) {
    const json_t *node;
    if (!sc->inputs || sc->inputs->type != JSON_ARRAY || sc->input_i >= sc->inputs->c.count)
        return NULL;
    node = sc->inputs->c.items[sc->input_i++];
    if (!node || node->type != JSON_STRING) return NULL;
    return st08013_strip_dup(node->str_val);
}

static int st08013_next_yes(st08013_script *sc, int def) {
    const json_t *node;
    if (!sc->yes || sc->yes->type != JSON_ARRAY || sc->yes_i >= sc->yes->c.count) return def;
    node = sc->yes->c.items[sc->yes_i++];
    if (!node) return 0;
    switch (node->type) {
    case JSON_BOOL: return node->bool_val ? 1 : 0;
    case JSON_NULL: return 0;
    case JSON_NUMBER: return node->num_val != 0.0;
    case JSON_STRING: return node->str_val && node->str_val[0];
    default: return 1;
    }
}

static void st08013_put(st08013_save *saves, size_t *nsaves, char **slot,
                        const char *key, char *value) {
    if (*nsaves >= ST08013_MAX_SAVES) {
        free(value);
        return;
    }
    saves[*nsaves].key = key;
    saves[*nsaves].value = value;
    (*nsaves)++;
    free(*slot);
    *slot = st08013_dup(value);
}

static char *st08013_rstrip_slashes(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    while (n > 0 && s[n - 1] == '/') n--;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *st08013_join_check(const char *url) {
    char *base = st08013_rstrip_slashes(url);
    const char *tail = "/api/v1/check";
    size_t n;
    char *out;
    if (!base) return NULL;
    n = strlen(base) + strlen(tail) + 1;
    out = (char *)malloc(n);
    if (!out) {
        free(base);
        return NULL;
    }
    snprintf(out, n, "%s%s", base, tail);
    free(base);
    return out;
}

json_t *w22_setup_424f2e_run(const json_t *req) {
    st08013_script sc;
    st08013_save saves[ST08013_MAX_SAVES];
    size_t nsaves = 0, i;
    const json_t *http;
    const char *kind = "ok";
    int reconfigure, found, outcome_ok = 1;
    const char *outcome = "ok";
    char *env_url, *env_account, *env_allowed, *env_groups;
    char *check = NULL, *url = NULL, *account = NULL, *typed;
    json_t *out = NULL, *arr;

    memset(&sc, 0, sizeof(sc));
    memset(saves, 0, sizeof(saves));
    if (!req || req->type != JSON_OBJECT) return NULL;

    env_url = st08013_dup(json_get_str(req, "existing_url", ""));
    env_account = st08013_dup(json_get_str(req, "existing_account", ""));
    env_allowed = st08013_dup(json_get_str(req, "existing_allowed", ""));
    env_groups = st08013_dup(json_get_str(req, "existing_groups", ""));
    sc.inputs = json_obj_get(req, "inputs");
    sc.yes = json_obj_get(req, "yes");
    reconfigure = json_get_bool(req, "reconfigure", 1);
    found = json_get_bool(req, "which", 0);
    http = json_obj_get(req, "http");
    if (http && http->type == JSON_OBJECT) kind = json_get_str(http, "kind", "ok");
    if (!kind || !*kind) kind = "ok";

    if (!reconfigure) {
        outcome = "declined_reconfigure";
        goto done;
    }

    typed = st08013_next_input(&sc);
    if (!typed) {
        outcome = "cancelled_url";
        goto done;
    }
    if (!*typed) {
        free(typed);
        typed = st08013_dup((env_url && *env_url) ? env_url : ST08013_DEFAULT_URL);
        if (!typed) {
            outcome = "cancelled_url";
            goto done;
        }
    }
    url = typed;
    check = st08013_join_check(url);

    if (!strcmp(kind, "status")) {
        int status = (int)json_get_num(http, "status", 0);
        if (status != 200 && !st08013_next_yes(&sc, 0)) {
            outcome = "declined_url";
            goto done;
        }
    } else if (!strcmp(kind, "raise")) {
        if (!st08013_next_yes(&sc, 1)) {
            outcome = "declined_url";
            goto done;
        }
    }
    st08013_put(saves, &nsaves, &env_url, "SIGNAL_HTTP_URL", st08013_dup(url));

    typed = st08013_next_input(&sc);
    if (!typed) {
        outcome = "cancelled_account";
        goto done;
    }
    if (!*typed) {
        free(typed);
        typed = st08013_dup(env_account ? env_account : "");
        if (!typed) {
            outcome = "cancelled_account";
            goto done;
        }
    }
    account = typed;
    if (!*account) {
        outcome = "account_required";
        goto done;
    }
    st08013_put(saves, &nsaves, &env_account, "SIGNAL_ACCOUNT", st08013_dup(account));

    {
        const char *fallback = (env_allowed && *env_allowed) ? env_allowed : account;
        typed = st08013_next_input(&sc);
        if (!typed) {
            outcome = "cancelled_allowed";
            goto done;
        }
        if (!*typed) {
            free(typed);
            typed = st08013_dup(fallback);
            if (!typed) {
                outcome = "cancelled_allowed";
                goto done;
            }
        }
        st08013_put(saves, &nsaves, &env_allowed, "SIGNAL_ALLOWED_USERS", typed);
        typed = NULL;
    }

    if (st08013_next_yes(&sc, 0)) {
        typed = st08013_next_input(&sc);
        if (!typed) {
            outcome = "cancelled_group";
            goto done;
        }
        if (!*typed) {
            free(typed);
            if (env_groups && *env_groups) typed = st08013_dup(env_groups);
            else typed = st08013_dup("*");
            if (!typed) {
                outcome = "cancelled_group";
                goto done;
            }
        }
        st08013_put(saves, &nsaves, &env_groups, "SIGNAL_GROUP_ALLOWED_USERS", typed);
        typed = NULL;
    }

done:
    (void)outcome_ok;
    out = json_object();
    if (!out) goto cleanup;
    arr = json_array();
    for (i = 0; i < nsaves; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(saves[i].key));
        json_append(pair, json_string(saves[i].value));
        json_append(arr, pair);
    }
    json_set(out, "saves", arr);
    json_set(out, "outcome", json_string(outcome));
    json_set(out, "found", json_bool(found ? 1 : 0));
    json_set(out, "checked_url", check ? json_string(check) : json_null());
    json_set(out, "groups_line",
             json_string((env_groups && *env_groups) ? "enabled" : "disabled"));

cleanup:
    free(env_url);
    free(env_account);
    free(env_allowed);
    free(env_groups);
    free(check);
    free(url);
    free(account);
    for (i = 0; i < nsaves; i++) free(saves[i].value);
    return out;
}
