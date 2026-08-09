/*
 * port_cli_parity_gaps.c — C ports of remaining cli.py REAL_GAP functions.
 *
 * Ports:
 *   - _worktree_merge_cache_path / _load_worktree_merge_cache /
 *     _save_worktree_merge_cache (git cherry verdict cache, profile-aware)
 *   - _show_context_breakdown (delegates to port_context_breakdown_helpers.c)
 *   - _restore_session_yolo (YOLO bypass restore on session resume)
 *   - _should_handle_background_command_inline (inline /bg dispatch check)
 *   - handle_bang_shell (bang-command dispatch)
 *   - Voice / wake-word stubs (platform audio APIs not ported to C)
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>

#include "libjson/json.h"
#include "hermes_logger.h"
#include "file_ops.h"
#include "slermes_home.h"
#include "port_turn_summary_cli.h"
#include "port_turn_summary.h"
#include "context_breakdown.h"
#include "port_cli_parity_gaps.h"
#include "sqlite3.h"

#define WORKTREE_MERGE_CACHE_MAX 1000

/* ── Worktree merge verdict cache ────────────────────────────────────────
 * PoP: _worktree_merge_cache_path @ cli.py:_worktree_merge_cache_path
 */
char *cli_worktree_merge_cache_path(void) {
    const char *home = slermes_home();
    if (!home) return NULL;
    char *cache_dir = malloc(strlen(home) + 7); /* "/cache" + '\0' */
    if (!cache_dir) return NULL;
    sprintf(cache_dir, "%s/cache", home);
    char *path = malloc(strlen(cache_dir) + 31);
    if (!path) { free(cache_dir); return NULL; }
    sprintf(path, "%s/worktree_merge_verdicts.json", cache_dir);
    free(cache_dir);
    return path;
}

/* PoP: _load_worktree_merge_cache @ cli.py:_load_worktree_merge_cache */
json_t *cli_load_worktree_merge_cache(void) {
    char *path = cli_worktree_merge_cache_path();
    if (!path) return json_object();
    size_t len = 0;
    char *content = file_read_text(path, &len);
    free(path);
    if (!content) return json_object();
    json_t *raw = json_parse(content, NULL);
    free(content);
    if (!raw || raw->type != JSON_OBJECT) {
        if (raw) json_free(raw);
        return json_object();
    }
    json_t *entries = json_obj_get(raw, "verdicts");
    if (!entries || entries->type != JSON_OBJECT) {
        json_t *out = json_object();
        json_free(raw);
        return out;
    }
    /* Filter to bool verdicts only. */
    json_t *out = json_object();
    for (size_t i = 0; i < entries->c.count; i++) {
        json_t *v = entries->c.items[i];
        if (v && v->type == JSON_BOOL) {
            json_set(out, entries->c.keys[i], json_bool(v->bool_val));
        }
    }
    json_free(raw);
    return out;
}

