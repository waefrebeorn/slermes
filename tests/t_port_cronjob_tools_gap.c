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

    /* ---- honest-NA behaviors ---- */
    cronjob_notify_provider_jobs_changed_safe();
    printf("{\"fn\":\"notify\",\"out\":\"noop\"}\n");

    json_t *ej = cronjob_execute_job_now(NULL);
    printf("{\"fn\":\"execnow\",\"has_error\":%s}\n",
           json_obj_get(ej, "error") ? "true" : "false");
    json_free(ej);

    json_t *cd = cronjob_dispatch(NULL);
    printf("{\"fn\":\"dispatch\",\"has_error\":%s}\n",
           json_obj_get(cd, "error") ? "true" : "false");
    json_free(cd);

    return 0;
}
