/* port_agent_reactions.c
 *
 * Pure regex-based reaction detection.
 * Faithful C11 port of agent/reactions.py.
 *
 * PoP: reactions_detect @ agent/reactions.py:detect_reaction
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "hermes_reactions.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <regex.h>
#include <pthread.h>

/* Unicode codepoints for hearts + affection emoji (same set as Python). */
static const char HEART_CHARS[] =
    "\u2764\u2665"
    "\U0001F970\U0001F60D\U0001F618"
    "\U0001F495\U0001F496\U0001F497\U0001F49E"
    "\U0001F49B\U0001F49C\U0001F49A\U0001F499"
    "\U0001F493\U0001F498\U0001F49D\U0001FA77";

static regex_t *compile_reactions_regex(void)
{
    const char *prefix =
        "\\bgood\\s*bot\\b"
        "|\\bi\\s*(?:love|luv)\\s*(?:you|u|ya)\\b"
        "|\\b(?:love|luv)\\s*(?:you|u|ya)\\b"
        "|\\bily(?:sm)?\\b"
        "|\\bthank\\s*(?:you|u)\\b"
        "|\\b(?:thanks|thx|tysm|ty)\\b"
        "|<3+";

    size_t plen = strlen(prefix);
    size_t hlen = strlen(HEART_CHARS);
    char *full = (char *)malloc(plen + 2 + hlen + 2);
    if (!full) return NULL;
    memcpy(full, prefix, plen);
    full[plen] = '[';
    memcpy(full + plen + 1, HEART_CHARS, hlen);
    full[plen + 1 + hlen] = ']';
    full[plen + 1 + hlen + 1] = '\0';

    regex_t *re = (regex_t *)malloc(sizeof(regex_t));
    if (!re) { free(full); return NULL; }
    int rc = regcomp(re, full, REG_EXTENDED | REG_ICASE);
    free(full);
    if (rc != 0) { free(re); return NULL; }
    return re;
}

static regex_t *g_reactions_re = NULL;
static pthread_once_t g_reactions_once = PTHREAD_ONCE_INIT;

static void reactions_init(void)
{
    g_reactions_re = compile_reactions_regex();
}

/* PoP: reactions_detect @ agent/reactions.py:detect_reaction */
const char *reactions_detect(const char *text)
{
    if (!text || !text[0]) return NULL;

    pthread_once(&g_reactions_once, reactions_init);
    regex_t *re = g_reactions_re;
    if (!re) return NULL;

    int rc = regexec(re, text, 0, NULL, 0);
    return (rc == 0) ? "vibe" : NULL;
}
