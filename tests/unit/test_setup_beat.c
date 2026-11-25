/* test_setup_beat.c -- the onboarding tool end to end, against a real state.db.
 *
 * Nothing is stubbed except the home directory: the test points SLERMES_HOME at a scratch dir, seeds
 * a session's model_config with a run record, and then drives setup_beat_tool the way the agent loop
 * does. That exercises the whole chain -- load, apply, save, render -- which is the part a registry
 * dispatch cannot reach (it has no session).
 */
#include "hermes_json.h"
#include "hermes_state_db.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir((p), 0777)
#endif

extern char *setup_beat_tool(const char *args_json, const char *session_id);

/* ── stubs: logging, home plumbing and tool-error helpers the state db links against but this path
 *    never reaches. Only infrastructure is stubbed; every line of onboarding logic is the real code. */
#include <stdarg.h>

const char *get_hermes_home(void) { return NULL; }
const char *hc_get_hermes_home_override(void) { return NULL; }
char *hs_resolve_journal_mode(void) { return NULL; }
void hermes_log(int level, const char *module, const char *fmt, ...) {
    (void)level; (void)module; (void)fmt;
}
char *tool_error(const char *message, ...) { (void)message; return NULL; }
char *tool_error_with_class(const char *kind, const char *message) { (void)kind; (void)message; return NULL; }
const char *tool_classify_error(const char *message) { (void)message; return "error"; }
void *tool_idem_new(const char *a, const char *b) { (void)a; (void)b; return NULL; }
void tool_idem_free(void *p) { (void)p; }
const char *tool_idem_key(const char *a) { (void)a; return NULL; }
const char *tool_idem_get(const char *a) { (void)a; return NULL; }
void tool_idem_attach(const char *a, const char *b) { (void)a; (void)b; }
void tool_idem_put(const char *a, const char *b, const char *c) { (void)a; (void)b; (void)c; }
bool tool_result_is_error(const char *a) { (void)a; return false; }

static int failures = 0;

