/*
 * scheduler_failure_copy.c — C port of upstream Hermes cron/scheduler_failure_copy.py.
 *
 * PoP: w14_schedulerf_f67f6c_output_dir @ cron/scheduler_failure_copy.py:cron_output_dir_display
 * PoP: w14_schedulerf_f67f6c_generic @ cron/scheduler_failure_copy.py:generic_failure_notice
 * PoP: w14_schedulerf_f67f6c_script_timeout @ cron/scheduler_failure_copy.py:script_timeout_notice
 * PoP: w14_schedulerf_f67f6c_inactivity @ cron/scheduler_failure_copy.py:inactivity_notice
 * PoP: w14_schedulerf_f67f6c_blocked_config @ cron/scheduler_failure_copy.py:blocked_config_notice
 * PoP: w14_schedulerf_f67f6c_provider_notice @ cron/scheduler_failure_copy.py:provider_failure_notice
 */
#include "noc_w14_schedulerf_f67f6c.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

char *w14_schedulerf_f67f6c_output_dir(const char *job_id, const char *home) {
    size_t n = strlen(home ? home : "") + strlen("/cron/output/") +
               strlen(job_id ? job_id : "") + strlen("/") + 1;
    char *p = (char *)malloc(n);
    if (!p) return NULL;
    snprintf(p, n, "%s/cron/output/%s/", home ? home : "", job_id ? job_id : "");
    return p;
}

/* Python str.format for the {job_id} / {relogin} placeholders used here. */
static char *render(const char *tmpl, const char *job_id, const char *relogin) {
    size_t cap = strlen(tmpl) * 2 + strlen(job_id ? job_id : "") * 4 +
                 strlen(relogin ? relogin : "") * 4 + 64;
    char *out = (char *)malloc(cap);
    size_t i = 0, k = 0;
    if (!out) return NULL;
    while (tmpl[i]) {
        if (!strncmp(tmpl + i, "{job_id}", 8)) {
            memcpy(out + k, job_id ? job_id : "", strlen(job_id ? job_id : ""));
            k += strlen(job_id ? job_id : "");
            i += 8;
        } else if (!strncmp(tmpl + i, "{relogin}", 9)) {
            memcpy(out + k, relogin ? relogin : "", strlen(relogin ? relogin : ""));
            k += strlen(relogin ? relogin : "");
            i += 9;
        } else {
            out[k++] = tmpl[i++];
        }
    }
    out[k] = '\0';
    return out;
}

char *w14_schedulerf_f67f6c_generic(const char *job_name, const char *job_id,
                                    const char *cleaned_error, const char *home) {
    char *dir = w14_schedulerf_f67f6c_output_dir(job_id, home);
    size_t cap = 512 + strlen(job_name ? job_name : "") * 2 + strlen(job_id ? job_id : "") * 8 +
                 strlen(cleaned_error ? cleaned_error : "") + strlen(dir ? dir : "");
    char *p = (char *)malloc(cap);
    if (!p) { free(dir); return NULL; }
    snprintf(p, cap,
        "⚠️ Cron '%s' failed: %s. See the full run with `hermes cron runs %s` "
        "(output saved under %s); run it again with `hermes cron run %s`, edit it with "
        "`hermes cron edit %s`, or pause it with `hermes cron pause %s`.",
        job_name ? job_name : "", cleaned_error ? cleaned_error : "",
        job_id ? job_id : "", dir ? dir : "", job_id ? job_id : "",
        job_id ? job_id : "", job_id ? job_id : "");
    free(dir);
    return p;
}

char *w14_schedulerf_f67f6c_script_timeout(const char *job_name, const char *job_id, const char *home) {
    char *dir = w14_schedulerf_f67f6c_output_dir(job_id, home);
    size_t cap = 512 + strlen(job_name ? job_name : "") + strlen(job_id ? job_id : "") * 6 +
                 strlen(dir ? dir : "");
    char *p = (char *)malloc(cap);
    if (!p) { free(dir); return NULL; }
    snprintf(p, cap,
        "⚠️ Cron '%s' failed: its script timed out. No model was invoked. Check the "
        "script's output under %s or `hermes cron runs %s`, then run it again with "
        "`hermes cron run %s`.",
        job_name ? job_name : "", dir ? dir : "", job_id ? job_id : "", job_id ? job_id : "");
    free(dir);
    return p;
}

char *w14_schedulerf_f67f6c_inactivity(const char *job_name, const char *job_id, const char *home) {
    char *dir = w14_schedulerf_f67f6c_output_dir(job_id, home);
    size_t cap = 640 + strlen(job_name ? job_name : "") + strlen(job_id ? job_id : "") * 6 +
                 strlen(dir ? dir : "");
    char *p = (char *)malloc(cap);
    if (!p) { free(dir); return NULL; }
    snprintf(p, cap,
        "⚠️ Cron '%s' failed: the job stalled — it stopped doing anything for too long "
        "and was cut off. Check what it was doing in the saved output under %s "
        "(`hermes cron runs %s`), then run it again with `hermes cron run %s`.",
        job_name ? job_name : "", dir ? dir : "", job_id ? job_id : "", job_id ? job_id : "");
    free(dir);
    return p;
}

