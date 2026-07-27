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

    hermes_state_db_close(db);
    unlink(path);

    printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
