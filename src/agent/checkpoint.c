/*
 * checkpoint.c — P98: Checkpoint manager for agent state.
 * Port of Python tools/checkpoint_manager.py (1640 LOC).
 * Supports: auto-save every N turns, named checkpoints,
 * rollback to any checkpoint, configurable max snapshots.
 *
 * v322: Added JSON filesystem persistence (checkpoint_persist_save/load/list/prune).
 * Port of Python: filesystem snapshot store (~85% of Python LOC).
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

/* ================================================================
 *  Checkpoint lifecycle
 * ================================================================ */

/* Initialize checkpoint manager */
/* Port of Python: CheckpointManager.__init__ */
void checkpoint_init(checkpoint_manager_t *mgr) {
    if (!mgr) return;
    memset(mgr, 0, sizeof(*mgr));
    mgr->checkpoints = NULL;
    mgr->count = 0;
    mgr->capacity = 0;
    mgr->max_snapshots = 10;      /* default max */
    mgr->auto_save_interval = 5;  /* auto-save every 5 turns */
    mgr->turn_counter = 0;
}

/* Free all checkpoints */
void checkpoint_free(checkpoint_manager_t *mgr) {
    if (!mgr) return;
    for (size_t i = 0; i < mgr->count; i++) {
        if (mgr->checkpoints[i].messages) {
            for (size_t j = 0; j < mgr->checkpoints[i].count; j++)
                message_free(mgr->checkpoints[i].messages[j]);
            free(mgr->checkpoints[i].messages);
        }
    }
    free(mgr->checkpoints);
    mgr->checkpoints = NULL;
    mgr->count = 0;
    mgr->capacity = 0;
}

/* Set checkpoint manager limits */
/* Port of Python: config-based limit setting */
void checkpoint_set_limits(checkpoint_manager_t *mgr, int max_snapshots, int auto_save_interval) {
    if (!mgr) return;
    if (max_snapshots > 0) mgr->max_snapshots = max_snapshots;
    if (auto_save_interval > 0) mgr->auto_save_interval = auto_save_interval;
}

