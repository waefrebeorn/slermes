/*
 * windows_proactor_accept.c -- real C11 port of the peer-drop classifier, the
 * install decision and the accept-retry choreography of upstream Hermes
 * gateway/windows_proactor_accept.py.
 *
 * PoP: w20_windowspro_18c6a5_dropped_peer @ gateway/windows_proactor_accept.py:dropped_peer
 * PoP: w20_windowspro_18c6a5_should_install @ gateway/windows_proactor_accept.py:install_proactor_accept_guard
 * PoP: w20_windowspro_18c6a5_replay @ gateway/windows_proactor_accept.py:GuardedAccept.__call__
 * PoP: w20_windowspro_18c6a5_exc_from_json @ gateway/windows_proactor_accept.py:GuardedAccept._issue_accept
 *
 * The live class holds asyncio futures; here the same settle/retry state
 * machine runs over a scripted accept sequence, so every branch (dropped peer,
 * WSAEFAULT at submission, cancellation, shutdown race) is exercised without a
 * socket. The subprocess-bearing proactor itself stays upstream.
 */
#include "noc_w20_windowspro_18c6a5.h"

#include <stdio.h>
#include <string.h>

/* ERROR_NETNAME_DELETED, WSAECONNRESET, WSAEFAULT from getpeername after AcceptEx. */
static const int PA_DROPPED_WINERRORS[] = {64, 10054, 10014};

void w20_windowspro_18c6a5_exc_from_json(const json_t *node, w20_windowspro_18c6a5_exc *out) {
    const char *type;
    const json_t *win;
    if (!out) return;
    out->is_reset = 0;
    out->is_oserror = 1;
    out->type = "OSError";
    out->winerror = -1;
    if (!node || node->type != JSON_OBJECT) return;
    type = json_get_str(node, "type", "OSError");
    out->type = type;
    if (!strcmp(type, "ConnectionResetError") || !strcmp(type, "ConnectionAbortedError")) {
        out->is_reset = 1;
    } else if (strcmp(type, "OSError") && strcmp(type, "TimeoutError") &&
               strcmp(type, "BlockingIOError") && strcmp(type, "BrokenPipeError") &&
               strcmp(type, "ConnectionError")) {
        out->is_oserror = 0;   /* RuntimeError and friends are not OSErrors */
    }
    win = json_obj_get(node, "winerror");
    if (win && win->type == JSON_NUMBER) out->winerror = (int)win->num_val;
}

int w20_windowspro_18c6a5_dropped_peer(const w20_windowspro_18c6a5_exc *exc) {
    size_t i;
    if (!exc) return 0;
    if (exc->is_reset) return 1;
    for (i = 0; i < sizeof(PA_DROPPED_WINERRORS) / sizeof(PA_DROPPED_WINERRORS[0]); i++) {
        if (exc->winerror == PA_DROPPED_WINERRORS[i]) return 1;
    }
    return 0;
}

int w20_windowspro_18c6a5_should_install(int is_proactor_loop, int has_proactor,
                                         int accept_callable, int already_guarded) {
    if (!is_proactor_loop) return 0;
    if (!has_proactor) return 0;
    if (!accept_callable) return 0;
    if (already_guarded) return 0;
    return 1;
}

/* --- scripted replay of GuardedAccept.__call__ ---------------------------- */

enum { PA_OK = 0, PA_CANCEL = 1, PA_ERROR = 2, PA_RAISE = 3 };

typedef struct {
    int kind;
    char conn[128];
    w20_windowspro_18c6a5_exc exc;
} pa_item;

static int pa_load_items(const json_t *script, pa_item *items, int cap) {
    int n = 0;
    size_t i;
    if (!script || script->type != JSON_ARRAY) return 0;
    for (i = 0; i < script->c.count && n < cap; i++) {
        const json_t *node = script->c.items[i];
        const char *kind;
        pa_item *it = &items[n];
        memset(it, 0, sizeof(*it));
        if (!node || node->type != JSON_OBJECT) continue;
        kind = json_get_str(node, "kind", "ok");
        if (!strcmp(kind, "cancel")) it->kind = PA_CANCEL;
        else if (!strcmp(kind, "error")) it->kind = PA_ERROR;
        else if (!strcmp(kind, "raise")) it->kind = PA_RAISE;
        else it->kind = PA_OK;
        snprintf(it->conn, sizeof(it->conn), "%s", json_get_str(node, "conn", ""));
        w20_windowspro_18c6a5_exc_from_json(node, &it->exc);
        n++;
    }
    return n;
}

