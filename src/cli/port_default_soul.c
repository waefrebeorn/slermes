/* Slermes C port — hermes_cli/default_soul.py (pure SOUL comparison helpers) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Faithful copy of _LEGACY_TEMPLATE_SOULS[0] and [1] from default_soul.py.
 * These carry zero user intent (comment-only scaffolds); a SOUL.md matching
 * one of them is safe to upgrade in place. */
static const char *LEGACY0 =
    "# Hermes Agent Persona\n"
    "\n"
    "<!--\n"
    "This file defines the agent's personality and tone.\n"
    "The agent will embody whatever you write here.\n"
    "Edit this to customize how Hermes communicates with you.\n"
    "\n"
    "Examples:\n"
    "  - \"You are a warm, playful assistant who uses kaomoji occasionally.\"\n"
    "  - \"You are a concise technical expert. No fluff, just facts.\"\n"
    "  - \"You speak like a friendly coworker who happens to know everything.\"\n"
    "\n"
    "This file is loaded fresh each message -- no restart needed.\n"
    "Delete the contents (or this file) to use the default personality.\n"
    "-->";
static const char *LEGACY1 =
    "# Hermes Agent Persona\n"
    "\n"
    "<!--\n"
    "This file defines the agent's personality and tone.\n"
    "The agent will embody whatever you write here.\n"
    "Edit this to customize how Hermes communicates with you.\n"
    "\n"
    "This file is loaded fresh each message -- no restart needed.\n"
    "Delete the contents (or this file) to use the default personality.\n"
    "-->";

/* PoP: hermes_cli_default_soul__normalize_soul @ hermes_cli/default_soul.py:_normalize_soul */
char *hermes_cli_default_soul_normalize(const char *text)
{
    if (!text) text = "";
    size_t n = strlen(text);
    char *out = malloc(n + 1);
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        char c = text[i];
        if (c == '\r') {
            if (i + 1 < n && text[i + 1] == '\n') i++;  /* CRLF -> LF */
            /* lone CR -> LF (already dropped) */
            out[o++] = '\n';
        } else {
            out[o++] = c;
        }
    }
    /* strip leading UTF-8 BOM (EF BB BF) */
    size_t start = 0;
    if (o >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB &&
        (unsigned char)out[2] == 0xBF) start = 3;
    /* trim trailing whitespace */
    size_t end = o;
    while (end > start && (out[end - 1] == ' ' || out[end - 1] == '\t' ||
           out[end - 1] == '\n' || out[end - 1] == '\r')) end--;
    /* trim leading whitespace */
    while (start < end && (out[start] == ' ' || out[start] == '\t' ||
           out[start] == '\n' || out[start] == '\r')) start++;
    memmove(out, out + start, end - start);
    out[end - start] = '\0';
    return out;
}

/* PoP: hermes_cli_default_soul__is_legacy_template_soul @ hermes_cli/default_soul.py:is_legacy_template_soul */
bool hermes_cli_default_soul_is_legacy(const char *text)
{
    char *norm = hermes_cli_default_soul_normalize(text);
    char *l0 = hermes_cli_default_soul_normalize(LEGACY0);
    char *l1 = hermes_cli_default_soul_normalize(LEGACY1);
    bool r = (strcmp(norm, l0) == 0) || (strcmp(norm, l1) == 0);
    free(norm); free(l0); free(l1);
    return r;
}
