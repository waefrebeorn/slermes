#ifndef AGENT_DISPLAY_HELPERS_H
#define AGENT_DISPLAY_HELPERS_H
#include <stddef.h>
void agent_display_oneline(const char *text, char *out, size_t outsz);
void agent_display_truncate_preview(const char *text, int max_len, char *out, size_t outsz);
void agent_display_shell_basename(const char *head, char *out, size_t outsz);
#endif