static void check(bool ok, const char *what) {
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static bool has(const json_t *o, const char *key) { return o && o->type == JSON_OBJECT && json_has(o, key); }

static const char *str(const json_t *o, const char *key) { return json_get_str(o, key, NULL); }

/* A record shaped like flow.new_record, as the record store keeps it. */
static const char *seed_record(void) {
    return "{"
        "\"run_id\":\"r_test\",\"phase\":\"onboarding\","
        "\"beats\":{\"name\":\"pending\",\"theme\":\"pending\",\"layout\":\"pending\","
                   "\"connectors\":\"pending\",\"plugins\":\"skipped\",\"tour\":\"skipped\","
                   "\"build\":\"pending\"},"
        "\"waiting\":null,"
        "\"picks\":{\"name\":null,\"accent\":null,\"mode\":null,\"layout\":null,"
                  "\"connected\":[],\"newly_connected\":[],\"installed\":[],\"tour\":null,"
                  "\"build\":null,\"build_options\":[]},"
        "\"look_around\":\"not_offered\","
        "\"facts\":{\"os\":\"linux\",\"arch\":\"x86_64\",\"machine_kind\":\"PC\","
                  "\"facts_source\":\"local\",\"locale\":\"en_US\",\"plugins\":[],\"tours\":false},"
        "\"ended_by\":null,\"started_at\":0,\"updated_at\":0}";
}

int main(void) {
    /* Point the whole tool at a scratch home. */
    char home[1024], dbpath[1200];
    snprintf(home, sizeof home, "%s", getenv("SB_TEST_HOME") ? getenv("SB_TEST_HOME") : ".testbin/sb_home");
    MKDIR(home);
    _putenv_s("SLERMES_HOME", home);
    snprintf(dbpath, sizeof dbpath, "%s/state.db", home);
    remove(dbpath);

    hermes_state_db_t *db = hermes_state_db_open(dbpath);
    if (!db) { printf("  FAIL cannot open %s\n", dbpath); return 1; }
    if (!hermes_state_create_session(db, "s_test", "local")) {
        printf("  FAIL cannot create the test session\n");
        return 1;
    }
    char patch[8192];
    snprintf(patch, sizeof patch, "{\"onboarding_run\":%s}", seed_record());
    check(hermes_state_patch_session_model_config(db, "s_test", patch), "seed the run record");
    hermes_state_db_close(db);

    /* 1. next -> the first unsettled beat, with its question. */
    char *out = setup_beat_tool("{\"beat\":\"next\"}", "s_test");
    json_t *o = out ? json_parse(out, NULL) : NULL;
    check(o && strcmp(str(o, "beat") ? str(o, "beat") : "", "name") == 0, "next -> the name card");
    check(o && has(o, "question"), "next -> a question for the card");
    json_free(o); free(out);

    /* 2. a value given in words settles the beat and reports the next one. */
    out = setup_beat_tool("{\"beat\":\"name\",\"value\":\" Ada \"}", "s_test");
    o = out ? json_parse(out, NULL) : NULL;
    check(o && strcmp(str(o, "recorded") ? str(o, "recorded") : "", "name") == 0, "value -> recorded name");
    check(o && strcmp(str(o, "next") ? str(o, "next") : "", "theme") == 0, "value -> next is theme");
    check(o && has(o, "reminder"), "value -> a reminder rides on it");
    json_free(o); free(out);

    /* 3. it was saved: the record now carries the trimmed name. */
    db = hermes_state_db_open(dbpath);
    char *raw = hermes_state_get_session_model_config_value(db, "s_test", "onboarding_run", "null");
    json_t *rec = raw ? json_parse(raw, NULL) : NULL;
    const json_t *picks = rec ? json_obj_get(rec, "picks") : NULL;
    check(picks && strcmp(json_get_str(picks, "name", ""), "Ada") == 0, "record saved with the trimmed name");
    const json_t *beats = rec ? json_obj_get(rec, "beats") : NULL;
    check(beats && strcmp(json_get_str(beats, "name", ""), "done") == 0, "record saved with the beat done");
    json_free(rec); free(raw); hermes_state_db_close(db);

    /* 4. skip settles a beat and says the run moved on. */
    out = setup_beat_tool("{\"beat\":\"theme\",\"skip\":true}", "s_test");
    o = out ? json_parse(out, NULL) : NULL;
    check(o && json_get_bool(o, "skipped", false), "skip -> skipped");
    check(o && strcmp(str(o, "next") ? str(o, "next") : "", "layout") == 0, "skip -> next is layout");
    json_free(o); free(out);

    /* 5. a beat with no direct event is a card, and cards need the desktop. */
    out = setup_beat_tool("{\"beat\":\"layout\"}", "s_test");
    o = out ? json_parse(out, NULL) : NULL;
    check(o && str(o, "error") &&
          strcmp(str(o, "error"), "setup_beat cards show only in the Hermes desktop app.") == 0,
          "card beat -> the desktop-only answer");
    json_free(o); free(out);

    /* 6. an unknown beat is refused before anything is loaded. */
    out = setup_beat_tool("{\"beat\":\"bogus\"}", "s_test");
    o = out ? json_parse(out, NULL) : NULL;
    check(o && str(o, "error") && strstr(str(o, "error"), "beat must be one of") != NULL,
          "unknown beat -> the enum error");
    json_free(o); free(out);

    /* 7. a session with no run says so. */
    db = hermes_state_db_open(dbpath);
    hermes_state_create_session(db, "s_empty", "local");
    hermes_state_db_close(db);
    out = setup_beat_tool("{\"beat\":\"next\"}", "s_empty");
    o = out ? json_parse(out, NULL) : NULL;
    check(o && str(o, "error") &&
          strcmp(str(o, "error"), "Onboarding is not running in this chat.") == 0,
          "no run record -> the not-running answer");
    json_free(o); free(out);

    /* 8. the run completes when the build beat settles: walk the remaining beats. */
    out = setup_beat_tool("{\"beat\":\"layout\",\"value\":\"terminal-deck\"}", "s_test"); free(out);
    out = setup_beat_tool("{\"beat\":\"connectors\",\"skip\":true}", "s_test"); free(out);
    out = setup_beat_tool("{\"beat\":\"build\",\"value\":\"hermes\"}", "s_test");
    o = out ? json_parse(out, NULL) : NULL;
    check(o && json_get_bool(o, "completed", false), "build settles -> run completes");
    json_free(o); free(out);

    printf("\nsetup_beat: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
