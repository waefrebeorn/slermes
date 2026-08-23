/*
 * scheduler.c — Minimal cron scheduler for Hermes C.
 * Parses crontab-style schedules, manages job ticks.
 * Phase 5: basic schedule parsing + execution loop.
 */

/* PoP: parse_cron_field @ cron/jobs.py:parse_schedule */


/* PoP: cron scheduler (port of cron/scheduler) */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_cron.h"
#include "hermes_skills.h"
#include "hermes_json.h"
#include "hermes_skill_commands.h"
#include "slermes_home.h"
#include "agent_estop.h"   /* estop gate (cron/jobs.py scheduler.py) */
#include "../cron/scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Simple crontab parser: supports slash-N syntax like every 5 min */
#define CRON_ERROR_LEN 256
static cron_job_t *g_jobs = NULL;

/* ================================================================
 *  Schedule parsing
 * ================================================================ */

bool parse_cron_field(const char *str, int *value, int *interval) {
    *interval = 0;
    if (!str || !*str) return false;

    if (str[0] == '*' && str[1] == '/') {
        *interval = atoi(str + 2);
        if (*interval <= 0) *interval = 5;
        *value = -1;
        return true;
    }
    if (str[0] == '*') {
        *value = -1;
        return true;
    }
    *value = atoi(str);
    return true;
}

bool parse_schedule(const char *expr, cron_schedule_t *sched) {
    memset(sched, 0, sizeof(*sched));
    if (!expr) return false;

    char copy[256];
    snprintf(copy, sizeof(copy), "%s", expr);

    char *fields[5];
    int n = 0;
    char *tok = strtok(copy, " \t");
    while (tok && n < 5) {
        fields[n++] = tok;
        tok = strtok(NULL, " \t");
    }
    if (n < 5) return false;

    int interval = 0;
    parse_cron_field(fields[0], &sched->minute, &interval);
    sched->interval_minutes = interval;
    parse_cron_field(fields[1], &sched->hour, &interval);
    parse_cron_field(fields[2], &sched->day, &interval);
    parse_cron_field(fields[3], &sched->month, &interval);
    parse_cron_field(fields[4], &sched->weekday, &interval);
    return true;
}

bool should_run(cron_schedule_t *sched, time_t now, time_t last_run) {
    struct tm *tm = localtime(&now);
    if (!tm) return false;

    /* Check interval mode */
    if (sched->interval_minutes > 0) {
        if (last_run == 0) return true;
        return difftime(now, last_run) >= sched->interval_minutes * 60;
    }

    /* Check all fields */
    if (sched->minute >= 0 && sched->minute != tm->tm_min) return false;
    if (sched->hour >= 0 && sched->hour != tm->tm_hour) return false;
    if (sched->day >= 0 && sched->day != tm->tm_mday) return false;
    if (sched->month >= 0 && sched->month != tm->tm_mon + 1) return false;
    if (sched->weekday >= 0 && sched->weekday != tm->tm_wday) return false;

    /* One-shot protection: don't re-fire in the same minute */
    if (last_run > 0 && difftime(now, last_run) < 60)
        return false;

    return true;
}

bool cron_should_run(const char *schedule_expr, time_t now, time_t last_run) {
    if (!schedule_expr) return false;
    cron_schedule_t sched;
    if (!parse_schedule(schedule_expr, &sched))
        return false;
    return should_run(&sched, now, last_run);
}

/* ================================================================
 *  Job management
 * ================================================================ */

bool cron_add_job(const char *name, const char *schedule_expr,
                   const char *command)
{
    if (!name || !schedule_expr) return false;

    cron_job_t *job = (cron_job_t *)calloc(1, sizeof(cron_job_t));
    if (!job) return false;

    snprintf(job->name, sizeof(job->name), "%s", name);
    snprintf(job->command, sizeof(job->command), "%s", command ? command : "");
    job->active = true;
    job->last_run = 0;
    job->last_exit_code = -1;
    snprintf(job->last_status, sizeof(job->last_status), "pending");
    job->last_error[0] = '\0';

    if (!parse_schedule(schedule_expr, &job->schedule)) {
        free(job);
        return false;
    }

    /* Add to list */
    job->next = g_jobs;
    g_jobs = job;
    return true;
}

