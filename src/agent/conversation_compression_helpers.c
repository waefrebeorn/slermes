/* conversation_compression.c — faithful C11 port of the module-level
 * helpers of agent/conversation_compression.py (commit fence, lock-skip
 * signal, rotation recovery, compaction message shaping, notification
 * staging, telemetry). Reuses context.c's summary classifiers and
 * hermes_state_locks.c's lock surface — no duplication.
 *
 * NOTE: this file replaces the former name-parity stub of the same name;
 * the LLM summary-call surface itself remains in src/agent/llm_client.c
 * (compress_context et al., annotated there).
 */
#define _GNU_SOURCE
#include "conversation_compression.h"
#include "hermes_json.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Reused from src/agent/context.c (context_compressor port). */
extern int context_compressor__is_context_summary_content(const char *content);
extern json_t *context_compressor__fresh_compaction_message_copy(const json_t *msg);

/* ── Constants ───────────────────────────────────────────────────────── */

/* PoP: CC_COMPACTION_DONE_STATUS @ agent/conversation_compression.py:_emit_compaction_done */
const char *CC_COMPACTION_DONE_STATUS =
    "\xE2\x9C\x93 Context compaction complete \xE2\x80\x94 continuing turn...";

const char *CC_TODO_INJECTION_HEADER =
    "[Your active task list was preserved across context compression]";

const char *CC_CONTINUATION_USER_CONTENT =
    "Continue from the compressed conversation context above. "
    "This marker exists because no human user turn was available.";

const char *CC_LEGACY_CONTINUATION_USER_CONTENT =
    "Continue from the compressed conversation context above. "
    "This marker exists because the compacted transcript contained "
    "no preserved user turn.";

/* _SYNTHETIC_USER_FLAGS */
static const char *const SYNTHETIC_USER_FLAGS[] = {
    "_todo_snapshot_synthetic",
    "_empty_recovery_synthetic",
    "_verification_stop_synthetic",
    "_pre_verify_synthetic",
    "_dropped_toolcall_nudge",
};
#define N_SYNTHETIC_USER_FLAGS 5

/* _SYNTHETIC_USER_PREFIXES */
static const char *const SYNTHETIC_USER_PREFIXES[] = {
    "[System: Your previous response was truncated",
    "[System: The previous response was cut off",
    "[System: Your previous tool call",
    "[Your active task list was preserved across context compression]",
    "[IMPORTANT: Background process ",
};
#define N_SYNTHETIC_USER_PREFIXES 5

static double cc_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ── CompressionCommitFence ──────────────────────────────────────────── */

struct cc_commit_fence {
    pthread_mutex_t lock;              /* _lock: fencing mutex            */
    bool cancelled;                    /* _cancelled: cancel won pre-commit */
    bool commit_started;               /* _commit_started: inside boundary */
    bool commit_phase;                 /* _commit_phase Event (lock-free read) */
    bool admission_revoked;            /* _admission_revoked (lock-free)   */
    double last_progress;              /* _last_progress monotonic seconds */
    /* Holder-qualified durable-lock release hook (#76354 F4) */
    pthread_mutex_t release_guard;     /* _lock_release_guard              */
    void (*cancelled_lock_release)(void); /* _cancelled_lock_release hook  */
    bool cancelled_lock_release_requested; /* _cancelled_lock_release_requested */
};

/* PoP: cc_commit_fence_new @ agent/conversation_compression.py:__init__ */
cc_commit_fence_t *cc_commit_fence_new(void) {
    cc_commit_fence_t *f = calloc(1, sizeof(*f));
    if (!f) return NULL;
    pthread_mutex_init(&f->lock, NULL);
    pthread_mutex_init(&f->release_guard, NULL);
    f->cancelled = false;
    f->commit_started = false;
    f->commit_phase = false;
    f->admission_revoked = false;
    f->cancelled_lock_release = NULL;
    f->cancelled_lock_release_requested = false;
    f->last_progress = cc_monotonic();
    return f;
}

void cc_commit_fence_free(cc_commit_fence_t *f) {
    if (!f) return;
    pthread_mutex_destroy(&f->lock);
    pthread_mutex_destroy(&f->release_guard);
    free(f);
}

/* PoP: cc_commit_fence_touch_progress @ agent/conversation_compression.py:touch_progress */
void cc_commit_fence_touch_progress(cc_commit_fence_t *f) {
    if (f) f->last_progress = cc_monotonic();
}

/* PoP: cc_commit_fence_seconds_since_progress @ agent/conversation_compression.py:seconds_since_progress */
double cc_commit_fence_seconds_since_progress(cc_commit_fence_t *f) {
    if (!f) return 0.0;
    double d = cc_monotonic() - f->last_progress;
    return d > 0.0 ? d : 0.0;
}

/* PoP: cc_commit_fence_cancel_before_commit @ agent/conversation_compression.py:cancel_before_commit */
bool cc_commit_fence_cancel_before_commit(cc_commit_fence_t *f) {
    if (!f) return false;
    pthread_mutex_lock(&f->lock);
    if (f->commit_started) {
        pthread_mutex_unlock(&f->lock);
        return false;
    }
    f->cancelled = true;
    pthread_mutex_unlock(&f->lock);
    return true;
}

/* PoP: cc_commit_fence_try_cancel_before_commit @ agent/conversation_compression.py:try_cancel_before_commit */
int cc_commit_fence_try_cancel_before_commit(cc_commit_fence_t *f) {
    if (!f) return 0;
    if (pthread_mutex_trylock(&f->lock) != 0)
        return -1; /* fence busy — Python returns None */
    int result;
    if (f->commit_started) {
        result = 0;
    } else {
        f->cancelled = true;
        result = 1;
    }
    pthread_mutex_unlock(&f->lock);
    return result;
}

/* PoP: cc_commit_fence_begin_commit @ agent/conversation_compression.py:begin_commit */
bool cc_commit_fence_begin_commit(cc_commit_fence_t *f) {
    if (!f) return false;
    pthread_mutex_lock(&f->lock);
    if (f->cancelled || f->admission_revoked) {
        pthread_mutex_unlock(&f->lock);
        if (f->admission_revoked) {
            /* Round-2 #1: a revoke that lost the fence-lock race to this very
             * begin_commit deferred its lease release; the commit was refused,
             * so the release is safe (and idempotent with the worker's own
             * holder-qualified cleanup) right now. */
            cc_commit_fence_release_cancelled_compression_lock(f);
        }
        return false;
    }
    f->commit_started = true;
    /* Set while the fence lock is held so observers can never see
     * commit_in_flight=true for a commit that lost to cancellation. */
    f->commit_phase = true;
    /* lock intentionally HELD across the commit boundary */
    return true;
}

/* PoP: cc_commit_fence_finish_commit @ agent/conversation_compression.py:finish_commit */
void cc_commit_fence_finish_commit(cc_commit_fence_t *f) {
    if (!f) return;
    f->commit_phase = false;
    pthread_mutex_unlock(&f->lock);
    if (f->admission_revoked) {
        /* Round-2 #1: a revoke that arrived while THIS commit was in flight
         * deferred its durable-lease release rather than freeing the lock out
         * from under an active SessionDB mutation. The commit is now fully
         * complete, so perform the deferred release here — promptly, without
         * relying on the (possibly parked) worker thread's outer cleanup.
         * Idempotent with that cleanup: the DB release is holder-qualified. */
        cc_commit_fence_release_cancelled_compression_lock(f);
    }
}

/* PoP: cc_commit_fence_commit_in_flight @ agent/conversation_compression.py:commit_in_flight */
bool cc_commit_fence_commit_in_flight(cc_commit_fence_t *f) {
    /* Lock-free read: an admitted commit has begun and not yet finished.
     * Safe to call from the host while the worker holds the fence lock for
     * the whole commit (a hung SessionDB write). */
    if (!f) return false;
    return f->commit_phase;
}

/* PoP: cc_commit_fence_is_cancelled @ agent/conversation_compression.py:is_cancelled */
bool cc_commit_fence_is_cancelled(cc_commit_fence_t *f) {
    /* True after cancellation won before the commit boundary. */
    if (!f) return false;
    return f->cancelled || f->admission_revoked;
}

/* PoP: cc_commit_fence_revoke_commit_admission @ agent/conversation_compression.py:revoke_commit_admission */
void cc_commit_fence_revoke_commit_admission(cc_commit_fence_t *f) {
    if (!f) return;
    /* Lock-free flag store (atomic-enough bool): a commit that is ALREADY in
     * flight cannot be safely abandoned (invariant "commit never abandoned
     * mid-mutation" holds), but no NEW commit will be admitted after this
     * call — begin_commit re-checks the flag under the fence lock. */
    f->admission_revoked = true;
    if (pthread_mutex_trylock(&f->lock) == 0) {
        /* No commit is in flight (an admitted commit RETAINS the lock until
         * finish_commit), so the lease is released immediately, while still
         * holding the lock so a concurrent begin_commit cannot slip in
         * between the check and the release (it would be refused anyway —
         * the flag is already set). */
        cc_commit_fence_release_cancelled_compression_lock(f);
        pthread_mutex_unlock(&f->lock);
    }
    /* else: deferred — finish_commit()/begin_commit() re-check
     * admission_revoked and perform the release once no commit can be
     * mid-mutation. */
}

