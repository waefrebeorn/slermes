#ifndef NOC_CLITUIWRAP_H
#define NOC_CLITUIWRAP_H
#include "hermes_json.h"
/* wrap = textwrap.wrap (whitespace split + greedy pack + subsequent_indent).
 * Both functions return new json_t trees (free with json_free). */
/* PoP: clituiwrap_wrap_rows @ hermes_cli/cli_tui_wrap.py:_wrap_rows */
json_t *clituiwrap_wrap_rows(const json_t *items, int width, const char *indent);
/* PoP: clituiwrap_prefix_wrapped_rows @ hermes_cli/cli_tui_wrap.py:_prefix_wrapped_rows */
json_t *clituiwrap_prefix_wrapped_rows(const char *label, int width,
                                       const char *first_prefix, const char *indent);
#endif
