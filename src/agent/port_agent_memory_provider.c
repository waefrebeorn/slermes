/*
 * port_agent_memory_provider.c — Port of Python agent/memory_provider.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: backup_paths */
typedef struct {
    char paths[16][1024];
    int count;
} backup_path_list_t;

backup_path_list_t memory_provider_backup_paths(void) {
    backup_path_list_t result = {0};
    /* Return external paths that store provider state outside HERMES_HOME */
    const char *home = getenv("HOME");
    if (!home) return result;
    
    /* Common external memory provider paths */
    const char *external_paths[] = {
        "/.honcho",
        "/.hindsight",
        "/.openviking",
        NULL
    };
    
    for (int i = 0; external_paths[i] && result.count < 16; i++) {
        snprintf(result.paths[result.count], 1024, "%s%s", home, external_paths[i]);
        result.count++;
    }
    return result;
}


/* PoP: is_trailing_utf8_punct @ agent/memory_provider.py:is_trivial_prompt */

/* Trivial words: (yes|no|ok|okay|sure|thanks|thank you|y|n|yep|nope|yeah|nah|
 * hi|hey|hello|yo|sup|continue|go ahead|do it|proceed|got it|cool|nice|great|
 * done|next|lgtm|k) — case-insensitive, anchored; only whitespace/punctuation
 * may follow (so "k8s" / "note" / "hindsight" do NOT match). */
static const char *TRIVIAL_WORDS[] = {
    "yes","no","ok","okay","sure","thanks","thank you","y","n","yep","nope",
    "yeah","nah","hi","hey","hello","yo","sup","continue","go ahead","do it",
    "proceed","got it","cool","nice","great","done","next","lgtm","k",
    NULL
};

/* Return 1 when the UTF-8 sequence at *p is one of the allowed trailing
 * punctuation codepoints: \u2018 \u2019 \u201c \u201d \u2014 \u2013 \u2026
 * \u00a0. Advances *p past it. */
static int is_trailing_utf8_punct(const char **p) {
    const unsigned char *u = (const unsigned char *)*p;
    if (u[0] == 0xC2 && u[1] == 0xA0) { *p += 2; return 1; }            /* \u00a0 */
    if (u[0] == 0xE2 && u[1] == 0x80) {
        unsigned char b = u[2];
        if (b == 0x98 || b == 0x99 || b == 0x9C || b == 0x9D ||          /* ' ' " " */
            b == 0x94 || b == 0x93 || b == 0xA6) {                      /* em dash, en dash, ellipsis */
            *p += 3; return 1;
        }
    }
    return 0;
}

/* Return 1 when the remainder is only whitespace / allowed punctuation. */
static int trivial_trailing_only(const char *s) {
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (isspace(c)) { s++; continue; }
        if (strchr("!?.:;,\"'~()[]{}<>*&^%$#@!+=`", c)) { s++; continue; }
        if (c >= 0x80) {
            if (is_trailing_utf8_punct(&s)) continue;
            return 0;
        }
        return 0;
    }
    return 1;
}

int memory_provider_is_trivial_prompt(const char *text) {
    if (!text || !*text) return 1;
    /* strip */
    const char *p = text;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 1;
    if (*p == '/') return 1;  /* slash command */

    /* Case-insensitive word-prefix match + trailing-only remainder. */
    for (int i = 0; TRIVIAL_WORDS[i]; i++) {
        const char *w = TRIVIAL_WORDS[i];
        size_t wl = strlen(w);
        size_t pl = strlen(p);
        if (pl < wl) continue;
        /* casefold compare */
        size_t j;
        for (j = 0; j < wl; j++) {
            unsigned char a = (unsigned char)p[j], b = (unsigned char)w[j];
            if (tolower(a) != tolower(b)) break;
        }
        if (j == wl && trivial_trailing_only(p + wl)) return 1;
    }
    return 0;
}
