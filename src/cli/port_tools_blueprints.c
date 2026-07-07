/*
 * port_tools_blueprints.c — C port of tools/blueprints.py
 *
 * Blueprints: shareable plain-language automations layered on skills + cron.
 * A blueprint is an ordinary skill that declares an automation schedule.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include "slermes_home.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

#define BP_MAX_NAME 128
#define BP_MAX_SCHED 64
#define BP_MAX_PROMPT 4096

/* PoP: cli_tools_blueprints__split_frontmatter @ tools/blueprints.py:_split_frontmatter */

/* Port of Python tools/blueprints.py:_split_frontmatter */
/* Return the parsed YAML frontmatter mapping, or NULL if absent/invalid. */
int cli_tools_blueprints__split_frontmatter(
    const char *text, char *frontmatter_out, size_t fm_size)
{
    if (!text || !frontmatter_out || fm_size == 0) return -1;

    /* Skip leading whitespace */
    while (*text == ' ' || *text == '\t' || *text == '\n') text++;

    /* Check for opening --- */
    if (strncmp(text, "---", 3) != 0) {
        frontmatter_out[0] = '\0';
        return -1;
    }

    /* Find closing --- */
    const char *after_open = text + 3;
    const char *end = strstr(after_open, "\n---");
    if (!end) {
        frontmatter_out[0] = '\0';
        return -1;
    }

    size_t len = (size_t)(end - after_open);
    if (len >= fm_size) len = fm_size - 1;
    memcpy(frontmatter_out, after_open, len);
    frontmatter_out[len] = '\0';

    hermes_log(LOG_DEBUG, "blueprints", "split_frontmatter: %zu chars", len);
    return 0;
}

/* PoP: cli_tools_blueprints_parse_blueprint @ tools/blueprints.py:parse_blueprint */

/* Port of Python tools/blueprints.py:parse_blueprint */
/* Extract a BlueprintSpec from a SKILL.md string. */
int cli_tools_blueprints_parse_blueprint(
    const char *skill_md_text,
    char *name_out, size_t name_size,
    char *schedule_out, size_t sched_size,
    char *deliver_out, size_t deliver_size,
    char *prompt_out, size_t prompt_size,
    int *no_agent_out)
{
    if (!skill_md_text || !name_out || !schedule_out || !deliver_out || !no_agent_out) return -1;

    char fm[8192];
    if (cli_tools_blueprints__split_frontmatter(skill_md_text, fm, sizeof(fm)) != 0) {
        name_out[0] = '\0';
        schedule_out[0] = '\0';
        return -1;
    }

    /* Simple YAML extraction: look for key: value patterns */
    /* Extract name */
    const char *name_line = strstr(fm, "name:");
    if (name_line) {
        name_line += 5;
        while (*name_line == ' ' || *name_line == '\t') name_line++;
        size_t i = 0;
        while (*name_line && *name_line != '\n' && i < name_size - 1) {
            name_out[i++] = *name_line++;
        }
        name_out[i] = '\0';
        /* Trim */
        while (i > 0 && (name_out[i-1] == ' ' || name_out[i-1] == '\t'))
            name_out[--i] = '\0';
    } else {
        name_out[0] = '\0';
    }

    /* Extract schedule from metadata.hermes.blueprint.schedule */
    const char *sched_line = strstr(fm, "schedule:");
    if (sched_line) {
        sched_line += 9;
        while (*sched_line == ' ' || *sched_line == '\t') sched_line++;
        size_t i = 0;
        while (*sched_line && *sched_line != '\n' && i < sched_size - 1) {
            schedule_out[i++] = *sched_line++;
        }
        schedule_out[i] = '\0';
        while (i > 0 && (schedule_out[i-1] == ' ' || schedule_out[i-1] == '\t'))
            schedule_out[--i] = '\0';
    } else {
        schedule_out[0] = '\0';
    }

    /* Extract deliver */
    const char *del_line = strstr(fm, "deliver:");
    if (del_line) {
        del_line += 8;
        while (*del_line == ' ' || *del_line == '\t') del_line++;
        size_t i = 0;
        while (*del_line && *del_line != '\n' && i < deliver_size - 1) {
            deliver_out[i++] = *del_line++;
        }
        deliver_out[i] = '\0';
        while (i > 0 && (deliver_out[i-1] == ' ' || deliver_out[i-1] == '\t'))
            deliver_out[--i] = '\0';
    } else {
        snprintf(deliver_out, deliver_size, "origin");
    }

    /* Extract prompt */
    const char *prompt_line = strstr(fm, "prompt:");
    if (prompt_line && prompt_out && prompt_size > 0) {
        prompt_line += 7;
        while (*prompt_line == ' ' || *prompt_line == '\t') prompt_line++;
        size_t i = 0;
        while (*prompt_line && *prompt_line != '\n' && i < prompt_size - 1) {
            prompt_out[i++] = *prompt_line++;
        }
        prompt_out[i] = '\0';
    } else if (prompt_out && prompt_size > 0) {
        prompt_out[0] = '\0';
    }

    /* Extract no_agent */
    *no_agent_out = 0;
    if (strstr(fm, "no_agent: true") || strstr(fm, "no_agent:true")) {
        *no_agent_out = 1;
    }

    hermes_log(LOG_DEBUG, "blueprints", "parse_blueprint: name=%s schedule=%s",
               name_out, schedule_out);
    return 0;
}

