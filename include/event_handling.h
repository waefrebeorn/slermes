#ifndef EVENT_HANDLING_H
#define EVENT_HANDLING_H

#include "app_state.h"
#include "gui_core.h"
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════════
 * Event Handling
 * ══════════════════════════════════════════════════════════════════════ */

/* Process a single event */
bool event_process(app_state_t *app, gc_event_t *ev);

/* Main event loop */
void event_run(app_state_t *app);

/* Handle resize */
void event_handle_resize(app_state_t *app, int w, int h);

/* Handle key press */
bool event_handle_key(app_state_t *app, int key, int mod);

/* Handle text input */
bool event_handle_text(app_state_t *app, const char *text);

/* Handle mouse events */
bool event_handle_mouse(app_state_t *app, gc_event_t *ev);

#endif /* EVENT_HANDLING_H */