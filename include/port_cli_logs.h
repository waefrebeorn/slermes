#ifndef PORT_CLI_LOGS_H
#define PORT_CLI_LOGS_H

#include <stddef.h>
#include <time.h>

/* Port of Python: hermes_cli/logs.py
 * ``hermes logs`` — pure log view/filter (no network). */

/* Pure helpers */
time_t cli_logs_parse_since(const char *since_str);          /* -1 if invalid */
time_t cli_logs_parse_line_timestamp(const char *line);      /* -1 if none */
char  *cli_logs_extract_level(const char *line);             /* malloc'd or NULL */
char  *cli_logs_extract_logger_name(const char *line);       /* malloc'd or NULL */
int    cli_logs_line_matches_component(const char *line,
                                       const char *prefixes[], int nprefix);
int    cli_logs_matches_filters(const char *line,
                                const char *min_level,
                                const char *session_filter,
                                time_t since,
                                const char *component_prefixes[], int ncomp);

/* File reading (returns malloc'd array of malloc'd lines; *out_n = count) */
char **cli_logs_read_last_n_lines(const char *path, int n, int *out_n);
char **cli_logs_read_tail(const char *path, int num_lines, int has_filters,
                          const char *min_level, const char *session_filter,
                          time_t since, const char *comp_prefixes[], int ncomp,
                          int *out_n);

/* Public commands */
void cli_logs_tail(const char *log_name, int num_lines, int follow,
                   const char *level, const char *session,
                   const char *since, const char *component);
void cli_logs_follow(const char *path, const char *min_level,
                     const char *session_filter, time_t since,
                     const char *comp_prefixes[], int ncomp);
void cli_logs_list(void);

#endif /* PORT_CLI_LOGS_H */
