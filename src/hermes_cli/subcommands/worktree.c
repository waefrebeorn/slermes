/*
 * worktree.c -- real C11 port of the ``hermes worktree`` subcommand parser of
 * upstream Hermes hermes_cli/subcommands/worktree.py.
 *
 * PoP: w20_worktree_959719_spec @ hermes_cli/subcommands/worktree.py:build_worktree_parser
 * PoP: w20_worktree_959719_canonical_action @ hermes_cli/subcommands/worktree.py:_dispatch_worktree
 *
 * The upstream builder is an argparse declaration: three subcommands (``list``
 * with aliases ``ls``/``audit``, ``prune``, ``restore``), their flags, dests,
 * types, defaults and help strings.  It carries no runtime behaviour beyond the
 * alias rewrite in ``_dispatch_worktree``, so the C home is the declaration as
 * data plus that rewrite.  ``hermes_cli.worktree_cmd.cmd_worktree`` (the real
 * work) stays upstream.
 */
#include "noc_w20_worktree_959719.h"

#include <string.h>

#define WT_NARGS_NONE (-1)

enum { WT_DEF_NULL = 0, WT_DEF_FALSE = 1, WT_DEF_TRUE = 2 };

typedef struct {
    const char *flags;   /* space-joined option strings; "" for a positional */
    const char *dest;
    const char *action;
    int nargs;           /* WT_NARGS_NONE for None */
    int default_kind;
    const char *type;    /* "float" or NULL */
    const char *metavar;
    const char *help;
} wt_opt;

typedef struct {
    const char *name;
    const char *aliases; /* space-joined, "" when none */
    const char *help;
    const wt_opt *options;
    int count;
} wt_sub;

static const wt_opt WT_LIST[] = {
    {"--repo", "repo", "store", WT_NARGS_NONE, WT_DEF_NULL, NULL, NULL,
     "Repo root (default: current repo)"},
    {"--json", "json", "store_true", 0, WT_DEF_FALSE, NULL, NULL,
     "Machine-readable audit output (trees, external trees, branches)"},
    {"--older-than", "older_than", "store", WT_NARGS_NONE, WT_DEF_NULL, "float", "DAYS",
     "Treat reapable trees younger than DAYS as keep"},
};

static const wt_opt WT_PRUNE[] = {
    {"--repo", "repo", "store", WT_NARGS_NONE, WT_DEF_NULL, NULL, NULL,
     "Repo root (default: current repo)"},
    {"--json", "json", "store_true", 0, WT_DEF_FALSE, NULL, NULL,
     "Machine-readable result (actions taken/planned, preserved trees)"},
    {"--older-than", "older_than", "store", WT_NARGS_NONE, WT_DEF_NULL, "float", "DAYS",
     "Only reap trees idle for at least DAYS days (safety gates still apply)"},
    {"--dry-run", "dry_run", "store_true", 0, WT_DEF_FALSE, NULL, NULL,
     "Show the plan without changing anything"},
    {"--trees-only", "trees_only", "store_true", 0, WT_DEF_FALSE, NULL, NULL,
     "Only remove worktrees; leave local branches alone"},
    {"--branches-only", "branches_only", "store_true", 0, WT_DEF_FALSE, NULL, NULL,
     "Only delete merged local branches; leave worktrees alone"},
};

static const wt_opt WT_RESTORE[] = {
    {"", "name", "store", WT_NARGS_NONE, WT_DEF_NULL, NULL, NULL,
     "Archive name or tree name (see `hermes worktree list`)"},
    {"--path", "path", "store", WT_NARGS_NONE, WT_DEF_NULL, NULL, NULL,
     "Restore here instead of the original path"},
    {"--repo", "repo", "store", WT_NARGS_NONE, WT_DEF_NULL, NULL, NULL,
     "Repo root (default: current repo)"},
};

static const wt_sub WT_SUBS[] = {
    {"list", "ls audit",
     "Classify every tree: age, size, verdict, reason (default action)",
     WT_LIST, (int)(sizeof(WT_LIST) / sizeof(WT_LIST[0]))},
    {"prune", "",
     "Remove safe trees and delete fully-merged local branches",
     WT_PRUNE, (int)(sizeof(WT_PRUNE) / sizeof(WT_PRUNE[0]))},
    {"restore", "",
     "Recreate an archived worktree (tracked, staged and untracked files)",
     WT_RESTORE, (int)(sizeof(WT_RESTORE) / sizeof(WT_RESTORE[0]))},
};

static const char *const WT_ROOT_HELP =
    "Audit and reclaim accumulated git worktrees and merged branches";
static const char *const WT_ROOT_DESC =
    "Audit, reclaim and restore the .worktrees/ trees Hermes sessions accumulate. "
    "Idle trees with uncommitted changes or unpushed commits are archived to "
    "refs/hermes/archive/<tree> before removal (worktrees.archive_dirty); `restore` "
    "brings one back. In-use trees are never touched. See: "
    "https://hermes-agent.nousresearch.com/docs/user-guide/git-worktrees";

/* Split a space-joined field into a JSON array of non-empty words. */
static json_t *wt_words(const char *joined) {
    json_t *arr = json_array();
    const char *p = joined ? joined : "";
    while (*p) {
        const char *start;
        char buf[256];
        size_t n;
        while (*p == ' ') p++;
        if (!*p) break;
        start = p;
        while (*p && *p != ' ') p++;
        n = (size_t)(p - start);
        if (n >= sizeof(buf)) n = sizeof(buf) - 1;
        memcpy(buf, start, n);
        buf[n] = '\0';
        json_append(arr, json_string(buf));
    }
    return arr;
}

static json_t *wt_opt_json(const wt_opt *o) {
    json_t *j = json_object();
    json_set(j, "flags", wt_words(o->flags));
    json_set(j, "dest", json_string(o->dest));
    json_set(j, "action", json_string(o->action));
    json_set(j, "nargs", o->nargs == WT_NARGS_NONE ? json_null() : json_number((double)o->nargs));
    json_set(j, "default", o->default_kind == WT_DEF_NULL ? json_null()
                          : json_bool(o->default_kind == WT_DEF_TRUE ? 1 : 0));
    json_set(j, "type", o->type ? json_string(o->type) : json_null());
    json_set(j, "metavar", o->metavar ? json_string(o->metavar) : json_null());
    json_set(j, "help", o->help ? json_string(o->help) : json_null());
    return j;
}

json_t *w20_worktree_959719_spec(void) {
    json_t *out = json_object();
    json_t *subs = json_array();
    size_t i, j;
    json_set(out, "root_help", json_string(WT_ROOT_HELP));
    json_set(out, "root_description", json_string(WT_ROOT_DESC));
    json_set(out, "dest", json_string("worktree_action"));
    for (i = 0; i < sizeof(WT_SUBS) / sizeof(WT_SUBS[0]); i++) {
        const wt_sub *s = &WT_SUBS[i];
        json_t *obj = json_object();
        json_t *opts = json_array();
        json_set(obj, "name", json_string(s->name));
        json_set(obj, "aliases", wt_words(s->aliases));
        json_set(obj, "help", json_string(s->help));
        for (j = 0; j < (size_t)s->count; j++) {
            json_append(opts, wt_opt_json(&s->options[j]));
        }
        json_set(obj, "options", opts);
        json_append(subs, obj);
    }
    json_set(out, "subcommands", subs);
    return out;
}

const char *w20_worktree_959719_canonical_action(const char *action) {
    if (!action) return "list";
    if (!strcmp(action, "ls") || !strcmp(action, "audit")) return "list";
    return action;
}
