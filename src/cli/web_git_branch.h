/*
 * web_git_branch.h - branch naming and diff-stat helpers.
 *
 * Module-internal header (port_web_git.c <-> web_git_branch.c).
 */
#ifndef WEB_GIT_BRANCH_H
#define WEB_GIT_BRANCH_H

#include <stddef.h>
#include <stdbool.h>
#include "port_web_git.h"

#ifndef UNTRACKED_LINE_MAX_BYTES
#define UNTRACKED_LINE_MAX_BYTES (1024 * 1024)
#endif

typedef void (*entry_cb)(const char *tag, const char *xy, const char *path, void *ud);

char * sanitize_branch(const char *name);
char * slugify(const char *name);
char * unique_dir(const char *base);
int untracked_insertions(const char *cwd, const char *rel);
void walk_entries(const char *raw, size_t raw_len, entry_cb cb, void *ud);

#endif /* WEB_GIT_BRANCH_H */
