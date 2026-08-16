/* Slermes C port — agent/prompt_builder.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: agent_prompt_builder_hud_surface_note_p @ agent/prompt_builder.py:hud_surface_note */
json_t *agent_prompt_builder_hud_surface_note_p(void) {
    return json_object();
}

/* PoP: agent_prompt_builder_agents_md_directory_chain_p @ agent/prompt_builder.py:_agents_md_directory_chain */
json_t *agent_prompt_builder_agents_md_directory_chain_p(void) {
    return json_object();
}
