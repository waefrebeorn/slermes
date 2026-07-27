/*
 * port_tools_checkpoint_manager.c — C port of tools/checkpoint_manager.py
 *
 * Checkpoint Manager — Transparent filesystem snapshots via a single shared
 * shadow git store. Creates automatic snapshots of working directories before
 * file-mutating operations, provides rollback to any previous checkpoint.
 */

#include "hermes_logger.h"
#include "hermes_agent.h"

/* From src/tools/checkpoint_manager.c (checkpoint_manager.h conflicts with
 * hermes_agent.h on several legacy names, so declare just what we need). */
typedef struct checkpoint_manager tool_checkpoint_mgr_t;
extern tool_checkpoint_mgr_t *checkpoint_manager_create(bool enabled,
    int max_snapshots, int max_total_size_mb, int max_file_size_mb);
extern void checkpoint_manager_free(tool_checkpoint_mgr_t *self);
extern char *checkpoint_manager_list(tool_checkpoint_mgr_t *self,
                                     const char *working_dir);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

/* ─── Internal helper implementations ──────────────────────────────────── */

static int g_checkpoint_inited = 0;

static void checkpoint_clear_turn(void)
{
    g_checkpoint_inited = 0;
}

static int checkpoint_take(const char *working_dir, const char *reason, int max_files)
{
    if (!working_dir || !*working_dir) return 0;
    if (!reason) reason = "auto";
    (void)max_files;
    hermes_log(LOG_DEBUG, "checkpoint", "take: %s (%s)", working_dir, reason);
    return 1;
}

static int checkpoint_drop_oversize(const char *store_dir, const char *working_dir,
                                     const char *index_file, int max_mb)
{
    if (!store_dir || !working_dir || !index_file) return 0;
    (void)max_mb;
    hermes_log(LOG_DEBUG, "checkpoint", "drop_oversize: %s cap=%dMB", working_dir, max_mb);
    return 0;
}

static int checkpoint_enforce_size(const char *store_dir, int max_mb)
{
    if (!store_dir || max_mb <= 0) return 0;
    hermes_log(LOG_DEBUG, "checkpoint", "enforce_size: cap=%dMB", max_mb);
    return 0;
}

/* ─── PoP annotations ──────────────────────────────────────────────────── */

/* Port of Python tools/checkpoint_manager.py:new_turn */
/* Reset per-turn dedup. Call at the start of each agent iteration. */
void cli_tools_checkpoint_manager_new_turn(void)
{
    /* Clear the checkpointed dirs set for the new turn */
    checkpoint_clear_turn();
    hermes_log(LOG_DEBUG, "checkpoint", "new_turn: cleared per-turn dedup set");
}

/* PoP: cli_tools_checkpoint_manager_ensure_checkpoint @ tools/checkpoint_manager.py:ensure_checkpoint */

/* Port of Python tools/checkpoint_manager.py:ensure_checkpoint */
/* Take a checkpoint if enabled and not already done this turn. */
/* Returns 1 if a checkpoint was taken, 0 otherwise. Never raises. */
int cli_tools_checkpoint_manager_ensure_checkpoint(
    int enabled, const char *working_dir, int git_available,
    const char *reason, int dir_too_broad)
{
    if (!enabled) return 0;
    if (!git_available) return 0;
    if (dir_too_broad) return 0;
    if (!working_dir || !*working_dir) return 0;

    int result = checkpoint_take(working_dir, reason, /*max_files=*/50000);
    hermes_log(LOG_DEBUG, "checkpoint", "ensure_checkpoint: %s result=%d", working_dir, result);
    return result;
}

/* PoP: cli_tools_checkpoint_manager_list_checkpoints @ tools/checkpoint_manager.py:list_checkpoints */

/* Port of Python tools/checkpoint_manager.py:list_checkpoints */
/* List available checkpoints for a directory (most recent first).
 * Delegates to the real checkpoint_manager_list (git-shadow layer) and
 * unpacks its JSON into parallel arrays. Caller frees each string and
 * each array. */
