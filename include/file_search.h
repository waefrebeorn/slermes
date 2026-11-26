/*
 * file_search.h — in-process implementation of the `search_files` tool.
 *
 * Content search: POSIX extended regex over file lines; file search: fnmatch over names.
 * No subprocess, so it cannot silently degrade to "no matches" when the shell lacks grep.
 */
#ifndef HERMES_FILE_SEARCH_H
#define HERMES_FILE_SEARCH_H

#include <stddef.h>

/* args: {"pattern", "path", "file_glob", "target": "content"|"files",
 *         "output_mode": "content"|"files_only"|"count", "limit"}.
 * Returns a malloc'd JSON string (caller frees):
 *   {pattern, path, target, output_mode, match_count, file_count, truncated, matches} */
char *file_search_run(const char *args_json);

#endif /* HERMES_FILE_SEARCH_H */