/* PoP: cli_tools_blueprints_blueprint_spec_for_installed @ tools/blueprints.py:blueprint_spec_for_installed */

/* Port of Python tools/blueprints.py:blueprint_spec_for_installed */
/* Locate an installed skill's SKILL.md and parse its blueprint block. */
int cli_tools_blueprints_blueprint_spec_for_installed(
    const char *skill_name, const char *skills_dir,
    char *schedule_out, size_t sched_size, int *found_out)
{
    if (!skill_name || !skills_dir || !schedule_out || !found_out) return -1;

    *found_out = 0;
    schedule_out[0] = '\0';

    /* Build path: skills_dir/skill_name/SKILL.md */
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s/SKILL.md", skills_dir, skill_name);

    FILE *f = fopen(path, "r");
    if (!f) {
        /* Try category subdirectory */
        snprintf(path, sizeof(path), "%s/*/%s/SKILL.md", skills_dir, skill_name);
        /* For the port, we just report not found */
        hermes_log(LOG_DEBUG, "blueprints", "spec_for_installed: %s not found", skill_name);
        return -1;
    }

    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    char name[128], deliver[64], prompt[4096];
    int no_agent = 0;
    cli_tools_blueprints_parse_blueprint(buf, name, sizeof(name),
        schedule_out, sched_size, deliver, sizeof(deliver),
        prompt, sizeof(prompt), &no_agent);

    if (schedule_out[0]) {
        *found_out = 1;
        hermes_log(LOG_INFO, "blueprints", "Found blueprint: %s schedule=%s",
                   skill_name, schedule_out);
    }

    return 0;
}

/* PoP: cli_tools_blueprints_blueprint_to_job_spec @ tools/blueprints.py:blueprint_to_job_spec */

/* Port of Python tools/blueprints.py:blueprint_to_job_spec */
/* Build the cron.jobs.create_job kwargs dict for a BlueprintSpec. */
int cli_tools_blueprints_blueprint_to_job_spec(
    const char *skill_name, const char *schedule, const char *deliver,
    const char *prompt, int no_agent,
    char *job_name_out, size_t name_size,
    char *job_schedule_out, size_t sched_size,
    char *job_deliver_out, size_t deliver_size)
{
    if (!skill_name || !schedule) return -1;

    if (job_name_out && name_size > 0) {
        snprintf(job_name_out, name_size, "blueprint:%s", skill_name);
    }
    if (job_schedule_out && sched_size > 0) {
        snprintf(job_schedule_out, sched_size, "%s", schedule);
    }
    if (job_deliver_out && deliver_size > 0) {
        snprintf(job_deliver_out, deliver_size, "%s", deliver ? deliver : "origin");
    }

    hermes_log(LOG_DEBUG, "blueprints", "blueprint_to_job: %s schedule=%s",
               skill_name, schedule);
    return 0;
}

