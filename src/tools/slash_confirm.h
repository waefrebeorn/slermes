#ifndef TOOLS_SLASH_CONFIRM_H
#define TOOLS_SLASH_CONFIRM_H
#include <stdbool.h>
void tools_slash_confirm_register(const char *session_key, const char *confirm_id,
                                  const char *command, char *(*handler)(const char *choice));
char *tools_slash_confirm_get_pending(const char *session_key);
void tools_slash_confirm_clear(const char *session_key);
bool tools_slash_confirm_clear_if_stale(const char *session_key, double timeout);
char *tools_slash_confirm_resolve(const char *session_key, const char *confirm_id,
                                  const char *choice, double timeout);
#endif
