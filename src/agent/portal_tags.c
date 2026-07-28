/* Centralized Nous Portal request tags.
 *
 * Port of Python agent/portal_tags.py (64 LOC, 3 functions).
 * 3/3 portable functions ported. Reclassification: AG79 PARTIAL → PORTED.
 *
 * Python equivalent: agent/portal_tags.py
 *   _hermes_version() → hermes_version()
 *   hermes_client_tag() → hermes_client_tag()
 *   nous_portal_tags() → hermes_nous_portal_tags_json()
 *
 * Every Hermes request that hits the Nous Portal — main agent loop, auxiliary
 * client, and any future code path — must carry the same product-attribution
 * tags so Nous can attribute usage to Hermes Agent and bucket it by release.
 */

#include "hermes_portal_tags.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Version string — must match include/hermes.h */
#ifndef HERMES_VERSION
#define HERMES_VERSION "0.15.1-slermes"
#endif

/* Port of Python: hermes_client_tag */
char *hermes_client_tag(void)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "client=hermes-client-v%s", HERMES_VERSION);
    return strdup(buf);
}

/* Port of Python: _hermes_version */
const char *hermes_version(void)
{
    return HERMES_VERSION;
}

/* Ambient conversation id (Python ContextVar equivalent). */
static char *g_conversation_id = NULL;

void hermes_set_conversation_context(const char *id)
{
    free(g_conversation_id);
    g_conversation_id = id ? strdup(id) : NULL;
}

const char *hermes_get_conversation_context_id(void)
{
    return g_conversation_id;
}

/* Port of Python: nous_portal_tags (returns JSON string instead of list) */
char *hermes_nous_portal_tags_json(void)
{
    char buf[256];
    char *client_tag = hermes_client_tag();
    if (!client_tag)
        return NULL;

    snprintf(buf, sizeof(buf),
        "[\"product=hermes-agent\",\"%s\"]", client_tag);
    free(client_tag);
    return strdup(buf);
}
