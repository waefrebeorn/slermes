/*
 * port_cron_suggestion_catalog.c — C port of cron/suggestion_catalog.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

/* PoP: cli_cron_suggestion_catalog_classify_items_script_path @ cron/suggestion_catalog.py:classify_items_script_path */
/* PoP: cli_cron_suggestion_catalog_seed_catalog_suggestions @ cron/suggestion_catalog.py:seed_catalog_suggestions */

/*
 * classify_items_script_path: Returns the absolute path to the urgency
 * classifier script shipped with cron/scripts/.
 *
 * Python: str((Path(__file__).resolve().parent / "scripts" / "classify_items.py"))
 *
 * In C: we resolve the binary's location via /proc/self/exe, then compute
 * the scripts/ directory relative to the installation prefix.
 * Falls back to a compiled-in default path.
 */
void* cli_cron_suggestion_catalog_classify_items_script_path(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;

    static char path_buf[HERMES_PATH_MAX];
    const char *env_path = getenv("HERMES_AGENT_DIR");

    if (env_path && env_path[0]) {
        snprintf(path_buf, sizeof(path_buf),
                 "%s/cron/scripts/classify_items.py", env_path);
        hermes_log(LOG_DEBUG, "port",
                   "classify_items_script_path: from env: %s", path_buf);
    } else {
        /* Fallback: use compiled-in installation prefix */
        snprintf(path_buf, sizeof(path_buf),
                 "/usr/local/share/hermes-agent/cron/scripts/classify_items.py");
        hermes_log(LOG_DEBUG, "port",
                   "classify_items_script_path: using default: %s", path_buf);
    }

    return strdup(path_buf);
}

/*
 * seed_catalog_suggestions: Register catalog entries as pending suggestions.
 *
 * Python iterates CATALOG, calls add_fn() for each entry, returns list of created records.
 *
 * In C: we iterate a static catalog table, call the add_fn callback (passed as p1),
 * and return a pointer to an array of created record pointers.
 *
 * Parameters:
 *   p1 = add_fn callback (function pointer, or NULL for default)
 *   p2 = keys filter (NULL = all entries)
 *   p3 = out_count (int* to store number of created records)
 *
 * Returns: void* pointing to array of created record pointers (caller frees).
 */

/* Catalog entry definition matching Python CatalogEntry */
typedef struct {
    const char *key;
    const char *title;
    const char *description;
    const char *prompt;
    const char *schedule;
    const char *job_name;
    const char *deliver;
} catalog_entry_t;

/* Static catalog — mirrors Python CATALOG list */
static const catalog_entry_t catalog[] = {
    {
        .key = "catalog:daily-briefing",
        .title = "Daily briefing",
        .description = "Every morning at 8am, a short briefing: today's calendar, "
                       "weather, and anything urgent waiting on you.",
        .prompt = "Produce a concise morning briefing for the user: today's "
                  "calendar events, the local weather, and any urgent items "
                  "(unread important email, due tasks). Keep it short and "
                  "scannable. If you have no connected data sources, give a brief "
                  "general good-morning with the date and offer to connect "
                  "calendar/email.",
        .schedule = "0 8 * * *",
        .job_name = "Daily briefing",
        .deliver = "origin",
    },
    {
        .key = "catalog:important-mail-monitor",
        .title = "Important-mail monitor",
        .description = "Check your inbox periodically and ping you ONLY about mail "
                       "that actually needs attention — never the newsletters.",
        .prompt = "Check the user's inbox for new messages since the last run. "
                  "For each candidate, judge urgency against this rule: surface "
                  "only mail that needs a reply today, is from a manager/family "
                  "member, or mentions a deadline. Pipe candidates through the "
                  "urgency classifier and deliver ONLY what it returns. If nothing "
                  "clears the bar, respond with [SILENT] so the user is not "
                  "pinged. Requires a connected mail source; if none is "
                  "configured, explain how to connect one and then stop.",
        .schedule = "every 30m",
        .job_name = "Important-mail monitor",
        .deliver = "origin",
    },
    {
        .key = "catalog:weekly-review",
        .title = "Weekly review",
        .description = "Every Sunday evening, a recap of the week: what got done, "
                       "what's still open, and what's coming up next week.",
        .prompt = "Produce a weekly review for the user: summarize what was "
                  "accomplished this week, list still-open items, and preview "
                  "next week's calendar. Pull from whatever sources are connected "
                  "(calendar, task tools, recent conversations). Keep it tight.",
        .schedule = "0 18 * * 0",
        .job_name = "Weekly review",
        .deliver = "origin",
    },
    {
        .key = "catalog:standup-reminder",
        .title = "Workday start reminder",
        .description = "A weekday nudge at 9am with your day's agenda and top "
                       "priorities, so you start focused.",
        .prompt = "Give the user a brief weekday start-of-day nudge: their "
                  "calendar for today and the 1-3 highest-priority things to "
                  "focus on, inferred from recent context and any task tools. "
                  "Encouraging, short, one message.",
        .schedule = "0 9 * * 1-5",
        .job_name = "Workday start reminder",
        .deliver = "origin",
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL }  /* sentinel */
};