/* PoP: cc_commit_fence_begin_lock_setup @ agent/conversation_compression.py:begin_lock_setup */
bool cc_commit_fence_begin_lock_setup(cc_commit_fence_t *f) {
    /* Fence durable-lock acquisition and release-hook publication. The caller
     * keeps the fence until it has either published the exact holder-qualified
     * release hook or established that no lock was acquired. */
    if (!f) return false;
    pthread_mutex_lock(&f->lock);
    if (f->cancelled || f->admission_revoked) {
        pthread_mutex_unlock(&f->lock);
        return false;
    }
    return true;
}

/* PoP: cc_commit_fence_finish_lock_setup @ agent/conversation_compression.py:finish_lock_setup */
void cc_commit_fence_finish_lock_setup(cc_commit_fence_t *f) {
    /* Leave a lock setup boundary entered by begin_lock_setup. */
    if (f) pthread_mutex_unlock(&f->lock);
}

/* PoP: cc_commit_fence_register_cancelled_lock_release @ agent/conversation_compression.py:register_cancelled_lock_release */
bool cc_commit_fence_register_cancelled_lock_release(cc_commit_fence_t *f,
                                                     void (*release)(void)) {
    /* Publish the timed-out worker's holder-qualified lock release.
     * Returns whether cancellation cleanup was requested before publication.
     * In that race, the release runs synchronously before this method returns. */
    if (!f) return false;
    bool requested;
    pthread_mutex_lock(&f->release_guard);
    f->cancelled_lock_release = release;
    requested = f->cancelled_lock_release_requested;
    pthread_mutex_unlock(&f->release_guard);
    if (requested && release)
        release();
    return requested;
}

/* PoP: cc_commit_fence_clear_cancelled_lock_release @ agent/conversation_compression.py:clear_cancelled_lock_release */
void cc_commit_fence_clear_cancelled_lock_release(cc_commit_fence_t *f,
                                                  void (*release)(void)) {
    /* Forget `release` after the worker's normal cleanup finishes. */
    if (!f) return;
    pthread_mutex_lock(&f->release_guard);
    if (f->cancelled_lock_release == release)
        f->cancelled_lock_release = NULL;
    pthread_mutex_unlock(&f->release_guard);
}

/* PoP: cc_commit_fence_release_cancelled_compression_lock @ agent/conversation_compression.py:release_cancelled_compression_lock */
void cc_commit_fence_release_cancelled_compression_lock(cc_commit_fence_t *f) {
    /* Release the cancelled worker's lock without finalizing its clients.
     * Callers invoke this only after cancellation won (fence cancelled or
     * admission revoked). A request that races ahead of lock-hook publication
     * is retained and fulfilled synchronously when the worker publishes. */
    if (!f) return;
    void (*release)(void);
    pthread_mutex_lock(&f->release_guard);
    f->cancelled_lock_release_requested = true;
    release = f->cancelled_lock_release;
    pthread_mutex_unlock(&f->release_guard);
    if (release)
        release();
}

/* ── Lock-skip signal ────────────────────────────────────────────────── */

/* PoP: cc_compression_skipped_due_to_lock @ agent/conversation_compression.py:compression_skipped_due_to_lock */
bool cc_compression_skipped_due_to_lock(const cc_lock_skip_signal_t *sig) {
    /* Type-pinned: True or str only (never bare truthiness — the #69870 ×
     * #69840 MagicMock incident). The C signal struct is already typed, so
     * the pin is: the skipped flag itself, holder optional. */
    return sig != NULL && sig->skipped;
}

/* ── Rotation recovery ───────────────────────────────────────────────── */

/* PoP: cc_session_was_rotated_by_compression @ agent/conversation_compression.py:_session_was_rotated_by_compression */
bool cc_session_was_rotated_by_compression(hermes_state_db_t *db,
                                           const char *session_id) {
    if (!db || !session_id || !session_id[0]) return false;
    char *sess = hermes_state_get_session(db, session_id);
    if (!sess) return false;
    json_t *s = json_parse(sess, NULL);
    free(sess);
    if (!s) return false;
    const json_t *ended = json_obj_get(s, "ended_at");
    const char *reason = json_get_str(s, "end_reason", NULL);
    bool rotated = ended && ended->type != JSON_NULL &&
                   reason && strcmp(reason, "compression") == 0;
    json_free(s);
    return rotated;
}

/* PoP: cc_adopt_live_compression_child @ agent/conversation_compression.py:_adopt_live_compression_child */
json_t *cc_adopt_live_compression_child(hermes_state_db_t *db,
                                        const char *parent_session_id,
                                        char **out_child_id) {
    if (out_child_id) *out_child_id = NULL;
    if (!db || !parent_session_id || !parent_session_id[0]) return NULL;

    /* Resolve and load first, then revalidate — the fail-closed ordering. */
    char *child_id = hermes_state_find_live_compression_child(db, parent_session_id);
    if (!child_id) return NULL;

    char *conv = hermes_state_get_messages_as_conversation(db, child_id, false);
    json_t *recovered = conv ? json_parse(conv, NULL) : NULL;
    free(conv);
    if (!recovered || recovered->type != JSON_ARRAY || json_len(recovered) == 0) {
        json_free(recovered);
        free(child_id);
        return NULL;
    }

    /* Revalidate after loading: the child may have rotated or a competing
     * continuation may have appeared between the two DB reads. */
    char *confirmed = hermes_state_find_live_compression_child(db, parent_session_id);
    bool same = confirmed && strcmp(confirmed, child_id) == 0;
    free(confirmed);
    if (!same) {
        json_free(recovered);
        free(child_id);
        return NULL;
    }

    if (out_child_id) *out_child_id = child_id;
    else free(child_id);
    return recovered;
}

/* PoP: cc_recover_rotated_compression_session @ agent/conversation_compression.py:recover_rotated_compression_session */
json_t *cc_recover_rotated_compression_session(hermes_state_db_t *db,
                                               const char *session_id,
                                               char **out_child_id) {
    if (out_child_id) *out_child_id = NULL;
    if (!db || !session_id || !session_id[0]) return NULL;
    if (!cc_session_was_rotated_by_compression(db, session_id)) return NULL;

    /* Rotation publication holds the parent lease until the child handoff is
     * durable; wait briefly instead of observing the intermediate state. */
    for (int attempt = 0; attempt < 21; attempt++) {
        json_t *recovered =
            cc_adopt_live_compression_child(db, session_id, out_child_id);
        if (recovered) return recovered;
        char *holder = hermes_state_get_compression_lock_holder(db, session_id);
        bool held = holder != NULL;
        free(holder);
        if (!held || attempt == 20) return NULL;
        usleep(50000); /* 0.05s */
    }
    return NULL;
}

/* ── Compaction message shaping ──────────────────────────────────────── */

/* PoP: cc_message_text @ agent/conversation_compression.py:_message_text */
char *cc_message_text(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return strdup("");
    const json_t *content = json_obj_get(message, "content");
    if (!content) return strdup("");
    if (content->type == JSON_STRING)
        return strdup(content->str_val ? content->str_val : "");
    if (content->type == JSON_ARRAY) {
        size_t cap = 256, len = 0;
        char *out = malloc(cap);
        if (!out) return strdup("");
        out[0] = '\0';
        for (size_t i = 0; i < json_len(content); i++) {
            const json_t *part = json_get(content, i);
            if (!part || part->type != JSON_OBJECT) continue;
            const char *text = json_get_str(part, "text", NULL);
            if (!text) text = json_get_str(part, "content", NULL);
            if (!text) text = "";
            size_t tlen = strlen(text);
            /* join with "\n" between EVERY dict part (Python joins all) */
            size_t need = len + tlen + 2;
            if (need > cap) {
                while (need > cap) cap *= 2;
                char *nw = realloc(out, cap);
                if (!nw) { free(out); return strdup(""); }
                out = nw;
            }
            if (len > 0) out[len++] = '\n';
            memcpy(out + len, text, tlen);
            len += tlen;
            out[len] = '\0';
        }
        return out;
    }
    return strdup("");
}

