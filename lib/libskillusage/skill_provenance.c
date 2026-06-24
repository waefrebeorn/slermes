/*
 * skill_provenance.c — Runtime skill write-origin tracking.
 *
 * PoP: skill_provenance_set @ skill_provenance:skill_provenance_set
 * PoP: skill_provenance_reset @ skill_provenance:skill_provenance_reset
 * PoP: skill_provenance_get @ skill_provenance:skill_provenance_get
 * PoP: skill_provenance_is_background_review @ skill_provenance:skill_provenance_is_background_review
 *
 * Port of Python tools/skill_provenance.py.
 */

#include "skill_provenance.h"
#include <string.h>

/* Current write origin, defaulting to foreground */
static char g_write_origin[SKILL_PROVENANCE_MAX_ORIGIN] = "foreground";

/* PoP: skill_provenance_set @ skill_provenance:skill_provenance_set */
skill_provenance_token_t skill_provenance_set(const char *origin)
{
    skill_provenance_token_t token;
    /* Save current value */
    strncpy(token.saved, g_write_origin, sizeof(token.saved) - 1);
    token.saved[sizeof(token.saved) - 1] = '\0';

    /* Set new value */
    if (origin && origin[0] != '\0') {
        strncpy(g_write_origin, origin, sizeof(g_write_origin) - 1);
    } else {
        strncpy(g_write_origin, "foreground", sizeof(g_write_origin) - 1);
    }
    g_write_origin[sizeof(g_write_origin) - 1] = '\0';

    return token;
}

/* PoP: skill_provenance_reset @ skill_provenance:skill_provenance_reset */
void skill_provenance_reset(skill_provenance_token_t token)
{
    strncpy(g_write_origin, token.saved, sizeof(g_write_origin) - 1);
    g_write_origin[sizeof(g_write_origin) - 1] = '\0';
}

/* PoP: skill_provenance_get @ skill_provenance:skill_provenance_get */
const char *skill_provenance_get(void)
{
    return g_write_origin;
}

/* PoP: skill_provenance_is_background_review @ skill_provenance:skill_provenance_is_background_review */
bool skill_provenance_is_background_review(void)
{
    return strcmp(g_write_origin, SKILL_PROVENANCE_BACKGROUND_REVIEW) == 0;
}
