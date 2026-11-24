/*
 * backend_retirement.c — real C11 port of upstream Hermes
 * hermes_cli/backend_retirement.py.
 *
 * PoP: w17_backendret_ac8bfc_acquire @ hermes_cli/backend_retirement.py:RetirementFence.acquire
 * PoP: w17_backendret_ac8bfc_release @ hermes_cli/backend_retirement.py:RetirementFence.release
 * PoP: w17_backendret_ac8bfc_commit  @ hermes_cli/backend_retirement.py:RetirementFence.commit
 * PoP: w17_backendret_ac8bfc_cancel  @ hermes_cli/backend_retirement.py:RetirementFence.cancel
 *
 * RetirementFence._expire() is inlined as noc_br_expire: a prepare token that is
 * uncommitted and past its deadline is discarded before each decision.
 */
#include "noc_w17_backendret_ac8bfc.h"
#include <string.h>

static void noc_br_expire(int *has_cur, int committed, double now, double expires) {
    if (*has_cur && !committed && now >= expires) *has_cur = 0;
}

static json_t *noc_br_ok(int ok) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(ok ? true : false));
    return o;
}

json_t *w17_backendret_ac8bfc_acquire(int preparing, int has_token, int active,
                                      int committed, double now, double expires) {
    noc_br_expire(&has_token, committed, now, expires);
    int ok = 0;
    if (!preparing && !has_token) { active += 1; ok = 1; }
    json_t *o = noc_br_ok(ok);
    json_set(o, "active", json_number((double)active));
    return o;
}

json_t *w17_backendret_ac8bfc_release(int active) {
    json_t *o = json_object();
    json_set(o, "active", json_number((double)(active - 1)));
    return o;
}

/* Python: self._expire(); if not token or token != self._token: return {"ok": False}
 *         self._committed = True; return {"ok": True} */
json_t *w17_backendret_ac8bfc_commit(const char *token, int token_is_null,
                                     int has_cur, const char *cur,
                                     int committed, double now, double expires) {
    noc_br_expire(&has_cur, committed, now, expires);
    int ok = 0;
    if (!token_is_null && token && token[0] && has_cur && cur && strcmp(token, cur) == 0)
        ok = 1;
    return noc_br_ok(ok);
}

/* Python: self._expire(); if not token or token != self._token or self._committed:
 *         return {"ok": False}; self._token = None; return {"ok": True} */
json_t *w17_backendret_ac8bfc_cancel(const char *token, int token_is_null,
                                     int has_cur, const char *cur,
                                     int committed, double now, double expires) {
    noc_br_expire(&has_cur, committed, now, expires);
    int ok = 0;
    if (!token_is_null && token && token[0] && has_cur && cur && !committed && strcmp(token, cur) == 0)
        ok = 1;
    return noc_br_ok(ok);
}
