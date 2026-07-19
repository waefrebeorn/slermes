/*
 * gateway_command_sanitize.h — opaque API for gateway slash-command name
 * sanitization, length clamping, and Telegram menu prioritization.
 *
 * Faithful C11 port of the *pure* helpers in hermes_cli/commands.py:
 *   _sanitize_telegram_name, _sanitize_slack_name, _clamp_command_names,
 *   _dedupe_sanitized_names, _requires_argument, _nested_mapping,
 *   _telegram_command_menu_config, telegram_menu_max_commands,
 *   _telegram_effective_priority, _prioritize_telegram_menu_commands.
 *
 * Everything here is pure string/tuple logic with no network, no filesystem,
 * and no live registry — config is passed in as an already-read JSON string
 * (the Python originals call read_raw_config()). The registry/plugin/skill
 * collection parts of commands.py remain REAL_GAP elsewhere; this module is
 * deliberately narrow and self-contained.
 *
 * Minimal includes: <stddef.h>, <stdbool.h> only. The cmd_entry_t struct is
 * this module's own value type (no god header, no cross-subsystem leakage).
 */

#ifndef SLERMES_GATEWAY_COMMAND_SANITIZE_H
#define SLERMES_GATEWAY_COMMAND_SANITIZE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Max command name length shared by Telegram and Discord (Slack too). */
#define CMD_NAME_LIMIT 32

/* A single gateway command entry: a clamped name plus optional description
 * and an opaque key (e.g. the original /skill-name used by the dispatcher).
 * name is always NUL-terminated and at most CMD_NAME_LIMIT bytes. */
typedef struct cmd_entry {
    char name[CMD_NAME_LIMIT + 1];
    char *description; /* malloc'd, may be NULL */
    char *key;         /* malloc'd metadata (cmd_key), may be NULL */
} cmd_entry_t;

/* Construct an entry (copies name/desc/key; name is clamped to CMD_NAME_LIMIT).
 * Returns a heap entry the caller must free with cmd_entry_free(). */
cmd_entry_t *cmd_entry_make(const char *name, const char *desc, const char *key);

/* Free an entry produced by cmd_entry_make(). Safe to call with NULL. */
void cmd_entry_free(cmd_entry_t *e);

/* ── Name sanitizers ───────────────────────────────────────────────── */

/* Telegram: 1-32 chars, lowercase a-z, digits, underscores only.
 * Lowercase → '-'→'_' → strip invalid → collapse '__' → strip leading/trailing
 * '_'. Returns malloc'd string (may be empty). Caller frees.
 * (PoP: hermes_cli/commands.py:_sanitize_telegram_name) */
char *commands_sanitize_telegram_name(const char *raw);

/* Slack: lowercase a-z, digits, hyphens, underscores; up to 32 chars.
 * Strips invalid chars, trims leading/trailing '-'/'_'. Returns malloc'd
 * string (may be empty). Caller frees.
 * (PoP: hermes_cli/commands.py:_sanitize_slack_name) */
char *commands_sanitize_slack_name(const char *raw);

/* True when selecting the command without text would be incomplete, i.e. the
 * args hint begins with '<'. (PoP: hermes_cli/commands.py:_requires_argument) */
bool commands_requires_argument(const char *args_hint);

/* Human-readable file size label ("512B", "2K", "1.0M", "1.4G").
 * (PoP: hermes_cli/commands.py:_file_size_label). Caller frees. */
char *commands_file_size_label(long size);

/* ── Tuple helpers ──────────────────────────────────────────────────── */

/* Dedupe a list of raw names after Telegram sanitization, preserving first
 * occurrence order. Returns a malloc'd array of malloc'd strings; *out_n is
 * set to the count. Caller frees each string and the array. */
char **commands_dedupe_sanitized_telegram(const char *const *names, int n, int *out_n);

/* ── Telegram command-menu config (reads already-loaded config JSON) ── */

/* Drill into a config mapping like platforms.telegram.extra.command_menu.
 * Accepts the *extracted* command_menu JSON object (may be NULL/empty →
 * defaults). The full-config drill-down is the caller's job — this keeps the
 * module clear of the deep nesting the bundled libjson cannot parse.
 * Returns a malloc'd JSON object {"max_commands":N,"priority_mode":"..",
 * "priority":[...]}. Caller frees.
 * (PoP: hermes_cli/commands.py:_telegram_command_menu_config +
 *       hermes_cli/commands.py:_nested_mapping) */
char *commands_telegram_menu_config_json(const char *menu_cfg_json);

/* Returns the configured Telegram BotCommand menu cap, safe-bounded to [1,100],
 * defaulting to 60. (PoP: hermes_cli/commands.py:telegram_menu_max_commands) */
int commands_telegram_menu_max_commands(const char *menu_cfg_json);

/* Effective Telegram priority list: sanitized configured priority merged with
 * the built-in default priority per priority_mode ("prepend"/"append"/"replace").
 * Returns a malloc'd array of malloc'd names; *out_n is the count. Caller frees. */
char **commands_telegram_effective_priority(const char *menu_cfg_json, int *out_n);

/* Reorder entries in place so priority-named commands lead (stable within
 * each tier by original position). (PoP: hermes_cli/commands.py:
 * _prioritize_telegram_menu_commands) */
void commands_prioritize_telegram_menu(cmd_entry_t *entries, int n,
                                       const char *menu_cfg_json);

/* ── Length clamp with collision avoidance ─────────────────────────── */

/* Clamp entries to CMD_NAME_LIMIT with collision avoidance against *reserved*
 * (and earlier survivors). Names > limit are truncated; if the truncated form
 * collides, a 0-9 digit suffix is appended to the 31-char prefix; if all 10
 * slots are taken the entry is dropped. Survivors are written into out[] (the
 * caller must allocate out_cap >= n entries and free each with cmd_entry_free),
 * and the survivor count is returned. *out_dropped receives the number dropped.
 * (PoP: hermes_cli/commands.py:_clamp_command_names) */
int commands_clamp_names(const cmd_entry_t *in, int n,
                         const char *const *reserved, int n_reserved,
                         cmd_entry_t *out, int out_cap, int *out_dropped);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_GATEWAY_COMMAND_SANITIZE_H */
