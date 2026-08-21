#ifndef HERMES_CONSOLE_ENGINE_PURE_H
#define HERMES_CONSOLE_ENGINE_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure, deterministic helpers ported from hermes_cli/console_engine.py.
 * All return freshly malloc'd strings (free with free()) or NULL. */

int console_engine_capture_output(char *(*fn)(void), char *out, size_t outsz);
int console_engine_is_status_footer_rule(const char *line);
char *console_engine_strip_status_footer(const char *text);
char *console_engine_table_summary(const char *summary, int limit);
char *console_engine_split_line(const char *line, char *errbuf, size_t errsz);
int console_engine_contains_shell_syntax(const char *line, const char *tokens_csv);
char *console_engine_format_sessions(const char *json_sessions);
char *console_engine_format_job(const char *json_job, const char *action);
char *console_engine_clean_summary(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CONSOLE_ENGINE_PURE_H */