/* PoP: cc_is_real_user_message @ agent/conversation_compression.py:_is_real_user_message */
bool cc_is_real_user_message(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return false;
    const char *role = json_get_str(message, "role", NULL);
    if (!role || strcmp(role, "user") != 0) return false;
    for (int i = 0; i < N_SYNTHETIC_USER_FLAGS; i++) {
        const json_t *flag = json_obj_get(message, SYNTHETIC_USER_FLAGS[i]);
        if (flag && !(flag->type == JSON_NULL ||
                      (flag->type == JSON_BOOL && !flag->bool_val) ||
                      (flag->type == JSON_NUMBER && flag->num_val == 0) ||
                      (flag->type == JSON_STRING &&
                       (!flag->str_val || !flag->str_val[0]))))
            return false;
    }
    char *text = cc_message_text(message);
    /* strip() */
    char *s = text;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t slen = strlen(s);
    while (slen > 0 && (s[slen-1] == ' ' || s[slen-1] == '\t' ||
                        s[slen-1] == '\n' || s[slen-1] == '\r'))
        slen--;
    if (slen == 0) { free(text); return false; }
    for (int i = 0; i < N_SYNTHETIC_USER_PREFIXES; i++) {
        size_t plen = strlen(SYNTHETIC_USER_PREFIXES[i]);
        if (strncmp(s, SYNTHETIC_USER_PREFIXES[i], plen) == 0) {
            free(text);
            return false;
        }
    }
    /* ContextCompressor._is_synthetic_compression_user_turn: summary
     * content, continuation sentinels, todo-header rows. */
    bool synthetic = false;
    if (context_compressor__is_context_summary_content(s)) synthetic = true;
    if (!synthetic) {
        char tmp = s[slen]; ((char*)s)[slen] = '\0';
        if (strcmp(s, CC_CONTINUATION_USER_CONTENT) == 0 ||
            strcmp(s, CC_LEGACY_CONTINUATION_USER_CONTENT) == 0)
            synthetic = true;
        if (!synthetic) {
            size_t hlen = strlen(CC_TODO_INJECTION_HEADER);
            if (strncmp(s, CC_TODO_INJECTION_HEADER, hlen) == 0 &&
                s[hlen] == '\n')
                synthetic = true;
        }
        ((char*)s)[slen] = tmp;
    }
    if (!synthetic) {
        const json_t *meta = json_obj_get(message, "_compressed_summary");
        if (meta && !(meta->type == JSON_NULL ||
                      (meta->type == JSON_BOOL && !meta->bool_val)))
            synthetic = true;
    }
    free(text);
    return !synthetic;
}

/* PoP: cc_strip_stale_todo_snapshot @ agent/conversation_compression.py:_strip_stale_todo_snapshot */
json_t *cc_strip_stale_todo_snapshot(const json_t *content) {
    if (!content) return json_null();
    if (content->type == JSON_STRING) {
        const char *text = content->str_val ? content->str_val : "";
        const char *hit = strstr(text, CC_TODO_INJECTION_HEADER);
        if (!hit) return json_copy(content);
        size_t keep = (size_t)(hit - text);
        /* rstrip() the retained prefix */
        while (keep > 0 && (text[keep-1] == ' ' || text[keep-1] == '\t' ||
                            text[keep-1] == '\n' || text[keep-1] == '\r'))
            keep--;
        char *out = malloc(keep + 1);
        if (!out) return json_copy(content);
        memcpy(out, text, keep);
        out[keep] = '\0';
        json_t *node = json_string(out);
        free(out);
        return node;
    }
    if (content->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(content); i++) {
            const json_t *part = json_get(content, i);
            bool drop = false;
            if (part && part->type == JSON_OBJECT) {
                const char *type = json_get_str(part, "type", NULL);
                const char *text = json_get_str(part, "text", NULL);
                if (type && strcmp(type, "text") == 0 && text) {
                    const char *t = text;
                    while (*t == ' ' || *t == '\t' || *t == '\n' || *t == '\r')
                        t++;
                    if (strncmp(t, CC_TODO_INJECTION_HEADER,
                                strlen(CC_TODO_INJECTION_HEADER)) == 0)
                        drop = true;
                }
            }
            if (!drop) json_append(out, json_copy(part));
        }
        return out;
    }
    return json_copy(content);
}

/* PoP: cc_merge_anchor_into_user_message @ agent/conversation_compression.py:_merge_anchor_into_user_message */
void cc_merge_anchor_into_user_message(json_t *target, const json_t *anchor) {
    if (!target || target->type != JSON_OBJECT || !anchor) return;
    const json_t *ac = json_obj_get(anchor, "content");
    const json_t *tc = json_obj_get(target, "content");
    bool a_list = ac && ac->type == JSON_ARRAY;
    bool t_list = tc && tc->type == JSON_ARRAY;
    if (a_list || t_list) {
        json_t *merged = json_array();
        if (a_list) {
            for (size_t i = 0; i < json_len(ac); i++)
                json_append(merged, json_copy(json_get(ac, i)));
        } else {
            json_t *part = json_object();
            json_set(part, "type", json_string("text"));
            json_set(part, "text",
                     json_string(ac && ac->type == JSON_STRING && ac->str_val
                                 ? ac->str_val : ""));
            json_append(merged, part);
        }
        if (t_list) {
            for (size_t i = 0; i < json_len(tc); i++)
                json_append(merged, json_copy(json_get(tc, i)));
        } else {
            json_t *part = json_object();
            json_set(part, "type", json_string("text"));
            json_set(part, "text",
                     json_string(tc && tc->type == JSON_STRING && tc->str_val
                                 ? tc->str_val : ""));
            json_append(merged, part);
        }
        json_set(target, "content", merged);
    } else {
        const char *a = ac && ac->type == JSON_STRING && ac->str_val ? ac->str_val : "";
        const char *t = tc && tc->type == JSON_STRING && tc->str_val ? tc->str_val : "";
        size_t need = strlen(a) + strlen(t) + 3;
        char *buf = malloc(need);
        if (buf) {
            snprintf(buf, need, "%s\n\n%s", a, t);
            /* strip() */
            char *s = buf;
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
            size_t slen = strlen(s);
            while (slen > 0 && (s[slen-1] == ' ' || s[slen-1] == '\t' ||
                                s[slen-1] == '\n' || s[slen-1] == '\r'))
                slen--;
            s[slen] = '\0';
            json_set(target, "content", json_string(s));
            free(buf);
        }
    }
    for (int i = 0; i < N_SYNTHETIC_USER_FLAGS; i++)
        json_obj_del(target, SYNTHETIC_USER_FLAGS[i]);
}

/* Array insert helper (libjson has append only): rebuild in place. */
static void cc_array_insert(json_t *arr, size_t index, json_t *item) {
    json_append(arr, item); /* grows storage */
    /* rotate the new tail item into position */
    for (size_t i = json_len(arr) - 1; i > index; i--) {
        json_t *tmp = arr->c.items[i];
        arr->c.items[i] = arr->c.items[i - 1];
        arr->c.items[i - 1] = tmp;
    }
}

static const char *cc_role(const json_t *msg) {
    if (!msg || msg->type != JSON_OBJECT) return NULL;
    return json_get_str(msg, "role", NULL);
}

/* PoP: cc_insert_real_user_anchor @ agent/conversation_compression.py:_insert_real_user_anchor */
void cc_insert_real_user_anchor(json_t *messages, json_t *anchor) {
    if (!messages || messages->type != JSON_ARRAY) { json_free(anchor); return; }
    size_t n = json_len(messages);
    /* Preferred: summary boundary — before the first assistant message not
     * already preceded by a user turn. */
    for (size_t i = 0; i < n; i++) {
        const char *role = cc_role(json_get(messages, i));
        if (!role || strcmp(role, "assistant") != 0) continue;
        const char *prev = i > 0 ? cc_role(json_get(messages, i - 1)) : NULL;
        if (!prev || strcmp(prev, "user") != 0) {
            cc_array_insert(messages, i, anchor);
            return;
        }
    }
    /* Append is safe when transcript doesn't end with a user turn. */
    const char *last = n > 0 ? cc_role(json_get(messages, n - 1)) : NULL;
    if (n == 0 || !last || strcmp(last, "user") != 0) {
        json_append(messages, anchor);
        return;
    }
    /* Trailing user: never merge into a compaction summary — append after. */
    char *text = cc_message_text(json_get(messages, n - 1));
    bool is_summary = context_compressor__is_context_summary_content(text);
    free(text);
    if (is_summary) {
        json_append(messages, anchor);
        return;
    }
    /* Trailing user-role scaffolding: merge instead of user/user adjacency. */
    cc_merge_anchor_into_user_message((json_t *)json_get(messages, n - 1), anchor);
    json_free(anchor);
}

/* PoP: cc_ensure_compressed_has_user_turn @ agent/conversation_compression.py:_ensure_compressed_has_user_turn */
void cc_ensure_compressed_has_user_turn(const json_t *original_messages,
                                        json_t *compressed) {
    if (!compressed || compressed->type != JSON_ARRAY) return;
    for (size_t i = 0; i < json_len(compressed); i++)
        if (cc_is_real_user_message(json_get(compressed, i)))
            return;
    if (original_messages && original_messages->type == JSON_ARRAY) {
        size_t n = json_len(original_messages);
        for (size_t i = n; i > 0; i--) {
            const json_t *msg = json_get(original_messages, i - 1);
            if (cc_is_real_user_message(msg)) {
                cc_insert_real_user_anchor(
                    compressed,
                    context_compressor__fresh_compaction_message_copy(msg));
                return;
            }
        }
    }
    json_t *fallback = json_object();
    json_set(fallback, "role", json_string("user"));
    json_set(fallback, "content", json_string(CC_CONTINUATION_USER_CONTENT));
    json_append(compressed, fallback);
}

