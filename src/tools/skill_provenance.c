/*
 * skill_provenance.c — canonical C11 port of tools/skill_provenance.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_skill_provenance_set_current_write_origin @ tools/skill_provenance.py:set_current_write_origin */
const char *skill_provenance_current_origin(void); /* forward */
int tools_skill_provenance_set_current_write_origin(const char *arg) {
    /* Python: set the current write-origin context (e.g. "background_review"). */
    char *g_o = (char *)skill_provenance_current_origin();
    if (arg && *arg) {
        snprintf(g_o, 128, "%s", arg);
    } else {
        g_o[0] = '\0';
    }
    return 0;
}

/* PoP: tools_skill_provenance_reset_current_write_origin @ tools/skill_provenance.py:reset_current_write_origin */
int tools_skill_provenance_reset_current_write_origin(const char *arg) {
    /* Python: clear the write-origin context. */
    (void)arg;
    return tools_skill_provenance_set_current_write_origin("");
}

/* PoP: tools_skill_provenance_get_current_write_origin @ tools/skill_provenance.py:get_current_write_origin */
int tools_skill_provenance_get_current_write_origin(const char *arg) {
    (void)arg;
    extern const char *skill_provenance_get(void);
    printf("%s\n", skill_provenance_get());
    return 0;
}

/* PoP: skill_provenance_current_origin @ tools/skill_provenance.py:is_background_review */
const char *skill_provenance_current_origin(void) {
    /* shared origin state, set by set_current_write_origin */
    static char g_origin[128];
    return g_origin;
}

/* === dissolved from ports_wrappers.c === */
int tools_skill_provenance_is_background_review(const char *arg) {
    /* Python: True while the current write origin is background_review. */
    (void)arg;
    return strcmp(skill_provenance_current_origin(), "background_review") == 0;
}