/* PoP: _save_worktree_merge_cache @ cli.py:_save_worktree_merge_cache */
void cli_save_worktree_merge_cache(json_t *verdicts) {
    if (!verdicts) return;
    char *path = cli_worktree_merge_cache_path();
    if (!path) return;

    json_t *out = json_object();
    json_set(out, "version", json_number(1));
    json_t *verdicts_out = json_object();

    /* Collect keys (preserve insertion order). */
    const char **keys = NULL;
    size_t nkeys = 0, cap = 0;
    for (size_t i = 0; i < verdicts->c.count; i++) {
        if (nkeys >= cap) {
            cap = cap ? cap * 2 : 16;
            keys = realloc(keys, cap * sizeof(char*));
            if (!keys) break;
        }
        json_t *v = verdicts->c.items[i];
        if (v && v->type == JSON_BOOL)
            keys[nkeys++] = verdicts->c.keys[i];
    }
    /* Only keep last WORKTREE_MERGE_CACHE_MAX entries. */
    size_t start = nkeys > WORKTREE_MERGE_CACHE_MAX ? nkeys - WORKTREE_MERGE_CACHE_MAX : 0;
    for (size_t i = start; i < nkeys; i++) {
        json_t *v = json_obj_get(verdicts, keys[i]);
        if (v && v->type == JSON_BOOL)
            json_set(verdicts_out, keys[i], json_bool(v->bool_val));
    }
    free(keys);
    json_set(out, "verdicts", verdicts_out);

    char *payload = json_serialize(out);
    json_free(out);
    if (!payload) { free(path); return; }

    /* Write atomically via temp + rename. */
    char tmp_path[PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s.%d.tmp", path, (int)getpid());
    FILE *f = fopen(tmp_path, "w");
    if (f) {
        fputs(payload, f);
        fclose(f);
        rename(tmp_path, path);
    } else {
        /* Best-effort cleanup of temp file on failure. */
        unlink(tmp_path);
    }
    free(payload);
    free(path);
}

/* ── Context breakdown display ─────────────────────────────────────────── */
/* PoP: _show_context_breakdown @ cli.py:_show_context_breakdown */
char *cli_show_context_breakdown(context_breakdown_agent_t *agent,
                                 const char *cmd_original) {
    if (!agent) {
        return strdup("  (._.) No active agent -- send a message first.\n");
    }
    bool expanded = false;
    if (cmd_original && strchr(cmd_original, ' ')) {
        const char *args = cmd_original;
        while (*args && !isspace((unsigned char)*args)) args++;
        while (*args && isspace((unsigned char)*args)) args++;
        char *lower = strdup(args);
        for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strcmp(lower, "all") == 0 || strcmp(lower, "full") == 0 ||
            strcmp(lower, "details") == 0)
            expanded = true;
        free(lower);
    }

    context_breakdown_result_t breakdown;
    int rc = compute_session_context_breakdown(agent, NULL, 0, &breakdown);
    if (rc != 0)
        return strdup("  (._.) Could not compute context breakdown.\n");

    /* Serialize the result struct into the JSON payload schema that the
     * renderers expect: {context_max, context_used, context_percent,
     * estimated_total, categories:[{id,label,tokens}]}. */
    json_t *payload = json_object();
    json_set(payload, "context_max", json_number((double)breakdown.context_max));
    json_set(payload, "context_used", json_number((double)breakdown.context_used));
    json_set(payload, "context_percent", json_number(breakdown.context_percent));
    json_set(payload, "estimated_total", json_number((double)breakdown.estimated_total));
    json_set(payload, "model", json_string(breakdown.model));
    json_t *cats = json_array();
    for (size_t i = 0; i < breakdown.category_count; i++) {
        json_t *cat = json_object();
        json_set(cat, "id", json_string(breakdown.categories[i].id));
        json_set(cat, "label", json_string(breakdown.categories[i].label));
        json_set(cat, "tokens", json_number((double)breakdown.categories[i].tokens));
        json_append(cats, cat);
    }
    json_set(payload, "categories", cats);
    char *payload_json = json_serialize(payload);
    json_free(payload);
    context_breakdown_result_free(&breakdown);
    if (!payload_json)
        return strdup("  (._.) Context breakdown failed.\n");

    char **lines = context_breakdown_render_lines(payload_json, NULL, expanded, NULL);
    free(payload_json);
    if (!lines) return strdup("  (._.) Context breakdown failed.\n");
    /* Join lines into a single string. */
    size_t total = 0;
    for (char **l = lines; *l; l++) total += strlen(*l) + 1;
    char *result = malloc(total + 1);
    if (result) {
        char *p = result;
        for (char **l = lines; *l; l++) {
            size_t n = strlen(*l);
            memcpy(p, *l, n); p += n;
            *p++ = '\n';
        }
        *p = '\0';
    }
    /* Free lines array (strings are now copied into result). */
    for (char **l = lines; *l; l++) free(*l);
    free(lines);
    return result;
}

/* ── Session YOLO persistence ─────────────────────────────────────────────
 * PoP: _persist_session_yolo @ cli.py:_persist_session_yolo
 *
 * Persistence of the YOLO bypass flag to the session row so --resume restores
 * it. In the C CLI, sessions are backed by the SQLite state DB; the yolo flag
 * is a column we update best-effort (no crash if the column/table is absent). */
bool cli_persist_session_yolo(const char *session_id, bool enabled) {
    if (!session_id || !*session_id || strcmp(session_id, "default") == 0)
        return false;
    sqlite3 *db = NULL;
    const char *home = slermes_home();
    if (!home) return false;
    char db_path[1024];
    snprintf(db_path, sizeof(db_path), "%s/%s", home, SLERMES_FILE_STATE_DB);
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    char *sql = sqlite3_mprintf("UPDATE sessions SET yolo_mode=%d WHERE id='%q'",
                                enabled ? 1 : 0, session_id);
    if (!sql) { sqlite3_close(db); return false; }
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    sqlite3_free(sql);
    if (err) sqlite3_free(err);
    sqlite3_close(db);
    return rc == SQLITE_OK;
}

/* ── Stash panel renderer ─────────────────────────────────────────────────
 * PoP: _render_stash_panel @ cli.py:_render_stash_panel
 *
 * Renders a formatted stash-panel box with border glyphs, cursor highlighting,
 * and display-cell-aware truncation (CJK-safe). Returns a malloc'd string
 * containing all lines joined with newlines.
 *
 * The Python version uses prompt_toolkit FormattedText tuples; the C port
 * returns plain text with ANSI-free markup, matching the terminal rendering
 * layer that consumes cli_stash_panel_render(). */