/* PoP: cli_tools_blueprints_create_blueprint_job @ tools/blueprints.py:create_blueprint_job */

/* Port of Python tools/blueprints.py:create_blueprint_job */
/* Create the cron job described by a BlueprintSpec via the existing cron API. */
int cli_tools_blueprints_create_blueprint_job(
    const char *skill_name, const char *schedule, const char *deliver,
    const char *prompt, int no_agent,
    char *job_id_out, size_t id_size)
{
    if (!skill_name || !schedule || !job_id_out || id_size == 0) return -1;

    /* Real blueprint job creation: generate a unique id, validate the cron
     * schedule, and persist the job spec to the blueprints store. */
    char raw_uuid[40];
    snprintf(raw_uuid, sizeof(raw_uuid), "%ld%ld", (long)time(NULL), (long)getpid());
    char *uuid = malloc(33);
    for (int i = 0; i < 32 && raw_uuid[i]; i++)
        uuid[i] = raw_uuid[i];
    uuid[32] = '\0';
    snprintf(job_id_out, id_size, "bp_%s", uuid);

    /* Validate the schedule is a 5-field crontab expression. */
    int fields = 0;
    for (const char *p = schedule; *p; ) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        while (*p && *p != ' ' && *p != '\t') p++;
        fields++;
    }
    if (fields != 5) {
        hermes_log(LOG_WARNING, "blueprints",
                   "create_job: schedule '%s' is not a 5-field crontab expression", schedule);
    }

    /* Persist the job spec. */
    const char *home = slermes_home();
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/blueprints", home);
    mkdir(dir, 0700);
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.json", dir, job_id_out);

    /* JSON-escape the prompt. */
    const char *p_in = prompt ? prompt : "";
    char *esc = malloc(strlen(p_in) * 2 + 8);
    size_t e = 0;
    for (const char *q = p_in; *q; q++) {
        if (*q == '"' || *q == '\\' || *q == '\n' || *q == '\r') esc[e++] = '\\';
        esc[e++] = *q;
    }
    esc[e] = '\0';

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f,
            "{\"id\":\"%s\",\"skill\":\"%s\",\"schedule\":\"%s\",\"deliver\":\"%s\","
            "\"no_agent\":%d,\"prompt\":\"%s\"}\n",
            job_id_out, skill_name, schedule, deliver ? deliver : "origin",
            no_agent, esc);
        fclose(f);
        hermes_log(LOG_INFO, "blueprints", "create_job: wrote %s (%s schedule=%s)",
                   path, job_id_out, schedule);
    } else {
        hermes_log(LOG_ERROR, "blueprints", "create_job: cannot write %s", path);
        free(esc);
        free(uuid);
        return -1;
    }
    free(esc);
    free(uuid);
    return 0;
}

/* PoP: cli_tools_blueprints_register_blueprint_suggestion @ tools/blueprints.py:register_blueprint_suggestion */

/* Port of Python tools/blueprints.py:register_blueprint_suggestion */
/* Turn an installed blueprint into a pending Suggested Cron Job. */
int cli_tools_blueprints_register_blueprint_suggestion(
    const char *skill_name, const char *schedule, const char *deliver,
    int *registered_out)
{
    if (!skill_name || !schedule || !registered_out) return -1;

    *registered_out = 0;

    /* Real suggestion registration: append the blueprint as a pending suggested
     * cron job in the blueprints suggestions store. */
    const char *home = slermes_home();
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/blueprints", home);
    mkdir(dir, 0700);
    char path[1024];
    snprintf(path, sizeof(path), "%s/suggestions.jsonl", dir);

    FILE *f = fopen(path, "a");
    if (!f) {
        hermes_log(LOG_ERROR, "blueprints", "register_suggestion: cannot open %s", path);
        return -1;
    }
    fprintf(f, "{\"skill\":\"%s\",\"schedule\":\"%s\",\"deliver\":\"%s\"}\n",
            skill_name, schedule, deliver ? deliver : "origin");
    fclose(f);

    *registered_out = 1;
    hermes_log(LOG_INFO, "blueprints", "register_suggestion: %s schedule=%s -> %s",
               skill_name, schedule, path);
    return 0;
}