/* Generate a checkpoint ID (timestamp-based) */
static void checkpoint_gen_id(char *buf, size_t sz, const char *label) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (label && label[0])
        snprintf(buf, sz, "%04d%02d%02d_%02d%02d%02d_%s",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec, label);
    else
        snprintf(buf, sz, "%04d%02d%02d_%02d%02d%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/* Evict oldest checkpoint if at capacity */
static void checkpoint_evict_oldest(checkpoint_manager_t *mgr) {
    while (mgr->count > 0 && (int)mgr->count >= mgr->max_snapshots) {
        /* Free oldest (index 0) */
        if (mgr->checkpoints[0].messages) {
            for (size_t j = 0; j < mgr->checkpoints[0].count; j++)
                message_free(mgr->checkpoints[0].messages[j]);
            free(mgr->checkpoints[0].messages);
        }
        memmove(&mgr->checkpoints[0], &mgr->checkpoints[1],
                (mgr->count - 1) * sizeof(checkpoint_t));
        mgr->count--;
    }
}

/* Save a named checkpoint. label can be NULL for auto-save. */
/* Port of Python: CheckpointManager.ensure_checkpoint, _take */
bool checkpoint_save(checkpoint_manager_t *mgr, agent_state_t *state,
                      const char *label) {
    if (!mgr || !state) return false;

    /* Evict oldest if at capacity */
    checkpoint_evict_oldest(mgr);

    /* Grow array if needed */
    if (mgr->count >= mgr->capacity) {
        size_t new_cap = mgr->capacity == 0 ? 8 : mgr->capacity * 2;
        checkpoint_t *new_cps = (checkpoint_t *)realloc(
            mgr->checkpoints, new_cap * sizeof(checkpoint_t));
        if (!new_cps) return false;
        mgr->checkpoints = new_cps;
        mgr->capacity = new_cap;
    }

    checkpoint_t *cp = &mgr->checkpoints[mgr->count];
    memset(cp, 0, sizeof(*cp));

    /* Generate ID */
    checkpoint_gen_id(cp->id, sizeof(cp->id), label);
    if (label)
        snprintf(cp->label, sizeof(cp->label), "%s", label);
    cp->created_at = time(NULL);

    /* Clone messages from state */
    cp->count = state->message_count;
    cp->capacity = cp->count + 16;
    cp->messages = (message_t **)calloc(cp->capacity, sizeof(message_t *));
    if (!cp->messages) return false;

    for (size_t i = 0; i < cp->count; i++) {
        cp->messages[i] = message_clone(state->messages[i]);
        if (!cp->messages[i]) {
            /* Partial clone — free what we have */
            for (size_t j = 0; j < i; j++)
                message_free(cp->messages[j]);
            free(cp->messages);
            cp->messages = NULL;
            cp->count = 0;
            return false;
        }
    }

    mgr->count++;
    return true;
}

/* Restore messages from a checkpoint by ID (or nullptr for most recent).
 * Returns true on success. */
/* Port of Python: CheckpointManager.restore */
bool checkpoint_restore(checkpoint_manager_t *mgr, agent_state_t *state,
                         const char *checkpoint_id) {
    if (!mgr || !state || mgr->count == 0) return false;

    checkpoint_t *cp = NULL;

    if (checkpoint_id && checkpoint_id[0]) {
        /* Find by ID */
        for (size_t i = 0; i < mgr->count; i++) {
            if (strcmp(mgr->checkpoints[i].id, checkpoint_id) == 0) {
                cp = &mgr->checkpoints[i];
                break;
            }
        }
        if (!cp) return false; /* not found */
    } else {
        /* Most recent checkpoint */
        cp = &mgr->checkpoints[mgr->count - 1];
    }

    /* Clear existing messages in state */
    context_clear(state);

    /* Clone checkpoint messages into state */
    for (size_t i = 0; i < cp->count; i++) {
        message_t *clone = message_clone(cp->messages[i]);
        if (!clone) return false;
        context_push(state, clone);
    }

    return true;
}

/* List saved checkpoints. Returns count written to ids/labels.
 * ids must be array of [64] strings, labels of [128]. */
/* Port of Python: CheckpointManager.list_checkpoints */
size_t checkpoint_list(const checkpoint_manager_t *mgr,
                        char (*ids)[64], char (*labels)[128],
                        size_t max_count) {
    if (!mgr || !ids || max_count == 0) return 0;

    size_t n = mgr->count < max_count ? mgr->count : max_count;
    for (size_t i = 0; i < n; i++) {
        snprintf(ids[i], 64, "%s", mgr->checkpoints[i].id);
        if (labels)
            snprintf(labels[i], 128, "%s", mgr->checkpoints[i].label);
    }
    return n;
}

/* Get checkpoint count */
/* Port of Python: list checkpoint count */
size_t checkpoint_count(const checkpoint_manager_t *mgr) {
    return mgr ? mgr->count : 0;
}

/* Try auto-save — returns true if a checkpoint was saved this call.
 * Only saves every auto_save_interval turns. */
/* Port of Python: CheckpointManager.new_turn */
bool checkpoint_try_autosave(checkpoint_manager_t *mgr, agent_state_t *state) {
    if (!mgr || !state) return false;

    /* Don't auto-save empty state */
    if (state->message_count < 2) return false;

    mgr->turn_counter++;
    if (mgr->turn_counter >= mgr->auto_save_interval) {
        mgr->turn_counter = 0;
        return checkpoint_save(mgr, state, "__auto__");
    }
    return false;
}

/* G29: Generate a diff string between current messages and a checkpoint.
 * Returns malloc'd string (caller must free), or NULL on error. */
/* Port of Python: CheckpointManager.diff */
char *checkpoint_diff(const checkpoint_t *cp, const agent_state_t *state) {
    if (!cp || !state) return NULL;

    /* Build a simple text diff showing message count changes and content of first differing message */
    char buf[4096];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "Checkpoint: %s (%s)\n", cp->id, cp->label[0] ? cp->label : "(unnamed)");
    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "Checkpoint had %zu messages, current has %zu messages\n",
        cp->count, state->message_count);

    /* Show messages that differ */
    size_t min_count = cp->count < state->message_count ? cp->count : state->message_count;
    for (size_t i = 0; i < min_count; i++) {
        const char *c1 = cp->messages[i]->content ? cp->messages[i]->content : "";
        const char *c2 = state->messages[i]->content ? state->messages[i]->content : "";
        if (strcmp(c1, c2) != 0) {
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                "Diff at message %zu:\n  Before: %.200s\n  After:  %.200s\n",
                i, c1, c2);
            break; /* Show first diff only */
        }
    }

    if (pos == 0)
        snprintf(buf, sizeof(buf), "No differences (identical state)\n");

    return strdup(buf);
}

