/*
 * noc_w24_listenercl_afdef6.h - C surface for upstream
 * hermes_cli/dashboard_auth/listener_client.py (gate w24_listenercl_afdef6).
 *
 * The two module globals the listener/sign-in plugin handshake is made of
 * (_current and _swap) as an explicit context, so the whole decision surface is
 * a sequence of value operations: offer the plugin's provider swap, sign in with
 * a client on a portal until the returned undo runs, and read what is current.
 * The plugin itself, its provider and its discovery import are the seam; this
 * file owns only what the offered/current values decide.
 */
#ifndef NOC_W24_LISTENERCL_AFDEF6_H
#define NOC_W24_LISTENERCL_AFDEF6_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W24_LISTENERCL_AFDEF6_MAX_UNDOS 32

typedef struct {
    int registered;                          /* _swap is not None */
    json_t *client_id;                       /* _current[0], owned, may be NULL */
    json_t *portal_url;                      /* _current[1], owned, may be NULL */
    int swap_calls;                          /* times an offered swap ran */
    int restore_calls;                       /* times an undo's restore ran */
    int undo_count;
    int undo_alive[W24_LISTENERCL_AFDEF6_MAX_UNDOS];
} w24_listenercl_afdef6_ctx;

/* Fresh module state: nothing current, no swap offered yet. */
void w24_listenercl_afdef6_init(w24_listenercl_afdef6_ctx *ctx);

/* Release the owned current tuple. */
void w24_listenercl_afdef6_clear(w24_listenercl_afdef6_ctx *ctx);

/* offer(swap): the Nous sign-in plugin registered its provider swap. */
void w24_listenercl_afdef6_offer(w24_listenercl_afdef6_ctx *ctx);

/* use(client_id, portal_url): None where no swap was offered (nothing changes);
 * else the undo handle (an int id) after the swap ran, or null with *raised=1
 * where the swap raised -- the module resets _current before re-raising. */
json_t *w24_listenercl_afdef6_use(w24_listenercl_afdef6_ctx *ctx,
                                  const json_t *client_id,
                                  const json_t *portal_url,
                                  int swap_raises,
                                  int *raised);

/* current(): [client_id, portal_url] while a listener signs in with its own
 * client, else null. */
json_t *w24_listenercl_afdef6_current(const w24_listenercl_afdef6_ctx *ctx);

/* undo(id): clears _current then runs restore().  1 when it ran, 0 for a handle
 * that is unknown or already spent. */
int w24_listenercl_afdef6_undo(w24_listenercl_afdef6_ctx *ctx, int id);

/* The whole handshake over a scripted op list:
 *   {"offered": bool, "swap_raises": bool,
 *    "ops": [{"op": "offer"} | {"op": "use", "client_id": ..., "portal_url": ...}
 *            | {"op": "undo", "id": n} | {"op": "current"}]}
 * -> {"current": ..., "results": [...], "swap_calls": n, "restore_calls": n}. */
json_t *w24_listenercl_afdef6_session(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
