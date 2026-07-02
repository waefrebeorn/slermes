/*
 * port_agent_title_generator.c — Port of Python agent/title_generator.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _title_language */
void title_language(char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    /* Read title language from config — simplified to env var */
    const char *lang = getenv("HERMES_TITLE_LANGUAGE");
    if (lang && *lang) {
        strncpy(out, lang, out_sz - 1);
        out[out_sz - 1] = '\0';
    } else {
        out[0] = '\0'; /* Empty = match user language */
    }
}

