/*
 * noc_w25_slashcomma_2c9db9.h - C surface for upstream
 * gateway/slash_commands_branch_thread.py (gate w25_slashcomma_2c9db9).
 *
 * The pure helpers behind ``/branch``'s destination choice on thread-capable
 * platforms.  No adapter, thread or gateway connection exists here: the
 * session source is a value, and what is ported is what those values decide.
 */
#ifndef NOC_W25_SLASHCOMMA_2C9DB9_H
#define NOC_W25_SLASHCOMMA_2C9DB9_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* raw -> {"stay_here": bool, "title": str}. */
json_t *w25_slashcomma_2c9db9_parse_branch_args(const json_t *raw);

/* source -> the chat that can host a sibling thread, or null. */
json_t *w25_slashcomma_2c9db9_branch_thread_parent(const json_t *source);

/* (source, parent_id, thread_id, title) -> the replaced SessionSource fields. */
json_t *w25_slashcomma_2c9db9_branch_dest_source(const json_t *source, const json_t *parent_id,
                                                 const json_t *thread_id, const json_t *title);

/* (platform, thread_id) -> the clickable pointer, or a backticked id. */
json_t *w25_slashcomma_2c9db9_format_thread_ref(const json_t *platform, const json_t *thread_id);

/* The platform table and flag literal as an object. */
json_t *w25_slashcomma_2c9db9_constants(void);

#ifdef __cplusplus
}
#endif
#endif
