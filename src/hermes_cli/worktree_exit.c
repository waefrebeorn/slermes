/*
 * worktree_exit.c — faithful C11 port of hermes_cli/worktree_exit.py.
 * Gate w11_worktreeex_93e054.
 *
 * PoP: w11_worktreeex_93e054_worktree_state @ hermes_cli/worktree_exit.py:worktree_state
 * PoP: w11_worktreeex_93e054_ask_exit_choice @ hermes_cli/worktree_exit.py:ask_exit_choice
 */
#include "hermes_json.h"
#include "noc_w11_worktreeex_93e054.h"
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

const char *w11_worktreeex_93e054_worktree_state(const struct json_t *info,
                                                 int is_dir, int is_dirty,
                                                 int has_unpushed) {
    json_t *path = json_obj_get(info, "path");
    if (!py_truthy(path) || !is_dir) return NULL;
    if (is_dirty) return "dirty";
    if (has_unpushed) return "unpushed";
    return NULL;
}

const char *w11_worktreeex_93e054_ask_exit_choice(const struct json_t *info,
                                                  int has_tty, const struct json_t *state,
                                                  int still_in_use,
                                                  const struct json_t *curses_idx,
                                                  int curses_error) {
    /* if not info or not _has_tty(): return KEEP */
    if (!info || info->type != JSON_OBJECT || json_object_size(info) == 0 || !has_tty)
        return "keep";
    /* state = worktree_state(info); if state is None: return KEEP */
    if (!state || state->type == JSON_NULL) return "keep";
    /* if _still_in_use(info["path"]): return KEEP */
    if (still_in_use) return "keep";
    /* curses failure -> idx is None -> KEEP */
    if (curses_error) return "keep";
    /* {1: ARCHIVE, 2: DISCARD}.get(idx, KEEP) if isinstance(idx, int) else KEEP */
    if (curses_idx && curses_idx->type == JSON_NUMBER) {
        long idx = (long)curses_idx->num_val;
        if (idx == 1) return "archive";
        if (idx == 2) return "discard";
    }
    return "keep";
}