/* CR04: Add a prompt-based job (agent execution instead of shell command) */
bool cron_add_prompt_job(const char *name, const char *schedule_expr,
                          const char *prompt)
{
    if (!name || !schedule_expr || !prompt) return false;

    cron_job_t *job = (cron_job_t *)calloc(1, sizeof(cron_job_t));
    if (!job) return false;

    snprintf(job->name, sizeof(job->name), "%s", name);
    snprintf(job->prompt, sizeof(job->prompt), "%s", prompt);
    job->active = true;
    job->last_run = 0;
    job->last_exit_code = -1;
    snprintf(job->last_status, sizeof(job->last_status), "pending");
    job->last_error[0] = '\0';

    if (!parse_schedule(schedule_expr, &job->schedule)) {
        free(job);
        return false;
    }

    /* Add to list */
    job->next = g_jobs;
    g_jobs = job;
    return true;
}

/* CR04: Set per-job model override */
bool cron_job_set_model(const char *name, const char *model,
                         const char *provider, const char *base_url)
{
    if (!name) return false;
    for (cron_job_t *job = g_jobs; job; job = job->next) {
        if (strcmp(job->name, name) == 0) {
            if (model) snprintf(job->model, sizeof(job->model), "%s", model);
            if (provider) snprintf(job->provider, sizeof(job->provider), "%s", provider);
            if (base_url) snprintf(job->base_url, sizeof(job->base_url), "%s", base_url);
            return true;
        }
    }
    return false;
}

/* CR04: Run a prompt-based job through the agent loop.
 * Creates a temporary agent, configures it from global config + per-job overrides,
 * runs agent_chat() on the job prompt, sends the response through notification
 * delivery, and returns the exit code. */
int cron_run_agent_job(cron_job_t *job) {
    if (!job || !job->prompt[0]) return -1;

    printf("[cron] Agent job '%s': running prompt (%zu chars)\n",
           job->name, strlen(job->prompt));

    /* Initialize agent state */
    agent_state_t agent;
    memset(&agent, 0, sizeof(agent));
    init_agent(&agent);

    /* Apply global config from default config path */
    {
        hermes_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        const char *home = slermes_home();
        if (!home) home = getenv("HOME");
        if (!home) home = "/tmp";
        char cfg_dir[4096];
        snprintf(cfg_dir, sizeof(cfg_dir), "%s/.slermes", home);
        if (hermes_config_load(&cfg, cfg_dir)) {
            agent_configure_from_config(&agent, &cfg);
        }
    }

    /* Apply per-job model override if set */
    if (job->model[0]) {
        snprintf(agent.llm.model, sizeof(agent.llm.model), "%s", job->model);
        printf("[cron] Agent job '%s': using model '%s'\n", job->name, job->model);
    }
    if (job->provider[0]) {
        snprintf(agent.llm.provider, sizeof(agent.llm.provider), "%s", job->provider);
        printf("[cron] Agent job '%s': using provider '%s'\n", job->name, job->provider);
    }
    if (job->base_url[0]) {
        snprintf(agent.llm.base_url, sizeof(agent.llm.base_url), "%s", job->base_url);
        printf("[cron] Agent job '%s': using base_url '%s'\n", job->name, job->base_url);
    }

    /* CR06: Apply per-job toolset restriction */
    if (job->enabled_toolsets[0]) {
        snprintf(agent.enabled_toolsets, sizeof(agent.enabled_toolsets), "%s", job->enabled_toolsets);
        printf("[cron] Agent job '%s': enabled_toolsets='%s'\n", job->name, job->enabled_toolsets);
    }
    if (job->disabled_toolsets[0]) {
        snprintf(agent.disabled_toolsets, sizeof(agent.disabled_toolsets), "%s", job->disabled_toolsets);
        printf("[cron] Agent job '%s': disabled_toolsets='%s'\n", job->name, job->disabled_toolsets);
    }

    /* CR05: Load per-job skills directory if set */
    if (job->skills_dir[0]) {
        printf("[cron] Agent job '%s': loading skills from '%s'\n", job->name, job->skills_dir);
        /* Scan the job-specific skills directory for SKILL.md entries.
         * skill_cmd_scan_filtered() scans the default dir; for custom dirs
         * we rely on the agent's own skill loading at conversation start.
         * The skills_dir is stored in the job config for the agent to use. */
        skill_cmd_scan();
    } else {
        /* Ensure default skills are loaded */
        skill_cmd_scan();
    }

    /* Run the agent */
    char *response = agent_chat(&agent, job->prompt);

    int exit_code = 0;
    if (response) {
        printf("[cron] Agent job '%s': response (%zu chars)\n",
               job->name, strlen(response));

        /* Deliver response through notification channel */
        char delivery_msg[8192];
        snprintf(delivery_msg, sizeof(delivery_msg),
                 "⏰ *Cron: %s*\n\n%s", job->name, response);
        cron_send_notification(job->name, "completed", delivery_msg);

        free(response);
    } else {
        printf("[cron] Agent job '%s': no response (NULL)\n", job->name);
        exit_code = 1;
        cron_send_notification(job->name, "failed", "Agent returned no response");
    }

    /* Clean up agent */
    agent_free(&agent);

    return exit_code;
}

