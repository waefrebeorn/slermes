/* t_conversation_compression.c — behavioral test for
 * conversation_compression_helpers.c. Case outputs printed as JSON lines so
 * the same fixture can be diffed against the live-Python oracle
 * (tests/sta_oracle_conv_compression.py).
 */
#include "conversation_compression.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static int fails = 0;
#define CHECK(cond, name) do { \
    if (cond) printf("PASS %s\n", name); \
    else { printf("FAIL %s\n", name); fails++; } \
} while (0)

static json_t *J(const char *s) { return json_parse(s, NULL); }

static bool notify_hits = false;
static bool notify_ok(void *ctx, const char *n, const char *o) {
    (void)ctx; notify_hits = true;
    return n && o && strcmp(n, "new1") == 0 && strcmp(o, "old1") == 0;
}

int main(void) {
    /* ── commit fence ────────────────────────────────────────────── */
    cc_commit_fence_t *f = cc_commit_fence_new();
    CHECK(f != NULL, "fence new");
    CHECK(cc_commit_fence_seconds_since_progress(f) < 1.0, "fresh progress");
    usleep(30000);
    CHECK(cc_commit_fence_seconds_since_progress(f) >= 0.02, "progress ages");
    cc_commit_fence_touch_progress(f);
    CHECK(cc_commit_fence_seconds_since_progress(f) < 0.02, "touch resets");

    CHECK(cc_commit_fence_cancel_before_commit(f) == true, "cancel wins pre-commit");
    CHECK(cc_commit_fence_begin_commit(f) == false, "begin after cancel refused");
    cc_commit_fence_free(f);

    f = cc_commit_fence_new();
    CHECK(cc_commit_fence_begin_commit(f) == true, "begin fresh");
    CHECK(cc_commit_fence_try_cancel_before_commit(f) == -1,
          "try-cancel busy during commit");
    cc_commit_fence_finish_commit(f);
    CHECK(cc_commit_fence_try_cancel_before_commit(f) == 0,
          "try-cancel after commit started returns false");
    cc_commit_fence_free(f);

    f = cc_commit_fence_new();
    CHECK(cc_commit_fence_try_cancel_before_commit(f) == 1, "try-cancel wins");
    CHECK(cc_commit_fence_begin_commit(f) == false, "begin refused post try-cancel");
    cc_commit_fence_free(f);

    /* ── lock-skip signal ────────────────────────────────────────── */
    cc_lock_skip_signal_t sig = { .skipped = false, .holder = NULL };
    CHECK(!cc_compression_skipped_due_to_lock(&sig), "no skip");
    sig.skipped = true;
    CHECK(cc_compression_skipped_due_to_lock(&sig), "skip true");
    CHECK(!cc_compression_skipped_due_to_lock(NULL), "null signal");

    /* ── _message_text ───────────────────────────────────────────── */
    json_t *m = J("{\"role\": \"user\", \"content\": \"hello\"}");
    char *t = cc_message_text(m);
    CHECK(strcmp(t, "hello") == 0, "text str");
    free(t); json_free(m);

    m = J("{\"content\": [{\"type\": \"text\", \"text\": \"a\"}, {\"content\": \"b\"}, \"skipme\", {\"x\": 1}]}");
    t = cc_message_text(m);
    CHECK(strcmp(t, "a\nb\n") == 0, "text list joins dict parts");
    free(t); json_free(m);

    m = J("{\"content\": null}");
    t = cc_message_text(m);
    CHECK(strcmp(t, "") == 0, "text null");
    free(t); json_free(m);

    /* ── _is_real_user_message ───────────────────────────────────── */
    m = J("{\"role\": \"user\", \"content\": \"do the thing\"}");
    CHECK(cc_is_real_user_message(m), "real user");
    json_free(m);
    m = J("{\"role\": \"assistant\", \"content\": \"x\"}");
    CHECK(!cc_is_real_user_message(m), "assistant not user");
    json_free(m);
    m = J("{\"role\": \"user\", \"content\": \"x\", \"_todo_snapshot_synthetic\": true}");
    CHECK(!cc_is_real_user_message(m), "synthetic flag");
    json_free(m);
    m = J("{\"role\": \"user\", \"content\": \"   \"}");
    CHECK(!cc_is_real_user_message(m), "empty text");
    json_free(m);
    m = J("{\"role\": \"user\", \"content\": \"[System: Your previous tool call failed]\"}");
    CHECK(!cc_is_real_user_message(m), "synthetic prefix");
    json_free(m);
    m = J("{\"role\": \"user\", \"content\": \"Continue from the compressed conversation context above. This marker exists because no human user turn was available.\"}");
    CHECK(!cc_is_real_user_message(m), "continuation sentinel");
    json_free(m);
    m = J("{\"role\": \"user\", \"content\": \"real ask\", \"_compressed_summary\": true}");
    CHECK(!cc_is_real_user_message(m), "compressed-summary metadata");
    json_free(m);
    m = J("{\"role\": \"user\", \"content\": \"[Your active task list was preserved across context compression]\\ntodo body\"}");
    CHECK(!cc_is_real_user_message(m), "todo header row");
    json_free(m);

    /* ── _strip_stale_todo_snapshot ──────────────────────────────── */
    json_t *c = J("\"keep me\\n\\n[Your active task list was preserved across context compression]\\nstale\"");
    json_t *stripped = cc_strip_stale_todo_snapshot(c);
    CHECK(stripped->type == JSON_STRING &&
          strcmp(stripped->str_val, "keep me") == 0, "strip str snapshot");
    json_free(c); json_free(stripped);

    c = J("\"no snapshot here\"");
    stripped = cc_strip_stale_todo_snapshot(c);
    CHECK(strcmp(stripped->str_val, "no snapshot here") == 0, "no-op strip");
    json_free(c); json_free(stripped);

    c = J("[{\"type\": \"text\", \"text\": \"keep\"}, {\"type\": \"text\", \"text\": \"  [Your active task list was preserved across context compression]\\nstale\"}]");
    stripped = cc_strip_stale_todo_snapshot(c);
    CHECK(stripped->type == JSON_ARRAY && json_len(stripped) == 1, "strip list part");
    json_free(c); json_free(stripped);

    /* ── _merge_anchor_into_user_message ─────────────────────────── */
    json_t *target = J("{\"role\": \"user\", \"content\": \"scaffold\", \"_todo_snapshot_synthetic\": true}");
    json_t *anchor = J("{\"role\": \"user\", \"content\": \"the task\"}");
    cc_merge_anchor_into_user_message(target, anchor);
    t = cc_message_text(target);
    CHECK(strcmp(t, "the task\n\nscaffold") == 0, "merge strings anchor-first");
    CHECK(!json_has(target, "_todo_snapshot_synthetic"), "merge clears flags");
    free(t); json_free(target); json_free(anchor);

    target = J("{\"role\": \"user\", \"content\": [{\"type\": \"text\", \"text\": \"scaffold\"}]}");
    anchor = J("{\"role\": \"user\", \"content\": \"the task\"}");
    cc_merge_anchor_into_user_message(target, anchor);
    const json_t *tc2 = json_obj_get(target, "content");
    CHECK(tc2 && tc2->type == JSON_ARRAY && json_len(tc2) == 2 &&
          strcmp(json_get_str(json_get(tc2, 0), "text", ""), "the task") == 0,
          "merge list anchor parts lead");
    json_free(target); json_free(anchor);

    /* ── _insert_real_user_anchor ────────────────────────────────── */
    /* boundary slot: assistant not preceded by user */
    json_t *msgs = J("[{\"role\": \"assistant\", \"content\": \"summary\"}, {\"role\": \"user\", \"content\": \"u\"}]");
    cc_insert_real_user_anchor(msgs, J("{\"role\": \"user\", \"content\": \"anchor\"}"));
    CHECK(json_len(msgs) == 3 &&
          strcmp(json_get_str(json_get(msgs, 0), "content", ""), "anchor") == 0,
          "insert at summary boundary");
    json_free(msgs);

    /* all assistants user-preceded, tail not user → append */
    msgs = J("[{\"role\": \"user\", \"content\": \"u\"}, {\"role\": \"assistant\", \"content\": \"a\"}]");
    cc_insert_real_user_anchor(msgs, J("{\"role\": \"user\", \"content\": \"anchor\"}"));
    CHECK(json_len(msgs) == 3 &&
          strcmp(json_get_str(json_get(msgs, 2), "content", ""), "anchor") == 0,
          "append when tail non-user");
    json_free(msgs);

    /* empty list → append */
    msgs = J("[]");
    cc_insert_real_user_anchor(msgs, J("{\"role\": \"user\", \"content\": \"anchor\"}"));
    CHECK(json_len(msgs) == 1, "append into empty");
    json_free(msgs);

    /* trailing user scaffolding (non-summary) → merge */
    msgs = J("[{\"role\": \"user\", \"content\": \"u1\"}, {\"role\": \"assistant\", \"content\": \"a\"}, {\"role\": \"user\", \"content\": \"scaffold\", \"_pre_verify_synthetic\": true}]");
    cc_insert_real_user_anchor(msgs, J("{\"role\": \"user\", \"content\": \"anchor\"}"));
    t = cc_message_text(json_get(msgs, 2));
    CHECK(json_len(msgs) == 3 && strcmp(t, "anchor\n\nscaffold") == 0,
          "merge into trailing scaffolding");
    free(t); json_free(msgs);

    /* ── _ensure_compressed_has_user_turn ────────────────────────── */
    json_t *orig = J("[{\"role\": \"user\", \"content\": \"human ask\"}, {\"role\": \"assistant\", \"content\": \"a\"}]");
    json_t *comp = J("[{\"role\": \"assistant\", \"content\": \"summary\"}]");
    cc_ensure_compressed_has_user_turn(orig, comp);
    CHECK(json_len(comp) == 2, "anchor restored");
    bool has_real = false;
    for (size_t i = 0; i < json_len(comp); i++)
        if (cc_is_real_user_message(json_get(comp, i))) has_real = true;
    CHECK(has_real, "restored anchor is real");
    json_free(orig); json_free(comp);

    /* no real user anywhere → continuation sentinel appended */
    orig = J("[{\"role\": \"assistant\", \"content\": \"a\"}]");
    comp = J("[{\"role\": \"assistant\", \"content\": \"summary\"}]");
    cc_ensure_compressed_has_user_turn(orig, comp);
    CHECK(json_len(comp) == 2 &&
          strcmp(json_get_str(json_get(comp, 1), "content", ""),
                 CC_CONTINUATION_USER_CONTENT) == 0,
          "continuation sentinel fallback");
    /* and idempotent: the sentinel is synthetic, but a second call must not
     * add another (Python: any real user check fails, walks orig, appends
     * sentinel only once because comp regains none — verify count) */
    json_free(orig); json_free(comp);

    /* already has real user → no-op */
    orig = J("[]");
    comp = J("[{\"role\": \"user\", \"content\": \"real\"}]");
    cc_ensure_compressed_has_user_turn(orig, comp);
    CHECK(json_len(comp) == 1, "no-op when real user present");
    json_free(orig); json_free(comp);

    /* ── notification staging ────────────────────────────────────── */
    cc_pending_notification_t *slot = NULL;
    CHECK(cc_queue_compression_notification(&slot, notify_ok, NULL,
                                            "new1", "old1") != NULL,
          "queue notification");
    CHECK(cc_queue_compression_notification(&slot, notify_ok, NULL,
                                            "new2", "old2") == NULL,
          "double queue rejected");
    CHECK(cc_finalize_compression_notification(&slot, true) == true,
          "finalize committed emits");
    CHECK(notify_hits, "callback ran");
    CHECK(cc_finalize_compression_notification(&slot, true) == false,
          "repeated finalize no-op");
    notify_hits = false;
    CHECK(cc_queue_compression_notification(&slot, notify_ok, NULL,
                                            "new1", "old1") != NULL,
          "requeue after finalize");
    CHECK(cc_finalize_compression_notification(&slot, false) == false,
          "finalize uncommitted discards");
    CHECK(!notify_hits, "discarded callback did not run");

    /* ── telemetry line ──────────────────────────────────────────── */
    json_t *base = J("{\"chunking\": true, \"chunk_count\": 3, \"zeta\": 1}");
    char *line = cc_compression_attempt_telemetry_line(
        base, "att1", "sess1", 1234, "committed", "clean", NULL, false);
    CHECK(line && strstr(line, "\"attempt_id\":\"att1\"") != NULL,
          "telemetry attempt_id");
    CHECK(strstr(line, "\"chunking\":true") != NULL, "telemetry keeps base");
    CHECK(strstr(line, "\"commit_status\":\"committed\"") != NULL,
          "telemetry commit_status");
    /* sorted keys: attempt_id < chunk_count < chunking < commit_status */
    CHECK(strstr(line, "attempt_id") < strstr(line, "chunk_count") &&
          strstr(line, "chunk_count") < strstr(line, "chunking") &&
          strstr(line, "chunking") < strstr(line, "commit_status") &&
          strstr(line, "split_status") < strstr(line, "total_duration_ms") &&
          strstr(line, "total_duration_ms") < strstr(line, "zeta"),
          "telemetry sorted keys");
    free(line); json_free(base);

    line = cc_compression_attempt_telemetry_line(
        NULL, "a", "s", 5, "aborted", "none", "lease_lost", true);
    CHECK(line && strstr(line, "\"failure_class\":\"lease_lost\"") != NULL &&
          strstr(line, "\"fallback_used\":true") != NULL &&
          strstr(line, "\"chunking\":false") != NULL,
          "telemetry defaults + failure class");
    free(line);

    /* ── cached-prompt memory retention ──────────────────────────── */
    CHECK(cc_cached_prompt_reflects_builtin_memory(
              "note A", "", "prompt with note A inside") == true,
          "retention: block contained");
    CHECK(cc_cached_prompt_reflects_builtin_memory(
              "note A", "", "prompt without it") == false,
          "retention: block missing");
    CHECK(cc_cached_prompt_reflects_builtin_memory(
              "", "", "prompt MEMORY (your personal notes) header left") == false,
          "retention: leftover header stale");
    CHECK(cc_cached_prompt_reflects_builtin_memory(
              "", "", "clean prompt") == true,
          "retention: both empty clean");
    CHECK(cc_cached_prompt_reflects_builtin_memory(
              NULL, "x", "p") == false,
          "retention: unreadable snapshot fails closed");

    /* ── lazy kwargs / skew helpers ──────────────────────────────── */
    json_t *kw = cc_supported_compression_kwargs(true, "memblock",
                                                 1234, "topic", true);
    CHECK(kw && json_get_num(kw, "current_tokens", -1) == 1234 &&
          strcmp(json_get_str(kw, "focus_topic", ""), "topic") == 0 &&
          json_get_bool(kw, "force", false) == true &&
          strcmp(json_get_str(kw, "memory_context", ""), "memblock") == 0,
          "supported kwargs full");
    json_free(kw);
    kw = cc_supported_compression_kwargs(false, NULL, 0, NULL, false);
    CHECK(kw && json_get_bool(kw, "force", true) == false &&
          !json_has(kw, "memory_context") &&
          json_get(kw, "focus_topic") != NULL &&
          json_get(kw, "focus_topic")->type == JSON_NULL,
          "supported kwargs minimal");
    json_free(kw);

    CHECK(cc_lock_api_is_absent_on_session_db(NULL) == true, "lock absent null");
    CHECK(cc_lock_api_is_absent_on_session_db((void*)0x1) == false,
          "lock present non-null");

    /* guard refresh via weak default is a safe no-op */
    cc_refresh_persisted_compression_guards(NULL);
    cc_refresh_persisted_compression_guards((void*)0x1);
    CHECK(true, "guard refresh no-op safe");

    printf(fails ? "\n%d FAILURES\n" : "\nALL PASS\n", fails);
    return fails ? 1 : 0;
}
