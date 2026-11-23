#ifndef NOC_CMD_SINK_H
#define NOC_CMD_SINK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: CommandSink/notices/prompt/display @ gateway/session_command_sink.py:CommandSink
 * PoP: capture/active_sink @ gateway/session_command_sink.py:capture
 *
 * A C model of the ContextVar-held CommandSink stack.  The active sink is the
 * top of the stack; depth 0 means `_SINK.get()` returned None.
 */
typedef struct {
    size_t      notices;        /* number of entries in notices: list[str] */
    const char *prompt;         /* NULL models None */
    const char *display;        /* NULL models None */
} cmdsink_sink_t;

/* Push a fresh CommandSink (notices=[], prompt=None, display=None); return the
 * new depth.  Pairs with cmdsink_release() for the contextmanager exit. */
int  cmdsink_capture(void);
/* Pop the most recent sink; return the remaining depth. */
int  cmdsink_release(void);
/* Current stack depth (0 == no active sink). */
int  cmdsink_depth(void);
/* Snapshot the active sink.  Returns 1 on success, 0 when no sink is active. */
int  cmdsink_active(cmdsink_sink_t *out);
/* The i-th stored notice of the active sink (borrowed), or NULL if absent. */
const char *cmdsink_notice_at(size_t index);
/* Mutate the active sink (no-op when no sink is active). */
void cmdsink_add_notice(const char *text);
void cmdsink_set_prompt(const char *value);   /* NULL -> None */
void cmdsink_set_display(const char *value);  /* NULL -> None */

#ifdef __cplusplus
}
#endif

#endif /* NOC_CMD_SINK_H */
