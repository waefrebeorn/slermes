/* Slermes C11 port of cron/suggestion_catalog.py — implementation.
 * PoP: exact port. Semantic source of truth = cron/suggestion_catalog.py. */
#include "suggestion_catalog.h"
#include "cron_suggestions.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* The curated set. Schedules use the cron/interval syntax create_job accepts. */
static const catalog_entry_t CATALOG[] = {
    {
        "catalog:daily-briefing",
        "Daily briefing",
        "Every morning at 8am, a short briefing: today's calendar, "
        "weather, and anything urgent waiting on you.",
        "Produce a concise morning briefing for the user: today's "
        "calendar events, the local weather, and any urgent items "
        "(unread important email, due tasks). Keep it short and "
        "scannable. If you have no connected data sources, give a brief "
        "general good-morning with the date and offer to connect "
        "calendar/email.",
        "0 8 * * *",
        "Daily briefing",
        "origin",
    },
    {
        "catalog:important-mail-monitor",
        "Important-mail monitor",
        "Check your inbox periodically and ping you ONLY about mail "
        "that actually needs attention \xe2\x80\x94 never the newsletters.",
        "Check the user's inbox for new messages since the last run. "
        "For each candidate, judge urgency against this rule: surface "
        "only mail that needs a reply today, is from a manager/family "
        "member, or mentions a deadline. Pipe candidates through the "
        "urgency classifier (run `python3 -m cron.scripts.classify_items "
        "--threshold 7 --criteria ...` from the hermes-agent install \xe2\x80\x94 "
        "resolve the script path at run time, do not assume a fixed "
        "location) and deliver ONLY what it returns. If nothing "
        "clears the bar, respond with [SILENT] so the user is not "
        "pinged. Requires a connected mail source; if none is "
        "configured, explain how to connect one and then stop.",
        "every 30m",
        "Important-mail monitor",
        "origin",
    },
    {
        "catalog:weekly-review",
        "Weekly review",
        "Every Sunday evening, a recap of the week: what got done, "
        "what's still open, and what's coming up next week.",
        "Produce a weekly review for the user: summarize what was "
        "accomplished this week, list still-open items, and preview "
        "next week's calendar. Pull from whatever sources are connected "
        "(calendar, task tools, recent conversations). Keep it tight.",
        "0 18 * * 0",
        "Weekly review",
        "origin",
    },
    {
        "catalog:standup-reminder",
        "Workday start reminder",
        "A weekday nudge at 9am with your day's agenda and top "
        "priorities, so you start focused.",
        "Give the user a brief weekday start-of-day nudge: their "
        "calendar for today and the 1-3 highest-priority things to "
        "focus on, inferred from recent context and any task tools. "
        "Encouraging, short, one message.",
        "0 9 * * 1-5",
        "Workday start reminder",
        "origin",
    },
};

#define CATALOG_COUNT (sizeof(CATALOG) / sizeof(CATALOG[0]))

const catalog_entry_t *suggestion_catalog(size_t *count) {
    if (count) *count = CATALOG_COUNT;
    return CATALOG;
}

/* PoP: classify_items_script_path @ cron/suggestion_catalog.py:classify_items_script_path */
char *classify_items_script_path(char *buf, size_t bufsz) {
    /* Python resolves <cron package dir>/scripts/classify_items.py. In the C
     * runtime the install root is $HERMES_ROOT (fallback: current directory). */
    const char *root = getenv("HERMES_ROOT");
    if (root && root[0])
        snprintf(buf, bufsz, "%s/cron/scripts/classify_items.py", root);
    else
        snprintf(buf, bufsz, "cron/scripts/classify_items.py");
    return buf;
}

/* Build a job_spec JSON object from a catalog entry. */
static json_t *entry_job_spec(const catalog_entry_t *e) {
    json_t *spec = json_new_object();
    json_object_set(spec, "prompt", json_new_string(e->prompt));
    json_object_set(spec, "schedule", json_new_string(e->schedule));
    json_object_set(spec, "name", json_new_string(e->name));
    json_object_set(spec, "deliver", json_new_string(e->deliver));
    return spec;
}

/* PoP: seed_catalog_suggestions @ cron/suggestion_catalog.py:seed_catalog_suggestions */
json_t *seed_catalog_suggestions(catalog_add_fn add_fn, const char *const *keys) {
    if (add_fn == NULL) add_fn = cron_sugg_add;

    json_t *created = json_new_array();
    for (size_t i = 0; i < CATALOG_COUNT; i++) {
        const catalog_entry_t *e = &CATALOG[i];
        if (keys != NULL) {
            int wanted = 0;
            for (const char *const *k = keys; *k; k++) {
                if (strcmp(*k, e->key) == 0) { wanted = 1; break; }
            }
            if (!wanted) continue;
        }
        json_t *spec = entry_job_spec(e);
        json_t *rec = add_fn(e->title, e->description, "catalog", spec, e->key);
        json_free(spec);
        if (rec != NULL) json_array_append(created, rec);
    }
    return created;
}
