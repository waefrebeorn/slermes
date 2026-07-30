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
    pthread_mutex_t lock;
    bool cancelled;
    bool commit_started;
    double last_progress; /* atomic-enough double store, mirrors CPython */
};

/* PoP: cc_commit_fence_new @ agent/conversation_compression.py:__init__ */
cc_commit_fence_t *cc_commit_fence_new(void) {
    cc_commit_fence_t *f = calloc(1, sizeof(*f));
    if (!f) return NULL;
    pthread_mutex_init(&f->lock, NULL);
    f->cancelled = false;
    f->commit_started = false;
    f->last_progress = cc_monotonic();
    return f;
}

void cc_commit_fence_free(cc_commit_fence_t *f) {
    if (!f) return;
    pthread_mutex_destroy(&f->lock);
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
    if (f->cancelled) {
        pthread_mutex_unlock(&f->lock);
        return false;
    }
    f->commit_started = true;
    /* lock intentionally HELD across the commit boundary */
    return true;
}

/* PoP: cc_commit_fence_finish_commit @ agent/conversation_compression.py:finish_commit */
void cc_commit_fence_finish_commit(cc_commit_fence_t *f) {
    if (f) pthread_mutex_unlock(&f->lock);
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

/* ── _CompactionActivityHeartbeat._touch ─────────────────────────────── */

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