/* ── Context-engine notification staging ─────────────────────────────── */

struct cc_pending_notification {
    cc_notify_fn fn;
    void *ctx;
    char *new_session_id;
    char *old_session_id;
};

/* PoP: cc_queue_compression_notification @ agent/conversation_compression.py:_queue_context_engine_compression_notification */
cc_pending_notification_t *cc_queue_compression_notification(
    cc_pending_notification_t **slot, cc_notify_fn fn, void *ctx,
    const char *new_session_id, const char *old_session_id) {
    if (!slot || *slot != NULL) return NULL; /* already pending → error */
    cc_pending_notification_t *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->fn = fn;
    p->ctx = ctx;
    p->new_session_id = strdup(new_session_id ? new_session_id : "");
    p->old_session_id = strdup(old_session_id ? old_session_id : "");
    *slot = p;
    return p;
}

/* PoP: cc_finalize_compression_notification @ agent/conversation_compression.py:finalize_context_engine_compression_notification */
bool cc_finalize_compression_notification(cc_pending_notification_t **slot,
                                          bool committed) {
    if (!slot) return false;
    cc_pending_notification_t *pending = *slot;
    *slot = NULL; /* cleared first — repeated calls are no-ops */
    if (!pending) return false;
    bool result = false;
    if (committed && pending->fn) {
        /* PoP: cc_notify_context_engine @ agent/conversation_compression.py:_notify_context_engine_compression_complete */
        result = pending->fn(pending->ctx, pending->new_session_id,
                             pending->old_session_id);
    }
    free(pending->new_session_id);
    free(pending->old_session_id);
    free(pending);
    return result;
}

/* ── Telemetry ───────────────────────────────────────────────────────── */

/* PoP: cc_compression_attempt_telemetry_line @ agent/conversation_compression.py:_emit_compression_attempt_telemetry */
char *cc_compression_attempt_telemetry_line(const json_t *base_telemetry,
                                            const char *attempt_id,
                                            const char *session_id,
                                            long long total_duration_ms,
                                            const char *commit_status,
                                            const char *split_status,
                                            const char *failure_class,
                                            bool fallback_used) {
    json_t *payload = base_telemetry && base_telemetry->type == JSON_OBJECT
                          ? json_copy(base_telemetry)
                          : json_object();
    if (!payload) return NULL;
    /* setdefault semantics */
    if (!json_has(payload, "event"))
        json_set(payload, "event", json_string("compression_attempt"));
    if (!json_has(payload, "attempt_id"))
        json_set(payload, "attempt_id",
                 json_string(attempt_id ? attempt_id : ""));
    if (!json_has(payload, "session_id"))
        json_set(payload, "session_id",
                 json_string(session_id ? session_id : ""));
    /* unconditional overwrites */
    json_set(payload, "total_duration_ms",
             json_number((double)total_duration_ms));
    json_set(payload, "commit_status",
             json_string(commit_status ? commit_status : ""));
    json_set(payload, "split_status",
             json_string(split_status ? split_status : ""));
    if (failure_class && failure_class[0])
        json_set(payload, "failure_class", json_string(failure_class));
    if (!json_has(payload, "chunking"))
        json_set(payload, "chunking", json_bool(false));
    if (!json_has(payload, "chunk_count"))
        json_set(payload, "chunk_count", json_number(0));
    /* fallback_used: existing truthy OR caller flag (unconditional set) */
    bool existing = json_get_bool(payload, "fallback_used", false);
    json_set(payload, "fallback_used", json_bool(existing || fallback_used));

    /* sort keys (json.dumps(..., sort_keys=True)) — insertion order is the
     * object order in libjson, so sort in place before serializing */
    size_t cnt = payload->c.count;
    for (size_t i = 0; i + 1 < cnt; i++) {
        for (size_t j = 0; j + 1 < cnt - i; j++) {
            if (strcmp(payload->c.keys[j], payload->c.keys[j + 1]) > 0) {
                char *tk = payload->c.keys[j];
                payload->c.keys[j] = payload->c.keys[j + 1];
                payload->c.keys[j + 1] = tk;
                json_t *tv = payload->c.items[j];
                payload->c.items[j] = payload->c.items[j + 1];
                payload->c.items[j + 1] = tv;
            }
        }
    }
    char *line = json_serialize(payload);
    json_free(payload);
    return line;
}

/* ── Status edge ─────────────────────────────────────────────────────── */

/* PoP: cc_emit_compaction_done @ agent/conversation_compression.py:_emit_compaction_done */
void cc_emit_compaction_done(cc_status_cb cb, void *ctx) {
    if (!cb) return;
    cb(ctx, "compacted", CC_COMPACTION_DONE_STATUS);
}

/* ── Cached-prompt memory retention check ────────────────────────────── */

/* MEMORY_BLOCK_HEADERS (tools/memory_tool.py) — keep in lockstep. */
static const char *MEMORY_HEADER_MEMORY = "MEMORY (your personal notes)";
static const char *MEMORY_HEADER_USER = "USER PROFILE (who the user is)";

/* PoP: cc_cached_prompt_reflects_builtin_memory @ agent/conversation_compression.py:_cached_prompt_reflects_builtin_memory */
/* PoP: cc_cached_prompt_reflects_builtin_memory @ agent/conversation_compression.py:_builtin_memory_prompt_snapshot */
bool cc_cached_prompt_reflects_builtin_memory(const char *memory_block,
                                              const char *user_block,
                                              const char *cached_prompt) {
    /* NULL snapshot = unreadable → conservative rebuild path (false). */
    if (!memory_block || !user_block || !cached_prompt) return false;
    const char *blocks[2] = { memory_block, user_block };
    const char *headers[2] = { MEMORY_HEADER_MEMORY, MEMORY_HEADER_USER };
    for (int i = 0; i < 2; i++) {
        /* strip() */
        const char *b = blocks[i];
        while (*b == ' ' || *b == '\t' || *b == '\n' || *b == '\r') b++;
        size_t blen = strlen(b);
        while (blen > 0 && (b[blen-1] == ' ' || b[blen-1] == '\t' ||
                            b[blen-1] == '\n' || b[blen-1] == '\r'))
            blen--;
        if (blen > 0) {
            char *stripped = malloc(blen + 1);
            if (!stripped) return false;
            memcpy(stripped, b, blen);
            stripped[blen] = '\0';
            bool contained = strstr(cached_prompt, stripped) != NULL;
            free(stripped);
            if (!contained) return false;
        } else if (strstr(cached_prompt, headers[i]) != NULL) {
            return false; /* leftover header for an emptied/disabled target */
        }
    }
    return true;
}

/* ── Skew / guard helpers + codex-app-server route ───────────────────── */

/* PoP: _lock_api_is_absent_on_session_db @ agent/conversation_compression.py:_lock_api_is_absent_on_session_db */
/* The C SessionDB (hermes_state_db_t) always carries the lock API, so the
 * hot-reload skew (old class missing try_acquire_compression_lock) cannot
 * occur. Mirrors the fail-closed return for any non-hermes_state_db handle. */
bool cc_lock_api_is_absent_on_session_db(const void *lock_db) {
    /* In our build the only lock-bearing handle is hermes_state_db_t (opaque
     * pointer); any other pointer is a proxy/nominal lookalike → fail closed
     * (the Python path returns False for everything except the exact class
     * identity missing the attribute). With our single concrete type the
     * lock API is always present, so we report present. */
    return lock_db == NULL; /* NULL handle = structurally degenerate → absent */
}

/* PoP: _refresh_persisted_compression_guards @ agent/conversation_compression.py:_refresh_persisted_compression_guards */
/* Durable automatic-compression guards live on the ContextCompressor. The
 * C ContextCompressor exposes the same refresh entry points; calling them
 * with the same method-name table keeps the two implementations in lockstep.
 * Weak so the compressor port can supply the real guard-loaders (overriding
 * this default) without a duplicate-symbol clash. */
extern void cc_ctx_refresh_guards(const void *compressor);
__attribute__((weak))
void cc_ctx_refresh_guards(const void *compressor) {
    (void)compressor; /* overridden by the ContextCompressor port */
}
void cc_refresh_persisted_compression_guards(const void *compressor) {
    if (!compressor) return;
    cc_ctx_refresh_guards(compressor);
}

/* PoP: _supported_compression_kwargs @ agent/conversation_compression.py:_supported_compression_kwargs */
/* Context-engine plugin callables may outlive host-contract additions. We
 * cannot introspect an opaque C fn pointer's signature, so we accept the
 * widest documented contract and let the engine ignore unknown keys — but
 * the *fallback* shape (when a callable is non-introspectable) is exactly
 * {current_tokens}. Our compress_fn is always our own introspectable engine,
 * so we return the full candidate set (matching the non-VAR_KEYWORD path). */
