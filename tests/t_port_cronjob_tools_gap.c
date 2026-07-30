/*
 * t_port_cronjob_tools_gap.c — residual-façade oracle harness (v558).
 * Tests the 7 previously-missing cronjob_tools.py functions:
 *   4 implemented faithfully: check_cronjob_requirements, validate_script_path,
 *                            format_job, validate_base_url
 *   3 honest-NA (assert honest behavior, never fake-success):
 *                            notify_provider_jobs_changed_safe (no-op),
 *                            execute_job_now (error), cronjob_dispatch (error)
 *
 * Output: one JSON object per case on stdout, fed to sta_oracle_cronjob_tools_gap.py
 * which recomputes against LIVE Python and asserts.
 *
 * JSON fields are extracted with json_obj_get/json_get_str (not json_serialize)
 * to stay consistent with the project's json ABI.
 */
#include "port_cronjob_tools.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* minimal JSON string escaper for harness output */
static void esc_print(const char *s) {
    if (!s) { fputs("null", stdout); return; }
    putchar('"');
    for (const char *q = s; *q; q++) {
        if (*q == '"' || *q == '\\') { putchar('\\'); putchar(*q); }
        else if (*q == '\n') { fputs("\\n", stdout); }
        else putchar(*q);
    }
    putchar('"');
}

/* print a json string field (or null) */
static void esc_json_str(const json_t *obj, const char *key) {
    const char *v = json_get_str(obj, key, NULL);
    esc_print(v);
}

