#ifndef NOC_KANTOOL_H
#define NOC_KANTOOL_H
/* C port of tools/kanban_toolset_context.py (gate: kantool).
 * A ContextVar is modelled as a tri-state int: -1 = unset (Python None),
 * 0 = False, 1 = True. scoped_kanban_toolset_selection sets it to
 * ("kanban" in (toolsets or ())) on entry and restores the token on exit. */
#include <stddef.h>

int kantool_requested(void);                 /* current tri-state value */
int kantool_enter(int requested);            /* set value, return prior token */
void kantool_exit(int token);                /* restore prior token */
int kantool_explicit(const char *const *toolsets, size_t n); /* "kanban" in (toolsets or ()) */
void kantool_reset(void);                    /* reset ContextVar to None */

#endif /* NOC_KANTOOL_H */