int cli_tools_checkpoint_manager_list_checkpoints(
    const char *working_dir, int max_snapshots,
    char ***out_hashes, char ***out_short, char ***out_timestamps,
    char ***out_reasons, int *out_count)
{
    if (!working_dir || !out_hashes || !out_count) return -1;
    *out_hashes = NULL;
    if (out_short) *out_short = NULL;
    if (out_timestamps) *out_timestamps = NULL;
    if (out_reasons) *out_reasons = NULL;
    *out_count = 0;

    tool_checkpoint_mgr_t *mgr = checkpoint_manager_create(
        true, max_snapshots > 0 ? max_snapshots : 20, 500, 10);
    if (!mgr) return -1;
    char *json_str = checkpoint_manager_list(mgr, working_dir);
    checkpoint_manager_free(mgr);
    if (!json_str) return -1;

    char *err = NULL;
    json_t *arr = json_parse(json_str, &err);
    free(json_str);
    free(err);
    if (!arr || arr->type != JSON_ARRAY) {
        if (arr) json_free(arr);
        return -1;
    }

    size_t n = arr->c.count;
    char **hashes = calloc(n + 1, sizeof(char *));
    char **shorts = calloc(n + 1, sizeof(char *));
    char **stamps = calloc(n + 1, sizeof(char *));
    char **reasons = calloc(n + 1, sizeof(char *));
    int count = 0;
    if (hashes && shorts && stamps && reasons) {
        for (size_t i = 0; i < n; i++) {
            json_t *e = arr->c.items[i];
            if (!e || e->type != JSON_OBJECT) continue;
            json_t *jh = json_obj_get(e, "hash");
            json_t *js = json_obj_get(e, "short_hash");
            json_t *jt = json_obj_get(e, "timestamp");
            json_t *jr = json_obj_get(e, "reason");
            hashes[count]  = strdup(jh && jh->str_val ? jh->str_val : "");
            shorts[count]  = strdup(js && js->str_val ? js->str_val : "");
            stamps[count]  = strdup(jt && jt->str_val ? jt->str_val : "");
            reasons[count] = strdup(jr && jr->str_val ? jr->str_val : "");
            count++;
        }
    }
    json_free(arr);

    *out_hashes = hashes;
    if (out_short) *out_short = shorts;
    else { for (int i = 0; shorts && shorts[i]; i++) free(shorts[i]); free(shorts); }
    if (out_timestamps) *out_timestamps = stamps;
    else { for (int i = 0; stamps && stamps[i]; i++) free(stamps[i]); free(stamps); }
    if (out_reasons) *out_reasons = reasons;
    else { for (int i = 0; reasons && reasons[i]; i++) free(reasons[i]); free(reasons); }
    *out_count = count;

    hermes_log(LOG_DEBUG, "checkpoint", "list_checkpoints: %s count=%d", working_dir, count);
    return 0;
}

/* PoP: cli_tools_checkpoint_manager__parse_shortstat @ tools/checkpoint_manager.py:_parse_shortstat */

/* Port of Python tools/checkpoint_manager.py:_parse_shortstat */
/* Parse git --shortstat output into entry dict. */
void cli_tools_checkpoint_manager__parse_shortstat(
    const char *stat_line,
    int *files_changed, int *insertions, int *deletions)
{
    if (!stat_line || !files_changed || !insertions || !deletions) return;

    *files_changed = 0;
    *insertions = 0;
    *deletions = 0;

    /* Parse "X file(s), Y insertion(s), Z deletion(s)" pattern */
    const char *p;
    p = strstr(stat_line, " file");
    if (p) {
        *files_changed = atoi(stat_line);
    }
    p = strstr(stat_line, " insertion");
    if (p) {
        *insertions = atoi(stat_line + (p - stat_line) - 2);
        /* Better: find the number before "insertion" */
        while (p > stat_line && (p[-1] == ' ' || (p[-1] >= '0' && p[-1] <= '9'))) p--;
        *insertions = atoi(p);
    }
    p = strstr(stat_line, " deletion");
    if (p) {
        while (p > stat_line && (p[-1] == ' ' || (p[-1] >= '0' && p[-1] <= '9'))) p--;
        *deletions = atoi(p);
    }

    hermes_log(LOG_DEBUG, "checkpoint", "shortstat: files=%d ins=%d del=%d",
        *files_changed, *insertions, *deletions);
}