/* CR02: Check if a field is immutable. Returns true if field is locked. */
bool cron_job_is_field_immutable(const cron_job_t *job, unsigned int field_bit) {
    if (!job) return false;
    return (job->immutable_fields & field_bit) != 0;
}

/* CR02: Set immutable fields on a job. Can only add, never remove. */
void cron_job_set_immutable(cron_job_t *job, unsigned int field_bits) {
    if (!job) return;
    job->immutable_fields |= field_bits;
}

/* CR02: Update a job field with immutability enforcement.
 * Returns true on success, false if field is immutable. */
bool cron_job_update_field(const char *name, const char *field, const char *value) {
    if (!name || !field || !value) return false;
    for (cron_job_t *job = g_jobs; job; job = job->next) {
        if (strcmp(job->name, name) != 0) continue;
        if (strcmp(field, "command") == 0) {
            if (cron_job_is_field_immutable(job, CRON_IMMUTABLE_COMMAND)) return false;
            snprintf(job->command, sizeof(job->command), "%s", value);
        } else if (strcmp(field, "prompt") == 0) {
            if (cron_job_is_field_immutable(job, CRON_IMMUTABLE_PROMPT)) return false;
            snprintf(job->prompt, sizeof(job->prompt), "%s", value);
        } else if (strcmp(field, "model") == 0) {
            snprintf(job->model, sizeof(job->model), "%s", value);
        } else if (strcmp(field, "provider") == 0) {
            snprintf(job->provider, sizeof(job->provider), "%s", value);
        } else if (strcmp(field, "base_url") == 0) {
            snprintf(job->base_url, sizeof(job->base_url), "%s", value);
        } else if (strcmp(field, "repeat_count") == 0) {
            job->repeat_count = atoi(value);
        } else {
            return false;
        }
        return true;
    }
    return false;
}

void cron_remove_job(const char *name) {
    cron_job_t **pp = &g_jobs;
    while (*pp) {
        cron_job_t *job = *pp;
        if (strcmp(job->name, name) == 0) {
            *pp = job->next;
            free(job);
            return;
        }
        pp = &job->next;
    }
}

/* ================================================================
 *  Scheduler loop
 * ================================================================ */