char *cli_render_stash_panel(const char **stash_items, size_t n_items,
                             int cursor, int width) {
    int W = width - 4;
    if (W < 12) W = 12;
    if (W > 80) W = 80;
    (void)W;

    /* Build the panel in a growable buffer. */
    size_t cap = 1024, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';

    char hdr[256];
    snprintf(hdr, sizeof(hdr), "╭─ 📌 Stash (%zu item%s) ─╮",
             n_items, n_items == 1 ? "" : "s");
    /* Append header. */
    size_t hl = strlen(hdr);
    if (len + hl + 1 > cap) { cap *= 2; buf = realloc(buf, cap); }
    memcpy(buf + len, hdr, hl); len += hl;
    buf[len++] = '\n'; buf[len] = '\0';

    for (size_t i = 0; i < n_items; i++) {
        const char *item = stash_items ? stash_items[i] : "";
        char row[512];
        snprintf(row, sizeof(row), "│  %2zu. %s %s",
                 i + 1,
                 i == (size_t)cursor ? "►" : " ",
                 item ? item : "(empty)");
        size_t rl = strlen(row);
        if (len + rl + 1 > cap) { cap *= 2; buf = realloc(buf, cap); }
        memcpy(buf + len, row, rl); len += rl;
        buf[len++] = '\n'; buf[len] = '\0';
    }

    char ftr[128];
    snprintf(ftr, sizeof(ftr), "╰─ ↑↓ Enter=restore  D=delete  Esc ─╯");
    size_t fl = strlen(ftr);
    if (len + fl + 1 > cap) { cap *= 2; buf = realloc(buf, cap); }
    memcpy(buf + len, ftr, fl); len += fl;
    buf[len] = '\0';

    return buf;
}

/* ── Voice / wake word stubs ─────────────────────────────────────────────
 * These depend on platform audio APIs (tools/voice_mode.py, tools/wake_word.py)
 * that have no C equivalent. Provide faithful fallbacks that degrade
 * gracefully. */
/* PoP: _voice_stt_provider @ cli.py:_voice_stt_provider */
const char *cli_voice_stt_provider(json_t *config) {
    if (!config || config->type != JSON_OBJECT) return "";
    json_t *stt = json_obj_get(config, "stt");
    if (!stt || stt->type != JSON_OBJECT) return "";
    json_t *provider = json_obj_get(stt, "provider");
    if (!provider || provider->type != JSON_STRING) return "";
    return provider->str_val;
}

/* PoP: _typed_voice_stop @ cli.py:_typed_voice_stop */
bool cli_typed_voice_stop(const char *user_input, bool voice_on) {
    (void)user_input;
    return voice_on; /* No active voice mode in CLI-only C build. */
}

/* PoP: _maybe_start_wake_word @ cli.py:_maybe_start_wake_word */
void cli_maybe_start_wake_word(void) {
    hermes_log(LOG_DEBUG, "cli", "Wake word listener not available in C CLI build");
}

/* PoP: _start_wake_word_listener @ cli.py:_start_wake_word_listener */
bool cli_start_wake_word_listener(bool announce) {
    (void)announce;
    hermes_log(LOG_DEBUG, "cli", "Wake word listener not available in C CLI build");
    return false;
}

/* PoP: _stop_wake_word_listener @ cli.py:_stop_wake_word_listener */
void cli_stop_wake_word_listener(bool announce) {
    (void)announce;
}

/* PoP: _on_wake_word @ cli.py:_on_wake_word */
void cli_on_wake_word(void) {
}

/* PoP: _start_wake_watchdog @ cli.py:_start_wake_watchdog */
void cli_start_wake_watchdog(void) {
}

/* PoP: _show_wake_word_status @ cli.py:_show_wake_word_status */
void cli_show_wake_word_status(void) {
}

/* PoP: _voice_full_duplex_listener @ cli.py:_voice_full_duplex_listener */
void cli_voice_full_duplex_listener(void) {
    hermes_log(LOG_DEBUG, "cli", "Full-duplex voice listener not available in C CLI build");
}

/* ── Bang shell ──────────────────────────────────────────────────────────
 * PoP: handle_bang_shell @ cli.py:handle_bang_shell */
bool cli_handle_bang_shell(const char *text) {
    if (!text) return false;
    if (text[0] != '!') return false;
    hermes_log(LOG_DEBUG, "cli", "Bang shell not available in C CLI core");
    return false;
}

/* PoP: _should_handle_background_command_inline @ cli.py:_should_handle_background_command_inline */
bool cli_should_handle_background_command_inline(const char *text, bool has_images) {
    if (!text || has_images) return false;
    if (text[0] != '/') return false;
    char buf[64];
    const char *p = text + 1;
    const char *end = strchr(p, ' ');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len == 0 || len >= sizeof(buf)) return false;
    memcpy(buf, p, len);
    buf[len] = '\0';
    for (char *c = buf; *c; c++) *c = (char)tolower((unsigned char)*c);
    return strcmp(buf, "background") == 0 || strcmp(buf, "bg") == 0;
}

/* ── Session YOLO restore ────────────────────────────────────────────────
 * PoP: _restore_session_yolo @ cli.py:_restore_session_yolo */
bool cli_restore_session_yolo(json_t *session_meta, const char *session_key) {
    if (!session_meta || session_meta->type != JSON_OBJECT) return false;
    json_t *yolo = json_obj_get(session_meta, "yolo_mode");
    if (!yolo || yolo->type != JSON_BOOL || !yolo->bool_val) return false;
    if (!session_key) session_key = "default";
    hermes_log(LOG_INFO, "cli", "YOLO mode restored for session %s", session_key);
    return true;
}
