/* hermes_state_repair.c — faithful C11 port of
 * agent/agent_runtime_helpers.py:repair_message_sequence, the pre-request
 * alternation repair (also invoked by get_messages_as_conversation with
 * repair_alternation=True at session restore). Operates on a flat array of
 * repair_msg_t. Three passes, mirrored exactly:
 *   Pass 0: merge consecutive assistant messages (union tool_calls,
 *           concatenated content, verification-candidate replacement,
 *           codex-interim exemption).
 *   Pass 1: drop stray tool messages whose tool_call_id doesn't match a
 *           known id from the preceding assistant turn (ids consumed after
 *           match so duplicates drop; user turn clears the known set).
 *   Pass 2: merge consecutive user messages with "\n\n" separator.
 * Returns the number of repairs. Self-contained unit.
 */

#include "hermes_state_db.h"
#include <stdlib.h>
#include <string.h>

/* ── helpers ──────────────────────────────────────────────────────────── */

static bool is_codex_interim(const repair_msg_t *m) {
    /* Python: codex_reasoning_items or codex_message_items or
     * finish_reason == "incomplete" */
    if (m->codex_items) return true;
    return m->finish_reason && strcmp(m->finish_reason, "incomplete") == 0;
}

static bool is_verification_candidate(const repair_msg_t *m) {
    return m->finish_reason &&
        (strcmp(m->finish_reason, "verification_required") == 0 ||
         strcmp(m->finish_reason, "verify_hook_continue") == 0);
}

static char *join_str(const char *a, const char *sep, const char *b) {
    size_t la = a ? strlen(a) : 0, ls = strlen(sep), lb = b ? strlen(b) : 0;
    char *out = malloc(la + ls + lb + 1);
    size_t off = 0;
    if (la) { memcpy(out, a, la); off += la; }
    if (la && lb) { memcpy(out + off, sep, ls); off += ls; }
    if (lb) { memcpy(out + off, b, lb); off += lb; }
    out[off] = '\0';
    return out;
}

/* strip() both sides then join with "\n", skipping empties (Pass 0). */
static char *strip_join_nl(const char *a, const char *b) {
    /* Python: "\n".join(p for p in (prev.strip(), new.strip()) if p) */
    char *sa = a ? strdup(a) : strdup("");
    char *sb = b ? strdup(b) : strdup("");
    /* trim in place */
    for (char *s2 = sa; ; s2 = sb) {
        char *s = s2;
        size_t l = strlen(s);
        size_t st = 0;
        while (st < l && (s[st] == ' ' || s[st] == '\t' || s[st] == '\n' || s[st] == '\r')) st++;
        while (l > st && (s[l-1] == ' ' || s[l-1] == '\t' || s[l-1] == '\n' || s[l-1] == '\r')) l--;
        memmove(s, s + st, l - st);
        s[l - st] = '\0';
        if (s2 == sb) break;
    }
    char *out;
    if (*sa && *sb) out = join_str(sa, "\n", sb);
    else out = strdup(*sa ? sa : sb);
    free(sa); free(sb);
    return out;
}

/* tool_call id set: flat array of strdup'd ids. */
typedef struct { char **ids; int n, cap; } idset_t;
static void idset_clear(idset_t *s) {
    for (int i = 0; i < s->n; i++) free(s->ids[i]);
    s->n = 0;
}
static void idset_add(idset_t *s, const char *id) {
    if (!id || !*id) return;
    for (int i = 0; i < s->n; i++)
        if (strcmp(s->ids[i], id) == 0) return;
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->ids = realloc(s->ids, sizeof(char*) * (size_t)s->cap);
    }
    s->ids[s->n++] = strdup(id);
}
static bool idset_take(idset_t *s, const char *id) {
    /* match + consume (Python: known_tool_ids.discard after match, #58327) */
    if (!id) return false;
    for (int i = 0; i < s->n; i++) {
        if (strcmp(s->ids[i], id) == 0) {
            free(s->ids[i]);
            s->ids[i] = s->ids[--s->n];
            return true;
        }
    }
    return false;
}

/* register both id and call_id of every tool_call (superset match, #58168).
 * tool_call_ids is a ";"-separated list of "id[,call_id]" entries. */
static void register_tool_calls(idset_t *s, const char *tool_call_ids) {
    if (!tool_call_ids) return;
    char *dup = strdup(tool_call_ids);
    char *save1 = NULL;
    for (char *tok = strtok_r(dup, ";", &save1); tok;
         tok = strtok_r(NULL, ";", &save1)) {
        char *save2 = NULL;
        for (char *part = strtok_r(tok, ",", &save2); part;
             part = strtok_r(NULL, ",", &save2))
            idset_add(s, part);
    }
    free(dup);
}

/* ── the repair ───────────────────────────────────────────────────────── */