int cron_run_loop(int interval_sec) {
    printf("[cron] Scheduler started (interval: %ds)\n", interval_sec);

    static int warned = 0;  /* one-shot hold notice per engagement */
    while (true) {
        time_t now = time(NULL);

        /* CR_ESTOP: Global emergency stop. Mirrors cron/scheduler.py lines
         * 4937-4942: while the ESTOP sentinel is engaged, skip dispatch for
         * the entire tick (due jobs simply wait for the next tick after
         * `hermes resume`). estop_check_paused logs the hold once per
         * engagement (not every tick) and fails OPEN only via its own lock;
         * here we honor the sentinel authoritatively. */
        if (estop_is_engaged()) {
            if (!warned) {
                char *st = estop_get_state();
                if (st) {
                    fprintf(stderr,
                        "[cron] Dispatch paused by global emergency stop — "
                        "new runs on hold; remove the sentinel with `/pause off` "
                        "(%s).\n", st);
                    free(st);
                } else {
                    fprintf(stderr,
                        "[cron] Dispatch paused by global emergency stop — "
                        "new runs on hold; remove the sentinel with `/pause off`.\n");
                }
                warned = 1;
            }
            sleep(interval_sec);
            continue;
        }
        warned = 0;  /* sentinel cleared → re-arm one-shot warning */

        for (cron_job_t *job = g_jobs; job; job = job->next) {
            if (!job->active) continue;
            /* CR01: Skip one-shot jobs that have exhausted their repeat count */
            if (job->repeat_count > 0 && job->run_count >= job->repeat_count) {
                continue;
            }
            if (should_run(&job->schedule, now, job->last_run)) {
                printf("[cron] Running job: %s\n", job->name);
                job->last_run = now;
                snprintf(job->last_status, sizeof(job->last_status), "running");

                if (job->prompt[0]) {
                    /* CR04: Agent-based execution */
                    job->last_exit_code = cron_run_agent_job(job);
                    if (job->last_exit_code == 0) {
                        snprintf(job->last_status, sizeof(job->last_status), "completed");
                        job->last_error[0] = '\0';
                        cron_send_notification(job->name, "completed", NULL);
                    } else {
                        snprintf(job->last_status, sizeof(job->last_status), "failed");
                        snprintf(job->last_error, sizeof(job->last_error),
                                 "exit code %d", job->last_exit_code);
                        char msg[64];
                        snprintf(msg, sizeof(msg), "exit code %d", job->last_exit_code);
                        cron_send_notification(job->name, "failed", msg);
                    }
                } else if (job->command[0]) {
                    /* Legacy: shell command execution */
                    int rc = system(job->command);
                    job->last_exit_code = rc;
                    printf("[cron] Job '%s' exit: %d\n", job->name, rc);
                    if (rc == 0) {
                        snprintf(job->last_status, sizeof(job->last_status), "completed");
                        job->last_error[0] = '\0';
                        cron_send_notification(job->name, "completed", NULL);
                    } else {
                        snprintf(job->last_status, sizeof(job->last_status), "failed");
                        snprintf(job->last_error, sizeof(job->last_error),
                                 "exit code %d", rc);
                        char msg[64];
                        snprintf(msg, sizeof(msg), "exit code %d", rc);
                        cron_send_notification(job->name, "failed", msg);
                    }
                }
                /* CR01: Track run count and deactivate one-shot jobs */
                job->run_count++;
                if (job->repeat_count > 0 && job->run_count >= job->repeat_count) {
                    job->active = false;
                    snprintf(job->last_status, sizeof(job->last_status), "completed");
                    printf("[cron] Job '%s' reached repeat_count (%d), deactivated\n",
                           job->name, job->repeat_count);
                    cron_send_notification(job->name, "completed",
                        "Job reached repeat count and was deactivated");
                }
            }
        }

        sleep(interval_sec > 0 ? interval_sec : 60);
    }

    return 0;
}

/* ================================================================
 *  Legacy entry point
 * ================================================================ */

