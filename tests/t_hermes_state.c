/*
 * t_hermes_state.c — behavioral test for the faithful C11 port of the
 * hermes_state.py SessionDB surface (src/agent/hermes_state/*). Mirrors the
 * invariants asserted by tests/hermes_state/*.py against a real sqlite
 * state.db. Self-verifying; exit 0 = all assertions pass.
 */

#include "hermes_state_db.h"
#include "state_usage_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); failures++; } \
                       else printf("ok: %s\n", m); } while (0)

int main(void) {
    char path[] = "/tmp/t_hs_XXXXXX.db";
    int fd = mkstemps(path, 3);
    if (fd < 0) { fprintf(stderr, "mkstemps failed\n"); return 2; }
    close(fd); unlink(path);

    hermes_state_db_t *db = hermes_state_db_open(path);
    CHECK(db != NULL, "hermes_state_db_open");
    if (!db) return 2;

    /* ── create / append / get_messages_around (test_get_messages_around) ── */
    hermes_state_create_session(db, "s1", "tui");
    long long m1 = hermes_state_append_message(db, "s1", "user", "hello", NULL, NULL, 1);
    long long m2 = hermes_state_append_message(db, "s1", "assistant", "hi", NULL, NULL, 2);
    long long m3 = hermes_state_append_message(db, "s1", "user", "next", NULL, NULL, 1);
    long long m4 = hermes_state_append_message(db, "s1", "assistant", "ok", NULL, NULL, 2);
    long long m5 = hermes_state_append_message(db, "s1", "user", "more", NULL, NULL, 1);
    CHECK(m1 == 1 && m5 == 5, "append_message ids 1..5");

    char *around = hermes_state_get_messages_around(db, "s1", 3, 2);
    CHECK(strstr(around, "\"id\":3") != NULL, "around(3,win=2) includes anchor 3");
    /* window before = min(2, ...) = m1,m2 ; after = m4,m5 -> 5 total */
    CHECK(strstr(around, "\"id\":1") && strstr(around, "\"id\":5"),
          "around window spans m1..m5");
    CHECK(strstr(around, "\"messages_before\":2") && strstr(around, "\"messages_after\":2"),
          "around counts before=2 after=2");
    free(around);

    /* window=0 -> only anchor */
    around = hermes_state_get_messages_around(db, "s1", 3, 0);
    CHECK(strstr(around, "\"messages_before\":0") && strstr(around, "\"messages_after\":0"),
          "around(3,win=0) before=0 after=0");
    free(around);

    /* negative window clamps to 0 */
    around = hermes_state_get_messages_around(db, "s1", 3, -5);
    CHECK(strstr(around, "\"messages_before\":0"), "negative window clamps to 0");
    free(around);

    /* anchor not in session -> empty */
    around = hermes_state_get_messages_around(db, "s1", 999, 3);
    CHECK(strstr(around, "\"window\":[]"), "missing anchor -> empty window");
    free(around);

    /* ── get_anchored_view bookends (test_get_anchored_view) ── */
    char *av = hermes_state_get_anchored_view(db, "s1", 3, 2, 3);
    CHECK(strstr(av, "\"bookend_start\":") && strstr(av, "\"bookend_end\":"),
          "anchored_view has bookend_start/end");
    CHECK(strstr(av, "\"messages_before\":2"), "anchored_view messages_before=2");
    free(av);

    /* ── get_messages_as_conversation (test_get_messages_as_conversation) ── */
    char *conv = hermes_state_get_messages_as_conversation(db, "s1", false);
    CHECK(strstr(conv, "\"role\":\"user\"") && strstr(conv, "\"role\":\"assistant\""),
          "conversation has user+assistant roles");
    CHECK(strstr(conv, "\"content\":\"hello\""), "conversation content present");
    free(conv);

    /* ── end_session first-writer-wins (test_session_archiving-ish) ── */
    hermes_state_end_session(db, "s1", "compression");
    /* a later stale end with different reason must NOT overwrite */
    hermes_state_end_session(db, "s1", "agent_close");
    char *s1 = hermes_state_get_session(db, "s1");
    CHECK(strstr(s1, "\"end_reason\":\"compression\""), "end_reason stays 'compression'");
    free(s1);

    /* ── set_session_archived ── */
    hermes_state_set_session_archived(db, "s1", true);
    s1 = hermes_state_get_session(db, "s1");
    CHECK(strstr(s1, "\"archived\":1"), "archived flag set");
    free(s1);
    hermes_state_set_session_archived(db, "s1", false);
    s1 = hermes_state_get_session(db, "s1");
    CHECK(strstr(s1, "\"archived\":0"), "archived flag cleared");
    free(s1);

    /* ── update_token_counts delta + absolute (test token counters) ── */
    hermes_state_create_session(db, "tok", "tui");
    hermes_state_update_token_counts(db, "tok", 10, 5, 1, 1, 2, 1, false);
    hermes_state_update_token_counts(db, "tok", 10, 5, 1, 1, 2, 1, false);
    s1 = hermes_state_get_session(db, "tok");
    CHECK(strstr(s1, "\"input_tokens\":20") && strstr(s1, "\"output_tokens\":10"),
          "delta accumulation 20/10");
    free(s1);
    hermes_state_update_token_counts(db, "tok", 100, 50, 0, 0, 0, 0, true);
    s1 = hermes_state_get_session(db, "tok");
    CHECK(strstr(s1, "\"input_tokens\":100") && strstr(s1, "\"output_tokens\":50"),
          "absolute set 100/50");
    free(s1);

    /* ── compression lineage / root / resume (test_conversation_root,
    *    test_resolve_resume_session_id, test_get_compression_lineage) ── */
    /* s1 -> child c1 (compression), c1 -> grandchild g1 (compression);
     * also a delegate child d1 off s1 (must be excluded). */
    hermes_state_create_session(db, "c1", "tui");
    hermes_state_append_message(db, "c1", "user", "u", NULL, NULL, 1);
    /* link c1 under s1 as compression continuation */
    hermes_state_link_child(db, "c1", "s1", "compression");
    hermes_state_create_session(db, "g1", "tui");
    hermes_state_append_message(db, "g1", "user", "u", NULL, NULL, 1);
    /* link g1 under c1 as compression continuation */
    hermes_state_link_child(db, "g1", "c1", "compression");
    /* delegate child d1 off s1 (must be ignored by compression walk) */
    hermes_state_create_session(db, "d1", "tui");
    hermes_state_set_model_config(db, "d1", "{\"_delegate_from\":\"x\"}");
    char *root = hermes_state_get_conversation_root(db, "g1");
    CHECK(strcmp(root, "s1") == 0, "root of g1 is s1");
    free(root);

    char *tip = hermes_state_get_compression_tip(db, "s1");
    CHECK(strcmp(tip, "g1") == 0, "compression tip of s1 is g1 (delegate excluded)");
    free(tip);

    char *resolved = hermes_state_resolve_resume_session_id(db, "s1");
    CHECK(strcmp(resolved, "g1") == 0, "resume(s1) -> g1 (has messages)");
    free(resolved);

    char *lin = hermes_state_get_compression_lineage(db, "s1");
    CHECK(strstr(lin, "\"s1\"") && strstr(lin, "\"c1\"") && strstr(lin, "\"g1\""),
          "compression lineage s1->c1->g1");
    CHECK(strstr(lin, "d1") == NULL, "delegate d1 excluded from lineage");
    free(lin);

    /* ── record_auxiliary_usage accumulation (test_aux_usage_accounting) ── */
    hermes_state_create_session(db, "aux", "tui");
    hermes_state_record_auxiliary_usage(db, "aux", "vision", "gemini", "google", NULL, 10, 5, 0, 0, 2, true, 0.001);
    hermes_state_record_auxiliary_usage(db, "aux", "vision", "gemini", "google", NULL, 7, 3, 0, 0, 1, true, 0.002);
    int calls = 0; long long in_t = 0, out_t = 0, rzn = 0; double cost = 0;
    state_usage_get_row((state_usage_db_t*)db, "aux", "gemini", "vision",
                        &calls, &in_t, &out_t, &rzn, &cost);
    CHECK(calls == 2 && in_t == 17 && out_t == 8 && rzn == 3, "aux usage 17/8/3 x2");

    /* ── archive lineage semantics (test_session_archiving.py) ──
     * archiving the surfaced tip must archive the projected root too. */
    hermes_state_create_session(db, "aroot", "cli");
    hermes_state_create_session(db, "atip", "cli");
    hermes_state_link_child(db, "atip", "aroot", "");     /* parent link */
    /* root must be compression-ended for the lineage walk */
    hermes_state_link_child(db, "aroot", "", "compression"); /* no-op parent, set reason */
    {
        /* aroot has no parent; use end_session-style update via link on child
         * only. Set aroot's end_reason directly through the public surface:
         * end_session gives first-writer-wins semantics. */
        hermes_state_end_session(db, "aroot", "compression");
    }
    CHECK(hermes_state_set_session_archived(db, "atip", true), "archive tip returns true");
    char *js = hermes_state_get_session(db, "aroot");
    CHECK(js && strstr(js, "\"archived\":1"), "archiving tip archives projected root");
    free(js);
    js = hermes_state_get_session(db, "atip");
    CHECK(js && strstr(js, "\"archived\":1"), "tip itself archived");
    free(js);
    CHECK(hermes_state_set_session_archived(db, "atip", false), "unarchive tip returns true");
    js = hermes_state_get_session(db, "aroot");
    CHECK(js && strstr(js, "\"archived\":0"), "unarchiving tip unarchives projected root");
    free(js);

    /* list_sessions_rich hides compression-away roots and archived rows */
    hermes_state_set_session_archived(db, "atip", true);
    char *lst = hermes_state_list_sessions_rich(db, false);
    CHECK(strstr(lst, "\"atip\"") == NULL && strstr(lst, "\"aroot\"") == NULL,
          "archived lineage hidden from default list");
    free(lst);
    lst = hermes_state_list_sessions_rich(db, true);
    CHECK(strstr(lst, "\"atip\"") != NULL, "archived_only lists the tip");
    CHECK(strstr(lst, "\"aroot\"") == NULL, "compression root hidden even in archived list");
    free(lst);

    /* pinned flag flips whole lineage too */
    CHECK(hermes_state_set_session_pinned(db, "atip", true), "pin tip");
    js = hermes_state_get_session(db, "aroot");
    CHECK(js && strstr(js, "\"pinned\":1"), "pinning tip pins projected root");
    free(js);

    /* ── alternation repair (test_restore_alternation_repair.py) ── */
    {
        /* user, assistant, user, user, assistant -> repair merges user pair */
        repair_msg_t r[5];
        memset(r, 0, sizeof r);
        r[0].role = strdup("user");      r[0].content = strdup("first ask");
        r[1].role = strdup("assistant"); r[1].content = strdup("first reply");
        r[2].role = strdup("user");      r[2].content = strdup("unanswered turn");
        r[3].role = strdup("user");      r[3].content = strdup("next turn");
        r[4].role = strdup("assistant"); r[4].content = strdup("next reply");
        int cnt = 5;
        int reps = hermes_state_repair_message_sequence(r, &cnt);
        CHECK(reps == 1 && cnt == 4, "repair merges user;user pair (1 repair, 4 msgs)");
        CHECK(strcmp(r[2].content, "unanswered turn\n\nnext turn") == 0,
              "merged user content joined with blank line");
        /* stability: running repair again is a no-op */
        int reps2 = hermes_state_repair_message_sequence(r, &cnt);
        CHECK(reps2 == 0 && cnt == 4, "repaired sequence is stable (no-op rerun)");
        for (int i = 0; i < cnt; i++) {
            free(r[i].role); free(r[i].content); free(r[i].tool_call_id);
            free(r[i].tool_call_ids); free(r[i].finish_reason);
            free(r[i].reasoning_content);
        }
    }
    {
        /* clean transcript -> no repairs */
        repair_msg_t r[2];
        memset(r, 0, sizeof r);
        r[0].role = strdup("user");      r[0].content = strdup("ask");
        r[1].role = strdup("assistant"); r[1].content = strdup("reply");
        int cnt = 2;
        CHECK(hermes_state_repair_message_sequence(r, &cnt) == 0 && cnt == 2,
              "repair noop on clean transcript");
        for (int i = 0; i < cnt; i++) { free(r[i].role); free(r[i].content); }
    }
    {
        /* orphan tool result dropped; matched tool result kept; duplicate dropped */
        repair_msg_t r[5];
        memset(r, 0, sizeof r);
        r[0].role = strdup("user");      r[0].content = strdup("go");
        r[1].role = strdup("assistant"); r[1].tool_call_ids = strdup("call_1");
        r[2].role = strdup("tool");      r[2].tool_call_id = strdup("call_1");
        r[3].role = strdup("tool");      r[3].tool_call_id = strdup("call_1"); /* dup */
        r[4].role = strdup("tool");      r[4].tool_call_id = strdup("call_9"); /* orphan */
        int cnt = 5;
        int reps = hermes_state_repair_message_sequence(r, &cnt);
        CHECK(reps == 2 && cnt == 3, "duplicate + orphan tool results dropped");
        for (int i = 0; i < cnt; i++) {
            free(r[i].role); free(r[i].content); free(r[i].tool_call_id);
            free(r[i].tool_call_ids); free(r[i].finish_reason);
            free(r[i].reasoning_content);
        }
    }
    {
        /* consecutive assistants merge: tool_calls union + content concat */
        repair_msg_t r[3];
        memset(r, 0, sizeof r);
        r[0].role = strdup("user");      r[0].content = strdup("go");
        r[1].role = strdup("assistant"); r[1].content = strdup("part one");
        r[1].tool_call_ids = strdup("call_a");
        r[2].role = strdup("assistant"); r[2].content = strdup("part two");
        r[2].tool_call_ids = strdup("call_b");
        int cnt = 3;
        int reps = hermes_state_repair_message_sequence(r, &cnt);
        CHECK(reps == 1 && cnt == 2, "assistant;assistant merged");
        CHECK(strcmp(r[1].content, "part one\npart two") == 0,
              "assistant content concatenated with newline");
        CHECK(strcmp(r[1].tool_call_ids, "call_a;call_b") == 0,
              "tool_calls unioned in order");
        for (int i = 0; i < cnt; i++) {
            free(r[i].role); free(r[i].content); free(r[i].tool_call_id);
            free(r[i].tool_call_ids); free(r[i].finish_reason);
            free(r[i].reasoning_content);
        }
    }

    hermes_state_db_close(db);
    unlink(path);

    printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