/* G30: Restore checkpoint and create a branch from that point.
 * Returns true if successful. */
/* Port of Python: CheckpointManager.restore with branching */
bool checkpoint_branch_restore(checkpoint_manager_t *mgr, agent_state_t *state,
                                const char *checkpoint_id, const char *new_session_id) {
    if (!mgr || !state) return false;

    /* Save current state as a branch before restoring */
    if (state->db && new_session_id && new_session_id[0]) {
        agent_session_branch(state, new_session_id, (int)state->message_count - 1);
    }

    /* Restore the checkpoint */
    return checkpoint_restore(mgr, state, checkpoint_id);
}

/* ================================================================
 *  Filesystem persistence (v322)
 * ================================================================ */

/* Get checkpoint base directory. Returns pointer to static buf. */
static const char *checkpoint_base_dir(void) {
    static char path[HERMES_PATH_MAX];
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return NULL;
    snprintf(path, sizeof(path), "%s/.hermes/checkpoints", home);
    return path;
}

/* Ensure checkpoint directory exists. Returns true on success. */
/* Port of Python: _init_store (simplified JSON store vs git store) */
bool checkpoint_init_dir(void) {
    const char *base = checkpoint_base_dir();
    if (!base) return false;
    struct stat st;
    if (stat(base, &st) == 0) return S_ISDIR(st.st_mode);
    /* Create parent and directory */
    char cmd[HERMES_PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s' 2>/dev/null", base);
    return system(cmd) == 0;
}

/* Save checkpoint to filesystem as JSON. Returns true on success. */
/* Port of Python: _take (filesystem persistence via JSON) */
bool checkpoint_persist_save(const checkpoint_t *cp) {
    if (!cp || !cp->messages || cp->count == 0) return false;
    if (!checkpoint_init_dir()) return false;

    const char *base = checkpoint_base_dir();
    char filepath[HERMES_PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", base, cp->id);

    /* Build JSON manually for the checkpoint metadata */
    char buf[65536];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "{\"id\":\"%s\",\"label\":\"%s\",\"created_at\":%ld,\"count\":%zu,\"messages\":[",
        cp->id, cp->label[0] ? cp->label : "", (long)cp->created_at, cp->count);

    for (size_t i = 0; i < cp->count && i < 50; i++) {
        const message_t *m = cp->messages[i];
        if (!m) continue;
        if (i > 0) buf[pos++] = ',';

        const char *role = "user";
        const char *content = m->content ? m->content : "";
        /* Escape JSON special chars in content */
        char escaped[2048];
        int epos = 0;
        for (const char *s = content; *s && epos < (int)sizeof(escaped) - 6; s++) {
            if (*s == '\\') { escaped[epos++] = '\\'; escaped[epos++] = '\\'; }
            else if (*s == '"') { escaped[epos++] = '\\'; escaped[epos++] = '"'; }
            else if (*s == '\n') { escaped[epos++] = '\\'; escaped[epos++] = 'n'; }
            else if (*s == '\t') { escaped[epos++] = '\\'; escaped[epos++] = 't'; }
            else if (*s == '\r') { escaped[epos++] = '\\'; escaped[epos++] = 'r'; }
            else escaped[epos++] = *s;
        }
        escaped[epos] = '\0';

        pos += snprintf(buf + pos, sizeof(buf) - pos,
            "{\"role\":\"%s\",\"content\":\"%.1800s\"}", role, escaped);
        if (pos >= (int)sizeof(buf) - 1024) break;
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "]}");

    /* Write to file */
    FILE *f = fopen(filepath, "w");
    if (!f) return false;
    size_t written = fwrite(buf, 1, (size_t)pos, f);
    fclose(f);
    return written == (size_t)pos;
}

/* Load checkpoint from filesystem JSON. Returns newly allocated checkpoint.
 * Caller must free with checkpoint_free(). */
/* Port of Python: restore from store */
checkpoint_t *checkpoint_persist_load(const char *checkpoint_id) {
    if (!checkpoint_id) return NULL;

    const char *base = checkpoint_base_dir();
    if (!base) return NULL;

    char filepath[HERMES_PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", base, checkpoint_id);

    /* Read file */
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize <= 0 || fsize > 65536) { fclose(f); return NULL; }
    rewind(f);

    char *content = (char *)calloc(1, (size_t)fsize + 1);
    if (!content) { fclose(f); return NULL; }
    size_t nread = fread(content, 1, (size_t)fsize, f);
    fclose(f);
    if (nread == 0) { free(content); return NULL; }

    /* Parse JSON */
    char *err = NULL;
    json_node_t *root = json_parse(content, &err);
    free(content);
    if (!root) { free(err); return NULL; }

    /* Extract fields */
    checkpoint_t *cp = (checkpoint_t *)calloc(1, sizeof(checkpoint_t));
    if (!cp) { json_free(root); return NULL; }

    const char *id = json_get_str(root, "id", NULL);
    if (id) snprintf(cp->id, sizeof(cp->id), "%s", id);
    const char *label = json_get_str(root, "label", NULL);
    if (label) snprintf(cp->label, sizeof(cp->label), "%s", label);
    cp->created_at = (time_t)json_get_num(root, "created_at", 0);
    size_t msg_count = (size_t)json_get_num(root, "count", 0);
    if (msg_count > 50) msg_count = 50;

    /* Parse messages array */
    json_node_t *msgs = json_obj_get(root, "messages");
    if (msgs && msgs->type == JSON_ARRAY && msg_count > 0) {
        cp->messages = (message_t **)calloc(msg_count, sizeof(message_t *));
        if (cp->messages) {
            cp->capacity = msg_count;
            for (size_t i = 0; i < msg_count; i++) {
                json_node_t *mnode = json_get(msgs, (int)i);
                if (!mnode) continue;
                const char *mcontent = json_get_str(mnode, "content", "");
                cp->messages[cp->count] = message_new(MSG_USER, mcontent);
                if (cp->messages[cp->count]) cp->count++;
            }
        }
    }

    json_free(root);
    return cp;
}

/* List persisted checkpoint files. Returns count; ids must be array of [64]. */
size_t checkpoint_persist_list(char (*ids)[64], size_t max_count) {
    if (!ids || max_count == 0) return 0;
    if (!checkpoint_init_dir()) return 0;

    const char *base = checkpoint_base_dir();
    char cmd[HERMES_PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "ls -1 '%s'/*.json 2>/dev/null | head -%zu", base, max_count);

    FILE *f = popen(cmd, "r");
    if (!f) return 0;

    size_t count = 0;
    char line[HERMES_PATH_MAX];
    while (fgets(line, sizeof(line), f) && count < max_count) {
        /* Extract filename from path */
        char *slash = strrchr(line, '/');
        char *name = slash ? slash + 1 : line;
        char *dot = strrchr(name, '.');
        if (dot) *dot = '\0';
        /* Strip trailing newline */
        size_t len = strlen(name);
        while (len > 0 && (name[len-1] == '\n' || name[len-1] == '\r'))
            name[--len] = '\0';
        if (name[0])
            snprintf(ids[count++], 64, "%s", name);
    }
    pclose(f);
    return count;
}

/* Prune checkpoint files older than retention_days. Returns count pruned. */
/* Port of Python: _prune, prune_checkpoints */
size_t checkpoint_persist_prune(int retention_days) {
    if (retention_days <= 0) return 0;

    const char *base = checkpoint_base_dir();
    if (!base) return 0;

    char cmd[HERMES_PATH_MAX + 128];
    snprintf(cmd, sizeof(cmd),
        "find '%s' -name '*.json' -type f -mtime +%d -delete -print 2>/dev/null | wc -l",
        base, retention_days);

    FILE *f = popen(cmd, "r");
    if (!f) return 0;
    char result[64] = "";
    if (fgets(result, sizeof(result), f)) {
        pclose(f);
        return (size_t)atol(result);
    }
    pclose(f);
    return 0;
}

/* Port of Python: maybe_auto_prune_checkpoints */
bool checkpoint_maybe_auto_prune(int retention_days, int min_interval_hours) {
    const char *base = checkpoint_base_dir();
    if (!base) return false;

    /* Check last_prune marker */
    char marker_path[HERMES_PATH_MAX];
    snprintf(marker_path, sizeof(marker_path), "%s/.last_prune", base);

    struct stat st;
    if (stat(marker_path, &st) == 0) {
        /* Marker exists — check if within min_interval_hours */
        time_t now = time(NULL);
        if (now - st.st_mtime < min_interval_hours * 3600) {
            return true; /* skipped — too recent */
        }
    }

    /* Run prune */
    size_t pruned = checkpoint_persist_prune(retention_days);

    /* Write marker */
    FILE *f = fopen(marker_path, "w");
    if (f) {
        time_t now = time(NULL);
        fprintf(f, "%ld\n", (long)now);
        fclose(f);
    }

    return pruned > 0;
}

/* Port of Python: _dir_file_count */
size_t checkpoint_dir_file_count(void) {
    const char *base = checkpoint_base_dir();
    if (!base) return 0;

    char cmd[HERMES_PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "find '%s' -type f 2>/dev/null | wc -l", base);
    FILE *f = popen(cmd, "r");
    if (!f) return 0;
    char result[64] = "";
    size_t count = 0;
    if (fgets(result, sizeof(result), f)) {
        count = (size_t)atol(result);
    }
    pclose(f);
    return count;
}

/* Port of Python: _dir_size_bytes */
size_t checkpoint_dir_size_bytes(void) {
    const char *base = checkpoint_base_dir();
    if (!base) return 0;

    char cmd[HERMES_PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "du -sb '%s' 2>/dev/null | cut -f1", base);
    FILE *f = popen(cmd, "r");
    if (!f) return 0;
    char result[64] = "";
    size_t bytes = 0;
    if (fgets(result, sizeof(result), f)) {
        bytes = (size_t)atol(result);
    }
    pclose(f);
    return bytes;
}

/* Port of Python: clear_all */
size_t checkpoint_clear_all(void) {
    const char *base = checkpoint_base_dir();
    if (!base) return 0;

    /* Count files before removal */
    size_t count = checkpoint_dir_file_count();

    char cmd[HERMES_PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' 2>/dev/null", base);
    system(cmd);

    return count;
}

/* Port of Python: format_checkpoint_list */
char *checkpoint_format_list(size_t count, const char (*ids)[64], const char (*labels)[128]) {
    if (count == 0) return strdup("No checkpoints found.\n");

    char *buf = (char *)malloc(4096);
    if (!buf) return NULL;
    int pos = 0;
    pos += snprintf(buf + pos, 4096 - pos, "📸 Checkpoints:\n\n");

    for (size_t i = 0; i < count && i < 50 && pos < 4000; i++) {
        const char *label = (labels && labels[i][0]) ? labels[i] : "(unnamed)";
        /* Truncate label to fit */
        char label_trunc[32];
        snprintf(label_trunc, sizeof(label_trunc), "%.31s", label);
        pos += snprintf(buf + pos, 4096 - pos, "  %zu. %s  %s\n",
                        i + 1, ids[i], label_trunc);
    }

    if (pos < 4000) {
        pos += snprintf(buf + pos, 4096 - pos,
            "\n  /rollback <N>         restore to checkpoint N\n"
            "  /rollback diff <N>    preview changes since checkpoint N\n");
    }

    return buf;
}

/* Port of Python: clear_legacy */
size_t checkpoint_clear_legacy(void) {
    const char *base = checkpoint_base_dir();
    if (!base) return 0;

    char cmd[HERMES_PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "find '%s' -maxdepth 1 -type d -name 'legacy-*' "
             "2>/dev/null | wc -l", base);
    FILE *f = popen(cmd, "r");
    if (!f) return 0;
    char result[64] = "";
    size_t count = 0;
    if (fgets(result, sizeof(result), f)) {
        count = (size_t)atol(result);
    }
    pclose(f);

    if (count > 0) {
        snprintf(cmd, sizeof(cmd),
                 "find '%s' -maxdepth 1 -type d -name 'legacy-*' "
                 "-exec rm -rf {} + 2>/dev/null", base);
        system(cmd);
    }
    return count;
}