/* PoP: repair_message_sequence @ agent/agent_runtime_helpers.py:repair_message_sequence */
int hermes_state_repair_message_sequence(repair_msg_t *msgs, int *count) {
    if (!msgs || !count || *count == 0) return 0;
    int repairs = 0;
    int n = *count;

    /* Pass 0: merge consecutive assistant messages. */
    int w = 0;
    for (int r = 0; r < n; r++) {
        repair_msg_t *msg = &msgs[r];
        if (w > 0 && msg->role && strcmp(msg->role, "assistant") == 0 &&
            msgs[w-1].role && strcmp(msgs[w-1].role, "assistant") == 0 &&
            !is_codex_interim(msg) && !is_codex_interim(&msgs[w-1])) {
            repair_msg_t *prev = &msgs[w-1];
            if (is_verification_candidate(prev)) {
                /* later response supersedes the provisional candidate */
                free(prev->content); free(prev->tool_call_ids);
                free(prev->finish_reason); free(prev->reasoning_content);
                free(prev->role); free(prev->tool_call_id);
                *prev = *msg;               /* transfer ownership */
                memset(msg, 0, sizeof *msg);
                repairs++;
                continue;
            }
            /* union tool_calls (";"-joined id lists, order preserved) */
            if (msg->tool_call_ids && *msg->tool_call_ids) {
                if (prev->tool_call_ids && *prev->tool_call_ids) {
                    char *u = join_str(prev->tool_call_ids, ";", msg->tool_call_ids);
                    free(prev->tool_call_ids);
                    prev->tool_call_ids = u;
                } else {
                    free(prev->tool_call_ids);
                    prev->tool_call_ids = strdup(msg->tool_call_ids);
                }
            }
            /* concatenate plain-text content */
            if (prev->content && msg->content) {
                char *j = strip_join_nl(prev->content, msg->content);
                free(prev->content);
                prev->content = j;
            } else if ((!prev->content || !*prev->content) && msg->content) {
                free(prev->content);
                prev->content = strdup(msg->content);
            }
            /* carry reasoning_content from later turn only if earlier lacks */
            if ((!prev->reasoning_content || !*prev->reasoning_content) &&
                msg->reasoning_content && *msg->reasoning_content) {
                free(prev->reasoning_content);
                prev->reasoning_content = strdup(msg->reasoning_content);
            }
            /* free the merged-away message */
            free(msg->role); free(msg->content); free(msg->tool_call_id);
            free(msg->tool_call_ids); free(msg->finish_reason);
            free(msg->reasoning_content);
            memset(msg, 0, sizeof *msg);
            repairs++;
            continue;
        }
        if (w != r) { msgs[w] = *msg; memset(msg, 0, sizeof *msg); }
        w++;
    }
    n = w;

    /* Pass 1: drop stray tool messages. */
    idset_t known = {0};
    w = 0;
    for (int r = 0; r < n; r++) {
        repair_msg_t *msg = &msgs[r];
        const char *role = msg->role ? msg->role : "";
        if (strcmp(role, "assistant") == 0) {
            idset_clear(&known);
            register_tool_calls(&known, msg->tool_call_ids);
            if (w != r) { msgs[w] = *msg; memset(msg, 0, sizeof *msg); }
            w++;
        } else if (strcmp(role, "tool") == 0) {
            if (msg->tool_call_id && idset_take(&known, msg->tool_call_id)) {
                if (w != r) { msgs[w] = *msg; memset(msg, 0, sizeof *msg); }
                w++;
            } else {
                /* orphan/duplicate tool result — drop */
                free(msg->role); free(msg->content); free(msg->tool_call_id);
                free(msg->tool_call_ids); free(msg->finish_reason);
                free(msg->reasoning_content);
                memset(msg, 0, sizeof *msg);
                repairs++;
            }
        } else {
            if (strcmp(role, "user") == 0) idset_clear(&known);
            if (w != r) { msgs[w] = *msg; memset(msg, 0, sizeof *msg); }
            w++;
        }
    }
    idset_clear(&known);
    free(known.ids);
    n = w;

    /* Pass 2: merge consecutive user messages ("\n\n" separator). */
    w = 0;
    for (int r = 0; r < n; r++) {
        repair_msg_t *msg = &msgs[r];
        if (w > 0 && msg->role && strcmp(msg->role, "user") == 0 &&
            msgs[w-1].role && strcmp(msgs[w-1].role, "user") == 0) {
            repair_msg_t *prev = &msgs[w-1];
            const char *pc = prev->content ? prev->content : "";
            const char *nc = msg->content ? msg->content : "";
            char *j;
            if (*pc && *nc) j = join_str(pc, "\n\n", nc);
            else j = strdup(*pc ? pc : nc);
            free(prev->content);
            prev->content = j;
            free(msg->role); free(msg->content); free(msg->tool_call_id);
            free(msg->tool_call_ids); free(msg->finish_reason);
            free(msg->reasoning_content);
            memset(msg, 0, sizeof *msg);
            repairs++;
            continue;
        }
        if (w != r) { msgs[w] = *msg; memset(msg, 0, sizeof *msg); }
        w++;
    }
    *count = w;
    return repairs;
}
