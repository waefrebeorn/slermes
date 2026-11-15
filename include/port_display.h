/* port_display.h -- pure helpers ported from agent/display.py. */
#ifndef SLERMES_PORT_DISPLAY_H
#define SLERMES_PORT_DISPLAY_H

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

#endif /* SLERMES_PORT_DISPLAY_H */