static void pa_event_warning(json_t *events, const w20_windowspro_18c6a5_exc *exc) {
    json_t *ev = json_object();
    json_set(ev, "event", json_string("warning"));
    json_set(ev, "type", json_string(exc->type));
    if (exc->winerror >= 0) json_set(ev, "winerror", json_number((double)exc->winerror));
    else json_set(ev, "winerror", json_null());
    json_append(events, ev);
}

static void pa_event_close(json_t *events, const char *conn) {
    json_t *ev = json_object();
    json_set(ev, "event", json_string("close"));
    json_set(ev, "conn", json_string(conn ? conn : ""));
    json_append(events, ev);
}

static json_t *pa_error_json(const w20_windowspro_18c6a5_exc *exc) {
    json_t *err = json_object();
    json_set(err, "type", json_string(exc->type ? exc->type : "OSError"));
    if (exc->winerror >= 0) json_set(err, "winerror", json_number((double)exc->winerror));
    else json_set(err, "winerror", json_null());
    return err;
}

json_t *w20_windowspro_18c6a5_replay(const json_t *script, int outer_cancelled) {
    pa_item items[64];
    int n = pa_load_items(script, items, 64);
    int pos = 0, has_cur = 0, outer_done = 0;
    const char *state = "pending";
    pa_item cur, outer_exc;
    char result_conn[128];
    json_t *out = json_object(), *events = json_array(), *closed = json_array();
    json_t *outer = json_object();

    memset(&cur, 0, sizeof(cur));
    memset(&outer_exc, 0, sizeof(outer_exc));
    result_conn[0] = '\0';

    if (outer_cancelled) { outer_done = 1; state = "cancelled"; }

    /* _issue_accept() at call time: a submission-time raise escapes __call__. */
    if (n > 0) {
        cur = items[pos++];
        has_cur = 1;
        if (cur.kind == PA_RAISE) {
            if (!w20_windowspro_18c6a5_dropped_peer(&cur.exc) || cur.exc.winerror == 10014) {
                state = "raised";
                outer_exc = cur;
                has_cur = 0;
            } else {
                cur.kind = PA_ERROR;   /* failed future carrying the same error */
            }
        }
    }

    while (has_cur) {
        if (outer_done) {
            /* Shutdown won the race: nobody takes ownership of the connection. */
            if (cur.kind == PA_OK) {
                pa_event_close(events, cur.conn);
                json_append(closed, json_string(cur.conn));
            }
            break;
        }
        if (cur.kind == PA_CANCEL) { state = "cancelled"; break; }
        if (cur.kind == PA_OK) {
            state = "result";
            snprintf(result_conn, sizeof(result_conn), "%s", cur.conn);
            break;
        }
        /* inner.exception() is set */
        if (!w20_windowspro_18c6a5_dropped_peer(&cur.exc)) {
            state = "exception";
            outer_exc = cur;
            break;
        }
        pa_event_warning(events, &cur.exc);
        if (pos >= n) { state = "pending"; break; }   /* re-arm never came back */
        cur = items[pos++];
        if (cur.kind == PA_RAISE) {
            /* `except OSError as rearm_exc: outer.set_exception(rearm_exc)` */
            state = cur.exc.is_oserror ? "exception" : "raised";
            outer_exc = cur;
            break;
        }
    }

    json_set(outer, "state", json_string(state));
    if (!strcmp(state, "result")) json_set(outer, "conn", json_string(result_conn));
    else json_set(outer, "conn", json_null());
    if (!strcmp(state, "exception") || !strcmp(state, "raised")) {
        json_set(outer, "error", pa_error_json(&outer_exc.exc));
    } else {
        json_set(outer, "error", json_null());
    }

    json_set(out, "events", events);
    json_set(out, "outer", outer);
    json_set(out, "closed", closed);
    return out;
}
