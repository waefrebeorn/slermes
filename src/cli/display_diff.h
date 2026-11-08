/*
 * display_diff.h — skin-aware ANSI unified-diff rendering.
 *
 * Module-internal header (display_core.c <-> display_diff.c).
 */
#ifndef DISPLAY_DIFF_H
#define DISPLAY_DIFF_H

#include <stddef.h>
#include "skin.h"   /* skin_t */

/* Active skin for display styling (defined in display_core.c) */
extern skin_t *g_display_skin;

void diff_ansi_init(void);
char * display_render_diff_section(const char *section);

#endif /* DISPLAY_DIFF_H */
