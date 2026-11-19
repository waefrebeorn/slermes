/* port_display.h -- pure helpers ported from agent/display.py. */
#ifndef PORT_DISPLAY_H
#define PORT_DISPLAY_H

#include <stddef.h>

/* _oneline: collapse all whitespace (including newlines) to single spaces. */
void st_oneline(const char *text, char *out, size_t cap);

/* _tail_trunc: tail-truncate to `limit` chars with "..."; <=0 means unlimited.
 * The result never exceeds `limit` -- for 1-3 the ellipsis is clipped. */
void st_tail_trunc(const char *text, long limit, char *out, size_t cap);

/* _clip: text[:n] plus "..." when longer. */
void st_clip(const char *text, size_t n, char *out, size_t cap);

/* _split_shell_words: quote-aware whitespace split. Writes up to `maxn` malloc'd
 * words into `out`; caller frees. Returns the count. */
int st_split_shell_words(const char *segment, char **out, size_t cap, int maxn);

/* _strip_shell_pipe_tail: drop a trailing "| head/tail/wc/sort/uniq ...". */
void st_strip_shell_pipe_tail(const char *segment, char *out, size_t cap);

/* _split_shell_compound: split on unquoted && / || / ; / newline, dropping pipe
 * tails per segment. Writes up to `maxn` malloc'd segments; caller frees. */
int st_split_shell_compound(const char *command, char **out, size_t cap, int maxn);

/* _shell_head_word: command name, skipping leading VAR=value assignments. */
void st_shell_head_word(const char *segment, char *out, size_t cap);

/* _clean_shell_segment: drop redirections ("> file", "2>&1"). */
void st_clean_shell_segment(const char *segment, char *out, size_t cap);

/* _is_shell_boundary_echo: True when the segment is an `echo` marker. */
int st_is_shell_boundary_echo(const char *segment);

/* summarize_shell_command: compact shell wrapper/plumbing for display.
 * `plus_one` / `plus_many` are the locale templates verbatim from
 * locales/en.yaml:3506-3507, with {first} and {count} substituted. */
void st_summarize_shell_command(const char *command, const char *plus_one,
                                const char *plus_many, char *out, size_t cap);

/* ── Pure formatting ports (src/agent/display_ports.c, disp_ prefix) ──────
 * Dict/list arguments arrive as JSON text and are parsed with libjson inside.
 * Every char* return is malloc'd (caller frees); NULL models Python None.
 * i18n templates are the bundled English (locales/en.yaml) strings. */

/* _http_url: candidate when it parses as an absolute http(s) URL, else NULL. */
char *disp_http_url(const char *candidate);
/* _hex_rgb: "#rrggbb" -> malloc'd JSON "[r,g,b]"; NULL on invalid. */
char *disp_hex_rgb(const char *h);
/* _fg: malloc'd truecolor foreground escape. */
char *disp_fg(int r, int g, int b);
/* _tinted_bg: malloc'd white-on-tint background escape. */
char *disp_tinted_bg(int r, int g, int b, int dominant);
/* _scan_quoted: malloc'd JSON array of [index, char, inside_quotes] triples. */
char *disp_scan_quoted(const char *text);
/* verbose_tool_args_text: compact JSON capped at max_len (0=no cap), optional
 * ``` code block. */
char *disp_verbose_tool_args_text(const char *args_json, long max_len, int code_block);
/* _delegate_task_goals: malloc'd JSON array of truncated goal strings. */
char *disp_delegate_task_goals(const char *tasks_json, long per_goal_len);
/* _delegate_action_preview: "list/steer/stop <id>" or NULL. */
char *disp_delegate_action_preview(const char *args_json);
/* _preview_browser_exec / _preview_delegate_task / _preview_process_manage /
 * _preview_todo_list / _preview_read_file / _preview_memory /
 * _preview_send_message / _preview_skill_view / _preview_clarify:
 * tool-call preview builders (malloc'd or NULL). */
char *disp_preview_browser_exec(const char *args_json, long max_len);
char *disp_preview_delegate_task(const char *args_json, long max_len);
char *disp_preview_process_manage(const char *args_json, long max_len);
char *disp_preview_todo_list(const char *args_json, long max_len);
char *disp_preview_read_file(const char *args_json, long max_len);
char *disp_preview_memory(const char *args_json, long max_len);
char *disp_preview_send_message(const char *args_json, long max_len);
char *disp_preview_skill_view(const char *args_json, long max_len);
char *disp_preview_clarify(const char *args_json, long max_len);
/* _primary_arg_preview: primary argument preview (malloc'd or NULL). */
char *disp_primary_arg_preview(const char *tool_name, const char *args_json, long max_len);
/* _tool_verb: curated friendly verb or NULL. */
char *disp_tool_verb(const char *tool_name);
/* bridge_generating_phrase: bridge phrase when friendly enabled, else NULL. */
char *disp_bridge_generating_phrase(const char *tool_name, int friendly);
/* _degraded_suffix: " [<reason> — <hint>]" suffix. */
char *disp_degraded_suffix(const char *data_json);
/* is_neutral_failure: result arrives as JSON text (dict or string). */
int disp_is_neutral_failure(const char *tool_name, const char *result_json);
/* _domain: host portion of a URL. */
char *disp_domain(const char *url);
/* _cute_trunc / _cute_path: tail/head truncate to an explicit preview cap. */
char *disp_cute_trunc(const char *s, long limit);
char *disp_cute_path(const char *p, long limit);

#endif /* PORT_DISPLAY_H */
