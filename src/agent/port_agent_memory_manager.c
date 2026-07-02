/*
 * port_agent_memory_manager.c — Port of Python agent/memory_manager.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _strip_skill_scaffolding */
char *strip_skill_scaffolding(const char *text) {
    if (!text) return NULL;
    
    /* Check if this is a skill invocation */
    const char *skill_marker = "/skill";
    const char *bundle_marker = "/bundle";
    
    if (strstr(text, skill_marker) || strstr(text, bundle_marker)) {
        /* Extract user instruction from skill message */
        const char *instr_start = strstr(text, "User instruction:");
        if (instr_start) {
            instr_start += 17; /* Skip "User instruction:" */
            while (*instr_start == ' ') instr_start++;
            return strdup(instr_start);
        }
        /* Bare skill invocation — no user content */
        return NULL;
    }
    
    /* Non-skill message — pass through */
    return strdup(text);
}

