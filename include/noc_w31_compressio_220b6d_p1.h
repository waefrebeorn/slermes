/*
 * noc_w31_compressio_220b6d_p1.h - C surface for upstream
 * persistence/sqlite/conversation/_store/compression.py (gate
 * w31_compressio_220b6d_p1): the compression lineage walks and predicates.
 *
 * Every sqlite connection, transaction and holder read stays in the caller.
 * What is ported here is the part that is a pure function of values: which
 * child continues a chain, which ancestors inherit a creating surface, which
 * session key a turn lease binds to, what a lineage or tool-rotation root is,
 * how a cooldown row expires, and the sessions-row parameters a compression
 * child is published with.
 */
#ifndef NOC_W31_COMPRESSIO_220B6D_P1_H
#define NOC_W31_COMPRESSIO_220B6D_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ``_CHAIN_CAP``: defensive bound on every lineage walk. */
#define W31_COMPRESSIO_220B6D_CHAIN_CAP 1000

/* ``_cooldown_row(exists, cooldown_until, error)`` ->
 * {session_exists, cooldown_until, error}; cooldown_until is float() of the
 * stored value or null. */
json_t *w31_compressio_220b6d_p1_cooldown_row(int session_exists,
                                              const json_t *cooldown_until,
                                              const json_t *error);

/* ``_walk_compression_chain(session_id, step)``: root-first ids through the
 * tip. steps is {"<id>": "<child id>"|null} - the answers ``step`` gives. */
json_t *w31_compressio_220b6d_p1_walk_chain(const char *session_id,
                                            const json_t *steps, int cap);

/* ``get_compression_tip``: last element of the walk, else session_id. */
json_t *w31_compressio_220b6d_p1_chain_tip(const char *session_id,
                                           const json_t *steps, int cap);

/* ``value in _UNSET_CREATED_SOURCES`` ({null, "", "unknown", "local"}). */
json_t *w31_compressio_220b6d_p1_unset_created(const json_t *value);

/* ``_is_explicit_fork_child_row``: branch/delegate/tool child (include_reset
 * adds the reset marker). Markers only count when bound to parent_session_id. */
json_t *w31_compressio_220b6d_p1_fork_child(const json_t *session,
                                            int include_reset);

/* ``_is_compression_child_row``: child of a compression-ended parent and not an
 * explicit fork. sessions is {"<id>": <row>} - the get_session answers. */
json_t *w31_compressio_220b6d_p1_compression_child(const json_t *child,
                                                   const json_t *sessions);

/* ``get_compression_lineage``: ancestors through tip, chronological.
 * children is {"<parent id>": [<row>, ...]} already in started_at ASC order. */
json_t *w31_compressio_220b6d_p1_lineage(const char *session_id,
                                         const json_t *sessions,
                                         const json_t *children);

/* ``get_tool_rotation_root``: root of a --source tool run's chain. */
json_t *w31_compressio_220b6d_p1_tool_root(const char *session_id,
                                           const json_t *sessions);

/* ``_session_turn_lease_key_on_conn``: walk compression parents to the lease
 * key; explicit forks and non-compression parents stop it. */
json_t *w31_compressio_220b6d_p1_lease_key(const char *session_id,
                                           const json_t *sessions);

/* ``get_compression_failure_cooldown``: {cooldown_until, remaining_seconds,
 * error} when armed and unexpired, else null. row is [until, error]|null. */
json_t *w31_compressio_220b6d_p1_cooldown_active(const char *session_id,
                                                 const json_t *row, double now);

/* ``stamp_compression_ancestors``: ids whose created_source this walk stamps,
 * in walk order. created is {"<id>": {parent_session_id, created_source}};
 * unset_ancestor is the ``_UNSET_ANCESTOR_SQL`` answer. */
json_t *w31_compressio_220b6d_p1_stamp_ancestors(const char *session_id,
                                                 const json_t *created,
                                                 const json_t *steps,
                                                 int unset_ancestor, int cap);

/* ``_publish_child_session_row``: the child sessions row's bound parameters.
 * surface_parent / surface_source are ``created_surface()``'s answers for the
 * parent's and the caller's label (null for local/unknown); prompt_hash is
 * ``_store_system_prompt``'s answer; own_profile is ``_own_profile_name``'s. */
json_t *w31_compressio_220b6d_p1_child_row(const json_t *parent,
                                           const json_t *model_config,
                                           const char *source,
                                           const char *model,
                                           const char *parent_session_id,
                                           const char *child_id,
                                           const char *cwd,
                                           const char *profile_name,
                                           const char *prompt_hash,
                                           const char *own_profile,
                                           const json_t *surface_parent,
                                           const json_t *surface_source,
                                           double now);

#ifdef __cplusplus
}
#endif
#endif