int main(void)
{
    /* ---- check_cronjob_requirements (env-driven) ---- */
    unsetenv("HERMES_INTERACTIVE");
    unsetenv("HERMES_GATEWAY_SESSION");
    unsetenv("HERMES_EXEC_ASK");
    printf("{\"fn\":\"req\",\"env\":\"none\",\"out\":%s}\n",
           cronjob_check_cronjob_requirements() ? "true" : "false");
    setenv("HERMES_INTERACTIVE", "1", 1);
    printf("{\"fn\":\"req\",\"env\":\"interactive=1\",\"out\":%s}\n",
           cronjob_check_cronjob_requirements() ? "true" : "false");
    unsetenv("HERMES_INTERACTIVE");
    setenv("HERMES_GATEWAY_SESSION", "true", 1);
    printf("{\"fn\":\"req\",\"env\":\"gateway=true\",\"out\":%s}\n",
           cronjob_check_cronjob_requirements() ? "true" : "false");
    setenv("HERMES_GATEWAY_SESSION", "no", 1);
    setenv("HERMES_EXEC_ASK", "off", 1);
    printf("{\"fn\":\"req\",\"env\":\"falsey\",\"out\":%s}\n",
           cronjob_check_cronjob_requirements() ? "true" : "false");
    unsetenv("HERMES_GATEWAY_SESSION");
    unsetenv("HERMES_EXEC_ASK");

    /* ---- validate_cron_script_path ---- */
    const char *sp_cases[][2] = {
        {"scripts/myjob.sh", "valid"},
        {"/abs/path.sh", "abs"},
        {"~/cripts/x.sh", "home"},
        {"C:/windows.sh", "drive"},
        {"../escape.sh", "traversal"},
        {"ok/../../bad.sh", "traversal2"},
        {"", "empty"},
        {NULL, NULL}
    };
    for (int i = 0; sp_cases[i][0]; i++) {
        char *r = cronjob_validate_cron_script_path(sp_cases[i][0]);
        printf("{\"fn\":\"script\",\"in\":");
        esc_print(sp_cases[i][0]);
        printf(",\"out\":");
        esc_print(r);
        printf("}\n");
        free(r);
    }

    /* ---- validate_cron_base_url ---- */
    struct { const char *prov; const char *bu; } bu_cases[] = {
        {NULL, NULL},
        {"openai", "https://evil.example.com"},
        {"custom", "https://my.endpoint.com"},
        {"custom:acme", "https://acme.endpoint.com"},
        {NULL, "https://x.com"},
        {"openai", NULL},
    };
    int nb = sizeof(bu_cases)/sizeof(bu_cases[0]);
    for (int i = 0; i < nb; i++) {
        char *r = cronjob_validate_cron_base_url(bu_cases[i].prov, bu_cases[i].bu);
        printf("{\"fn\":\"baseurl\",\"prov\":");
        esc_print(bu_cases[i].prov);
        printf(",\"bu\":");
        esc_print(bu_cases[i].bu);
        printf(",\"out\":");
        esc_print(r);
        printf("}\n");
        free(r);
    }

    /* ---- format_job (extract key fields) ---- */
    json_t *job = json_object();
    json_set(job, "prompt", json_string("This is a very long prompt that exceeds one hundred characters to force the preview truncation logic to kick in properly and verify the ellipsis is appended at the cut point."));
    json_set(job, "id", json_string("job-123"));
    json_set(job, "skill", json_string("daily_report"));
    json_set(job, "model", json_string("gpt-4"));
    json_set(job, "provider", json_string("openai"));
    json_set(job, "schedule_display", json_string("0 9 * * *"));
    json_set(job, "repeat", json_string("daily"));
    json_set(job, "deliver", json_string("telegram"));
    json_set(job, "enabled", json_bool(1));
    json_set(job, "state", json_string("scheduled"));
    json_t *fj = cronjob_format_job(job);
    printf("{\"fn\":\"format\",\"fields\":{");
    printf("\"job_id\":"); esc_json_str(fj, "job_id");
    printf(",\"name\":"); esc_json_str(fj, "name");
    printf(",\"skill\":"); esc_json_str(fj, "skill");
    printf(",\"model\":"); esc_json_str(fj, "model");
    printf(",\"provider\":"); esc_json_str(fj, "provider");
    printf(",\"schedule\":"); esc_json_str(fj, "schedule");
    printf(",\"deliver\":"); esc_json_str(fj, "deliver");
    printf(",\"enabled\":%s", json_is_true(json_obj_get(fj, "enabled")) ? "true" : "false");
    printf(",\"state\":"); esc_json_str(fj, "state");
    printf(",\"prompt_preview_len\":%zu", strlen(json_get_str(fj, "prompt_preview", "")));
    printf("}}\n");
    json_free(fj);
    json_free(job);

    /* ---- REAL dispatcher + execute_job_now lifecycle (contract assertions) ----
     * These delegate to the real C scheduler (cron_cmd_handler over g_cron_store,
     * firing via run_one_job). We drive a full create -> list -> run-now -> remove
     * lifecycle in an isolated SLERMES_HOME temp dir and assert the behaviour
     * contract (LIVE Python uses a different store backend, so we assert shape,
     * not byte-identical JSON — same rule as the cron oracle). */

    /* notify: real best-effort provider notification, returns void, must not crash */
    cronjob_notify_provider_jobs_changed_safe();
    printf("{\"fn\":\"notify\",\"out\":\"ok\"}\n");

    /* Create a job via the dispatcher. Use a harmless command. */
    json_t *cargs = json_object();
    json_set(cargs, "action", json_string("add"));
    json_set(cargs, "name", json_string("gap_test_job"));
    json_set(cargs, "schedule", json_string("*/5 * * * *"));
    json_set(cargs, "command", json_string("true"));
    json_t *cres = cronjob_dispatch(cargs);
    json_free(cargs);
    const char *cstatus = json_get_str(cres, "status", "");
    printf("{\"fn\":\"dispatch_add\",\"status\":");
    esc_print(cstatus);
    printf(",\"has_error\":%s}\n", json_obj_get(cres, "error") ? "true" : "false");
    json_free(cres);

    /* list should include the created job */
    json_t *largs = json_object();
    json_set(largs, "action", json_string("list"));
    json_t *lres = cronjob_dispatch(largs);
    json_free(largs);
    json_t *jobs = json_obj_get(lres, "jobs");
    int found = 0, count = 0;
    if (jobs && json_is_array(jobs)) {
        count = json_array_size(jobs);
        for (int i = 0; i < count; i++) {
            json_t *j = json_array_get(jobs, i);
            const char *nm = json_get_str(j, "name", "");
            if (strcmp(nm, "gap_test_job") == 0) found = 1;
        }
    }
    printf("{\"fn\":\"dispatch_list\",\"count\":%d,\"found\":%s}\n",
           count, found ? "true" : "false");
    json_free(lres);

    /* execute_job_now on the real job -> claimed+success true (runs `true`) */
    json_t *jref = json_object();
    json_set(jref, "id", json_string("gap_test_job"));
    json_set(jref, "name", json_string("gap_test_job"));
    json_t *ej = cronjob_execute_job_now(jref);
    json_free(jref);
    printf("{\"fn\":\"execnow_real\",\"claimed\":%s,\"success\":%s}\n",
           json_is_true(json_obj_get(ej, "claimed")) ? "true" : "false",
           json_is_true(json_obj_get(ej, "success")) ? "true" : "false");
    json_free(ej);

    /* execute_job_now on a MISSING job -> claimed=false (nothing fired) */
    json_t *jmiss = json_object();
    json_set(jmiss, "id", json_string("no_such_job_xyz"));
    json_set(jmiss, "name", json_string("no_such_job_xyz"));
    json_t *ejm = cronjob_execute_job_now(jmiss);
    json_free(jmiss);
    printf("{\"fn\":\"execnow_missing\",\"claimed\":%s}\n",
           json_is_true(json_obj_get(ejm, "claimed")) ? "true" : "false");
    json_free(ejm);

    /* execute_job_now with no id/name -> claimed=false + error */
    json_t *jempty = json_object();
    json_t *eje = cronjob_execute_job_now(jempty);
    json_free(jempty);
    printf("{\"fn\":\"execnow_noid\",\"claimed\":%s,\"has_error\":%s}\n",
           json_is_true(json_obj_get(eje, "claimed")) ? "true" : "false",
           json_obj_get(eje, "error") ? "true" : "false");
    json_free(eje);

    /* remove the job -> status removed */
    json_t *rargs = json_object();
    json_set(rargs, "action", json_string("remove"));
    json_set(rargs, "name", json_string("gap_test_job"));
    json_t *rres = cronjob_dispatch(rargs);
    json_free(rargs);
    printf("{\"fn\":\"dispatch_remove\",\"status\":");
    esc_print(json_get_str(rres, "status", ""));
    printf("}\n");
    json_free(rres);

    return 0;
}