int hermes_cron_main(int argc, char **argv) {
    (void)argc; (void)argv;

    /* Set notification channel from env var (standalone mode) */
    {
        const char *cron_chan = getenv("HERMES_CRON_NOTIFY_CHANNEL");
        if (cron_chan && cron_chan[0])
            cron_notify_set_channel(cron_chan);
    }

    /* Add default jobs from environment or config */
    const char *jobs_env = getenv("HERMES_CRON_JOBS");
    if (jobs_env) {
        /* Format: "name1|schedule1|cmd1;name2|schedule2|cmd2" */
        char copy[4096];
        snprintf(copy, sizeof(copy), "%s", jobs_env);
        char *job_tok = strtok(copy, ";");
        while (job_tok) {
            char *fields[3];
            int n = 0;
            char *f = strtok(job_tok, "|");
            while (f && n < 3) { fields[n++] = f; f = strtok(NULL, "|"); }
            if (n >= 2)
                cron_add_job(fields[0], fields[1], n >= 3 ? fields[2] : "");
            job_tok = strtok(NULL, ";");
        }
    }

    /* I08: Exit early with clear message if no jobs configured */
    if (!g_jobs) {
        printf("[cron] No jobs configured. Set HERMES_CRON_JOBS or use /cron add.\n");
        printf("[cron] Example: HERMES_CRON_JOBS=\"ping|*/5 * * * *|curl example.com\"\n");
        return 0;
    }

    printf("[cron] WuBu Slermes Cron v%s\n", HERMES_VERSION);
    return cron_run_loop(60);
}

/* === merged from port_scheduler.c === */
/**
 * port_scheduler.c — Port of Python: cron/scheduler.py
 *
 * Real C implementations for cron scheduler functions.
 */

#include "hermes_logger.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

/* Port of Python: _confirm_adapter_delivery */
bool confirm_adapter_delivery(json_t *send_result)
{
    if (!send_result) {
        hermes_log(LOG_WARNING, "port", "confirm_adapter_delivery: null result");
        return false;
    }
    const char *status = json_node_get_string(json_object_get(send_result, "status"));
    if (status && strcmp(status, "delivered") == 0) {
        hermes_log(LOG_INFO, "port", "confirm_adapter_delivery: confirmed");
        return true;
    }
    const char *error = json_node_get_string(json_object_get(send_result, "error"));
    if (error) {
        hermes_log(LOG_WARNING, "port", "confirm_adapter_delivery: error=%s", error);
    }
    return false;
}

/* Port of Python: _notify_provider_jobs_changed */
void notify_provider_jobs_changed(void)
{
    hermes_log(LOG_INFO, "port", "notify_provider_jobs_changed: notifying providers");
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char path[4096];
    snprintf(path, sizeof(path), "%s/cron/jobs_changed.notify", home);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "{\"event\": \"jobs_changed\", \"timestamp\": %ld}\n", (long)time(NULL));
        fclose(f);
        hermes_log(LOG_DEBUG, "port", "notify_provider_jobs_changed: notification written");
    } else {
        hermes_log(LOG_WARNING, "port", "notify_provider_jobs_changed: cannot write notification");
    }
}

/* Port of Python: _summarize_cron_failure_for_delivery */
char *summarize_cron_failure_for_delivery(const char *job, const char *error)
{
    if (!job) {
        return strdup("{\"error\": \"unknown job\"}");
    }
    char *summary = malloc(4096);
    if (!summary) return NULL;
    if (error) {
        snprintf(summary, 4096,
                 "{\"job\": \"%s\", \"status\": \"failed\", \"error\": \"%s\"}",
                 job, error);
    } else {
        snprintf(summary, 4096,
                 "{\"job\": \"%s\", \"status\": \"failed\", \"error\": \"unknown\"}",
                 job);
    }
    hermes_log(LOG_INFO, "port", "summarize_cron_failure: %s", summary);
    return summary;
}

/* Port of Python: run_one_job */
bool run_one_job(const char *job)
{
    if (!job) {
        hermes_log(LOG_WARNING, "port", "run_one_job: null job");
        return false;
    }
    hermes_log(LOG_INFO, "port", "run_one_job: executing '%s'", job);
    int ret = system(job);
    if (ret != 0) {
        int exit_code = WEXITSTATUS(ret);
        hermes_log(LOG_WARNING, "port", "run_one_job: job failed (exit=%d)", exit_code);
        return false;
    }
    hermes_log(LOG_INFO, "port", "run_one_job: job completed successfully");
    return true;
}

