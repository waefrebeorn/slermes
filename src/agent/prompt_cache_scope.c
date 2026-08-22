/*
 * prompt_cache_scope.c — C11 port of agent/prompt_cache_scope.py.
 *
 * resolve_prompt_cache_scope() maps the physical session id to the ROOT of
 * its compression lineage via SessionDB.get_compression_lineage (fork-aware,
 * #79193). Resolution is memoized per (agent, session_id, db-presence): the
 * lineage walk runs once per transcript segment, not per API call (#79017).
 * A failed walk on a persisting agent is NOT memoized — falling back to the
 * physical id must not pin a wrong scope after the session row lands.
 */
#define _POSIX_C_SOURCE 200809L
#include "prompt_cache_scope.h"

#include <stdlib.h>
#include <string.h>

/* PoP: _lineage_root @ agent/prompt_cache_scope.py:_lineage_root */
char *_lineage_root(const char *session_id, const prompt_cache_scope_db *session_db)
{
    if (!session_db || !session_db->get_compression_lineage)
        return NULL;
    /* Python wraps get_compression_lineage in try/except — a C callback that
     * returns NULL models both "raised" and "None". */
    json_t *lineage = session_db->get_compression_lineage(
        session_db->ctx, session_id ? session_id : "");
    if (!lineage)
        return NULL;
    char *root = NULL;
    if (lineage->type == JSON_ARRAY && json_len(lineage) > 0) {
        json_t *first = json_get(lineage, 0);
        if (first && first->type == JSON_STRING && first->str_val
            && first->str_val[0])
            root = strdup(first->str_val);
    }
    json_free(lineage);
    return root;
}

void prompt_cache_scope_agent_memo_free(prompt_cache_scope_agent *agent)
{
    if (!agent)
        return;
    free(agent->memo_sid);
    free(agent->memo_scope);
    agent->memo_sid = NULL;
    agent->memo_scope = NULL;
    agent->has_memo = 0;
}

/* PoP: resolve_prompt_cache_scope @ agent/prompt_cache_scope.py:resolve_prompt_cache_scope */
char *resolve_prompt_cache_scope(prompt_cache_scope_agent *agent)
{
    if (!agent)
        return strdup("");
    const char *raw_sid = agent->session_id;   /* str(None or "") -> "" */
    if (!raw_sid || !*raw_sid)
        return strdup("");
    const prompt_cache_scope_db *db = agent->_session_db;

    /* Memo key includes DB presence: an agent that starts DB-less and gains a
     * handle later must re-resolve instead of staying pinned. */
    int key_db_present = (db != NULL);
    if (agent->has_memo && agent->memo_sid && agent->memo_scope
        && strcmp(agent->memo_sid, raw_sid) == 0
        && agent->memo_db_present == key_db_present)
        return strdup(agent->memo_scope);

    char *root = NULL;
    if (db != NULL) {
        root = _lineage_root(raw_sid, db);
        /* _lineage_root failure == degraded fallback, handled below exactly
         * like Python's except-return-None path. */
    }
    const char *scope = (root && root[0]) ? root : raw_sid;
    /* Copy the scope before freeing root (scope may alias root). */
    char *result = strdup(scope);

    int should_memoize =
        (root != NULL) || (db == NULL) || agent->_persist_disabled;
    if (should_memoize) {
        /* setattr(_prompt_cache_scope_memo, (key, scope)); frozen/slotted
         * doubles skip memoization silently — allocation failure here is the
         * same benign degradation. */
        char *new_sid = strdup(raw_sid);
        char *new_scope = strdup(scope);
        if (new_sid && new_scope) {
            free(agent->memo_sid);
            free(agent->memo_scope);
            agent->memo_sid = new_sid;
            agent->memo_scope = new_scope;
            agent->memo_db_present = key_db_present;
            agent->has_memo = 1;
        } else {
            free(new_sid);
            free(new_scope);
        }
    }
    free(root);
    return result;
}

/* PoP: resolve_prompt_cache_scope_safe @ agent/prompt_cache_scope.py:resolve_prompt_cache_scope_safe */
char *resolve_prompt_cache_scope_safe(prompt_cache_scope_agent *agent)
{
    /* try: return resolve(...) or None; except: None. The C resolution is
     * total, so only the empty-scope branch maps to NULL here. */
    char *scope = resolve_prompt_cache_scope(agent);
    if (!scope)
        return NULL;
    if (!scope[0]) {
        free(scope);
        return NULL;
    }
    return scope;
}
