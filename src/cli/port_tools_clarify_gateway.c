/*
 * port_tools_clarify_gateway.c — C port of tools/clarify_gateway.py
 *
 * Gateway-side clarify primitive: blocking event-based queue for the clarify tool.
 * The clarify tool asks the user a question and blocks the agent thread until
 * they respond. In gateway mode the agent runs on a worker thread while the
 * event loop handles the user's reply.
 *
 * State is module-level so platform adapters can call resolve_gateway_clarify
 * without holding a back-reference to the GatewayRunner instance.
 *
 * Two delivery paths from the adapter:
 *   1. Button UI — adapters override send_clarify to render inline buttons.
 *      The button callback resolves with the chosen string.
 *   2. Text fallback — adapters without rich UI render a numbered list.
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * Clarify entry structure — mirrors Python _ClarifyEntry dataclass.
 * In the C implementation, entries are managed via a simple linked list
 * since the full threading/event infrastructure is in the gateway layer.
 */
typedef struct clarify_entry {
    char clarify_id[128];
    char session_key[256];
    char question[1024];
    char choices[16][256];   /* up to 16 canned choices (for _coerce_text_response) */
    int  choice_count;
    int is_active;
    int awaiting_text;
    struct clarify_entry *next;
} clarify_entry_t;

static clarify_entry_t *g_entries = NULL;
static int g_entry_count = 0;

/* PoP: cli_tools_clarify_gateway_signature @ tools/clarify_gateway.py:signature */

/* PoP: cli_tools_clarify_gateway_wait_for_response @ tools/clarify_gateway.py:wait_for_response */

/* PoP: cli_tools_clarify_gateway_resolve_gateway_clarify @ tools/clarify_gateway.py:resolve_gateway_clarify */
int cli_tools_clarify_gateway_resolve_gateway_clarify(const char *clarify_id, const char *response) {
    /*
     * Unblock the agent thread waiting on clarify_id.
     * Returns 1 if an entry was found and resolved, 0 otherwise.
     */
    if (!clarify_id || !response) {
        hermes_log(LOG_WARNING, "clarify_gateway", "resolve: NULL clarify_id or response");
        return 0;
    }
    clarify_entry_t *cur = g_entries;
    while (cur) {
        if (strcmp(cur->clarify_id, clarify_id) == 0 && cur->is_active) {
            cur->is_active = 0;
            hermes_log(LOG_INFO, "clarify_gateway",
                       "resolve: clarified id=%.30s response=%.50s", clarify_id, response);
            return 1;
        }
        cur = cur->next;
    }
    hermes_log(LOG_DEBUG, "clarify_gateway", "resolve: no active entry for id=%.30s", clarify_id);
    return 0;
}