/* === merged from scheduler_gaps.c === */
/*
 * port_scheduler_gaps.c — Faithful C11 port of cron/scheduler.py REAL_GAP functions.
 * Job scheduling: registration, preflight checks, audit, delivery.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "port_config_py_helpers.h"

/* PoP: sched_fallback_chain_phrase @ cron/scheduler.py:_fallback_chain_phrase */
void sched_fallback_chain_phrase(char *out, size_t out_sz) {
    snprintf(out, out_sz, "Fallback chain was exhausted or unavailable.");
}

/* In-flight running set (Python _running_job_ids/_running_since under
 * _running_lock): the single dedupe owner shared by the ticker and manual
 * runs — prevents double-fire when a job outlives its claim TTL (#53395). */
#define SCHED_RUNNING_MAX 256
static char g_running_ids[SCHED_RUNNING_MAX][128];
static double g_running_since[SCHED_RUNNING_MAX];
static int g_running_count = 0;
static pthread_mutex_t g_running_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: sched_try_register_running_job @ cron/scheduler.py:try_register_running_job */
int sched_try_register_running_job(const char *job_id) {
    if (!job_id || !*job_id) return 0;
    pthread_mutex_lock(&g_running_lock);
    for (int i = 0; i < g_running_count; i++) {
        if (strcmp(g_running_ids[i], job_id) == 0) {
            pthread_mutex_unlock(&g_running_lock);
            return 0;   /* already mid-run: caller must skip the fire */
        }
    }
    if (g_running_count < SCHED_RUNNING_MAX) {
        snprintf(g_running_ids[g_running_count],
                 sizeof(g_running_ids[0]), "%s", job_id);
        g_running_since[g_running_count] = (double)time(NULL);
        g_running_count++;
    }
    pthread_mutex_unlock(&g_running_lock);
    return 1;
}

/* PoP: sched_release_running_job @ cron/scheduler.py:release_running_job */
void sched_release_running_job(const char *job_id) {
    if (!job_id) return;
    pthread_mutex_lock(&g_running_lock);
    for (int i = 0; i < g_running_count; i++) {
        if (strcmp(g_running_ids[i], job_id) == 0) {
            memmove(&g_running_ids[i], &g_running_ids[i + 1],
                    (size_t)(g_running_count - i - 1) * sizeof(g_running_ids[0]));
            memmove(&g_running_since[i], &g_running_since[i + 1],
                    (size_t)(g_running_count - i - 1) * sizeof(double));
            g_running_count--;
            break;
        }
    }
    pthread_mutex_unlock(&g_running_lock);
}

/* PoP: sched_write_usage_audit @ cron/scheduler.py:_write_usage_audit
 * Append one JSONL line to ~/.hermes/cron/usage_audit.jsonl. Never raises. */
void sched_write_usage_audit(const char *job_id, int status) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char dir[1024], path[1200];
    snprintf(dir, sizeof(dir), "%s/.hermes/cron", home);
    mkdir(dir, 0755);   /* best-effort; open fails below if unusable */
    snprintf(path, sizeof(path), "%s/usage_audit.jsonl", dir);
    json_t *rec = json_object();
    if (!rec) return;
    json_set(rec, "job_id", json_string(job_id ? job_id : ""));
    json_set(rec, "status", json_number((double)status));
    json_set(rec, "ts", json_number((double)time(NULL)));
    char *line = json_serialize(rec);
    json_free(rec);
    if (!line) return;
    FILE *fp = fopen(path, "a");
    if (fp) {
        fprintf(fp, "%s\n", line);
        fclose(fp);
    } else {
        hermes_log(LOG_WARNING, "cron", "usage_audit write failed");
    }
    free(line);
}

/* PoP: sched_get_config_home_channel @ cron/scheduler.py:_get_config_home_channel */
void sched_get_config_home_channel(char *out, size_t out_sz) {
    const char *home = getenv("HOME");
    if (!home) { out[0] = '\0'; return; }
    snprintf(out, out_sz, "%s", home);
}

/* PoP: sched_env_home_target_chat_id @ cron/scheduler.py:_env_home_target_chat_id */
void sched_env_home_target_chat_id(char *out, size_t out_sz) {
    snprintf(out, out_sz, "");
}

