/*
 * prompt_cache_scope.h — C11 port of agent/prompt_cache_scope.py.
 *
 * Rotation-stable logical cache scope for prompt_cache_key derivation:
 * maps the physical session id to the ROOT of its compression lineage so
 * context-compression rotation does not move a conversation into a fresh
 * cache bucket (issue #79017). NOT get_conversation_root — fork children and
 * delegate subagents keep their isolated scope (#79161).
 */
#ifndef AGENT_PROMPT_CACHE_SCOPE_H
#define AGENT_PROMPT_CACHE_SCOPE_H

#include <json.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DB handle seam mirroring SessionDB.get_compression_lineage(session_id) ->
 * list[str] | None. The oracle injects deterministic lineages here. */
typedef struct {
    void *ctx;
    json_t *(*get_compression_lineage)(void *ctx, const char *session_id);
} prompt_cache_scope_db;

/* Minimal agent surface consumed by resolve_prompt_cache_scope (mirrors the
 * Python duck-typed attribute reads: session_id, _session_db,
 * _persist_disabled, plus the memo slot). Strings borrowed, not owned. */
typedef struct {
    const char *session_id;
    prompt_cache_scope_db *_session_db;   /* NULL = no DB attached */
    int _persist_disabled;
    /* memo slot (_prompt_cache_scope_memo): key=(sid, db is not None) */
    int has_memo;
    char *memo_sid;            /* owned */
    int memo_db_present;
    char *memo_scope;          /* owned */
} prompt_cache_scope_agent;

/* PoP: _lineage_root @ agent/prompt_cache_scope.py:_lineage_root */
/* Compression-lineage root of session_id, or NULL. Defensive about the DB
 * handle: non-list/empty results are ignored. Caller frees. */
char *_lineage_root(const char *session_id, const prompt_cache_scope_db *session_db);

/* PoP: resolve_prompt_cache_scope @ agent/prompt_cache_scope.py:resolve_prompt_cache_scope */
char *resolve_prompt_cache_scope(prompt_cache_scope_agent *agent);

/* PoP: resolve_prompt_cache_scope_safe @ agent/prompt_cache_scope.py:resolve_prompt_cache_scope_safe */
char *resolve_prompt_cache_scope_safe(prompt_cache_scope_agent *agent);

/* Free the memo strings held by an agent record (helper for owners of the
 * struct; Python frees with the object). */
void prompt_cache_scope_agent_memo_free(prompt_cache_scope_agent *agent);

#endif /* AGENT_PROMPT_CACHE_SCOPE_H */
