#ifndef HERMES_SESSION_RECAP_H
#define HERMES_SESSION_RECAP_H

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

void session_coerce_text(const json_t *value, char *out, size_t out_cap);
void session_tool_call_name_and_args(const json_t *tool_call, char *name_out, size_t name_cap, json_t **args_out);
void session_count_visible_turns(const json_t *messages, int *users, int *assistants, int *tools);
int session_latest_user_prompt(const json_t *messages, char *out, size_t out_cap);
int session_latest_assistant_text(const json_t *messages, char *out, size_t out_cap);
json_t *session_recent_window(const json_t *messages, int window);
void session_shortened_path(const char *path, const char *cwd, const char *home, char *out, size_t out_cap);
void session_truncate(const char *text, int limit, char *out, size_t out_cap);
void session_summarise_tool_activity(const json_t *tool_calls, json_t **counts_out, json_t **files_out);
char *session_build_recap(const json_t *messages, const char *session_title, const char *session_id, const char *platform);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SESSION_RECAP_H */
