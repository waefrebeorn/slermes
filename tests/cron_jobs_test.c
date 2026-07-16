/*
 * cron_jobs_test.c — real-behavior test for port_cron_jobs.c
 *
 * Exercises the persist-on-disk job store in a temp SLERMES_HOME and asserts
 * faithful behavior: schedule parsing, creation, normalization, persistence,
 * CRUD, run bookkeeping, due-jobs, output retention, and skill-ref rewriting.
 *
 * MIT License — Slermes Fork
 */
#include "cron_jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } \
} while (0)

/* Normalize a job's skills to a comma string for comparison. */
static char *skills_csv(const json_t *job) {
    json_t *sk = json_object_get(job, "skills");
    char *buf = malloc(256);
    buf[0] = '\0';
    if (json_is_array(sk)) {
        size_t n = json_array_size(sk);
        for (size_t i = 0; i < n; i++) {
            const char *s = json_string_value(json_array_get(sk, i));
            if (i) strncat(buf, ",", 255 - strlen(buf));
            strncat(buf, s ? s : "", 255 - strlen(buf));
        }
    }
    return buf;
}

static void rmrf(const char *p) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", p);
    system(cmd);
}

int main(void) {
    char tmpl[] = "/tmp/cronjob_test.XXXXXX";
    char *home = mkdtemp(tmpl);
    if (!home) { printf("cannot make temp home\n"); return 2; }
    setenv("SLERMES_HOME", home, 1);

    /* --- schedule parsing --- */
    char *err = NULL;
    json_t *sch = cronjobs_parse_schedule("every 30m", &err);
    CHECK(sch != NULL, "parse 'every 30m'");
    CHECK(strcmp(json_object_get_string(sch, "kind", ""), "interval") == 0, "every 30m kind=interval");
    CHECK((int)json_get_num(sch, "minutes", 0) == 30, "every 30m minutes=30");
    json_free(sch);

    sch = cronjobs_parse_schedule("0 9 * * *", &err);
    CHECK(sch != NULL, "parse cron '0 9 * * *'");
    CHECK(strcmp(json_object_get_string(sch, "kind", ""), "cron") == 0, "cron kind");
    CHECK(strcmp(json_object_get_string(sch, "expr", ""), "0 9 * * *") == 0, "cron expr");
    json_free(sch);

    sch = cronjobs_parse_schedule("2026-02-03T14:00:00", &err);
    CHECK(sch != NULL, "parse timestamp");
    CHECK(strcmp(json_object_get_string(sch, "kind", ""), "once") == 0, "timestamp kind=once");
    CHECK(json_object_get_string(sch, "run_at", NULL) != NULL, "timestamp run_at set");
    json_free(sch);

    CHECK(cronjobs_parse_duration("30m") == 30, "duration 30m=30");
    CHECK(cronjobs_parse_duration("2h") == 120, "duration 2h=120");
    CHECK(cronjobs_parse_duration("1d") == 1440, "duration 1d=1440");
    CHECK(cronjobs_parse_duration("bogus") == -1, "duration bogus=-1");

    sch = cronjobs_parse_schedule("not a schedule", &err);
    CHECK(sch == NULL, "invalid schedule rejected");
    CHECK(err != NULL, "invalid schedule sets err");
    free(err);

    /* --- create_job (interval) --- */
    cronjobs_create_opts o;
    memset(&o, 0, sizeof(o));
    o.schedule = "every 30m";
    o.prompt = "Ping the user";
    o.skill = "weather";
    json_t *job = cronjobs_create_job(&o, &err);
    CHECK(job != NULL, "create interval job");
    if (job) {
        CHECK(strcmp(json_object_get_string(job, "schedule_display", ""), "every 30m") == 0,
              "schedule_display=every 30m");
        char *sk = skills_csv(job);
        CHECK(strcmp(sk, "weather") == 0, "skills=[weather]");
        free(sk);
        CHECK(json_get_bool(job, "enabled", false), "enabled=true");
        CHECK(strcmp(json_object_get_string(job, "state", ""), "scheduled") == 0, "state=scheduled");
        CHECK(strcmp(json_object_get_string(job, "deliver", ""), "local") == 0, "deliver=local");
        CHECK(json_object_get_string(job, "next_run_at", NULL) != NULL, "next_run_at computed");
    }
    const char *jid = job ? json_object_get_string(job, "id", "") : "";
    char jid_copy[64];
    snprintf(jid_copy, sizeof(jid_copy), "%s", jid);
    json_free(job);

    /* persistence: file exists + parses back */
    char jf[2048];
    snprintf(jf, sizeof(jf), "%s/cron/jobs.json", home);
    struct stat st;
    CHECK(stat(jf, &st) == 0, "jobs.json written");
    json_t *jobs = cronjobs_load_jobs(&err);
    CHECK(jobs != NULL, "load_jobs");
    CHECK(json_array_size(jobs) == 1, "one job persisted");
    json_free(jobs);

    /* --- get_job --- */
    json_t *got = cronjobs_get_job(jid_copy);
    CHECK(got != NULL, "get_job by id");
    CHECK(strcmp(json_object_get_string(got, "prompt", ""), "Ping the user") == 0, "get_job prompt");
    json_free(got);

    /* --- resume/pause via name ref --- */
    json_t *paused = cronjobs_pause_job(jid_copy, "testing");
    CHECK(paused != NULL, "pause job");
    CHECK(strcmp(json_object_get_string(paused, "state", ""), "paused") == 0, "state=paused");
    CHECK(json_object_get_string(paused, "paused_reason", NULL) != NULL, "paused_reason set");
    json_free(paused);
    json_t *resumed = cronjobs_resume_job(jid_copy);
    CHECK(resumed != NULL && strcmp(json_object_get_string(resumed, "state", ""), "scheduled") == 0,
          "resume -> scheduled");
    json_free(resumed);

    /* --- trigger (next_run_at = now) --- */
    json_t *trig = cronjobs_trigger_job(jid_copy);
    CHECK(trig != NULL, "trigger job");
    json_free(trig);

    /* --- update_job: change schedule + repeat --- */
    json_t *upd = json_object();
    json_set(upd, "schedule", json_string("every 1h"));
    json_t *rep = json_object();
    json_set(rep, "times", json_number(5));
    json_set(upd, "repeat", rep);
    json_t *updated = cronjobs_update_job(jid_copy, upd, &err);
    CHECK(updated != NULL, "update_job");
    if (updated) {
        json_t *s2 = json_object_get(updated, "schedule");
        CHECK(strcmp(json_object_get_string(s2, "kind", ""), "interval") == 0, "updated schedule kind=interval");
        CHECK((int)json_get_num(s2, "minutes", 0) == 60, "updated minutes=60");
    }
    json_free(upd);
    json_free(updated);

    /* --- immutable field guard: id --- */
    json_t *bad = json_object();
    json_set(bad, "id", json_string("hax"));
    json_t *rej = cronjobs_update_job(jid_copy, bad, &err);
    CHECK(rej == NULL, "update id rejected");
    CHECK(err != NULL, "update id error set");
    free(err); err = NULL;
    json_free(bad);

    /* --- mark_job_run + repeat completion removal --- */
    /* create a 2-shot once job */
    cronjobs_create_opts o2;
    memset(&o2, 0, sizeof(o2));
    o2.schedule = "2026-02-03T14:00:00";
    o2.prompt = "Run twice";
    o2.skill = "backup";
    o2.has_repeat = true; o2.repeat = 2;
    json_t *job2 = cronjobs_create_job(&o2, &err);
    CHECK(job2 != NULL, "create once job with repeat=2");
    const char *jid2 = json_object_get_string(job2, "id", "");
    char jid2c[64]; snprintf(jid2c, sizeof(jid2c), "%s", jid2);
    json_free(job2);

    /* Faithful ticker flow: dispatch preclaims completed, then mark_job_run. */
    cronjobs_claim_dispatch(jid2c);
    cronjobs_mark_job_run(jid2c, true, NULL, NULL);
    json_t *after1 = cronjobs_get_job(jid2c);
    CHECK(after1 != NULL, "job2 still present after 1 run");
    json_t *r1 = json_object_get(after1, "repeat");
    CHECK((int)json_get_num(r1, "completed", 0) == 1, "completed=1 after run");
    json_free(after1);
    cronjobs_claim_dispatch(jid2c);
    cronjobs_mark_job_run(jid2c, true, NULL, NULL);
    json_t *gone = cronjobs_get_job(jid2c);
    CHECK(gone == NULL, "job2 removed after reaching repeat limit");
    if (gone) json_free(gone);

    /* --- mark_job_run failure on recurring advances next_run + keeps job --- */
    cronjobs_create_opts o3;
    memset(&o3, 0, sizeof(o3));
    o3.schedule = "every 15m";
    o3.prompt = "Health check";
    o3.skill = "monitor";
    json_t *job3 = cronjobs_create_job(&o3, &err);
    const char *jid3 = json_object_get_string(job3, "id", "");
    char jid3c[64]; snprintf(jid3c, sizeof(jid3c), "%s", jid3);
    json_free(job3);
    cronjobs_mark_job_run(jid3c, false, "boom", "deliver failed");
    json_t *j3 = cronjobs_get_job(jid3c);
    CHECK(j3 != NULL, "recurring job kept after failure");
    CHECK(strcmp(json_object_get_string(j3, "last_status", ""), "error") == 0, "last_status=error");
    CHECK(json_object_get_string(j3, "last_error", NULL) != NULL, "last_error set");
    CHECK(json_object_get_string(j3, "last_delivery_error", NULL) != NULL, "delivery error set");
    CHECK(json_object_get_string(j3, "next_run_at", NULL) != NULL, "next_run_at kept (scheduled)");
    json_free(j3);

    /* --- due jobs: set next_run_at far in past via trigger, then get_due_jobs --- */
    cronjobs_trigger_job(jid3c); /* next_run_at = now */
    json_t *due = cronjobs_get_due_jobs();
    CHECK(due != NULL, "get_due_jobs returns array");
    bool found = false;
    for (size_t i = 0; i < json_array_size(due); i++) {
        if (strcmp(json_object_get_string(json_array_get(due, i), "id", ""), jid3c) == 0)
            found = true;
    }
    CHECK(found, "triggered job is due");
    json_free(due);

    /* --- output save + retention --- */
    char *outpath = cronjobs_save_job_output(jid3c, "# Hello\noutput body");
    CHECK(outpath != NULL, "save_job_output");
    if (outpath) {
        struct stat os;
        CHECK(stat(outpath, &os) == 0, "output file exists");
        free(outpath);
    }

    /* --- skill refs: referenced + rewrite --- */
    json_t *refs = cronjobs_referenced_skill_names();
    CHECK(refs != NULL, "referenced_skill_names");
    bool has_weather = false;
    for (size_t i = 0; i < json_array_size(refs); i++)
        if (strcmp(json_string_value(json_array_get(refs, i)), "weather") == 0) has_weather = true;
    CHECK(has_weather, "weather referenced");
    json_free(refs);

    json_t *consolidated = json_object();
    json_set(consolidated, "weather", json_string("weather-pro"));
    json_t *report = cronjobs_rewrite_skill_refs(consolidated, NULL);
    CHECK(report != NULL, "rewrite_skill_refs");
    CHECK(json_get_num(report, "jobs_updated", 0) >= 1, "jobs_updated >= 1");
    json_free(report);
    json_free(consolidated);
    /* verify weather -> weather-pro on our interval job */
    json_t *j_updated = cronjobs_get_job(jid_copy);
    char *sk2 = skills_csv(j_updated);
    CHECK(strcmp(sk2, "weather-pro") == 0, "skill rewritten to weather-pro");
    free(sk2);
    json_free(j_updated);

    /* --- remove_job --- */
    CHECK(cronjobs_remove_job(jid3c), "remove recurring job");
    CHECK(cronjobs_get_job(jid3c) == NULL, "job gone after remove");
    CHECK(cronjobs_remove_job(jid_copy), "remove interval job");

    /* --- empty load when no store --- */
    rmrf(jf);
    json_t *empty = cronjobs_load_jobs(&err);
    CHECK(empty != NULL && json_array_size(empty) == 0, "empty load when no store");
    json_free(empty);

    printf("\n%s: %d passed, %d failed\n", g_fail ? "FAILED" : "ALL TESTS PASSED", g_pass, g_fail);
    rmrf(home);
    return g_fail ? 1 : 0;
}