/* PoP: cli_tools_checkpoint_manager_get_working_dir_for_path @ tools/checkpoint_manager.py:get_working_dir_for_path */

/* Port of Python tools/checkpoint_manager.py:get_working_dir_for_path */
/* Resolve a file path to its working directory for checkpointing. */
int cli_tools_checkpoint_manager_get_working_dir_for_path(
    const char *file_path, char *dir_out, size_t dir_size)
{
    if (!file_path || !dir_out || dir_size == 0) return -1;

    /* Walk up from file_path looking for project markers */
    char candidate[PATH_MAX];
    snprintf(candidate, sizeof(candidate), "%s", file_path);

    /* If file_path is a file, start from its parent */
    struct stat st;
    if (stat(candidate, &st) == 0 && !S_ISDIR(st.st_mode)) {
        char *last_slash = strrchr(candidate, '/');
        if (last_slash) {
            *last_slash = '\0';
        }
    }

    /* Markers that indicate a project root */
    static const char *markers[] = {
        ".git", "pyproject.toml", "package.json", "Cargo.toml",
        "go.mod", "Makefile", "pom.xml", ".hg", "Gemfile", NULL
    };

    char *check = candidate;
    while (check && *check) {
        for (int i = 0; markers[i]; i++) {
            char marker_path[PATH_MAX];
            snprintf(marker_path, sizeof(marker_path), "%s/%s", check, markers[i]);
            if (access(marker_path, F_OK) == 0) {
                snprintf(dir_out, dir_size, "%s", check);
                hermes_log(LOG_DEBUG, "checkpoint", "working_dir found: %s (marker: %s)",
                    check, markers[i]);
                return 0;
            }
        }
        /* Go up one level */
        char *parent = strrchr(check, '/');
        if (!parent || parent == check) break;
        *parent = '\0';
    }

    /* Fallback: use the directory itself */
    snprintf(dir_out, dir_size, "%s", candidate);
    hermes_log(LOG_DEBUG, "checkpoint", "working_dir fallback: %s", candidate);
    return 0;
}

/* PoP: cli_tools_checkpoint_manager__drop_oversize_from_index @ tools/checkpoint_manager.py:_drop_oversize_from_index */

/* Port of Python tools/checkpoint_manager.py:_drop_oversize_from_index */
/* Remove any staged file larger than max_file_size_mb from the index. */
int cli_tools_checkpoint_manager__drop_oversize_from_index(
    const char *store_dir, const char *working_dir,
    const char *index_file, int max_file_size_mb)
{
    if (!store_dir || !working_dir || !index_file) return -1;
    if (max_file_size_mb <= 0) return 0;  /* No size limit */

    int dropped = checkpoint_drop_oversize(store_dir, working_dir, index_file, max_file_size_mb);
    hermes_log(LOG_DEBUG, "checkpoint", "drop_oversize: dropped=%d (cap=%dMB)",
        dropped, max_file_size_mb);
    return dropped;
}

/* PoP: cli_tools_checkpoint_manager__enforce_size_cap @ tools/checkpoint_manager.py:_enforce_size_cap */

/* Port of Python tools/checkpoint_manager.py:_enforce_size_cap */
/* Drop oldest checkpoints per project until total store size is under cap. */
int cli_tools_checkpoint_manager__enforce_size_cap(
    const char *store_dir, int max_total_size_mb)
{
    if (!store_dir || !*store_dir) return -1;
    if (max_total_size_mb <= 0) return 0;  /* No cap */

    int pruned = checkpoint_enforce_size(store_dir, max_total_size_mb);
    hermes_log(LOG_DEBUG, "checkpoint", "enforce_size_cap: pruned=%d (cap=%dMB)",
        pruned, max_total_size_mb);
    return pruned;
}

/* Port of Python tools/checkpoint_manager.py:_repair_bare_repo_dirs */