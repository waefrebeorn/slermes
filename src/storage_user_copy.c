/*
 * storage_user_copy.c — real C11 port of the pure copy leaves of
 * storage_user_copy.py.
 *
 * PoP: storageuser_failure_details @ storage_user_copy.py:storage_failure_details
 * PoP: storageuser_describe_ex     @ storage_user_copy.py:describe_storage_failure
 * PoP: storageuser_recovery_docs_url @ storage_user_copy.py:STORAGE_RECOVERY_DOCS_URL
 *
 * The live describe_storage_failure reads three external facts
 * (classify_persistence_error, is_disk_full_error, profile_cli_selector).
 * They are injected as explicit arguments here rather than mutating process
 * or config state, so the pure table lookup + placeholder substitution is the
 * exact observable under test.
 */
#include "noc_w5_storageuser_ed737a.h"
#include <stdlib.h>
#include <string.h>

#define SU_DOCTOR "Run `hermes {profile_arg}doctor --fix` to diagnose and repair."
#define SU_DOCS_URL "https://hermes-agent.nousresearch.com/docs/user-guide/session-storage-recovery"

typedef struct {
    const char *cause;
    const char *code;
    const char *gloss;
    const char *action;
} SuFailure;

static const SuFailure SU_FAILURES[] = {
    {"locked", "storage_locked",
     "the session database is locked by another Hermes process",
     "Wait a moment and try again; if it persists, stop the other Hermes process "
     "(`hermes {profile_arg}gateway stop`)."},
    {"disk_full", "disk_full",
     "the disk is full",
     "Free some disk space, then try again."},
    {"disk", "storage_readonly",
     "the session database file is read-only or not writable",
     SU_DOCTOR},
    {"corrupt", "storage_corrupt",
     "the session database file is damaged",
     SU_DOCTOR " Recovery: `hermes {profile_arg}sessions recover --source <state.db> --inspect-only`."},
    {"session_row_missing", "storage_session_missing",
     "this session's database row was deleted while the chat was still open "
     "and could not be recreated",
     "Try again; if it persists, run `hermes {profile_arg}doctor`."},
    {"fts_index", "storage_index_corrupt",
     "the session search index is damaged (the messages themselves are intact)",
     "Run `hermes {profile_arg}doctor --fix` (or `hermes {profile_arg}sessions repair`) "
     "to rebuild it."},
    {"replaced", "storage_replaced",
     "the session database file was replaced while Hermes was running",
     "Stop Hermes (`hermes {profile_arg}gateway stop`), run `hermes {profile_arg}doctor`, "
     "then start it again."},
    {"deleted_wal", "storage_replaced",
     "another Hermes process still holds an old copy of the session database's "
     "write-ahead log, so Hermes stopped writing to keep the file safe",
     "Nothing is lost. Quit every Hermes process on this profile (Desktop app, "
     "`hermes {profile_arg}gateway stop`, dashboard, cron), run `hermes {profile_arg}doctor` "
     "\xe2\x80\x94 it names any process still holding the log \xe2\x80\x94 then start Hermes again. "
     "Do not run `doctor --fix` or delete any state.db files while they run. Guide: " SU_DOCS_URL},
    {"compression", "storage_busy",
     "another process is compressing this session",
     "Send your message again once compression finishes."},
    {"compression_closed", "storage_session_rotated",
     "this session was rotated by context compression",
     "Refresh the client (or start a new turn) and send your message again."},
    {"turn_lease", "storage_busy",
     "another Hermes process took over this session",
     "Wait for it to finish, then send your message again."},
    {"unknown", "storage_unavailable",
     "the session database could not be opened",
     SU_DOCTOR},
};
static const size_t SU_N = sizeof(SU_FAILURES) / sizeof(SU_FAILURES[0]);

static int su_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/*
 * PoP: storageuser_failure_details @ storage_user_copy.py:storage_failure_details
 */
char *storageuser_failure_details(const char *text, long long limit) {
    const char *s = text ? text : "";
    size_t n = strlen(s), i, o = 0;
    char *norm = (char *)malloc(n + 1);
    char *out;
    if (!norm) return NULL;
    /* " ".join(str(text or "").split()) */
    for (i = 0; i < n; ) {
        if (su_space((unsigned char)s[i])) { i++; continue; }
        if (o) norm[o++] = ' ';
        while (i < n && !su_space((unsigned char)s[i])) norm[o++] = s[i++];
    }
    norm[o] = '\0';
    if ((long long)o <= limit) return norm;
    if (limit < 3) { free(norm); return strdup("..."); }
    {
        size_t keep = (size_t)(limit - 3);
        if (keep > o) keep = o;
        while (keep > 0 && su_space((unsigned char)norm[keep - 1])) keep--;
        out = (char *)malloc(keep + 4);
        if (!out) { free(norm); return NULL; }
        memcpy(out, norm, keep);
        memcpy(out + keep, "...", 4);
        free(norm);
        return out;
    }
}

/* Replace every "{profile_arg}" occurrence in action with profile_arg. */
static char *su_expand(const char *action, const char *profile) {
    const char *needle = "{profile_arg}";
    size_t nl = strlen(needle), pl = strlen(profile ? profile : "");
    size_t cnt = 0, i, o = 0, alen = strlen(action);
    char *out;
    const char *p = action;
    while ((p = strstr(p, needle)) != NULL) { cnt++; p += nl; }
    if (!cnt) return strdup(action);
    out = (char *)malloc(alen - cnt * nl + cnt * pl + 1);
    if (!out) return NULL;
    p = action;
    while (*p) {
        if (!strncmp(p, needle, nl)) {
            memcpy(out + o, profile ? profile : "", pl);
            o += pl;
            p += nl;
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
    (void)i;
    return out;
}

/*
 * PoP: storageuser_describe_ex @ storage_user_copy.py:describe_storage_failure
 */
json_t *storageuser_describe_ex(const char *cause, int disk_full, const char *profile_arg) {
    const char *c = cause ? cause : "";
    const char *key = c;
    const SuFailure *f = NULL, *fallback = &SU_FAILURES[SU_N - 1];
    size_t i;
    json_t *out = json_object();
    char *action;
    if (!strcmp(c, "disk") && disk_full) key = "disk_full";
    for (i = 0; i < SU_N; i++) {
        if (!strcmp(SU_FAILURES[i].cause, key)) { f = &SU_FAILURES[i]; break; }
    }
    if (!f) f = fallback;
    action = su_expand(f->action, profile_arg);
    json_set(out, "cause", json_string(c));
    json_set(out, "code", json_string(f->code));
    json_set(out, "gloss", json_string(f->gloss));
    json_set(out, "action", json_string(action ? action : ""));
    free(action);
    return out;
}

/*
 * PoP: storageuser_recovery_docs_url @ storage_user_copy.py:STORAGE_RECOVERY_DOCS_URL
 */
const char *storageuser_recovery_docs_url(void) { return SU_DOCS_URL; }
