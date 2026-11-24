#ifndef NOC_W18_FILETOOLS_B76F5D_H
#define NOC_W18_FILETOOLS_B76F5D_H

#include "hermes_json.h"

/* C home for upstream Hermes tools/file_tools_write_guards.py.
 * Pure "file-tool display text" detectors + stale-write refusal builder. */
int     w18_filetools_b76f5d_is_internal_file_status_text(const char *content);
int     w18_filetools_b76f5d_looks_like_read_file_line_numbered_content(const char *content);
int     w18_filetools_b76f5d_is_internal_file_tool_content(const char *content);
json_t *w18_filetools_b76f5d_stale_write_refusal(const char *filepath, const char *reason,
                                                 const char *resolved_or_null);

#endif /* NOC_W18_FILETOOLS_B76F5D_H */
