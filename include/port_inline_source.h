/* port_inline_source.h -- pure helpers ported from gateway/status_inline_source.py. */
#ifndef PORT_INLINE_SOURCE_H
#define PORT_INLINE_SOURCE_H

#include <stddef.h>

/* Index of the ``-c`` token when *tokens* is an interpreter running inline
 * source, or -1. *tokens* must be CASE-PRESERVING. */
int st_inline_source_flag_index(const char **tokens, size_t n);

/* True when *tokens* is ``python -c <src> [args]``. */
int st_command_line_runs_inline(const char **tokens, size_t n);

#endif /* PORT_INLINE_SOURCE_H */
