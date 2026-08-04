/*
 * scheduler.c — Minimal cron scheduler for Hermes C.
 * Parses crontab-style schedules, manages job ticks.
 * Phase 5: basic schedule parsing + execution loop.
 */

/* PoP: parse_schedule @ cron/jobs.py:parse_schedule */


/* PoP: cron scheduler (port of cron/scheduler) */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_cron.h"
#include "hermes_skills.h"
#include "hermes_json.h"
#include "hermes_skill_commands.h"
#include "slermes_home.h"
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

    while (true) {
        time_t now = time(NULL);

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