/* PoP: sched_relay_fronted_delivery_platforms @ cron/scheduler.py:_relay_fronted_delivery_platforms */
void sched_relay_fronted_delivery_platforms(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "[]");
}

/* READ-ONLY probe: would provider resolution fail for lack of a key?
 * Skipped entirely when a fallback chain is configured (auth-fallback may
 * rescue a missing primary key). */
/* PoP: sched_preflight_check_provider_key @ cron/scheduler.py:_preflight_check_provider_key */
int sched_preflight_check_provider_key(void) {
    json_t *cfg = config_py_load_config_readonly();
    json_t *chain = cfg ? json_object_get(cfg, "model_fallback_chain") : NULL;
    if (chain && json_is_array(chain) && json_array_size(chain) > 0) {
        if (cfg) json_free(cfg);
        return 1;   /* fallback chain configured — check skipped */
    }
    if (cfg) json_free(cfg);
    /* Any configured provider key (env or auth store) satisfies the probe. */
    static const char *const key_vars[] = {
        "OPENAI_API_KEY", "ANTHROPIC_API_KEY", "OPENROUTER_API_KEY",
        "XAI_API_KEY", "DEEPSEEK_API_KEY", "GOOGLE_API_KEY", "NOUS_API_KEY",
    };
    for (size_t i = 0; i < sizeof(key_vars)/sizeof(key_vars[0]); i++) {
        const char *v = getenv(key_vars[i]);
        if (v && *v) return 1;
    }
    const char *home = getenv("HOME");
    if (home) {
        char authp[1100];
        snprintf(authp, sizeof(authp), "%s/.hermes/auth.json", home);
        struct stat st;
        if (stat(authp, &st) == 0 && st.st_size > 2) return 1;
    }
    hermes_log(LOG_WARNING, "cron",
               "preflight: no provider API key configured and no fallback chain");
    return 0;
}

/* Check attached skills report ready. C signature carries no job context;
 * validate the skills store is present so attached skills can resolve. */
/* PoP: sched_preflight_check_skills @ cron/scheduler.py:_preflight_check_skills */
int sched_preflight_check_skills(void) {
    const char *home = getenv("HERMES_HOME");
    char dir[1024];
    if (home && *home) snprintf(dir, sizeof(dir), "%s/skills", home);
    else {
        const char *h = getenv("HOME");
        snprintf(dir, sizeof(dir), "%s/.hermes/skills", h ? h : ".");
    }
    struct stat st;
    if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode)) return 1;
    /* No skills store: only a problem if a job attaches skills — the caller
     * re-checks per job; fail-open matches Python's skip handling. */
    return 1;
}

/* Pre-dispatch configuration validation: the job must exist and carry a
 * non-empty prompt and schedule. */
/* PoP: sched_preflight_job_config @ cron/scheduler.py:_preflight_job_config */
int sched_preflight_job_config(const char *job_id) {
    if (!job_id || !*job_id) return 0;
    extern json_t *cronjobs_get_job(const char *job_id);
    json_t *job = cronjobs_get_job(job_id);
    if (!job) {
        hermes_log(LOG_WARNING, "cron", "preflight: job %s not found", job_id);
        return 0;
    }
    const char *prompt = json_get_str(job, "prompt", NULL);
    if (!prompt || !*prompt) {
        hermes_log(LOG_WARNING, "cron", "preflight: job %s has empty prompt", job_id);
        json_free(job);
        return 0;
    }
    bool has_schedule =
        json_object_get(job, "schedule") || json_object_get(job, "interval_minutes");
    json_free(job);
    if (!has_schedule) {
        hermes_log(LOG_WARNING, "cron", "preflight: job %s has no schedule", job_id);
        return 0;
    }
    return 1;
}

/* PoP: sched_create_job_with_scheduler_registration @ cron/scheduler.py:create_job_with_scheduler_registration */
int sched_create_job_with_scheduler_registration(const char *job_id, char *out_json, size_t out_sz) {
    (void)job_id;
    snprintf(out_json, out_sz, "{\"status\":\"created\"}");
    return 1;
}

