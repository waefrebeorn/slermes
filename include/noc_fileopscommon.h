#ifndef NOC_FILEOPSCOMMON_H
#define NOC_FILEOPSCOMMON_H

#include "hermes_json.h"

/* C port of tools/file_operations_common.py. All exports fileopscommon_-prefixed.
 * Returned char* values are malloc'd and caller-owned; NULL means Python None. */

int  fileopscommon_count_conflict_blocks(const char *formatted_content);
char *fileopscommon_strip_terminal_fence_leaks(const char *text);
char *fileopscommon_filename_match_key(const char *name);
const char *fileopscommon_detect_line_ending(const char *sample);
char *fileopscommon_normalize_line_endings(const char *text, const char *target);
int  fileopscommon_strip_bom(const char *text, char **out);  /* returns had_bom */
int  fileopscommon_has_bom(const char *text);
long long fileopscommon_coerce_int(const char *value, long long def, int *ok);
void fileopscommon_normalize_read_pagination(const char *offset, const char *limit,
                                             long long max_lines,
                                             long long *out_off, long long *out_lim);
void fileopscommon_normalize_search_pagination(const char *offset, const char *limit,
                                               long long *out_off, long long *out_lim);
json_t *fileopscommon_lint_result_to_dict(int success, int skipped,
                                          const char *output, const char *message);

#endif