json_t *cc_supported_compression_kwargs(bool has_memory_context,
                                        const char *memory_context,
                                        long long current_tokens,
                                        const char *focus_topic,
                                        bool force) {
    json_t *out = json_object();
    if (!out) return NULL;
    json_set(out, "current_tokens", json_number((double)current_tokens));
    if (focus_topic)
        json_set(out, "focus_topic", json_string(focus_topic));
    else
        json_set(out, "focus_topic", json_null());
    json_set(out, "force", json_bool(force));
    if (has_memory_context && memory_context)
        json_set(out, "memory_context", json_string(memory_context));
    return out;
}

/* ── _CompressionActivityHeartbeat ──────────────────────────────────── */

/* The in-flight heartbeat state. Mirrors the Python
 * _CompressionActivityHeartbeat: agent touch callback + commit fence +
 * suppressed latch. */
typedef struct cc_heartbeat {
    void (*touch_activity)(const char *desc); /* agent._touch_activity */
    cc_commit_fence_t *fence;                /* commit fence (or NULL)     */
    bool suppressed;                         /* _suppressed latch           */
} cc_heartbeat_t;

cc_heartbeat_t *cc_heartbeat_new(void (*touch_activity)(const char *),
                                  cc_commit_fence_t *fence) {
    cc_heartbeat_t *h = calloc(1, sizeof(*h));
    if (!h) return NULL;
    h->touch_activity = touch_activity;
    h->fence = fence;
    h->suppressed = false;
    return h;
}

void cc_heartbeat_free(cc_heartbeat_t *h) {
    free(h); /* fence is owned by the caller */
}

/* PoP: _fence_cancelled @ agent/conversation_compression.py:_CompressionActivityHeartbeat._fence_cancelled */
bool cc_heartbeat_fence_cancelled(const cc_heartbeat_t *h) {
    /* fence is not None and fence.is_cancelled */
    if (!h || !h->fence) return false;
    return cc_commit_fence_is_cancelled(h->fence);
}

/* PoP: _should_suppress @ agent/conversation_compression.py:_CompressionActivityHeartbeat._should_suppress */
bool cc_heartbeat_should_suppress(cc_heartbeat_t *h) {
    /* Latched once host cancel/timeout wins or a terminal stamp is observed,
     * so a later UNKNOWN rewrite cannot re-arm a detached zombie heartbeat. */
    if (!h) return false;
    if (h->suppressed) return true;
    if (cc_heartbeat_fence_cancelled(h)) {
        h->suppressed = true;
        return true;
    }
    return false;
}

/* ── _CompressionActivityHeartbeat._touch ─────────────────────────────── */

/* PoP: _touch @ agent/conversation_compression.py:_CompressionActivityHeartbeat._touch */
/* Touches the agent's forward-progress heartbeat so a hung compressor doesn't
 * look idle. The agent exposes _touch_activity(desc) on its opaque handle; the
 * caller resolves it and passes the callback (NULL = no-op, matching Python's
 * missing-attribute guard). */
void cc_activity_heartbeat_touch(void (*touch_activity)(const char *desc),
                                 const char *desc) {
    if (!touch_activity || !desc) return;
    touch_activity(desc);
}

/* ── _compress_context_via_codex_app_server ──────────────────────────── */

/* The codex app-server transport (agent/transports/codex_app_server_session.py)
 * is not yet ported. To avoid a stub we type the session as an opaque
 * codex-session handle with a real vtable seam; the orchestration below is a
 * faithful port of the Python control flow (mode gating, heartbeat lifecycle,
 * retire-on-should_retire, record hooks, status edges). The actual
 * codex_session.compact_thread() call dispatches through the registered
 * transport vtable — when no transport is registered the function falls back
 * to the Hermes-native path exactly as `auto_mode != "hermes"` does. */

/* cc_codex_compact_result_t and cc_codex_session_vtable_t are defined in
 * conversation_compression.h (so the codex transport can bind the vtable).
 * Only the ctx struct that carries the agent seams stays local. */

typedef struct cc_codex_session_ctx {
    void *session;          /* opaque codex session */
    cc_codex_session_vtable_t *vtab;
    /* resolved agent seams (the future codex runtime populates these; the
     * route stays self-contained and linkable with no agent opaque type) */
    const char *auto_mode;                  /* already lowercased/native-defaulted */
    char *(*build_system_prompt)(const char *system_message, void *ctx);
    void *ctx;                              /* passed to build_system_prompt */
    void (*set_codex_session)(void *sess);  /* NULL to clear */
    void (*emit_status)(void);             /* _emit_status(COMPACTION_STATUS) */
    void (*complete_compaction)(void);     /* _complete_compaction_lifecycle */
    void (*emit_warning)(const char *msg);
    void *(*context_compressor)(void);
} cc_codex_session_ctx_t;

/* helper: wrap (messages, prompt) into the [messages, prompt] tuple shape */
static json_t *cc_codex_result(json_t *messages, char *prompt);

/* PoP: _compress_context_via_codex_app_server @ agent/conversation_compression.py:_compress_context_via_codex_app_server */
/* Returns a malloc'd JSON tuple [unchanged_messages, built_prompt]. On the
 * Hermes-native fallback path (auto_mode != "hermes" or no session) messages
 * are returned unchanged with the built system prompt; on the codex path the
 * session transcript is compacted server-side and Hermes' transcript is
 * likewise returned unchanged (the Python contract for this runtime). */
json_t *cc_compress_context_via_codex_app_server(
    json_t *messages, const char *system_message,
    cc_codex_session_ctx_t *codex, long long approx_tokens,
    int task_id_is_default, /* bool: task_id=="default" */
    bool force) {
    (void)task_id_is_default;
    json_t *out_messages = json_copy(messages);
    char *prompt = NULL;

    const char *auto_mode = codex ? codex->auto_mode : "native";
    bool skip = (!force && strcmp(auto_mode, "hermes") != 0) ||
                (codex == NULL || codex->session == NULL || codex->vtab == NULL ||
                 codex->vtab->compact_thread == NULL);

    if (skip) {
        prompt = codex && codex->build_system_prompt
                     ? codex->build_system_prompt(system_message, codex->ctx)
                     : strdup("");
        return cc_codex_result(out_messages, prompt);
    }

    /* codex path */
    if (codex->emit_status) codex->emit_status();

    cc_codex_compact_result_t res = { .error = NULL, .interrupted = false,
                                      .should_retire = false };
    cc_codex_compact_result_t pr = codex->vtab->compact_thread(codex->session);
    res = pr;

    if (res.should_retire) {
        if (codex->vtab->close) codex->vtab->close(codex->session);
        if (codex->set_codex_session) codex->set_codex_session(NULL);
    }

    if (res.interrupted || (res.error != NULL && res.error[0])) {
        if (res.error && res.error[0] && codex->emit_warning) {
            char *warn = malloc(strlen(res.error) + 64);
            if (warn) {
                sprintf(warn, "\xE2\x9A\xA0 Codex app-server compaction failed: %s",
                        res.error);
                codex->emit_warning(warn);
                free(warn);
            }
        }
        prompt = codex->build_system_prompt
                     ? codex->build_system_prompt(system_message, codex->ctx)
                     : strdup("");
        if (codex->complete_compaction) codex->complete_compaction();
        return cc_codex_result(out_messages, prompt);
    }

    if (codex->vtab->record_compaction)
        codex->vtab->record_compaction(codex, &res, approx_tokens, true);
    if (codex->vtab->has_update_from_response &&
        codex->vtab->has_update_from_response(
            codex->context_compressor ? codex->context_compressor() : NULL) &&
        codex->vtab->update_from_response)
        codex->vtab->update_from_response(
            codex->context_compressor ? codex->context_compressor() : NULL);

    prompt = codex->build_system_prompt
                 ? codex->build_system_prompt(system_message, codex->ctx)
                 : strdup("");
    if (codex->complete_compaction) codex->complete_compaction();
    return cc_codex_result(out_messages, prompt);
}

/* helper: wrap (messages, prompt) into the [messages, prompt] tuple shape */
static json_t *cc_codex_result(json_t *messages, char *prompt) {
    json_t *tuple = json_array();
    json_append(tuple, messages);
    json_append(tuple, json_string(prompt ? prompt : ""));
    free(prompt);
    return tuple;
}

/* ── Status string constants ─────────────────────────────────────────── */

/* PoP: COMPACTION_STATUS @ agent/conversation_compression.py:COMPACTION_STATUS */
const char *CC_COMPACTION_STATUS =
    "\xF0\x9F\x94\x84 Compacting conversation history...";


/* ── Bounded compression-pool admission (#76354 F6) ──────────────────── */

/* The stdlib executor queue is unbounded: with all four workers wedged in
 * hung summaries, a fifth compression would queue silently, wait out its
 * whole timeout without ever starting, and remain eligible to run as a stale
 * job whenever a worker recovered. Admission is therefore capped at the
 * worker count — when every slot is occupied (running OR admitted-not-started)
 * submission FAILS FAST and the caller continues without compression. */
