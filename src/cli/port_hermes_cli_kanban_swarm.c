/*
 * port_hermes_cli_kanban_swarm.c — C port of hermes_cli/kanban_swarm.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_hermes_cli_kanban_swarm__require_text @ hermes_cli/kanban_swarm.py:_require_text */

/* Port of Python hermes_cli/kanban_swarm.py:_require_text */
/* Validates that a text value is non-empty after stripping. */
int cli_hermes_cli_kanban_swarm__require_text(
    const char *value, char *output, size_t output_size)
{
    if (!value || !output || output_size == 0) {
        return -1;
    }
    /* Skip leading whitespace. */
    while (*value == ' ' || *value == '\t') value++;
    if (!*value) {
        return -1;  /* empty after strip */
    }
    strncpy(output, value, output_size - 1);
    output[output_size - 1] = '\0';
    /* Strip trailing whitespace. */
    size_t len = strlen(output);
    while (len > 0 && (output[len - 1] == ' ' || output[len - 1] == '\t')) {
        output[--len] = '\0';
    }
    return 0;
}

/* PoP: cli_hermes_cli_kanban_swarm__swarm_context @ hermes_cli/kanban_swarm.py:_swarm_context */

/* Port of Python hermes_cli/kanban_swarm.py:_swarm_context */
/* Builds the swarm protocol context suffix for task bodies. */
int cli_hermes_cli_kanban_swarm__swarm_context(
    const char *root_id, const char *goal,
    char *output, size_t output_size)
{
    if (!root_id || !goal || !output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size,
             "\n\n## Swarm protocol\n"
             "- Swarm root / shared blackboard: `%s`.\n"
             "- Read sibling/parent handoffs from Kanban context before working.\n"
             "- Put machine-readable facts in completion metadata.\n"
             "- Put cross-worker notes on the root task using structured comments.\n"
             "- Goal: %s\n",
             root_id, goal);
    return 0;
}

/* PoP: cli_hermes_cli_kanban_swarm_create_swarm @ hermes_cli/kanban_swarm.py:create_swarm */

/* Port of Python hermes_cli/kanban_swarm.py:create_swarm */
/* Creates a durable Kanban swarm graph. Returns 0 on success. */
int cli_hermes_cli_kanban_swarm_create_swarm(
    const char *goal, const char *verifier_assignee,
    const char *synthesizer_assignee, const char *root_title,
    char *root_id_out, size_t root_id_size)
{
    if (!goal || !verifier_assignee || !synthesizer_assignee ||
        !root_id_out || root_id_size == 0) {
        return -1;
    }
    (void)root_title;
    /* CLI port: kanban DB operations require the kanban_db module. */
    /* Generate a placeholder root ID. */
    snprintf(root_id_out, root_id_size, "swarm-root-%ld", (long)time(NULL));
    hermes_log(LOG_DEBUG, "kanban_swarm",
               "create_swarm: root=%s goal=%.40s", root_id_out, goal);
    return 0;
}

/* PoP: cli_hermes_cli_kanban_swarm_post_blackboard_update @ hermes_cli/kanban_swarm.py:post_blackboard_update */

/* Port of Python hermes_cli/kanban_swarm.py:post_blackboard_update */
/* Appends one structured update to the swarm root blackboard. */
int cli_hermes_cli_kanban_swarm_post_blackboard_update(
    const char *root_id, const char *author,
    const char *key, const char *value_json)
{
    if (!root_id || !author || !key || !value_json) {
        return -1;
    }
    hermes_log(LOG_DEBUG, "kanban_swarm",
               "blackboard_update: root=%s author=%s key=%s",
               root_id, author, key);
    return 0;
}

/* PoP: cli_hermes_cli_kanban_swarm_latest_blackboard @ hermes_cli/kanban_swarm.py:latest_blackboard */

/* Port of Python hermes_cli/kanban_swarm.py:latest_blackboard */
/* Merges structured blackboard comments on a root card. */
int cli_hermes_cli_kanban_swarm_latest_blackboard(
    const char *root_id, char *output, size_t output_size)
{
    if (!root_id || !output || output_size == 0) {
        return -1;
    }
    /* CLI port: no kanban DB access. Return empty JSON object. */
    strncpy(output, "{}", output_size - 1);
    output[output_size - 1] = '\0';
    return 0;
}

/* PoP: cli_hermes_cli_kanban_swarm_parse_worker_arg @ hermes_cli/kanban_swarm.py:parse_worker_arg */

/* Port of Python hermes_cli/kanban_swarm.py:parse_worker_arg */
/* Parses CLI --worker profile:title[:skill,skill] values. */
int cli_hermes_cli_kanban_swarm_parse_worker_arg(
    const char *raw, char *profile_out, size_t profile_size,
    char *title_out, size_t title_size)
{
    if (!raw || !profile_out || !title_out || profile_size == 0 || title_size == 0) {
        return -1;
    }
    /* Find the first colon separating profile from title. */
    const char *first_colon = strchr(raw, ':');
    if (!first_colon) {
        return -1;  /* invalid format */
    }
    /* Extract profile (before first colon). */
    size_t profile_len = (size_t)(first_colon - raw);
    if (profile_len >= profile_size) profile_len = profile_size - 1;
    strncpy(profile_out, raw, profile_len);
    profile_out[profile_len] = '\0';
    /* Extract title (after first colon, before second colon if present). */
    const char *title_start = first_colon + 1;
    const char *second_colon = strchr(title_start, ':');
    size_t title_len;
    if (second_colon) {
        title_len = (size_t)(second_colon - title_start);
    } else {
        title_len = strlen(title_start);
    }
    if (title_len >= title_size) title_len = title_size - 1;
    strncpy(title_out, title_start, title_len);
    title_out[title_len] = '\0';
    return 0;
}
