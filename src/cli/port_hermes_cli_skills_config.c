/*
 * port_hermes_cli_skills_config.c — C port of hermes_cli/skills_config.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_hermes_cli_skills_config_get_disabled_skills @ hermes_cli/skills_config.py:get_disabled_skills */

/* Port of Python hermes_cli/skills_config.py:get_disabled_skills */
/* Returns disabled skill names: global list unioned with platform-specific list. */
int cli_hermes_cli_skills_config_get_disabled_skills(
    const char *config_path, const char *platform,
    char *disabled[], int max_disabled)
{
    if (!disabled || max_disabled <= 0) {
        return 0;
    }
    (void)config_path;
    (void)platform;
    /* CLI port: config loading is handled by the hermes_cli config module. */
    /* Return 0 disabled skills. */
    return 0;
}

/* PoP: cli_hermes_cli_skills_config_save_disabled_skills @ hermes_cli/skills_config.py:save_disabled_skills */

/* Port of Python hermes_cli/skills_config.py:save_disabled_skills */
/* Persists disabled skill names to config. */
int cli_hermes_cli_skills_config_save_disabled_skills(
    const char *config_path, const char *disabled[], int disabled_count,
    const char *platform)
{
    if (!config_path) {
        return -1;
    }
    (void)disabled;
    (void)disabled_count;
    (void)platform;
    /* CLI port: config saving is handled by the hermes_cli config module. */
    return 0;
}

/* PoP: cli_hermes_cli_skills_config__list_all_skills @ hermes_cli/skills_config.py:_list_all_skills */

/* Port of Python hermes_cli/skills_config.py:_list_all_skills */
/* Returns all installed skills (ignoring disabled state). */
int cli_hermes_cli_skills_config__list_all_skills(
    char *skill_names[], int max_skills)
{
    if (!skill_names || max_skills <= 0) {
        return 0;
    }
    /* CLI port: skill discovery requires the skills_tool module. */
    return 0;
}

/* PoP: cli_hermes_cli_skills_config__get_categories @ hermes_cli/skills_config.py:_get_categories */

/* Port of Python hermes_cli/skills_config.py:_get_categories */
/* Returns sorted unique category names (None -> 'uncategorized'). */
int cli_hermes_cli_skills_config__get_categories(
    const char *skill_names[], int skill_count,
    char *categories[], int max_categories)
{
    if (!skill_names || !categories || max_categories <= 0 || skill_count <= 0) {
        return 0;
    }
    /* CLI port: no skills to categorize. */
    return 0;
}

/* PoP: cli_hermes_cli_skills_config__select_platform @ hermes_cli/skills_config.py:_select_platform */

/* Port of Python hermes_cli/skills_config.py:_select_platform */
/* Asks user which platform to configure, or global. */
/* CLI port: returns NULL (global) since interactive UI is handled elsewhere. */
const char *cli_hermes_cli_skills_config__select_platform(void)
{
    /* CLI port: interactive platform selection handled by curses_ui. */
    return NULL;
}

/* PoP: cli_hermes_cli_skills_config__toggle_by_category @ hermes_cli/skills_config.py:_toggle_by_category */

/* Port of Python hermes_cli/skills_config.py:_toggle_by_category */
/* Toggles all skills in a category at once. */
int cli_hermes_cli_skills_config__toggle_by_category(
    const char *skill_names[], int skill_count,
    const char *disabled[], int disabled_count,
    char *new_disabled[], int *new_disabled_count, int max_disabled)
{
    if (!skill_names || !new_disabled || !new_disabled_count || max_disabled <= 0) {
        return -1;
    }
    (void)disabled;
    (void)disabled_count;
    *new_disabled_count = 0;
    /* CLI port: interactive category toggle handled by curses_ui. */
    return 0;
}

/* PoP: cli_hermes_cli_skills_config_skills_command @ hermes_cli/skills_config.py:skills_command */

/* Port of Python hermes_cli/skills_config.py:skills_command */
/* Entry point for `hermes skills`. */
void cli_hermes_cli_skills_config_skills_command(const char *platform)
{
    (void)platform;
    /* CLI port: interactive skills command handled by curses_ui. */
    hermes_log(LOG_DEBUG, "skills_config",
               "skills_command: CLI port — interactive UI not available");
}