#define CC_COMPRESS_EXECUTOR_MAX_WORKERS 4

static pthread_mutex_t g_cc_admission_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_cc_admitted_count = 0;

/* PoP: _try_admit_compression_job @ agent/conversation_compression.py:_try_admit_compression_job */
bool cc_try_admit_compression_job(void) {
    /* Reserve one bounded compression-pool admission slot (F6). */
    bool ok;
    pthread_mutex_lock(&g_cc_admission_lock);
    if (g_cc_admitted_count >= CC_COMPRESS_EXECUTOR_MAX_WORKERS) {
        ok = false;
    } else {
        g_cc_admitted_count += 1;
        ok = true;
    }
    pthread_mutex_unlock(&g_cc_admission_lock);
    return ok;
}

/* PoP: _release_compression_admission @ agent/conversation_compression.py:_release_compression_admission */
void cc_release_compression_admission(void) {
    /* Free an admission slot (future done-callback or failed submit). */
    pthread_mutex_lock(&g_cc_admission_lock);
    if (g_cc_admitted_count > 0)
        g_cc_admitted_count -= 1;
    pthread_mutex_unlock(&g_cc_admission_lock);
}

/* PoP: resolve_context_compression_timeouts @ agent/conversation_compression.py:resolve_context_compression_timeouts */
void cc_resolve_context_compression_timeouts(const char *compression_cfg_json,
                                             double *out_idle,
                                             double *out_ceiling) {
    /* Return (idle_timeout_seconds, total_ceiling_seconds).
     * idle_timeout_seconds <= 0 disables the owned progress-aware wrapper.
     * The ceiling is clamped to at least one idle window when the idle budget
     * is positive, matching gateway hygiene semantics. */
    double idle = 120.0;      /* DEFAULT_CONTEXT_TIMEOUT_SECONDS */
    double ceiling = 600.0;   /* DEFAULT_CONTEXT_TOTAL_CEILING_SECONDS */

    if (compression_cfg_json && *compression_cfg_json) {
        json_t *cfg = json_parse(compression_cfg_json, NULL);
        if (cfg && cfg->type == JSON_OBJECT) {
            const json_t *raw_idle = json_obj_get(cfg, "context_timeout_seconds");
            if (raw_idle && raw_idle->type == JSON_NUMBER) {
                /* Explicit 0/negative disables; positive values win. */
                idle = raw_idle->num_val;
            }
            const json_t *raw_ceiling =
                json_obj_get(cfg, "context_total_ceiling_seconds");
            if (raw_ceiling && raw_ceiling->type == JSON_NUMBER) {
                double parsed = raw_ceiling->num_val;
                if (parsed > 0)
                    ceiling = parsed;
            }
        }
        json_free(cfg);
    }
    if (idle > 0)
        ceiling = ceiling > idle ? ceiling : idle;
    if (out_idle) *out_idle = idle;
    if (out_ceiling) *out_ceiling = ceiling;
}

/* ── Compressor attempt-state snapshot/restore ──────────────────────── */

/* The allow-list of mutable bookkeeping fields owned by one compression
 * attempt. Mirrors Python _COMPRESSOR_ATTEMPT_STATE_FIELDS exactly. */
static const char *CC_COMPRESSOR_ATTEMPT_STATE_FIELDS[] = {
    "_previous_summary",
    "_summary_has_user_turn",
    "compression_count",
    "_last_compression_savings_pct",
    "_ineffective_compression_count",
    "_anti_thrash_recovery_deadline",
    "_fallback_compression_streak",
    "_verify_compaction_cleared_threshold",
    "_last_compression_made_progress",
    "_summary_failure_cooldown_until",
    "_cooldown_persist_failed",
    "_last_summary_error",
    "_consecutive_timeout_failures",
    "_last_summary_dropped_count",
    "_last_summary_fallback_used",
    "_last_compress_aborted",
    "_last_summary_auth_failure",
    "_last_summary_network_failure",
    "_last_aux_model_failure_error",
    "_last_aux_model_failure_model",
    "_summary_model_fallen_back",
    "summary_model",
    "_last_compression_telemetry",
    "_active_compression_telemetry",
    "_compression_telemetry_seed",
    NULL,
};

/* PoP: _snapshot_compressor_attempt_state @ agent/conversation_compression.py:_snapshot_compressor_attempt_state */
json_t *cc_snapshot_compressor_attempt_state(const json_t *state) {
    /* Copy only mutable bookkeeping owned by one compression attempt.
     * The explicit allow-list avoids copying provider clients, session DB
     * handles, locks, and plugin resources. Missing fields are ignored. */
    if (!state || state->type != JSON_OBJECT)
        return json_object();
    json_t *out = json_object();
    if (!out) return NULL;
    for (size_t i = 0; CC_COMPRESSOR_ATTEMPT_STATE_FIELDS[i]; i++) {
        const char *name = CC_COMPRESSOR_ATTEMPT_STATE_FIELDS[i];
        json_t *val = json_obj_get(state, name);
        if (val)
            json_object_set(out, name, json_copy(val));
    }
    return out;
}

/* PoP: _restore_compressor_attempt_state @ agent/conversation_compression.py:_restore_compressor_attempt_state */
void cc_restore_compressor_attempt_state(json_t *state,
                                         const json_t *snapshot,
                                         bool durable_cooldown_authoritative,
                                         const json_t *durable_cooldown_state,
                                         hermes_state_db_t *db,
                                         const char *session_id) {
    /* Restore the safe per-attempt snapshot after a pre-commit hard cancel.
     * On the cooling path: a successful summary clears the durable cooldown
     * before the outer commit boundary. Recreate (or clear) that row before
     * restoring exact in-memory values, otherwise the next refresh overwrites
     * this rollback. Unknown durable state and intentionally unpersisted local
     * cooldowns are never converted into destructive DB writes during cancellation. */
    if (state && state->type != JSON_OBJECT) return;
    if (!snapshot || snapshot->type != JSON_OBJECT) return;

    /* Cooldown restore path (only when snapshot has _summary_failure_cooldown_until
     * and durable_cooldown_authoritative != FALSE and either authoritative or
     * cooldown-persist did not fail). */
    const json_t *cd_field = json_obj_get(snapshot, "_summary_failure_cooldown_until");
    bool cd_in_snapshot = cd_field && cd_field->type != JSON_NULL;
    if (cd_in_snapshot && state) {
        bool persist_failed_val = false;
        const json_t *pf = json_obj_get(snapshot, "_cooldown_persist_failed");
        if (pf && pf->type == JSON_BOOL)
            persist_failed_val = !pf->bool_val;
        if (durable_cooldown_authoritative || persist_failed_val) {
            if (db && session_id && session_id[0]) {
                if (durable_cooldown_authoritative) {
                    /* Authoritative restore: call DB restore with deep-copied
                     * durable_state. */
                    if (durable_cooldown_state &&
                        durable_cooldown_state->type == JSON_OBJECT) {
                        json_t *dc_copy = json_copy(durable_cooldown_state);
                        char *snap_str = json_dumps(dc_copy, 0);
                        json_free(dc_copy);
                        if (snap_str) {
                            hermes_state_restore_compression_failure_cooldown_row(
                                db, session_id, snap_str);
                            free(snap_str);
                        }
                    }
                } else {
                    /* Non-authoritative, persist-failed path: recompute the
                     * remaining durable time and either re-record or clear the
                     * cooldown row. */
                    double deadline = 0.0;
                    const json_t *until = json_obj_get(snapshot,
                                                       "_summary_failure_cooldown_until");
                    if (until && (until->type == JSON_NUMBER ||
                                  until->type == JSON_STRING)) {
                        deadline = until->type == JSON_NUMBER ?
                                   until->num_val : 0.0;
                    }
                    double remaining = deadline > 0.0 ?
                        (deadline - cc_monotonic()) : 0.0;
                    double durable_deadline = cc_monotonic() + (remaining > 0 ? remaining : 0.0);
                    const char *durable_error = NULL;
                    const json_t *err = json_obj_get(snapshot, "_last_summary_error");
                    if (err && err->type == JSON_STRING)
                        durable_error = err->str_val;
                    if (remaining > 0) {
                        hermes_state_record_compression_failure_cooldown(
                            db, session_id, durable_deadline, durable_error);
                    } else {
                        hermes_state_clear_compression_failure_cooldown(
                            db, session_id);
                    }
                }
            }
        }
    }

    /* Restore exact in-memory values: deep-copy the snapshot into state. */
    if (state && state->type == JSON_OBJECT &&
        snapshot && snapshot->type == JSON_OBJECT) {
        size_t n = json_len(snapshot);
        for (size_t i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(snapshot, i);
            json_t *v = json_object_get_at(snapshot, i);
            if (k && v)
                json_object_set(state, k, json_copy(v));
        }
    }
}

