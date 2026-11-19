/* Oracle harness for port_jobs_pure.c — emits one JSONL record per case:
 * {"func":<c_name>,"in":{...},"out":<what the C port produced>}.
 * The oracle (tests/oracle_jobs_pure.py) computes the LIVE cron/jobs.py
 * reference and diffs. Include path covers src/cron (bare-name include). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_jobs_pure.c"

typedef struct { const char *func; const char *in_json; } jp_case;

static const jp_case cases[] = {
    {"cronjobs_pure_classify_dispatch_lateness", "{\"lateness\":0,\"grace\":120}"},
    {"cronjobs_pure_classify_dispatch_lateness", "{\"lateness\":250,\"grace\":7200}"},
    {"cronjobs_pure_classify_dispatch_lateness", "{\"lateness\":300.5,\"grace\":7200}"},
    {"cronjobs_pure_classify_dispatch_lateness", "{\"lateness\":7200,\"grace\":7200}"},
    {"cronjobs_pure_classify_dispatch_lateness", "{\"lateness\":7200.1,\"grace\":7200}"},
    {"cronjobs_pure_classify_dispatch_lateness", "{\"lateness\":-5,\"grace\":120}"},
    {"cronjobs_pure_interval_schedule", "{\"minutes\":1}"},
    {"cronjobs_pure_interval_schedule", "{\"minutes\":30}"},
    {"cronjobs_pure_interval_schedule", "{\"minutes\":1440}"},
    {"cronjobs_pure_interval_schedule", "{\"minutes\":0}"},
    {"cronjobs_pure_is_recoverable_error_job", "{\"job\":{\"state\":\"error\",\"schedule\":{\"kind\":\"cron\",\"expr\":\"0 9 * * *\"}}}"},
    {"cronjobs_pure_is_recoverable_error_job", "{\"job\":{\"state\":\"error\",\"schedule\":{\"kind\":\"interval\",\"minutes\":5}}}"},
    {"cronjobs_pure_is_recoverable_error_job", "{\"job\":{\"state\":\"error\",\"schedule\":{\"kind\":\"once\",\"run_at\":\"2026-01-01T00:00:00+00:00\"}}}"},
    {"cronjobs_pure_is_recoverable_error_job", "{\"job\":{\"state\":\"completed\",\"schedule\":{\"kind\":\"cron\"}}}"},
    {"cronjobs_pure_is_recoverable_error_job", "{\"job\":{\"state\":\"error\"}}"},
    {"cronjobs_pure_is_recoverable_error_job", "{\"job\":{\"state\":\"error\",\"schedule\":null}}"},
    {"cronjobs_pure_parse_clock_time", "{\"text\":\" 9 am \"}"},
    {"cronjobs_pure_parse_clock_time", "{\"text\":\"12am\"}"},
    {"cronjobs_pure_parse_clock_time", "{\"text\":\"14:00\"}"},
    {"cronjobs_pure_parse_clock_time", "{\"text\":\"noon\"}"},
    {"cronjobs_pure_parse_clock_time", "{\"text\":\"13pm\"}"},
    {"cronjobs_pure_parse_clock_time", "{\"text\":\"9:60\"}"},
    {"cronjobs_pure_natural_every_to_cron", "{\"rest\":\"monday 9am\"}"},
    {"cronjobs_pure_natural_every_to_cron", "{\"rest\":\"weekday at 9am\"}"},
    {"cronjobs_pure_natural_every_to_cron", "{\"rest\":\"monday, wednesday at 9am\"}"},
    {"cronjobs_pure_natural_every_to_cron", "{\"rest\":\"weekend 10:30pm\"}"},
    {"cronjobs_pure_natural_every_to_cron", "{\"rest\":\"monday and friday 7\"}"},
    {"cronjobs_pure_natural_every_to_cron", "{\"rest\":\"funday 9am\"}"},
    {"cronjobs_pure_normalize_enabled_toolsets", "{\"value\":null}"},
    {"cronjobs_pure_normalize_enabled_toolsets", "{\"value\":[]}"},
    {"cronjobs_pure_normalize_enabled_toolsets", "{\"value\":[\"web\",\" files \",\"\"]}"},
    {"cronjobs_pure_normalize_enabled_toolsets", "{\"value\":[\"a\",\"a\"]}"},
    {"cronjobs_pure_normalize_enabled_toolsets", "{\"value\":\"web\"}"},
    {"cronjobs_pure_normalize_enabled_toolsets", "{\"value\":[null,3,true,\" x \"]}"},
    {"cronjobs_pure_complete_job_record", "{\"job\":{\"id\":\"j1\",\"enabled\":true,\"state\":\"scheduled\",\"next_run_at\":\"2026-01-01T00:00:00+00:00\"}}"},
    {"cronjobs_pure_complete_job_record", "{\"job\":{}}"},
    {"cronjobs_pure_complete_job_record", "{\"job\":{\"state\":\"error\",\"paused_at\":\"x\",\"enabled\":false}}"},
    {"cronjobs_pure_activate_job_record", "{\"job\":{\"id\":\"j2\",\"enabled\":false,\"state\":\"paused\",\"paused_at\":\"t\",\"paused_reason\":\"r\"}}"},
    {"cronjobs_pure_activate_job_record", "{\"job\":{}}"},
    {"cronjobs_pure_activate_job_record", "{\"job\":{\"state\":\"completed\"}}"},
    {"cronjobs_pure_oneshot_past_grace_error", "{\"run_at\":\"2026-01-01T00:00:00+00:00\"}"},
    {"cronjobs_pure_oneshot_past_grace_error", "{\"run_at\":\"\"}"},
    {"cronjobs_pure_oneshot_past_grace_error", "{\"run_at\":\"yesterday\"}"},
    {"cronjobs_pure_reject_terminal_activation", "{\"job\":{\"state\":\"completed\",\"name\":\"Done\"},\"updated\":{\"enabled\":true},\"job_id\":\"j1\"}"},
    {"cronjobs_pure_reject_terminal_activation", "{\"job\":{\"state\":\"error\",\"schedule\":{\"kind\":\"cron\"}},\"updated\":{\"enabled\":true},\"job_id\":\"j2\"}"},
    {"cronjobs_pure_reject_terminal_activation", "{\"job\":{\"state\":\"completed\"},\"updated\":{\"state\":\"completed\"},\"job_id\":\"j3\"}"},
    {"cronjobs_pure_reject_terminal_activation", "{\"job\":{\"state\":\"completed\"},\"updated\":{\"next_run_at\":\"2026-01-01T00:00:00+00:00\"},\"job_id\":\"j4\"}"},
    {"cronjobs_pure_reject_terminal_activation", "{\"job\":{\"state\":\"scheduled\"},\"updated\":{\"enabled\":true},\"job_id\":\"j5\"}"},
    {"cronjobs_pure_reject_terminal_activation", "{\"job\":{\"state\":\"completed\",\"name\":null},\"updated\":{\"enabled\":1},\"job_id\":\"j6\"}"},
    {"cronjobs_pure_elapsed_seconds", "{\"later\":\"2026-01-01T12:00:00+00:00\",\"earlier\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_elapsed_seconds", "{\"later\":\"2026-01-01T12:00:00+02:00\",\"earlier\":\"2026-01-01T09:30:00-01:00\"}"},
    {"cronjobs_pure_elapsed_seconds", "{\"later\":\"2026-01-01T00:00:01.500000+00:00\",\"earlier\":\"2026-01-01T00:00:00+00:00\"}"},
    {"cronjobs_pure_elapsed_seconds", "{\"later\":\"2024-03-01T00:00:00+00:00\",\"earlier\":\"2024-02-28T00:00:00+00:00\"}"},
    {"cronjobs_pure_elapsed_seconds", "{\"later\":\"2026-01-01T10:00:00+00:00\",\"earlier\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_instant_after", "{\"left\":\"2026-01-01T12:00:00+00:00\",\"right\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_instant_after", "{\"left\":\"2026-01-01T12:00:00+02:00\",\"right\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_instant_after", "{\"left\":\"2026-01-01T09:00:00+00:00\",\"right\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_instant_after", "{\"left\":\"2026-01-01T10:00:00.000001+00:00\",\"right\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_instant_at_or_before", "{\"left\":\"2026-01-01T10:00:00+00:00\",\"right\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_instant_at_or_before", "{\"left\":\"2026-01-01T09:00:00+00:00\",\"right\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_instant_at_or_before", "{\"left\":\"2026-01-01T11:00:00+00:00\",\"right\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_instant_at_or_before", "{\"left\":\"2026-01-01T12:00:00+02:00\",\"right\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_instant_before", "{\"left\":\"2026-01-01T09:00:00+00:00\",\"right\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_instant_before", "{\"left\":\"2026-01-01T10:00:00+00:00\",\"right\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_instant_before", "{\"left\":\"2026-01-01T11:00:00+00:00\",\"right\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_instant_before", "{\"left\":\"2026-01-01T12:00:00+02:00\",\"right\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_seconds_after", "{\"dt\":\"2026-01-01T23:59:30+05:30\",\"seconds\":45}"},
    {"cronjobs_pure_seconds_after", "{\"dt\":\"2026-01-01T00:00:00.250000Z\",\"seconds\":0.5}"},
    {"cronjobs_pure_seconds_after", "{\"dt\":\"2026-03-01T00:00:00+00:00\",\"seconds\":-86400}"},
    {"cronjobs_pure_seconds_after", "{\"dt\":\"2025-12-31T23:59:59-02:00\",\"seconds\":1}"},
    {"cronjobs_pure_seconds_after", "{\"dt\":\"2026-06-15T12:00:00+00:00\",\"seconds\":0}"},
    {"cronjobs_pure_timezone_offset_mismatch", "{\"stored\":\"2026-01-01T00:00:00+02:00\",\"current\":\"2026-06-01T00:00:00+02:00\"}"},
    {"cronjobs_pure_timezone_offset_mismatch", "{\"stored\":\"2026-01-01T00:00:00+02:00\",\"current\":\"2026-01-01T00:00:00+03:00\"}"},
    {"cronjobs_pure_timezone_offset_mismatch", "{\"stored\":\"2026-01-01T00:00:00\",\"current\":\"2026-01-01T00:00:00+02:00\"}"},
    {"cronjobs_pure_timezone_offset_mismatch", "{\"stored\":\"2026-01-01T00:00:00Z\",\"current\":\"2026-01-01T00:00:00+00:00\"}"},
    {"cronjobs_pure_timezone_offset_mismatch", "{\"stored\":\"2026-01-01T00:00:00\",\"current\":\"2026-01-01T00:00:00\"}"},
    {"cronjobs_pure_stored_wall_clock_is_future", "{\"stored\":\"2026-01-01T21:00:00+10:00\",\"current\":\"2026-01-01T13:00:00+02:00\"}"},
    {"cronjobs_pure_stored_wall_clock_is_future", "{\"stored\":\"2026-01-01T12:00:00+02:00\",\"current\":\"2026-01-01T12:00:00+00:00\"}"},
    {"cronjobs_pure_stored_wall_clock_is_future", "{\"stored\":\"2026-01-01T09:00:00+00:00\",\"current\":\"2026-01-01T10:00:00+00:00\"}"},
    {"cronjobs_pure_stored_wall_clock_is_future", "{\"stored\":\"2026-01-01T00:00:00.000001+00:00\",\"current\":\"2026-01-01T00:00:00+00:00\"}"},
    {"cronjobs_pure_stored_wall_clock_is_future", "{\"stored\":\"2026-01-02T00:00:00\",\"current\":\"2026-01-01T23:59:59\"}"},
};

static json_t *err_obj(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg ? msg : ""));
    return o;
}

static json_t *run_case(const char *func, const json_t *in) {
    if (strcmp(func, "cronjobs_pure_classify_dispatch_lateness") == 0) {
        return json_string(cronjobs_pure_classify_dispatch_lateness(
            json_get_num(in, "lateness", 0.0), json_get_num(in, "grace", 0.0)));
    }
    if (strcmp(func, "cronjobs_pure_interval_schedule") == 0) {
        return cronjobs_pure_interval_schedule((long long)json_get_num(in, "minutes", 0.0));
    }
    if (strcmp(func, "cronjobs_pure_is_recoverable_error_job") == 0) {
        return json_bool(cronjobs_pure_is_recoverable_error_job(json_obj_get(in, "job")));
    }
    if (strcmp(func, "cronjobs_pure_parse_clock_time") == 0) {
        int h = 0, m = 0;
        if (!cronjobs_pure_parse_clock_time(json_get_str(in, "text", ""), &h, &m)) return json_null();
        json_t *a = json_array();
        json_append(a, json_number(h));
        json_append(a, json_number(m));
        return a;
    }
    if (strcmp(func, "cronjobs_pure_natural_every_to_cron") == 0) {
        char *r = cronjobs_pure_natural_every_to_cron(json_get_str(in, "rest", ""));
        json_t *o = r ? json_string(r) : json_null();
        free(r);
        return o;
    }
    if (strcmp(func, "cronjobs_pure_normalize_enabled_toolsets") == 0) {
        char *err = NULL;
        json_t *r = cronjobs_pure_normalize_enabled_toolsets(json_obj_get(in, "value"), &err);
        if (err) { json_t *o = err_obj(err); free(err); return o; }
        return r ? r : json_null();
    }
    if (strcmp(func, "cronjobs_pure_complete_job_record") == 0) {
        json_t *job = json_copy(json_obj_get(in, "job"));
        cronjobs_pure_complete_job_record(job);
        return job;
    }
    if (strcmp(func, "cronjobs_pure_activate_job_record") == 0) {
        json_t *job = json_copy(json_obj_get(in, "job"));
        cronjobs_pure_activate_job_record(job);
        return job;
    }
    if (strcmp(func, "cronjobs_pure_oneshot_past_grace_error") == 0) {
        char *msg = cronjobs_pure_oneshot_past_grace_error(json_get_str(in, "run_at", ""));
        json_t *o = err_obj(msg);
        free(msg);
        return o;
    }
    if (strcmp(func, "cronjobs_pure_reject_terminal_activation") == 0) {
        char *msg = cronjobs_pure_reject_terminal_activation(
            json_obj_get(in, "job"), json_obj_get(in, "updated"), json_get_str(in, "job_id", ""));
        if (!msg) return json_null();
        json_t *o = err_obj(msg);
        free(msg);
        return o;
    }
    if (strcmp(func, "cronjobs_pure_elapsed_seconds") == 0) {
        return json_number(cronjobs_pure_elapsed_seconds(
            json_get_str(in, "later", ""), json_get_str(in, "earlier", "")));
    }
    if (strcmp(func, "cronjobs_pure_instant_after") == 0) {
        return json_bool(cronjobs_pure_instant_after(
            json_get_str(in, "left", ""), json_get_str(in, "right", "")));
    }
    if (strcmp(func, "cronjobs_pure_instant_at_or_before") == 0) {
        return json_bool(cronjobs_pure_instant_at_or_before(
            json_get_str(in, "left", ""), json_get_str(in, "right", "")));
    }
    if (strcmp(func, "cronjobs_pure_instant_before") == 0) {
        return json_bool(cronjobs_pure_instant_before(
            json_get_str(in, "left", ""), json_get_str(in, "right", "")));
    }
    if (strcmp(func, "cronjobs_pure_seconds_after") == 0) {
        char *r = cronjobs_pure_seconds_after(json_get_str(in, "dt", ""), json_get_num(in, "seconds", 0.0));
        json_t *o = r ? json_string(r) : json_null();
        free(r);
        return o;
    }
    if (strcmp(func, "cronjobs_pure_timezone_offset_mismatch") == 0) {
        return json_bool(cronjobs_pure_timezone_offset_mismatch(
            json_get_str(in, "stored", ""), json_get_str(in, "current", "")));
    }
    if (strcmp(func, "cronjobs_pure_stored_wall_clock_is_future") == 0) {
        return json_bool(cronjobs_pure_stored_wall_clock_is_future(
            json_get_str(in, "stored", ""), json_get_str(in, "current", "")));
    }
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    size_t n = sizeof cases / sizeof cases[0];
    for (size_t i = 0; i < n; i++) {
        char *err = NULL;
        json_t *in = json_parse(cases[i].in_json, &err);
        if (!in) {
            fprintf(stderr, "case %zu parse error: %s\n", i, err ? err : "?");
            free(err);
            return 2;
        }
        json_t *out = run_case(cases[i].func, in);
        if (!out) {
            fprintf(stderr, "case %zu: unknown func %s\n", i, cases[i].func);
            json_free(in);
            return 2;
        }
        json_t *rec = json_object();
        json_set(rec, "func", json_string(cases[i].func));
        json_set(rec, "in", in);
        json_set(rec, "out", out);
        char *ser = json_serialize(rec);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(rec);
    }
    return 0;
}
