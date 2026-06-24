/*
 * port_agent_skill_commands.c — Port of Python agent/skill_commands.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _extract_bundle_user_instruction */
const char *extract_bundle_user_instruction(const char *message) {
    if (!message) return NULL;
    
    /* Extract user instruction from a bundle invocation message */
    const char *marker = "User instruction:";
    const char *instr = strstr(message, marker);
    if (!instr) return NULL;
    
    instr += strlen(marker);
    while (*instr == ' ') instr++;
    
    if (*instr == '\0') return NULL;
    return instr;
}


/* Port of Python: _extract_single_skill_user_instruction */
const char *extract_single_skill_user_instruction(const char *message) {
    if (!message) return NULL;
    
    /* Same as bundle instruction extraction for single skills */
    return extract_bundle_user_instruction(message);
}


/* Port of Python: extract_user_instruction_from_skill_message */
char *extract_user_instruction_from_skill_message(const char *message) {
    if (!message) return NULL;
    
    const char *instr = extract_bundle_user_instruction(message);
    if (instr) return strdup(instr);
    
    /* Check for bare skill invocation (no instruction) */
    if (strstr(message, "/skill") || strstr(message, "/bundle")) {
        return NULL; /* No user content worth remembering */
    }
    
    /* Non-skill message — pass through */
    return strdup(message);
}

