/*
 * port_slack_cli.h -- pure helper ported from hermes_cli/slack_cli.py.
 *
 * The slash-command discovery (slack_native_slashes -> the command registry
 * and plugin entries) stays in Python; the manifest assembly is here.
 */
#ifndef PORT_SLACK_CLI_H
#define PORT_SLACK_CLI_H

#include <stddef.h>

/* slack_cli.py:_build_full_manifest -- build the full Slack app manifest JSON.
 *
 *   bot_name, bot_description   display_information (name capped at 35 chars,
 *                               description at 140)
 *   include_assistant           selects "assistant" when messaging_experience
 *                               is NULL
 *   messaging_experience        "assistant" | "agent" | "none" (case- and
 *                               whitespace-insensitive); NULL derives it
 *   long_description            optional; omitted when NULL
 *   slashes                     flat NULL-terminated
 *                               [name, description, usage_hint, ...] array,
 *                               the shape slack_native_slashes() returns
 *   err                         receives 1 when messaging_experience is
 *                               invalid (the Python raises ValueError)
 *
 * Returns malloc'd JSON; caller frees. NULL when err is set or on OOM.
 */
char *sl_build_full_manifest(const char *bot_name, const char *bot_description,
                             int include_assistant, const char *messaging_experience,
                             const char *long_description,
                             const char *const *slashes, int *err);

#endif /* PORT_SLACK_CLI_H */
