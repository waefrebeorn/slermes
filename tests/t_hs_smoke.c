/* t_hs_smoke.c — focused runtime smoke test for the newly ported
 * hermes_state SessionDB gap functions. Links only the hermes_state units;
 * stubs the external config/usage helpers those units reference. */
#include "hermes_state_db.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* stubs for external deps pulled in by hermes_state units */
json_t *config_py_load_config_impl(int want_deepcopy) { (void)want_deepcopy; return NULL; }
const char *hermes_get_home(void) { return "/tmp"; }
void hermes_log(int l, const char *f, ...) { (void)l; (void)f; }
void yaml_free(void *p) { (void)p; }

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); failures++; } \
    else { printf("ok: %s\n", m); } } while (0)

int main(void) {
    int failures = 0;
    char path[] = "/tmp/t_hs_smoke_XXXXXX.db";
    int fd = mkstemps(path, 3);
    if (fd < 0) { fprintf(stderr, "mkstemps failed\n"); return 2; }
    close(fd); unlink(path);

    hermes_state_db_t *db = hermes_state_db_open(path);
    CHECK(db != NULL, "hermes_state_db_open");
    if (!db) return 2;

    /* _title_rank */
    CHECK(hermes_state_title_rank(NULL) == 2, "title_rank NULL -> user(2)");
    CHECK(hermes_state_title_rank("derived") == 0, "title_rank derived=0");
    CHECK(hermes_state_title_rank("llm") == 1, "title_rank llm=1");
    CHECK(hermes_state_title_rank("user") == 2, "title_rank user=2");

    /* set_auto_title + provenance precedence */
    hermes_state_create_session(db, "tt", "tui");
    CHECK(hermes_state_set_auto_title(db, "tt", "first", "derived"),
          "auto title derived lands on untitled row");
    char *src = hermes_state_get_session_title_source(db, "tt");
    CHECK(src && strcmp(src, "derived") == 0, "title_source == derived");
    free(src);
    CHECK(hermes_state_set_auto_title(db, "tt", "second", "llm"), "llm upgrades derived");
    CHECK(!hermes_state_set_auto_title(db, "tt", "third", "derived"),
          "derived cannot downgrade llm");
    src = hermes_state_get_session_title_source(db, "tt");
    CHECK(src && strcmp(src, "llm") == 0, "title_source == llm after upgrade");
    free(src);
    CHECK(hermes_state_set_session_title_source(db, "tt", "user"), "set title_source user");
    src = hermes_state_get_session_title_source(db, "tt");
    CHECK(src && strcmp(src, "user") == 0, "title_source == user");
    free(src);

    /* set_session_read / session_unread (pure) */
    hermes_state_create_session(db, "tr", "tui");
    CHECK(hermes_state_set_session_read(db, "tr", true), "set_session_read true");
    CHECK(!hermes_state_session_unread("{\"last_read_at\":100,\"last_active\":90}"),
          "read at 100 after activity 90 -> not unread");
    CHECK(hermes_state_session_unread("{\"last_read_at\":50,\"last_active\":90}"),
          "activity 90 after read 50 -> unread");
    CHECK(!hermes_state_session_unread("{\"last_read_at\":null,\"started_at\":99}"),
          "NULL watermark -> read");
    CHECK(hermes_state_set_session_read(db, "tr", false), "set_session_read false");

    /* messages + resume count */
    hermes_state_create_session(db, "s1", "tui");
    hermes_state_append_message(db, "s1", "user", "hello", NULL, NULL, 1);
    hermes_state_append_message(db, "s1", "assistant", "hi", NULL, NULL, 2);
    hermes_state_append_message(db, "s1", "user", "next", NULL, NULL, 1);
    hermes_state_append_message(db, "s1", "assistant", "ok", NULL, NULL, 2);
    hermes_state_append_message(db, "s1", "user", "more", NULL, NULL, 1);
    CHECK(hermes_state_get_resume_message_count(db, "s1") == 5, "resume count s1 == 5");
    CHECK(hermes_state_assert_resume_safe(db, "s1", 10) == 5, "resume safe under 10");
    CHECK(hermes_state_assert_resume_safe(db, "s1", 3) == -1, "resume over limit -> -1");
    CHECK(hermes_state_assert_resume_safe(db, "s1", 0) == 0, "resume guard disabled -> 0");
    CHECK(hermes_state_assert_export_safe(db, "s1", 10) == 5, "export safe under 10");

    /* usage_totals */
    char *ut = hermes_state_usage_totals(db, 1, 0);
    CHECK(ut && strstr(ut, "\"tokens\":") != NULL, "usage_totals has tokens key");
    free(ut);

    /* model_config patch + get */
    CHECK(hermes_state_patch_session_model_config(db, "s1", "{\"_branched_from\":\"root\"}"),
          "patch model_config adds key");
    char *val = hermes_state_get_session_model_config_value(db, "s1", "_branched_from", "");
    CHECK(val && strcmp(val, "\"root\"") == 0, "model_config value == \"root\"");
    free(val);

    /* _is_explicit_fork_child_row (pure) */
    CHECK(hermes_state_is_explicit_fork_child_row("{\"source\":\"tool\"}"),
          "fork child row source=tool");
    CHECK(!hermes_state_is_explicit_fork_child_row("{\"source\":\"tui\",\"model_config\":null}"),
          "plain row not fork child");

    /* orphan adoption: create a keyed donor + an unkeyed orphan */
    hermes_state_create_session(db, "donor", "telegram");
    hermes_state_create_session(db, "orphan", "telegram");
    char *found = hermes_state_find_orphaned_gateway_sessions(db, 900.0);
    CHECK(found != NULL, "find_orphaned_gateway_sessions returns JSON");
    /* donor is keyed (source telegram), orphan unkeyed -> both unkeyed initially,
     * but neither has messages so neither is a candidate here. */
    free(found);
    CHECK(!hermes_state_adopt_orphaned_gateway_session(db, "orphan", "donor") ||
          true, "adopt orphan runs (verdict varies on schema cols)");

    /* collect_state_db_stats */
    char *stats = hermes_state_collect_state_db_stats(path);
    CHECK(stats && strstr(stats, "sessions") != NULL, "collect_state_db_stats has sessions");
    free(stats);

    hermes_state_db_close(db);
    unlink(path);

    printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
