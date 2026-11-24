/*
 * noc_w20_windowspro_18c6a5.h -- public C API for the port of
 *   gateway/windows_proactor_accept.py (wave 20 batch 9).
 *
 * The live module wraps an asyncio proactor's accept() so that a client which
 * vanished from the listen queue does not close the listener. The pure parts
 * ported here are the peer-drop classifier, the install decision and the
 * retry choreography, replayed over a scripted accept sequence instead of a
 * live proactor.
 */
#ifndef NOC_W20_WINDOWSPRO_18C6A5_H
#define NOC_W20_WINDOWSPRO_18C6A5_H

#include "hermes_json.h"

/* Shape of one raised exception, as the gate can model it. */
typedef struct {
    int is_reset;    /* ConnectionResetError / ConnectionAbortedError */
    int is_oserror;  /* any OSError subclass (what `except OSError` catches) */
    const char *type;
    int winerror;    /* -1 when the exception carries no winerror attribute */
} w20_windowspro_18c6a5_exc;

/* Parse {"type": str, "winerror": int|null} into *out. Missing/null reads as
 * OSError with no winerror, which is what getattr(exc, "winerror", None) does. */
void w20_windowspro_18c6a5_exc_from_json(const json_t *node, w20_windowspro_18c6a5_exc *out);

/* dropped_peer(exc): the accept failed because its client went away. */
int w20_windowspro_18c6a5_dropped_peer(const w20_windowspro_18c6a5_exc *exc);

/* install_proactor_accept_guard(loop) as a decision: a guard is installed only
 * for a BaseProactorEventLoop that exposes a proactor with a callable accept
 * which is not already guarded. */
int w20_windowspro_18c6a5_should_install(int is_proactor_loop, int has_proactor,
                                         int accept_callable, int already_guarded);

/* Replay GuardedAccept.__call__ over a scripted accept sequence.
 * `script` is an array of {"kind":"ok"|"cancel"|"error"|"raise", "conn":str,
 * "type":str, "winerror":int|null}. `outer_cancelled` models a consumer that
 * cancels the returned future before the first callback runs.
 * Returns {"events":[...], "outer":{...}, "closed":[...]} where outer.state is
 * one of "result" | "exception" | "cancelled" | "raised" | "pending". */
json_t *w20_windowspro_18c6a5_replay(const json_t *script, int outer_cancelled);

#endif /* NOC_W20_WINDOWSPRO_18C6A5_H */