char *w14_schedulerf_f67f6c_blocked_config(const char *job_name, const char *reason) {
    char *r = dupstr(reason ? reason : "");
    size_t len;
    size_t cap;
    char *p;
    if (!r) return NULL;
    len = strlen(r);
    while (len && isspace((unsigned char)r[len - 1])) r[--len] = '\0';  /* rstrip + new NUL */
    if (len && r[len - 1] != '.' && r[len - 1] != '!' && r[len - 1] != '?') {
        r[len] = '.';
        r[len + 1] = '\0';
    }
    cap = 512 + strlen(job_name ? job_name : "") * 2 + strlen(r);
    p = (char *)malloc(cap);
    if (p) {
        snprintf(p, cap,
            "⛔ Cron '%s' did not run: %s Nothing was charged. Hermes will try again at "
            "the next scheduled time and will not repeat this alert; check with "
            "`hermes cron doctor`.",
            job_name ? job_name : "", r);
    }
    free(r);
    return p;
}

static const char *action_for(const char *reason) {
    if (!strcmp(reason, "billing") || !strcmp(reason, "billing_unverified"))
        return "Top up or wait for the limit to reset, or pin another provider with "
               "`hermes cron edit {job_id} --provider <name>`.";
    if (!strcmp(reason, "auth") || !strcmp(reason, "auth_permanent"))
        return "Sign in again with /login (or `{relogin}` in a terminal), or pin a working "
               "provider with `hermes cron edit {job_id} --provider <name>`, then "
               "`hermes cron run {job_id}` to retry.";
    if (!strcmp(reason, "model_not_found"))
        return "Pick another model with `hermes cron edit {job_id} --model <name>`.";
    if (!strcmp(reason, "upstream_blocked"))
        return "A firewall in front of the provider blocked the request (not your key): set "
               "a User-Agent via `extra_headers` on the provider's custom_providers entry, "
               "or pin another provider with `hermes cron edit {job_id} --provider <name>`.";
    if (!strcmp(reason, "context_overflow") || !strcmp(reason, "payload_too_large"))
        return "Shorten the job's prompt with `hermes cron edit {job_id} --prompt <text>`.";
    if (!strcmp(reason, "content_policy_blocked"))
        return "Reword the job's prompt with `hermes cron edit {job_id} --prompt <text>`, or "
               "pick another model with `hermes cron edit {job_id} --model <name>`.";
    if (!strcmp(reason, "provider_policy_blocked"))
        return "Retrying won't help: check the account's status and data/privacy settings with "
               "the provider, or pin another model with `hermes cron edit {job_id} --model <name>`.";
    if (!strcmp(reason, "guardrail_blocked"))
        return "The provider's prompt-injection guardrail blocked the request (not your key; "
               "tool output is scanned too): set it to Redact or Flag in the provider's settings "
               "(OpenRouter: Workspaces → Guardrails), or pin another provider with "
               "`hermes cron edit {job_id} --provider <name>`.";
    return "Run it again with `hermes cron run {job_id}`, or edit it with "
           "`hermes cron edit {job_id}`.";
}

static int is_transient(const char *reason) {
    return !strcmp(reason, "timeout") || !strcmp(reason, "rate_limit") ||
           !strcmp(reason, "upstream_rate_limit") || !strcmp(reason, "overloaded") ||
           !strcmp(reason, "server_error");
}

char *w14_schedulerf_f67f6c_provider_notice(const char *job_name, const char *job_id,
                                           const char *reason, const char *backup_provider_phrase,
                                           const char *cause, const char *relogin) {
    char *action, *p;
    size_t cap;

    if (!cause) return NULL;  /* cause is None -> Python returns None */
    if (is_transient(reason ? reason : "")) {
        size_t a = strlen(backup_provider_phrase ? backup_provider_phrase : "");
        cap = a + strlen(job_id ? job_id : "") + 256;
        action = (char *)malloc(cap);
        if (!action) return NULL;
        snprintf(action, cap, "%s It will run again at its next scheduled time; "
                 "`hermes cron run %s` tries now.",
                 backup_provider_phrase ? backup_provider_phrase : "", job_id ? job_id : "");
    } else {
        action = render(action_for(reason ? reason : ""), job_id ? job_id : "", relogin ? relogin : "");
        if (!action) return NULL;
    }
    cap = 384 + strlen(job_name ? job_name : "") + strlen(cause) + strlen(action) +
          strlen(job_id ? job_id : "");
    p = (char *)malloc(cap);
    if (p) {
        snprintf(p, cap, "⚠️ Cron '%s' failed: %s. %s Run log: `hermes cron runs %s`.",
                 job_name ? job_name : "", cause, action, job_id ? job_id : "");
    }
    free(action);
    return p;
}
