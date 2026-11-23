#ifndef NOC_W7_SESSIONSUR_F0A029_H
#define NOC_W7_SESSIONSUR_F0A029_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_sessionsour_f0a029_create_surface @ agent/session_source.py:create_surface
 * "desktop" maps to the "gui" create surface; other sources are unchanged.
 * Returns a pointer to `source` or to a static literal (never malloc'd). */
const char *w7_sessionsour_f0a029_create_surface(const char *source);

/* PoP: w7_sessionsour_f0a029_session_source_for @ agent/session_source.py:session_source_for
 * Seam: `source`/`single_query`/`explicit` stand in for the three env lookups.
 * Returns a malloc'd string; caller frees. */
char *w7_sessionsour_f0a029_session_source_for(const char *platform, const char *source,
                                               bool single_query, bool explicit);

/* PoP: w7_sessionsour_f0a029_is_ui_transport @ agent/session_source.py:UI_TRANSPORT_SOURCES */
bool w7_sessionsour_f0a029_is_ui_transport(const char *source);
/* PoP: w7_sessionsour_f0a029_is_cli_family @ agent/session_source.py:CLI_FAMILY_SOURCES */
bool w7_sessionsour_f0a029_is_cli_family(const char *source);
/* PoP: w7_sessionsour_f0a029_is_daybreak @ agent/session_source.py:DAYBREAK_SOURCES */
bool w7_sessionsour_f0a029_is_daybreak(const char *source);
/* PoP: w7_sessionsour_f0a029_is_attended @ agent/session_source.py:ATTENDED_SOURCES */
bool w7_sessionsour_f0a029_is_attended(const char *source);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_SESSIONSUR_F0A029_H */
