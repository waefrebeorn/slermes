/*
 * listener_client.c - C11 port of upstream
 * hermes_cli/dashboard_auth/listener_client.py (gate w24_listenercl_afdef6).
 *
 * PoP: w24_listenercl_afdef6_offer @ hermes_cli/dashboard_auth/listener_client.py:offer
 * PoP: w24_listenercl_afdef6_use @ hermes_cli/dashboard_auth/listener_client.py:use
 * PoP: w24_listenercl_afdef6_current @ hermes_cli/dashboard_auth/listener_client.py:current
 * PoP: w24_listenercl_afdef6_undo @ hermes_cli/dashboard_auth/listener_client.py:use.<locals>.undo
 * PoP: w24_listenercl_afdef6_session @ hermes_cli/dashboard_auth/listener_client.py:<module handshake>
 *
 * Ported: the handshake itself.  ``offer`` records that the Nous sign-in plugin
 * registered a provider swap; ``use`` signs in with a client on a portal for as
 * long as the returned undo has not run, returns None (changing nothing) where
 * the plugin is not loaded, and resets the current client before re-raising when
 * the swap fails; the undo clears the current client and then restores.  The
 * provider swap is a seam: it is modelled as a callable that either succeeds
 * (counted) or raises (the swap_raises flag), and the plugin's own provider,
 * its discovery import and the portal are left to the caller.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_listenercl_afdef6.h"

static int lc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len((json_t *)v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

void w24_listenercl_afdef6_init(w24_listenercl_afdef6_ctx *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}

void w24_listenercl_afdef6_clear(w24_listenercl_afdef6_ctx *ctx) {
    if (!ctx) return;
    json_free(ctx->client_id);
    json_free(ctx->portal_url);
    ctx->client_id = NULL;
    ctx->portal_url = NULL;
}

void w24_listenercl_afdef6_offer(w24_listenercl_afdef6_ctx *ctx) {
    if (ctx) ctx->registered = 1;                 /* _swap = swap */
}

json_t *w24_listenercl_afdef6_use(w24_listenercl_afdef6_ctx *ctx,
                                  const json_t *client_id,
                                  const json_t *portal_url,
                                  int swap_raises,
                                  int *raised) {
    int id;

    if (raised) *raised = 0;
    if (!ctx) return json_null();
    if (!ctx->registered) return json_null();      /* no plugin: None, unchanged */

    /* _current = (client_id, portal_url) */
    w24_listenercl_afdef6_clear(ctx);
    ctx->client_id = client_id ? json_copy(client_id) : json_null();
    ctx->portal_url = portal_url ? json_copy(portal_url) : json_null();

    ctx->swap_calls++;                             /* restore = _swap() */
    if (swap_raises) {                             /* except BaseException */
        w24_listenercl_afdef6_clear(ctx);          /* _current = None */
        if (raised) *raised = 1;
        return json_null();                        /* the exception propagates */
    }
    if (ctx->undo_count >= W24_LISTENERCL_AFDEF6_MAX_UNDOS) return json_null();
    id = ctx->undo_count;
    ctx->undo_alive[id] = 1;
    ctx->undo_count++;
    return json_number((double)id);
}

json_t *w24_listenercl_afdef6_current(const w24_listenercl_afdef6_ctx *ctx) {
    json_t *pair;
    if (!ctx || !ctx->client_id || !ctx->portal_url) return json_null();
    pair = json_array();
    json_append(pair, json_copy(ctx->client_id));
    json_append(pair, json_copy(ctx->portal_url));
    return pair;
}

int w24_listenercl_afdef6_undo(w24_listenercl_afdef6_ctx *ctx, int id) {
    if (!ctx || id < 0 || id >= ctx->undo_count || !ctx->undo_alive[id]) return 0;
    ctx->undo_alive[id] = 0;
    w24_listenercl_afdef6_clear(ctx);              /* _current = None */
    ctx->restore_calls++;                          /* restore() */
    return 1;
}

static void lc_result(json_t *results, const char *op, int raised, int returned_null,
                      int undo_id, int ran, const w24_listenercl_afdef6_ctx *ctx) {
    json_t *row = json_object();
    json_set(row, "op", json_string(op));
    json_set(row, "raised", json_bool(raised ? true : false));
    json_set(row, "returned_null", json_bool(returned_null ? true : false));
    json_set(row, "undo_id", undo_id < 0 ? json_null() : json_number((double)undo_id));
    json_set(row, "ran", json_bool(ran ? true : false));
    json_set(row, "current", w24_listenercl_afdef6_current(ctx));
    json_append(results, row);
}

json_t *w24_listenercl_afdef6_session(const json_t *req) {
    w24_listenercl_afdef6_ctx ctx;
    json_t *out = json_object();
    json_t *results = json_array();
    const json_t *ops;
    size_t i, n;

    w24_listenercl_afdef6_init(&ctx);
    if (!req || req->type != JSON_OBJECT) {
        json_set(out, "current", json_null());
        json_set(out, "results", results);
        json_set(out, "swap_calls", json_number(0));
        json_set(out, "restore_calls", json_number(0));
        return out;
    }
    if (lc_truthy(json_obj_get(req, "offered"))) w24_listenercl_afdef6_offer(&ctx);

    ops = json_obj_get(req, "ops");
    n = (ops && ops->type == JSON_ARRAY) ? json_len(ops) : 0;
    for (i = 0; i < n; i++) {
        const json_t *op = json_get(ops, i);
        const char *name = (op && op->type == JSON_OBJECT) ? json_get_str(op, "op", "") : "";
        if (!strcmp(name, "offer")) {
            w24_listenercl_afdef6_offer(&ctx);
            lc_result(results, "offer", 0, 0, -1, 0, &ctx);
        } else if (!strcmp(name, "use")) {
            int raised = 0;
            json_t *token = w24_listenercl_afdef6_use(&ctx,
                                                      json_obj_get(op, "client_id"),
                                                      json_obj_get(op, "portal_url"),
                                                      lc_truthy(json_obj_get(req, "swap_raises")),
                                                      &raised);
            int returned_null = (!token || token->type == JSON_NULL);
            int id = returned_null ? -1 : (int)token->num_val;
            json_free(token);
            lc_result(results, "use", raised, returned_null, id, 0, &ctx);
        } else if (!strcmp(name, "undo")) {
            const json_t *idv = json_obj_get(op, "id");
            int id = (idv && idv->type == JSON_NUMBER) ? (int)idv->num_val : -1;
            int ran = w24_listenercl_afdef6_undo(&ctx, id);
            lc_result(results, "undo", 0, 0, -1, ran, &ctx);
        } else {
            /* "current" and any unknown op only read the module state. */
            lc_result(results, name, 0, 0, -1, 0, &ctx);
        }
    }

    json_set(out, "current", w24_listenercl_afdef6_current(&ctx));
    json_set(out, "results", results);
    json_set(out, "swap_calls", json_number((double)ctx.swap_calls));
    json_set(out, "restore_calls", json_number((double)ctx.restore_calls));
    w24_listenercl_afdef6_clear(&ctx);
    return out;
}