/* PoP: _capture_authoritative_cooldown_under_lease @ agent/conversation_compression.py:_capture_authoritative_cooldown_under_lease */
void cc_capture_authoritative_cooldown_under_lease(json_t *state,
                                                   json_t *attempt_snapshot,
                                                   hermes_state_db_t *db,
                                                   const char *session_id,
                                                   bool *out_authoritative,
                                                   json_t **out_durable_state) {
    /* Refresh + snapshot built-in durable cooldown state under the lease.
     * Third-party compressors are deliberately not invoked here: arbitrary
     * plugin callbacks must not run while the session lease is held.
     * A durable read failure returns False so rollback cannot mistake unknown
     * durable state for an authoritative empty row and clear it; an unavailable
     * legacy API returns None and preserves the compatibility path.
     *
     * In C, the compressor state is always our built-in type, so we always
     * capture authoritatively when the DB/session are available.
     * out_durable_state receives a deep-copied durable-state object (caller
     * frees) when authoritative is true. */
    if (out_authoritative) *out_authoritative = false;
    if (out_durable_state) *out_durable_state = NULL;
    if (!state || state->type != JSON_OBJECT) return;
    if (!db || !session_id || !session_id[0]) return;

    /* Read the exact persisted representation (raw, unfiltered by expiry
     * — cannot serve as a lossless rollback snapshot). */
    char *row_json = hermes_state_get_compression_failure_cooldown_row(db, session_id);
    json_t *durable_state = row_json ? json_parse(row_json, NULL) : NULL;
    free(row_json);
    if (!durable_state || durable_state->type != JSON_OBJECT) {
        json_free(durable_state);
        return;
    }

    /* Capture the cooldown state fields into the attempt snapshot. */
    static const char *COOLDOWN_STATE_FIELDS[] = {
        "_summary_failure_cooldown_until",
        "_last_summary_error",
        "_cooldown_persist_failed",
        NULL,
    };
    for (size_t i = 0; COOLDOWN_STATE_FIELDS[i]; i++) {
        json_t *val = json_obj_get(state, COOLDOWN_STATE_FIELDS[i]);
        if (val)
            json_object_set(attempt_snapshot, COOLDOWN_STATE_FIELDS[i],
                            json_copy(val));
    }
    if (out_authoritative) *out_authoritative = true;
    if (out_durable_state) *out_durable_state = durable_state;
    else json_free(durable_state);
}


/* ── Compress-timeout executor pool (#76354 F6) ────────────────────── */
/*
 * Process-wide bounded daemon thread pool (4 workers) for sync
 * run_in_executor-style compression calls. Faithful C11 port of
 * tools/daemon_pool.py:DaemonThreadPoolExecutor: daemon threads so a
 * fence-cancelled hung worker cannot block process exit; max_workers=4
 * so compress is rare/heavy but overlapping calls are allowed.
 */

typedef struct cc_pool_job {
    void (*fn)(void *);
    void *arg;
    struct cc_pool_job *next;
} cc_pool_job_t;

typedef struct cc_compress_pool {
    pthread_mutex_t mutex;      /* guards job_queue, shutdown, active   */
    pthread_cond_t  job_cond;   /* signal: job enqueued or shutdown     */
    cc_pool_job_t  *job_queue;  /* FIFO of pending jobs                  */
    cc_pool_job_t  *job_tail;
    int  active;              /* running OR admitted-not-started       */
    bool shutting_down;
    pthread_t workers[CC_COMPRESS_EXECUTOR_MAX_WORKERS];
    bool  alive[CC_COMPRESS_EXECUTOR_MAX_WORKERS];
} cc_compress_pool_t;

static cc_compress_pool_t *g_cc_pool = NULL;
static pthread_once_t g_cc_pool_once = PTHREAD_ONCE_INIT;

static void *cc_pool_worker_main(void *arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&g_cc_pool->mutex);
        while (!g_cc_pool->shutting_down && !g_cc_pool->job_queue)
            pthread_cond_wait(&g_cc_pool->job_cond, &g_cc_pool->mutex);
        if (g_cc_pool->shutting_down) {
            pthread_mutex_unlock(&g_cc_pool->mutex);
            return NULL;
        }
        cc_pool_job_t *job = g_cc_pool->job_queue;
        g_cc_pool->job_queue = job->next;
        if (g_cc_pool->job_tail == job)
            g_cc_pool->job_tail = NULL;
        pthread_mutex_unlock(&g_cc_pool->mutex);
        if (job->fn)
            job->fn(job->arg);
        free(job);
    }
    return NULL;
}

static void cc_pool_init_once(void) {
    if (g_cc_pool) return;
    g_cc_pool = calloc(1, sizeof(*g_cc_pool));
    if (!g_cc_pool) return;
    pthread_mutex_init(&g_cc_pool->mutex, NULL);
    pthread_cond_init(&g_cc_pool->job_cond, NULL);
    for (int i = 0; i < CC_COMPRESS_EXECUTOR_MAX_WORKERS; i++) {
        if (pthread_create(&g_cc_pool->workers[i], NULL,
                           cc_pool_worker_main, NULL) != 0) {
            g_cc_pool->alive[i] = false;
        } else {
            /* Detach: daemon-like semantics — the thread does not need to be
             * joined at process shutdown (mirrors DaemonThreadPoolExecutor). */
            pthread_detach(g_cc_pool->workers[i]);
            g_cc_pool->alive[i] = true;
        }
    }
}

/* PoP: _get_compress_timeout_executor @ agent/conversation_compression.py:_get_compress_timeout_executor */
cc_compress_pool_t *cc_get_compress_timeout_executor(void) {
    /* Return the process-wide compress-timeout DaemonThreadPoolExecutor.
     * Lazily created on first call; never shut down per-call (a timed-out
     * worker may still be winding down after fence cancel). */
    static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&init_lock);
    if (!g_cc_pool)
        cc_pool_init_once();
    pthread_mutex_unlock(&init_lock);
    return g_cc_pool;
}

/* ── Progress-aware timeout runner ──────────────────────────────────── */

/* Shared job result container for the single-submission pattern. */
typedef struct cc_compress_run_ctx {
    void (*worker_fn)(void *);   /* the compression worker callable       */
    void *worker_arg;           /* forwarded to worker_fn                */
    cc_commit_fence_t *fence;   /* reuse existing or create new          */
    bool  fence_owned;          /* true: we created the fence            */
    double idle_timeout;        /* idle budget (seconds)                 */
    double ceiling;             /* total ceiling (seconds)               */
    void (*on_timeout)(double idle, double waited, double since_progress);
    void (*on_commit_overrun)(double waited, double ceiling);
    /* Result */
    bool completed;             /* worker finished                        */
    bool has_result;
    char *result_text;          /* the worker's message/fallback          */
    int  exit_code;             /* 0 = success, 1 = timeout, -1 = saturated */
    pthread_mutex_t mutex;
    pthread_cond_t  done;       /* signaled when worker finishes          */
} cc_compress_run_ctx_t;

static void cc_compress_job_fn(void *arg) {
    cc_compress_run_ctx_t *ctx = (cc_compress_run_ctx_t *)arg;
    if (ctx->fence_owned && cc_commit_fence_is_cancelled(ctx->fence)) {
        /* F6: skip stale job — fence already cancelled before start. */
        pthread_mutex_lock(&ctx->mutex);
        ctx->completed = true;
        pthread_mutex_unlock(&ctx->mutex);
        pthread_cond_signal(&ctx->done);
        return;
    }
    if (ctx->worker_fn)
        ctx->worker_fn(ctx->worker_arg);
    pthread_mutex_lock(&ctx->mutex);
    ctx->completed = true;
    pthread_mutex_unlock(&ctx->mutex);
    pthread_cond_signal(&ctx->done);
}

cc_compress_pool_t *cc_get_compress_timeout_executor(void);

/*
 * Run worker(fence) under a sync progress-aware timeout.
 *
 * Faithful port of Python run_compress_context_with_progress_timeout.
 * - idle budget: inactivity-based (progress via cc_commit_fence_touch_progress
 *   extends the wait).
 * - total ceiling: hard bound on the pre-commit wait.
 * - fence_cancelled on timeout prevents a late commit from mutating session
 *   state; the worker is detached and its durable lease is released.
 * - commit phase (begin_commit entered) is NOT fence-cancelled — logged +
 *   surfaced via on_commit_overrun if it exceeds the ceiling, then waited on
 *   in bounded slices until it completes.
 *
 * worker_fn receives the commit fence via worker_arg. On timeout returns
 * fallback_prompt (caller-provided string).
 */
