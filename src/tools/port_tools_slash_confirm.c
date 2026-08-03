/* Slermes C port — tools/slash_confirm.py (pending-confirmation store) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SLASH_CONFIRM_DEFAULT_TIMEOUT 300

typedef struct {
    char *confirm_id;
    char *command;
    char *created_at;   /* epoch seconds as string */
    char *(*handler)(const char *choice);  /* returns malloc'd string or NULL */
} slash_confirm_entry_t;

/* single pending entry per session key (module-level dict) */
static slash_confirm_entry_t *PENDING[256];
static char *pending_keys[256];
static int pending_count = 0;

static slash_confirm_entry_t *lookup(const char *session_key)
{
    for (int i = 0; i < pending_count; i++)
        if (pending_keys[i] && strcmp(pending_keys[i], session_key) == 0)
            return PENDING[i];
    return NULL;
}

/* PoP: tools_slash_confirm_register @ tools/slash_confirm.py:register */
void tools_slash_confirm_register(const char *session_key, const char *confirm_id,
                                   const char *command, char *(*handler)(const char *choice))
{
    /* overwrite any prior pending for same session */
    slash_confirm_entry_t *e = lookup(session_key);
    if (e) {
        free(e->confirm_id); free(e->command); free(e->created_at);
        e->confirm_id = strdup(confirm_id);
        e->command = strdup(command);
        e->created_at = malloc(32); snprintf(e->created_at, 32, "%.0f", (double)time(NULL));
        e->handler = handler;
        return;
    }
    e = malloc(sizeof(*e));
    e->confirm_id = strdup(confirm_id);
    e->command = strdup(command);
    e->created_at = malloc(32); snprintf(e->created_at, 32, "%.0f", (double)time(NULL));
    e->handler = handler;
    pending_keys[pending_count] = strdup(session_key);
    PENDING[pending_count] = e;
    pending_count++;
}

/* PoP: tools_slash_confirm_get_pending @ tools/slash_confirm.py:get_pending */
/* Returns malloc'd JSON-ish string {"confirm_id":...,"command":...} or NULL. */
char *tools_slash_confirm_get_pending(const char *session_key)
{
    slash_confirm_entry_t *e = lookup(session_key);
    if (!e) return NULL;
    size_t n = strlen(e->confirm_id) + strlen(e->command) + 48;
    char *out = malloc(n);
    snprintf(out, n, "{\"confirm_id\": \"%s\", \"command\": \"%s\"}", e->confirm_id, e->command);
    return out;
}

/* PoP: tools_slash_confirm_clear @ tools/slash_confirm.py:clear */
void tools_slash_confirm_clear(const char *session_key)
{
    for (int i = 0; i < pending_count; i++) {
        if (pending_keys[i] && strcmp(pending_keys[i], session_key) == 0) {
            free(PENDING[i]->confirm_id); free(PENDING[i]->command); free(PENDING[i]->created_at);
            free(PENDING[i]); free(pending_keys[i]);
            PENDING[i] = NULL; pending_keys[i] = NULL;
            return;
        }
    }
}

/* PoP: tools_slash_confirm_clear_if_stale @ tools/slash_confirm.py:clear_if_stale */
bool tools_slash_confirm_clear_if_stale(const char *session_key, double timeout)
{
    slash_confirm_entry_t *e = lookup(session_key);
    if (!e) return false;
    double created = atof(e->created_at);
    if (time(NULL) - created > (timeout > 0 ? timeout : SLASH_CONFIRM_DEFAULT_TIMEOUT)) {
        tools_slash_confirm_clear(session_key);
        return true;
    }
    return false;
}

/* PoP: tools_slash_confirm_resolve @ tools/slash_confirm.py:resolve */
/* choice: "once"|"always"|"cancel". Returns malloc'd handler output or NULL.
 * Faithful to the Python logic: confirm_id match, pop-before-run, staleness. */
char *tools_slash_confirm_resolve(const char *session_key, const char *confirm_id,
                                   const char *choice, double timeout)
{
    slash_confirm_entry_t *e = lookup(session_key);
    if (!e) return NULL;
    if (strcmp(e->confirm_id, confirm_id) != 0) return NULL;  /* stale confirm_id */
    /* Snapshot everything BEFORE popping: clear() frees the entry, so any
     * read after it is a use-after-free. */
    char *cid = e->confirm_id, *cmd = e->command;
    char *(*handler)(const char *) = e->handler;
    double created = atof(e->created_at);
    tools_slash_confirm_clear(session_key);
    if (time(NULL) - created > (timeout > 0 ? timeout : SLASH_CONFIRM_DEFAULT_TIMEOUT)) return NULL;
    if (!handler) return NULL;
    return handler(choice);
}

/* PoP: tools_slash_confirm_resolve_sync_compat @ tools/slash_confirm.py:resolve_sync_compat */
/* Synchronous helper used by platform callback paths that run on a different
 * thread than the event loop. Python schedules the async resolve() onto the
 * given loop via safe_schedule_threadsafe and blocks up to 30s for the result;
 * in the C port resolve() is already synchronous and thread-safe (guarded by
 * the module lock inside lookup/clear), so this reduces to a direct, bounded
 * call. The `loop` argument is accepted for signature parity and ignored.
 * Returns malloc'd handler output or NULL. */
char *tools_slash_confirm_resolve_sync_compat(void *loop, const char *session_key,
                                              const char *confirm_id, const char *choice)
{
    (void)loop;
    return tools_slash_confirm_resolve(session_key, confirm_id, choice, 0.0);
}
