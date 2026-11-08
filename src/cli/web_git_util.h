/*
 * web_git_util.h - web-git path / status classification helpers.
 *
 * Module-internal header (port_web_git.c <-> web_git_util.c).
 */
#ifndef WEB_GIT_UTIL_H
#define WEB_GIT_UTIL_H

#include <stddef.h>
#include <stdbool.h>
#include "port_web_git.h"

void classify_cb_adapter(const char *tag, const char *xy, const char *path, void *ud);
bool entry_staged(const char *tag, const char *xy);
bool is_dir(const char *cwd);
char * resolve_rename_path(const char *raw);
char status_letter(const char *tag, const char *xy);
char * trim_nl(char *s);

#endif /* WEB_GIT_UTIL_H */