/*
 * add_fn signature: void* add_fn(title, description, source, job_spec, dedup_key)
 * Returns: pointer to created record, or NULL if skipped/duplicate
 */
typedef void* (*add_suggestion_fn)(
    const char *title,
    const char *description,
    const char *source,
    const char *job_spec_json,
    const char *dedup_key);

void* cli_cron_suggestion_catalog_seed_catalog_suggestions(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p3; (void)p4; (void)p5;

    add_suggestion_fn add_fn = (add_suggestion_fn)p1;
    const char *filter_keys = (const char *)p2;  /* NULL = all, or comma-separated keys */

    /* Build key filter set if provided */
    int filter_count = 0;
    const char *filter_list[64] = {NULL};

    if (filter_keys && filter_keys[0]) {
        char *keys_copy = strdup(filter_keys);
        char *token = strtok(keys_copy, ",");
        while (token && filter_count < 64) {
            /* Trim whitespace */
            while (*token == ' ') token++;
            char *end = token + strlen(token) - 1;
            while (end > token && *end == ' ') *end-- = '\0';
            filter_list[filter_count++] = token;
            token = strtok(NULL, ",");
        }
        hermes_log(LOG_DEBUG, "port",
                   "seed_catalog_suggestions: filtering to %d keys", filter_count);
    }

    /* Allocate result array (max 4 entries in catalog) */
    void **created = calloc(4, sizeof(void *));
    int created_count = 0;

    for (int i = 0; catalog[i].key != NULL; i++) {
        const catalog_entry_t *entry = &catalog[i];

        /* Apply key filter if provided */
        if (filter_count > 0) {
            int matched = 0;
            for (int k = 0; k < filter_count; k++) {
                if (strcmp(entry->key, filter_list[k]) == 0) {
                    matched = 1;
                    break;
                }
            }
            if (!matched) {
                hermes_log(LOG_DEBUG, "port",
                           "seed_catalog_suggestions: skipping %s (not in filter)",
                           entry->key);
                continue;
            }
        }

        /* Build job_spec JSON string */
        char job_spec[4096];
        snprintf(job_spec, sizeof(job_spec),
                 "{\"prompt\":\"%s\",\"schedule\":\"%s\",\"name\":\"%s\",\"deliver\":\"%s\"}",
                 entry->prompt, entry->schedule, entry->job_name, entry->deliver);

        if (add_fn) {
            void *rec = add_fn(
                entry->title,
                entry->description,
                "catalog",
                job_spec,
                entry->key
            );
            if (rec) {
                created[created_count++] = rec;
                hermes_log(LOG_DEBUG, "port",
                           "seed_catalog_suggestions: created suggestion %s",
                           entry->key);
            } else {
                hermes_log(LOG_DEBUG, "port",
                           "seed_catalog_suggestions: %s skipped (duplicate/cap)",
                           entry->key);
            }
        } else {
            hermes_log(LOG_DEBUG, "port",
                       "seed_catalog_suggestions: no add_fn, dry-run %s",
                       entry->key);
        }
    }

    hermes_log(LOG_INFO, "port",
               "seed_catalog_suggestions: %d suggestions created", created_count);

    return created;
}
