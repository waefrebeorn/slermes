/*
 * update_custody.c — C home for upstream Hermes hermes_cli/update_custody.py.
 *
 * PoP: w12_updatecust_05309c_git_subcommand @ hermes_cli/update_custody.py:git_subcommand
 * PoP: w12_updatecust_05309c_is_local_mutator @ hermes_cli/update_custody.py:is_local_mutator
 * PoP: w12_updatecust_05309c_git_argv @ hermes_cli/update_custody.py:git_argv
 */
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

#include "noc_w12_updatecust_05309c.h"

static const char *const GLOBAL_WITH_VALUE[] = {
    "-c", "-C", "--git-dir", "--work-tree", "--namespace", "--exec-path",
    "--config-env", "--super-prefix", "--attr-source", "--list-cmds"
};
static const char *const MUTATORS[] = {
    "add", "am", "apply", "checkout", "checkout-index", "cherry-pick", "clean", "commit",
    "gc", "merge", "mv", "pack-objects", "read-tree", "rebase", "reset", "restore", "revert",
    "rm", "stash", "switch", "update-index", "update-ref", "symbolic-ref", "tag", "branch", "worktree"
};

static int str_in(const char *const *set, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++) if (!strcmp(set[i], s)) return 1;
    return 0;
}
static int is_global_value(const char *s) {
    return str_in(GLOBAL_WITH_VALUE, sizeof(GLOBAL_WITH_VALUE)/sizeof(GLOBAL_WITH_VALUE[0]), s);
}
static int is_mutator(const char *s) {
    return str_in(MUTATORS, sizeof(MUTATORS)/sizeof(MUTATORS[0]), s);
}
static const char *arg_str(const json_t *arr, size_t i) {
    json_t *v = json_get(arr, i);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* git_subcommand over one array starting at `start`, with a carried skip flag. */
static const char *sub_one(const json_t *arr, size_t start, int *skip) {
    for (size_t i = start; i < json_len(arr); i++) {
        if (*skip) { *skip = 0; continue; }
        const char *a = arg_str(arr, i);
        if (is_global_value(a)) { *skip = 1; continue; }
        if (a[0] != '-') return a;   /* empty string counts, like Python's not startswith('-') */
    }
    return NULL;
}

/* git_cmd[1:] + args, as one iterator. */
static const char *sub_cmd_plus_args(const json_t *git_cmd, const json_t *args) {
    int skip = 0;
    const char *r = sub_one(git_cmd, git_cmd && json_len(git_cmd) > 0 ? 1 : 0, &skip);
    if (r) return r;
    return sub_one(args, 0, &skip);
}

json_t *w12_updatecust_05309c_git_subcommand(const json_t *args) {
    int skip = 0;
    const char *r = sub_one(args, 0, &skip);
    return r ? json_string(r) : json_null();
}

json_t *w12_updatecust_05309c_is_local_mutator(const json_t *args) {
    int skip = 0;
    const char *r = sub_one(args, 0, &skip);
    return json_bool(r && is_mutator(r) ? true : false);
}

json_t *w12_updatecust_05309c_git_argv(const json_t *git_cmd, const json_t *args, const char *devnull) {
    json_t *out = json_array();
    if (git_cmd && git_cmd->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(git_cmd); i++)
            json_append(out, json_string(arg_str(git_cmd, i)));
    /* GIT_NO_DETACH */
    json_append(out, json_string("-c"));
    json_append(out, json_string("gc.autoDetach=false"));
    json_append(out, json_string("-c"));
    json_append(out, json_string("maintenance.auto=false"));
    const char *sub = sub_cmd_plus_args(git_cmd, args);
    if (sub && is_mutator(sub)) {
        /* _MUTATOR_CONFIG (devnull injected from the fixture; oracle patches the live global). */
        const char *extra[] = {"-c", "core.fsmonitor=false", "-c", "credential.helper=", "-c", NULL};
        for (size_t i = 0; i < 5; i++) json_append(out, json_string(extra[i]));
        size_t need = strlen(devnull ? devnull : "") + 32;
        char *hooks = (char *)malloc(need);
        if (hooks) {
            snprintf(hooks, need, "core.hooksPath=%s", devnull ? devnull : "");
            json_append(out, json_string(hooks));
            free(hooks);
        }
    }
    if (args && args->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(args); i++)
            json_append(out, json_string(arg_str(args, i)));
    return out;
}

void hermes_cli_update_custody_port_pending(void) { (void)0; }