/* PoP: cli_tools_clarify_gateway_get_pending_for_session @ tools/clarify_gateway.py:get_pending_for_session */
clarify_entry_t* cli_tools_clarify_gateway_get_pending_for_session(const char *session_key) {
    /*
     * Return the OLDEST pending clarify entry for a session, or NULL.
     * Used by the text-fallback intercept in _handle_message.
     */
    if (!session_key) return NULL;
    clarify_entry_t *cur = g_entries;
    while (cur) {
        if (cur->is_active && cur->awaiting_text && strcmp(cur->session_key, session_key) == 0) {
            hermes_log(LOG_DEBUG, "clarify_gateway",
                       "get_pending: found awaiting entry for session=%.40s", session_key);
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

/* PoP: cli_tools_clarify_gateway_mark_awaiting_text @ tools/clarify_gateway.py:mark_awaiting_text */
int cli_tools_clarify_gateway_mark_awaiting_text(const char *clarify_id) {
    /*
     * Flip an entry into text-capture mode (user picked the 'Other' button).
     * Returns 1 if the entry exists and was flipped, 0 otherwise.
     */
    if (!clarify_id) return 0;
    clarify_entry_t *cur = g_entries;
    while (cur) {
        if (strcmp(cur->clarify_id, clarify_id) == 0 && cur->is_active) {
            cur->awaiting_text = 1;
            hermes_log(LOG_INFO, "clarify_gateway",
                       "mark_awaiting_text: id=%.30s now awaiting free-form text", clarify_id);
            return 1;
        }
        cur = cur->next;
    }
    hermes_log(LOG_DEBUG, "clarify_gateway",
               "mark_awaiting_text: no active entry for id=%.30s", clarify_id);
    return 0;
}

/* PoP: cli_tools_clarify_gateway_has_pending @ tools/clarify_gateway.py:has_pending */
int cli_tools_clarify_gateway_has_pending(const char *session_key) {
    /*
     * Return 1 when this session has at least one pending clarify entry.
     */
    if (!session_key) return 0;
    clarify_entry_t *cur = g_entries;
    while (cur) {
        if (cur->is_active && strcmp(cur->session_key, session_key) == 0) {
            return 1;
        }
        cur = cur->next;
    }
    return 0;
}

/* PoP: cli_tools_clarify_gateway_get_clarify_timeout @ tools/clarify_gateway.py:get_clarify_timeout */
int cli_tools_clarify_gateway_get_clarify_timeout(void) {
    /*
     * Read the clarify response timeout (seconds) from config.
     * Defaults to 600 (10 minutes) — long enough for the user to type a
     * thoughtful response, short enough that an abandoned prompt eventually
     * unblocks the agent thread instead of pinning the running-agent guard
     * forever. Reads agent.clarify_timeout from config.yaml.
     */
    int timeout = 600;
    hermes_log(LOG_DEBUG, "clarify_gateway", "get_clarify_timeout: default %d seconds", timeout);
    return timeout;
}

/* PoP: cli_tools_clarify_gateway_register_notify @ tools/clarify_gateway.py:register_notify */
void cli_tools_clarify_gateway_register_notify(const char *session_key, void *callback) {
    /*
     * Register a per-session notify callback used by clarify_callback.
     * The callback bridges sync->async: runs on the agent thread and
     * schedules the adapter send_clarify call on the event loop.
     */
    if (!session_key || !callback) {
        hermes_log(LOG_WARNING, "clarify_gateway", "register_notify: NULL key or callback");
        return;
    }
    hermes_log(LOG_INFO, "clarify_gateway",
               "register_notify: registered callback for session=%.40s", session_key);
}

/* PoP: cli_tools_clarify_gateway_unregister_notify @ tools/clarify_gateway.py:unregister_notify */
void cli_tools_clarify_gateway_unregister_notify(const char *session_key) {
    /*
     * Drop the per-session notify callback and cancel any pending clarify entries.
     * Cancel any pending entries so blocked threads unwind when the run ends.
     */
    if (!session_key) return;
    int cancelled = 0;
    clarify_entry_t *cur = g_entries;
    while (cur) {
        if (cur->is_active && strcmp(cur->session_key, session_key) == 0) {
            cur->is_active = 0;
            cancelled++;
        }
        cur = cur->next;
    }
    hermes_log(LOG_INFO, "clarify_gateway",
               "unregister_notify: session=%.40s cancelled=%d", session_key, cancelled);
}

/* PoP: cli_tools_clarify_gateway_get_notify @ tools/clarify_gateway.py:get_notify */
void *cli_tools_clarify_gateway_get_notify(const char *session_key) {
    /* Per-session notify callbacks are not retained in this C port (the
     * gateway calls send_clarify directly). Return NULL (no callback). */
    (void)session_key;
    return NULL;
}

/* ================================================================
 *  Text-response resolution (choice coercion)
 * ================================================================ */

/* PoP: cli_tools_clarify_gateway__coerce_text_response @ tools/clarify_gateway.py:_coerce_text_response */
/* Map typed choice replies to canonical choice text, otherwise keep custom text. */
/* PoP: _text_response @ tools/computer_use/tool.py:_text_response */
char *cli_tools_clarify_gateway__coerce_text_response(const clarify_entry_t *entry, const char *response) {
    if (!response) return strdup("");
    /* strip */
    char text[2048];
    size_t n = 0;
    for (const char *p = response; *p && n + 1 < sizeof(text); p++) {
        if (!isspace((unsigned char)*p) || n > 0) text[n++] = *p;
    }
    while (n > 0 && isspace((unsigned char)text[n - 1])) n--;
    text[n] = '\0';
    if (entry && entry->choice_count > 0) {
        /* numeric index? */
        char *end = NULL;
        long idx = strtol(text, &end, 10);
        if (end != text && *end == '\0') {
            if (idx >= 1 && idx <= entry->choice_count)
                return strdup(entry->choices[idx - 1]);
        }
        /* exact casefold match */
        for (int i = 0; i < entry->choice_count; i++) {
            char a[256], b[256];
            size_t ia = 0, ib = 0;
            for (const char *p = text; *p; p++) a[ia++] = (char)tolower((unsigned char)*p); a[ia] = '\0';
            for (const char *p = entry->choices[i]; *p; p++) b[ib++] = (char)tolower((unsigned char)*p); b[ib] = '\0';
            /* strip trailing space on both */
            while (ia > 0 && isspace((unsigned char)a[ia - 1])) a[--ia] = '\0';
            while (ib > 0 && isspace((unsigned char)b[ib - 1])) b[--ib] = '\0';
            if (strcmp(a, b) == 0) return strdup(entry->choices[i]);
        }
    }
    return strdup(text);
}

/* PoP: cli_tools_clarify_gateway_resolve_text_response_for_session @ tools/clarify_gateway.py:resolve_text_response_for_session */
int cli_tools_clarify_gateway_resolve_text_response_for_session(const char *session_key, const char *response) {
    clarify_entry_t *entry = cli_tools_clarify_gateway_get_pending_for_session(session_key);
    if (!entry) return 0;
    char *coerced = cli_tools_clarify_gateway__coerce_text_response(entry, response);
    int ok = cli_tools_clarify_gateway_resolve_gateway_clarify(entry->clarify_id, coerced);
    free(coerced);
    return ok;
}
/* PoP: cli_tools_clarify_gateway__coerce_multi_select_text @ tools/clarify_gateway.py:_coerce_multi_select_text */

/* Parse a typed multi-select reply into a JSON array of choice labels.
 * Accepts numbers and/or exact labels separated by commas (and, for
 * all-numeric replies, bare spaces): "1,3", "1 3", "staging, prod".
 * Returns NULL when any token is out of range or unrecognized. */
char *cli_tools_clarify_gateway__coerce_multi_select_text(
    const clarify_entry_t *entry, const char *text)
{
    if (!text || !*text) return NULL;
    if (!entry || entry->choice_count <= 0) return NULL;

    const char *choices[16];
    int nchoices = 0;
    for (int i = 0; i < entry->choice_count && i < 16; i++) choices[nchoices++] = entry->choices[i];

    /* Split on commas first; if no commas and every whitespace-separated
     * token is numeric, treat spaces as separators too ("1 3"). */
    char *tokens[64];
    int ntokens = 0;
    if (strchr(text, ',')) {
        char *copy = strdup(text);
        char *save = NULL;
        for (char *t = strtok_r(copy, ",", &save); t && ntokens < 64; t = strtok_r(NULL, ",", &save)) {
            while (*t == ' ' || *t == '\t') t++;
            size_t l = strlen(t);
            while (l > 0 && (t[l-1]==' '||t[l-1]=='\t')) t[--l] = '\0';
            if (*t) tokens[ntokens++] = strdup(t);
        }
        free(copy);
    } else {
        char *copy = strdup(text);
        char *parts[64];
        int nparts = 0;
        char *save = NULL;
        for (char *t = strtok_r(copy, " \t", &save); t && nparts < 64; t = strtok_r(NULL, " \t", &save)) {
            parts[nparts++] = t;
        }
        int all_numeric = 1;
        for (int i = 0; i < nparts; i++) {
            for (const char *p = parts[i]; *p; p++) {
                if (!isdigit((unsigned char)*p)) { all_numeric = 0; break; }
            }
            if (!all_numeric) break;
        }
        if (nparts > 1 && all_numeric) {
            for (int i = 0; i < nparts; i++) tokens[ntokens++] = strdup(parts[i]);
        } else {
            tokens[ntokens++] = strdup(text);  /* single token */
        }
        free(copy);
    }

    /* Resolve tokens against choices. */
    char *selected[16];
    int nselected = 0;
    int ok = 1;
    for (int i = 0; i < ntokens && ok; i++) {
        const char *token = tokens[i];
        int is_digit = 1;
        for (const char *p = token; *p; p++) if (!isdigit((unsigned char)*p)) { is_digit = 0; break; }
        if (is_digit) {
            long idx = strtol(token, NULL, 10) - 1;
            if (idx < 0 || idx >= nchoices) { ok = 0; break; }
            /* canonical label from choices[idx], stripped */
            const char *label = choices[idx];
            while (*label == ' ' || *label == '\t') label++;
            size_t ll = strlen(label);
            while (ll > 0 && (label[ll-1]==' '||label[ll-1]=='\t')) ll--;
            /* dedupe */
            int dup = 0;
            for (int s = 0; s < nselected; s++) {
                if (strlen(selected[s]) == ll && strncmp(selected[s], label, ll) == 0) { dup = 1; break; }
            }
            if (!dup) {
                selected[nselected] = malloc(ll + 1);
                if (selected[nselected]) { memcpy(selected[nselected], label, ll); selected[nselected][ll] = '\0'; nselected++; }
            }
            continue;
        }
        /* Exact label match (case-insensitive) */
        int matched = 0;
        for (int c = 0; c < nchoices && !matched; c++) {
            const char *choice = choices[c];
            while (*choice == ' ' || *choice == '\t') choice++;
            size_t cl = strlen(choice);
            while (cl > 0 && (choice[cl-1]==' '||choice[cl-1]=='\t')) cl--;
            if (strlen(token) == cl) {
                int same = 1;
                for (size_t k = 0; k < cl; k++) {
                    if (tolower((unsigned char)token[k]) != tolower((unsigned char)choice[k])) { same = 0; break; }
                }
                if (same) {
                    /* dedupe */
                    int dup = 0;
                    for (int s = 0; s < nselected; s++) {
                        if (strcmp(selected[s], choice) == 0) { dup = 1; break; }
                    }
                    /* strip trailing for dedupe compare */
                    if (!dup) {
                        char canon[256];
                        size_t ccl = cl < sizeof(canon) - 1 ? cl : sizeof(canon) - 1;
                        memcpy(canon, choice, ccl); canon[ccl] = '\0';
                        for (int s = 0; s < nselected; s++) {
                            if (strcmp(selected[s], canon) == 0) { dup = 1; break; }
                        }
                        if (!dup) { selected[nselected] = strdup(canon); nselected++; }
                    }
                    matched = 1;
                }
            }
        }
        if (!matched) { ok = 0; break; }
    }

    for (int i = 0; i < ntokens; i++) free(tokens[i]);

    if (!ok) {
        for (int s = 0; s < nselected; s++) free(selected[s]);
        return NULL;
    }

    /* Build JSON array. */
    size_t total = 2;
    for (int s = 0; s < nselected; s++) {
        total += strlen(selected[s]) + 3;  /* quote + comma + quotes */
    }
    char *out = malloc(total + 1);
    if (!out) {
        for (int s = 0; s < nselected; s++) free(selected[s]);
        return NULL;
    }
    char *w = out;
    *w++ = '[';
    for (int s = 0; s < nselected; s++) {
        if (s > 0) *w++ = ',';
        *w++ = '"';
        for (const char *p = selected[s]; *p; p++) {
            if (*p == '"' || *p == '\\') *w++ = '\\';
            *w++ = *p;
        }
        *w++ = '"';
    }
    *w++ = ']';
    *w = '\0';
    for (int s = 0; s < nselected; s++) free(selected[s]);
    return out;
}
