/*
 * kanban_pr_acceptance.c — C home for upstream Hermes hermes_cli/kanban_pr_acceptance.py.
 * Gate w26_kanbanprac_090283.
 *
 * PoP: w26_kanbanprac_090283_validate_contract @ hermes_cli/kanban_pr_acceptance.py:validate_contract
 * PoP: w26_kanbanprac_090283_classify @ hermes_cli/kanban_pr_acceptance.py:_classify
 * PoP: w26_kanbanprac_090283_constants @ hermes_cli/kanban_pr_acceptance.py:_GH_LOGIN_KEYS / _GH_GITHUB_LOGIN_KEYS / _GH_TRANSPORT_KEYS / _REPO / _PR
 *
 * Every network call (``gh api``), the subprocess env and the SQLite receipt
 * write are seams.  Ported here is what decides a card's completion *without*
 * the network: which completion contracts are legal at all, and how one check
 * run / legacy status on the exact captured head is classified.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_kanbanprac_090283.h"

/* _REPO = re.compile(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+") */
static int kp_repo_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
           || c == '_' || c == '.' || c == '-';
}

/* _REPO.fullmatch */
static int kp_is_repo(const char *s) {
    const char *slash;
    const char *p;
    size_t left, right;
    if (!s) return 0;
    slash = strchr(s, '/');
    if (!slash) return 0;
    if (strchr(slash + 1, '/')) return 0;
    left = (size_t)(slash - s);
    right = strlen(slash + 1);
    if (left == 0 || right == 0) return 0;
    for (p = s; p < slash; p++) if (!kp_repo_char(*p)) return 0;
    for (p = slash + 1; *p; p++) if (!kp_repo_char(*p)) return 0;
    return 1;
}

/* _PR.fullmatch: https://github.com/<owner>/<repo>/pull/<n>, n starts 1-9. */
#define KP_PR_PREFIX "https://github.com/"
static int kp_is_pr(const char *s) {
    const char *rest, *pull, *num;
    char *repo;
    size_t repo_len;
    int ok;
    if (!s) return 0;
    if (strncmp(s, KP_PR_PREFIX, strlen(KP_PR_PREFIX)) != 0) return 0;
    rest = s + strlen(KP_PR_PREFIX);
    pull = strstr(rest, "/pull/");
    if (!pull) return 0;
    /* the ".html" and query forms are not an exact PR URL */
    num = pull + strlen("/pull/");
    if (!*num) return 0;
    if (num[0] < '1' || num[0] > '9') return 0;
    for (const char *p = num + 1; *p; p++) if (*p < '0' || *p > '9') return 0;
    repo_len = (size_t)(pull - rest);
    repo = (char *)malloc(repo_len + 1);
    if (!repo) return 0;
    memcpy(repo, rest, repo_len);
    repo[repo_len] = '\0';
    ok = kp_is_repo(repo);
    free(repo);
    return ok;
}

json_t *w26_kanbanprac_090283_validate_contract(const json_t *value) {
    json_t *out = json_object();
    const char *s;
    if (!value || value->type == JSON_NULL) {
        json_set(out, "ok", json_bool(true));
        json_set(out, "value", json_string("local-only"));
        return out;
    }
    if (value->type == JSON_STRING && value->str_val
        && strcmp(value->str_val, "local-only") == 0) {
        json_set(out, "ok", json_bool(true));
        json_set(out, "value", json_string("local-only"));
        return out;
    }
    s = value->type == JSON_STRING ? value->str_val : NULL;
    if (!s || (!kp_is_repo(s) && !kp_is_pr(s))) {
        json_set(out, "ok", json_bool(false));
        json_set(out, "error",
                 json_string("completion_contract must be local-only, OWNER/REPO, "
                             "or an exact GitHub PR URL"));
        return out;
    }
    json_set(out, "ok", json_bool(true));
    json_set(out, "value", json_string(s));
    return out;
}

/* check.get("head_sha", check.get("sha")) */
static const json_t *kp_head_sha(const json_t *check) {
    const json_t *v;
    if (!check || check->type != JSON_OBJECT) return NULL;
    v = json_obj_get(check, "head_sha");
    if (v) return v;
    return json_obj_get(check, "sha");
}

json_t *w26_kanbanprac_090283_classify(const json_t *check, const char *sha, int is_run) {
    const json_t *head = kp_head_sha(check);
    const json_t *status, *outcome;
    if (!sha) sha = "";
    /* Old-head successes are never transferable. */
    if (!head || head->type != JSON_STRING || !head->str_val
        || strcmp(head->str_val, sha) != 0)
        return json_string("stale");
    if (is_run) {
        status = json_obj_get(check, "status");
        if (!status || status->type != JSON_STRING || !status->str_val
            || strcmp(status->str_val, "completed") != 0)
            return json_string("pending");
        outcome = json_obj_get(check, "conclusion");
    } else {
        outcome = json_obj_get(check, "state");
    }
    if (outcome && outcome->type == JSON_STRING && outcome->str_val) {
        if (strcmp(outcome->str_val, "success") == 0) return json_string("success");
        if (strcmp(outcome->str_val, "failure") == 0) return json_string("failure");
        if (strcmp(outcome->str_val, "pending") == 0) return json_string("pending");
    }
    return json_string("infra");
}

json_t *w26_kanbanprac_090283_constants(void) {
    json_t *o = json_object();
    json_t *login = json_array(), *github = json_array(), *transport = json_array();
    /* _GH_LOGIN_KEYS: every key gh can read a login from. */
    static const char *const KP_LOGIN_KEYS[] = {
        "GH_TOKEN", "GITHUB_TOKEN", "GH_ENTERPRISE_TOKEN", "GITHUB_ENTERPRISE_TOKEN",
        "GH_HOST", "GH_CONFIG_DIR",
    };
    /* _GH_GITHUB_LOGIN_KEYS: the ones github.com actually honours. */
    static const char *const KP_GITHUB_KEYS[] = {"GH_TOKEN", "GITHUB_TOKEN", "GH_CONFIG_DIR"};
    /* _GH_TRANSPORT_KEYS: not a login, but gh cannot dial without them. */
    static const char *const KP_TRANSPORT_KEYS[] = {
        "HTTPS_PROXY", "HTTP_PROXY", "ALL_PROXY", "NO_PROXY", "https_proxy",
        "http_proxy", "all_proxy", "no_proxy", "SSL_CERT_FILE", "SSL_CERT_DIR",
    };
    size_t i;
    for (i = 0; i < sizeof(KP_LOGIN_KEYS) / sizeof(KP_LOGIN_KEYS[0]); i++)
        json_append(login, json_string(KP_LOGIN_KEYS[i]));
    for (i = 0; i < sizeof(KP_GITHUB_KEYS) / sizeof(KP_GITHUB_KEYS[0]); i++)
        json_append(github, json_string(KP_GITHUB_KEYS[i]));
    for (i = 0; i < sizeof(KP_TRANSPORT_KEYS) / sizeof(KP_TRANSPORT_KEYS[0]); i++)
        json_append(transport, json_string(KP_TRANSPORT_KEYS[i]));
    json_set(o, "gh_login_keys", login);
    json_set(o, "gh_github_login_keys", github);
    json_set(o, "gh_transport_keys", transport);
    json_set(o, "repo_pattern", json_string("[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+"));
    json_set(o, "pr_pattern",
             json_string("https://github\\.com/([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+)"
                         "/pull/([1-9][0-9]*)"));
    return o;
}