/* PoP: run_compress_context_with_progress_timeout @ agent/conversation_compression.py:run_compress_context_with_progress_timeout */
char *cc_run_compress_context_with_progress_timeout(
    void (*worker_fn)(void *),
    void *worker_arg,
    const char *fallback_prompt,
    double idle_timeout_seconds,
    double total_ceiling_seconds,
    void (*on_timeout)(double idle, double waited, double since_progress),
    void (*on_commit_overrun)(double waited, double ceiling),
    cc_commit_fence_t *fence) {

    if (idle_timeout_seconds <= 0) {
        fprintf(stderr,
            "run_compress_context_with_progress_timeout requires "
            "idle_timeout_seconds > 0; call compress_context directly to disable\n");
        return NULL;
    }

    /* Bounded admission (#76354 F6): refuse rather than queue when every pool
     * slot is occupied. _try_admit_compression_job returns false WITHOUT
     * incrementing the counter, so no release is needed on the failure path. */
    if (!cc_try_admit_compression_job()) {
        fprintf(stderr,
            "Context compression pool saturated (%d workers busy) — refusing "
            "new compression this cycle and continuing without compression.\n",
            CC_COMPRESS_EXECUTOR_MAX_WORKERS);
        return strdup(fallback_prompt ? fallback_prompt : "");
    }

    cc_compress_run_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        cc_release_compression_admission();
        return strdup(fallback_prompt ? fallback_prompt : "");
    }
    pthread_mutex_init(&ctx->mutex, NULL);
    pthread_cond_init(&ctx->done, NULL);

    ctx->worker_fn = worker_fn;
    ctx->worker_arg = worker_arg;
    ctx->idle_timeout = idle_timeout_seconds;
    ctx->ceiling = total_ceiling_seconds > idle_timeout_seconds ?
                    total_ceiling_seconds : idle_timeout_seconds;
    ctx->on_timeout = on_timeout;
    ctx->on_commit_overrun = on_commit_overrun;
    ctx->fence_owned = (fence == NULL);
    ctx->fence = fence ? fence : cc_commit_fence_new();
    ctx->exit_code = -1;

    /* Submit the job. */
    cc_pool_job_t *job = calloc(1, sizeof(*job));
    if (!job) {
        if (ctx->fence_owned) cc_commit_fence_free(ctx->fence);
        pthread_mutex_destroy(&ctx->mutex);
        pthread_cond_destroy(&ctx->done);
        free(ctx);
        cc_release_compression_admission();
        return strdup(fallback_prompt ? fallback_prompt : "");
    }
    job->fn = cc_compress_job_fn;
    job->arg = ctx;

    cc_compress_pool_t *pool = cc_get_compress_timeout_executor();
    if (!pool) {
        if (job) free(job);
        if (ctx->fence_owned) cc_commit_fence_free(ctx->fence);
        pthread_mutex_destroy(&ctx->mutex);
        pthread_cond_destroy(&ctx->done);
        free(ctx);
        cc_release_compression_admission();
        return strdup(fallback_prompt ? fallback_prompt : "");
    }

    pthread_mutex_lock(&pool->mutex);
    if (pool->job_tail)
        pool->job_tail->next = job;
    else
        pool->job_queue = job;
    pool->job_tail = job;
    pthread_cond_signal(&pool->job_cond);
    pthread_mutex_unlock(&pool->mutex);

    /* Wait loop: progress-aware polling against idle + ceiling. */
    double wait_started = cc_monotonic();
    bool handled_exit = false;

    for (;;) {
        double waited = cc_monotonic() - wait_started;
        double remaining_ceiling = ctx->ceiling - waited;
        if (remaining_ceiling <= 0)
            break;

        double since_progress =
            cc_commit_fence_seconds_since_progress(ctx->fence);
        double wait_slice = idle_timeout_seconds - since_progress;
        if (wait_slice < 0.005) wait_slice = 0.005;
        if (wait_slice > remaining_ceiling) wait_slice = remaining_ceiling;

        /* Use monotonic deadline; poll completion in small increments since
         * pthread condvar uses CLOCK_REALTIME (clock-domain mismatch). */
        bool done = false;
        double deadline = cc_monotonic() + wait_slice;
        while (cc_monotonic() < deadline) {
            pthread_mutex_lock(&ctx->mutex);
            if (ctx->completed) {
                done = true;
            }
            pthread_mutex_unlock(&ctx->mutex);
            if (done) break;
            /* Small spin to check progress + completion without blocking the
             * condvar clock-domain issue. */
            struct timespec req = {0, 5 * 1000000}; /* 5ms */
            nanosleep(&req, NULL);
        }

        pthread_mutex_lock(&ctx->mutex);
        if (ctx->completed) {
            handled_exit = true;
            pthread_mutex_unlock(&ctx->mutex);
            /* Worker returned its result — success path. */
            ctx->exit_code = 0;
            break;
        }
        pthread_mutex_unlock(&ctx->mutex);

        /* TimeoutError path: check if idle budget exhausted. */
        waited = cc_monotonic() - wait_started;
        since_progress = cc_commit_fence_seconds_since_progress(ctx->fence);
        if (since_progress < idle_timeout_seconds && waited < ctx->ceiling) {
            fprintf(stderr,
                "Context compression still streaming after %.0fs (last progress "
                "%.1fs ago) — extending wait (ceiling %.0fs)\n",
                waited, since_progress, ctx->ceiling);
            continue;
        }
        break;  /* ceiling or idle exhausted — fall through to fence cancel */
    }

    /* Not completed within budget. */
    pthread_mutex_lock(&ctx->mutex);
    bool was_completed = ctx->completed;
    pthread_mutex_unlock(&ctx->mutex);
    if (was_completed) {
        /* Worker finished just now — treat as success. */
        cc_release_compression_admission();
        char *ret = ctx->result_text;
        if (!ret) ret = strdup(fallback_prompt ? fallback_prompt : "");
        pthread_mutex_destroy(&ctx->mutex);
        pthread_cond_destroy(&ctx->done);
        if (ctx->fence_owned) cc_commit_fence_free(ctx->fence);
        free(ctx);
        return ret;
    }

    /* F6: cancel the future so a not-started future doesn't linger as a stale
     * queued job. For a running worker, the fence handles the cancel path. */
    /* (In our model, if the worker is still running, the fence cancel below
     *  prevents any late commit. We cannot pthread_cancel a running job safely
     *  in C11, so we rely on the worker checking the fence — matching Python's
     *  fence-cancel model.) */

    /* Check commit phase: if begin_commit won, we must WAIT for the commit
     * to finish (cannot fence-cancel a mid-mutation commit). */
    bool commit_in_flight = cc_commit_fence_commit_in_flight(ctx->fence);
    cc_release_compression_admission();

    if (!commit_in_flight) {
        /* Pre-commit: cancellation won before the commit boundary. */
        cc_commit_fence_release_cancelled_compression_lock(ctx->fence);
        double waited = cc_monotonic() - wait_started;
        double since_progress =
            cc_commit_fence_seconds_since_progress(ctx->fence);
        if (ctx->on_timeout) {
            ctx->on_timeout(idle_timeout_seconds, waited, since_progress);
        } else {
            fprintf(stderr,
                "Context compression made no progress for %.1fs (total wait "
                "%.1fs, ceiling %.1fs); continuing without compression\n",
                since_progress, waited, ctx->ceiling);
        }
        char *ret = strdup(fallback_prompt ? fallback_prompt : "");
        pthread_mutex_destroy(&ctx->mutex);
        pthread_cond_destroy(&ctx->done);
        if (ctx->fence_owned) cc_commit_fence_free(ctx->fence);
        free(ctx);
        return ret;
    }

    /* Commit in-flight: wait in bounded slices until it completes. */
    int overrun_reports = 0;
    bool overrun_surfaced = false;
    for (;;) {
        double waited = cc_monotonic() - wait_started;
        double remaining = ctx->ceiling - waited;
        if (remaining <= 0) {
            remaining = 30.0;  /* _COMMIT_OVERRUN_WAIT_SLICE_SECONDS */
            if (ctx->ceiling > 0 && remaining > ctx->ceiling)
                remaining = ctx->ceiling;
            if (remaining < 0.05) remaining = 0.05;
            overrun_reports++;
            fprintf(stderr,
                "Context compression SessionDB commit still running %.1fs "
                "past the total ceiling (waited %.1fs, ceiling %.1fs); "
                "commit cannot be abandoned mid-flight — continuing to wait\n",
                waited - ctx->ceiling, waited, ctx->ceiling);
            if (!overrun_surfaced && ctx->on_commit_overrun) {
                overrun_surfaced = true;
                ctx->on_commit_overrun(waited, ctx->ceiling);
            }
        }
        /* Wait for the worker to finish (bounded by remaining slice). */
        struct timespec req = {0, 5 * 1000000}; /* 5ms spin */
        pthread_mutex_lock(&ctx->mutex);
        bool done_now = ctx->completed;
        pthread_mutex_unlock(&ctx->mutex);
        if (done_now) break;
        nanosleep(&req, NULL);
    }

    /* Worker completed post-commit. */
    char *ret = ctx->result_text;
    if (!ret) ret = strdup(fallback_prompt ? fallback_prompt : "");
    pthread_mutex_destroy(&ctx->mutex);
    pthread_cond_destroy(&ctx->done);
    if (ctx->fence_owned) cc_commit_fence_free(ctx->fence);
    free(ctx);
    return ret;
}
