/*
 * port_agent_skill_utils.c — Port of Python agent/skill_utils.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: is_skill_support_path */
bool is_skill_support_path(const char *path) {
    if (!path) return false;
    /* Check if path is within the skill support directory */
    const char *skill_support = getenv("HERMES_SKILL_SUPPORT");
    if (!skill_support) skill_support = "/etc/hermes/skills";
    
    size_t support_len = strlen(skill_support);
    return (strncmp(path, skill_support, support_len) == 0);
}