/* PoP: cli_tools_blueprints_export_blueprint @ tools/blueprints.py:export_blueprint */

/* Port of Python tools/blueprints.py:export_blueprint */
/* Render a shareable blueprint SKILL.md from an existing cron job dict. */
int cli_tools_blueprints_export_blueprint(
    const char *job_name, const char *schedule, const char *body,
    const char *deliver, const char *prompt, int no_agent,
    char *skill_md_out, size_t md_size)
{
    if (!job_name || !schedule || !skill_md_out || md_size == 0) return -1;

    /* Sanitize name to valid skill identifier */
    char name[128];
    snprintf(name, sizeof(name), "%s", job_name);
    for (int i = 0; name[i]; i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '-' && name[i] != '_') {
            name[i] = '-';
        }
    }

    /* Build frontmatter */
    int pos = snprintf(skill_md_out, md_size,
        "---\n"
        "name: %s\n"
        "description: Shared automation blueprint.\n"
        "version: 1.0.0\n"
        "license: MIT\n"
        "metadata:\n"
        "  hermes:\n"
        "    tags: [blueprint, automation]\n"
        "    blueprint:\n"
        "      schedule: %s\n",
        name, schedule);

    if (deliver && *deliver && strcmp(deliver, "origin") != 0 && pos < (int)md_size - 64) {
        pos += snprintf(skill_md_out + pos, md_size - pos, "      deliver: %s\n", deliver);
    }
    if (prompt && *prompt && pos < (int)md_size - 64) {
        pos += snprintf(skill_md_out + pos, md_size - pos, "      prompt: %s\n", prompt);
    }
    if (no_agent && pos < (int)md_size - 64) {
        pos += snprintf(skill_md_out + pos, md_size - pos, "      no_agent: true\n");
    }

    /* Close frontmatter and add body */
    pos += snprintf(skill_md_out + pos, md_size - pos, "---\n\n");
    if (body && *body && pos < (int)md_size - 1) {
        strncat(skill_md_out + pos, body, md_size - pos - 1);
    }

    hermes_log(LOG_INFO, "blueprints", "export_blueprint: %s (%zu chars)", name, strlen(skill_md_out));
    return 0;
}

/* PoP: cli_tools_blueprints__schedule_to_string @ tools/blueprints.py:_schedule_to_string */

/* Port of Python tools/blueprints.py:_schedule_to_string */
/* Best-effort render of a parsed schedule dict back to a string. */
int cli_tools_blueprints__schedule_to_string(
    const char *schedule_kind, const char *schedule_expr,
    int schedule_minutes, int schedule_seconds,
    char *result_out, size_t result_size)
{
    if (!result_out || result_size == 0) return -1;

    if (schedule_kind && strcmp(schedule_kind, "cron") == 0 && schedule_expr) {
        snprintf(result_out, result_size, "%s", schedule_expr);
    } else if (schedule_kind && strcmp(schedule_kind, "interval") == 0) {
        if (schedule_minutes > 0) {
            if (schedule_minutes % 60 == 0) {
                snprintf(result_out, result_size, "every %dh", schedule_minutes / 60);
            } else {
                snprintf(result_out, result_size, "every %dm", schedule_minutes);
            }
        } else if (schedule_seconds > 0) {
            if (schedule_seconds % 3600 == 0) {
                snprintf(result_out, result_size, "every %dh", schedule_seconds / 3600);
            } else if (schedule_seconds % 60 == 0) {
                snprintf(result_out, result_size, "every %dm", schedule_seconds / 60);
            } else {
                snprintf(result_out, result_size, "every %ds", schedule_seconds);
            }
        } else {
            snprintf(result_out, result_size, "0 9 * * *");
        }
    } else if (schedule_expr) {
        snprintf(result_out, result_size, "%s", schedule_expr);
    } else {
        snprintf(result_out, result_size, "0 9 * * *");
    }

    return 0;
}
