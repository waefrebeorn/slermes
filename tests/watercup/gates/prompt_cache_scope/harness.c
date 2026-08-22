/* gate: resolve_prompt_cache_scope — traced C copy of
 * src/agent/prompt_cache_scope.c::resolve_prompt_cache_scope (LIVE twin:
 * agent/prompt_cache_scope.py::resolve_prompt_cache_scope).
 *
 * Links the REAL shipped port; the DB seam is injected per probe.
 *
 * Branch ladder (matches twin.py):
 *   B1: session_id empty           -> ""
 *   B2: memo hit (same sid+db)     -> memoized scope
 *   B3: db attached?
 *   B4: lineage root found?
 *   B5: memoize? (root found OR no db OR persist_disabled)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libjson/json.h>
#include "watercup.h"
#include "prompt_cache_scope.h"

/* Probe table, driven by argv: each probe is "sid|lineage_json|persist_disabled|repeat"
 *   lineage_json = JSON array or "null"   repeat = resolve N times (memo test) */
static json_t *fake_lineage(void *ctx, const char *session_id) {
    (void)session_id;
    if (!ctx) return json_null();
    return json_parse((const char *)ctx, NULL);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s resolve_prompt_cache_scope <probe>...\n"
                        "probe = sid|lineage_json|persist_disabled|repeat\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        char probe[4096];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *fields[4] = {NULL, NULL, NULL, NULL};
        int nf = 0;
        char *p = probe;
        fields[nf++] = p;
        while (*p && nf < 4) {
            if (*p == '|') { *p = '\0'; fields[nf++] = p + 1; }
            p++;
        }
        const char *sid        = fields[0] ? fields[0] : "";
        const char *lineage    = fields[1] && fields[1][0] && strcmp(fields[1],"null")!=0
                               ? fields[1] : NULL;
        int persist_disabled   = fields[2] ? atoi(fields[2]) : 0;
        int repeat             = fields[3] ? atoi(fields[3]) : 1;
        if (repeat < 1) repeat = 1;

        WCUP_BEGIN("resolve_prompt_cache_scope");
        prompt_cache_scope_db db = { lineage ? (void *)lineage : NULL, fake_lineage };
        prompt_cache_scope_agent agent;
        memset(&agent, 0, sizeof agent);
        agent.session_id = sid;
        agent._session_db = lineage ? &db : NULL;
        agent._persist_disabled = persist_disabled;

        char result[512];
        result[0] = '\0';
        for (int r = 0; r < repeat; r++) {
            /* B1: empty sid */
            if (!agent.session_id || !agent.session_id[0]) {
                WCUP_BRANCH(1);
                snprintf(result, sizeof result, "");
                break;
            }
            WCUP_BRANCH(0);
            /* B2: memo hit */
            if (agent.has_memo && agent.memo_sid &&
                strcmp(agent.memo_sid, agent.session_id) == 0 &&
                agent.memo_db_present == (agent._session_db != NULL)) {
                WCUP_BRANCH(1);
                snprintf(result, sizeof result, "%s", agent.memo_scope);
                continue;
            }
            WCUP_BRANCH(0);
            /* B3: db attached */
            char *root = NULL;
            if (agent._session_db) {
                WCUP_BRANCH(1);
                root = _lineage_root(agent.session_id, agent._session_db);
            } else {
                WCUP_BRANCH(0);
            }
            /* B4: root found */
            const char *scope = (root && root[0]) ? root : agent.session_id;
            if (root && root[0]) {
                WCUP_BRANCH(1);
            } else {
                WCUP_BRANCH(0);
            }
            /* B5: memoize? */
            if (root || !agent._session_db || agent._persist_disabled) {
                WCUP_BRANCH(1);
                free(agent.memo_sid);
                free(agent.memo_scope);
                agent.memo_sid = strdup(agent.session_id);
                agent.memo_db_present = agent._session_db != NULL;
                agent.memo_scope = strdup(scope);
                agent.has_memo = 1;
            } else {
                WCUP_BRANCH(0);
            }
            snprintf(result, sizeof result, "%s", scope);
            free(root);
        }
        char esc[600];
        size_t o = 0;
        for (const char *q = result; *q && o + 2 < sizeof esc; q++) {
            if (*q == '"' || *q == '\\') esc[o++] = '\\';
            esc[o++] = *q;
        }
        esc[o] = '\0';
        (void)esc;
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("resolve_prompt_cache_scope");
        prompt_cache_scope_agent_memo_free(&agent);
    }
    return 0;
}
