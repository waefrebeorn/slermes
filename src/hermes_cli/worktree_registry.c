/*
 * worktree_registry.c — C port of upstream Hermes hermes_cli/worktree_registry.py.
 *
 * PoP: w16_worktreere_0628f9_read_repos @ hermes_cli/worktree_registry.py:_read
 * PoP: w16_worktreere_0628f9_registered @ hermes_cli/worktree_registry.py:registered_repos
 * PoP: w16_worktreere_0628f9_merge_kept @ hermes_cli/worktree_registry.py:record_worktree_repo
 *
 * Filesystem reads, the .git existence check, and the cross-process lock are
 * injected as explicit fixture arguments; no filesystem is touched here.
 */
#include "noc_w16_worktreere_0628f9.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

/* _REGISTRY_NAME @ hermes_cli/worktree_registry.py:_REGISTRY_NAME */
static const char *const _REGISTRY_NAME = "worktree_repos.json";

static int in_existing(const struct json_t *existing, const char *name) {
    if (!json_is_array(existing) || !name) return 0;
    for (size_t i = 0; i < json_len(existing); i++) {
        const json_t *v = json_get(existing, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, name)) return 1;
    }
    return 0;
}

static int in_array(const struct json_t *arr, const char *name) {
    if (!json_is_array(arr) || !name) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, name)) return 1;
    }
    return 0;
}

struct json_t *w16_worktreere_0628f9_read_repos(const struct json_t *data) {
    (void)_REGISTRY_NAME;
    struct json_t *out = json_array();
    if (!json_is_object(data)) return out;
    const json_t *repos = json_obj_get(data, "repos");
    if (!json_is_array(repos)) return out;
    for (size_t i = 0; i < json_len(repos); i++) {
        const json_t *r = json_get(repos, i);
        if (r && r->type == JSON_STRING && r->str_val && r->str_val[0])
            json_append(out, json_copy(r));
    }
    return out;
}

struct json_t *w16_worktreere_0628f9_registered(const struct json_t *repos,
                                               const struct json_t *existing) {
    struct json_t *out = json_array();
    if (!json_is_array(repos)) return out;
    for (size_t i = 0; i < json_len(repos); i++) {
        const json_t *r = json_get(repos, i);
        if (r && r->type == JSON_STRING && r->str_val && in_existing(existing, r->str_val))
            json_append(out, json_copy(r));
    }
    return out;
}

struct json_t *w16_worktreere_0628f9_merge_kept(const struct json_t *current,
                                               const char *repo,
                                               const struct json_t *existing) {
    struct json_t *out = json_array();
    if (!repo || !repo[0]) {  /* Python: if not repo_root: return */
        if (json_is_array(current))
            for (size_t i = 0; i < json_len(current); i++)
                json_append(out, json_copy(json_get(current, i)));
        return out;
    }
    if (json_is_array(current)) {
        for (size_t i = 0; i < json_len(current); i++) {
            const json_t *r = json_get(current, i);
            if (!r || r->type != JSON_STRING || !r->str_val || !r->str_val[0]) continue;
            if (in_array(out, r->str_val)) continue;          /* dict.fromkeys */
            if (!in_existing(existing, r->str_val)) continue; /* _repo_exists filter */
            json_append(out, json_copy(r));
        }
    }
    if (!in_array(out, repo)) json_append(out, json_string(repo));
    return out;
}
