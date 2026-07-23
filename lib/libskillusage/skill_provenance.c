/*
 * skill_provenance.c — Runtime skill write-origin tracking.
 *
 * Port of Python tools/skill_provenance.py.
 *
 * Tracks whether skill writes (skill_manage create/patch/edit) originate
 * from a foreground user action ("foreground") or from the background
 * review fork ("background_review"). The curator only consolidates/prunes
 * skills created under "background_review" (agent-sediment writes).
 *
 * The Python source uses contextvars.ContextVar (per-async-task state).
 * The C build is single-threaded/synchronous, so a module-global variable
 * with a save/restore token is the faithful analog.
 */

#include "skill_provenance.h"
#include <string.h>

/* Current write origin, defaulting to foreground */
static char g_write_origin[SKILL_PROVENANCE_MAX_ORIGIN] = "foreground";

/* PoP: skill_provenance_set @ tools/skill_provenance.py:set_current_write_origin */
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

/* PoP: skill_provenance_reset @ tools/skill_provenance.py:reset_current_write_origin */
void skill_provenance_reset(skill_provenance_token_t token)
{
    strncpy(g_write_origin, token.saved, sizeof(g_write_origin) - 1);
    g_write_origin[sizeof(g_write_origin) - 1] = '\0';
}

/* PoP: skill_provenance_get @ tools/skill_provenance.py:get_current_write_origin */
const char *skill_provenance_get(void)
{
    return g_write_origin;
}

/* PoP: skill_provenance_is_background_review @ tools/skill_provenance.py:is_background_review */
bool skill_provenance_is_background_review(void)
{
    return strcmp(g_write_origin, SKILL_PROVENANCE_BACKGROUND_REVIEW) == 0;
}
