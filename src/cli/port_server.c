/*
 * port_server.c
 *
 * Pure helpers ported from tui_gateway/server.py (106 defs, previously 0
 * ported).
 *
 *   sv_busy_input_mode       <- _load_busy_input_mode
 *   sv_canonical_profile     <- _canonical_profile_request
 *   sv_current_profile       <- _current_profile_name
 *   sv_event_frame           <- _event_frame
 *
 * The dispatch / db / profile-scope / transport half stays in Python.
 */

#include "port_server.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* server.py: the busy-input modes the config accepts */
static const char *const SV_BUSY_MODES[] = { "queue", "steer", "interrupt" };
#define SV_BUSY_MODES_N (sizeof(SV_BUSY_MODES) / sizeof(SV_BUSY_MODES[0]))

/* ===========================================================================
 *  _load_busy_input_mode  (server.py:_load_busy_input_mode)
 *
 *      raw = str(cfg.get("busy_input_mode", "") or "").strip().lower()
 *      return raw if raw in {"queue", "steer", "interrupt"} else "interrupt"
 *
 *  Unknown / empty values fall back to "interrupt" - the only mode that cannot
 *  lose the user's input. Caller frees.
 * =========================================================================== */

/* PoP: _load_busy_input_mode @ tui_gateway/server.py:_load_busy_input_mode */
char *sv_busy_input_mode(const char *raw)
{
    /* str(value or "") - a NULL/empty value is falsy */
    char *s;
    if (raw && raw[0]) s = strdup(raw);
    else { s = (char *)malloc(1); if (s) s[0] = '\0'; }
    if (!s) return NULL;

    for (size_t i = 0; s[i]; i++) s[i] = (char)tolower((unsigned char)s[i]);
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) s[--b] = '\0';
    memmove(s, s + a, b - a + 1);

    for (size_t i = 0; i < SV_BUSY_MODES_N; i++)
        if (strcmp(s, SV_BUSY_MODES[i]) == 0) return s;

    free(s);
    return strdup("interrupt");
}

/* ===========================================================================
 *  _canonical_profile_request  (server.py:_canonical_profile_request)
 *
 *      if name.casefold() in {".hermes", "hermes"}:
 *          if not (profiles_root / normalize(name)).is_dir():
 *              return "default"
 *      return name
 *
 *  `Path(default_home).name` was historically sent as a profile id; those
 *  basenames are installation details. A real named profile of that name wins
 *  ("hermes" is a legal id); otherwise the name fails closed to "default".
 *  `has_hermes_dir` stands in for the profiles-root is_dir() probe.
 *  Caller frees.
 * =========================================================================== */

/* PoP: _canonical_profile_request @ tui_gateway/server.py:_canonical_profile_request */
char *sv_canonical_profile(const char *name, int has_hermes_dir)
{
    if (!name) return strdup("default");

    /* casefold() in Python is Unicode-aware; profile names are ASCII, so a
     * lower() comparison is the same answer here. */
    size_t n = strlen(name);
    char *low = (char *)malloc(n + 1);
    if (!low) return strdup("default");
    for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)name[i]);
    low[n] = '\0';

    int is_hermes = (strcmp(low, ".hermes") == 0 || strcmp(low, "hermes") == 0);
    free(low);

    if (is_hermes && !has_hermes_dir) return strdup("default");
    return strdup(name);
}

/* ===========================================================================
 *  _current_profile_name  (server.py:_current_profile_name)
 *
 *      try: return get_active_profile_name() or "default"
 *      except Exception: return "default"
 *
 *  `active` is the resolver's answer, or NULL when it raised. Caller frees.
 * =========================================================================== */

/* PoP: _current_profile_name @ tui_gateway/server.py:_current_profile_name */
char *sv_current_profile(const char *active)
{
    if (active && active[0]) return strdup(active);
    return strdup("default");
}

/* ===========================================================================
 *  _event_frame  (server.py:_event_frame)
 *
 *      _contracts.check_payload(event, payload)
 *      params = {"type": event, "session_id": sid,
 *                **({"payload": payload} if payload is not None else {})}
 *      return {"jsonrpc": "2.0", "method": "event", "params": params}
 *
 *  The payload key is OMITTED (not null) when there is no payload. Emits the
 *  JSON-RPC notification envelope into `out`.
 * =========================================================================== */

/* PoP: _event_frame @ tui_gateway/server.py:_event_frame */
void sv_event_frame(char *out, size_t cap, const char *event, const char *sid,
                    const char *payload)
{
    if (!out || cap == 0) return;

    size_t el = event ? strlen(event) : 0;
    size_t sl = sid ? strlen(sid) : 0;
    size_t pl = payload ? strlen(payload) : 0;
    char *ee = (char *)malloc(el * 6 + 1);
    char *es = (char *)malloc(sl * 6 + 1);
    char *ep = (char *)malloc(pl * 6 + 1);
    if (!ee || !es || !ep) { free(ee); free(es); free(ep); out[0] = '\0'; return; }

    size_t k = 0;
    for (size_t i = 0; i < el; i++) {
        unsigned char c = (unsigned char)event[i];
        if (c == '"' || c == '\\') { ee[k++] = '\\'; ee[k++] = (char)c; }
        else if (c < 0x20) { k += (size_t)snprintf(ee + k, 7, "\\u%04x", c); }
        else ee[k++] = (char)c;
    }
    ee[k] = '\0';
    k = 0;
    for (size_t i = 0; i < sl; i++) {
        unsigned char c = (unsigned char)sid[i];
        if (c == '"' || c == '\\') { es[k++] = '\\'; es[k++] = (char)c; }
        else if (c < 0x20) { k += (size_t)snprintf(es + k, 7, "\\u%04x", c); }
        else es[k++] = (char)c;
    }
    es[k] = '\0';
    k = 0;
    for (size_t i = 0; i < pl; i++) {
        unsigned char c = (unsigned char)payload[i];
        if (c == '"' || c == '\\') { ep[k++] = '\\'; ep[k++] = (char)c; }
        else if (c < 0x20) { k += (size_t)snprintf(ep + k, 7, "\\u%04x", c); }
        else ep[k++] = (char)c;
    }
    ep[k] = '\0';

    if (payload)
        snprintf(out, cap,
                 "{\"jsonrpc\":\"2.0\",\"method\":\"event\",\"params\":{\"type\":\"%s\","
                 "\"session_id\":\"%s\",\"payload\":\"%s\"}}", ee, es, ep);
    else
        snprintf(out, cap,
                 "{\"jsonrpc\":\"2.0\",\"method\":\"event\",\"params\":{\"type\":\"%s\","
                 "\"session_id\":\"%s\"}}", ee, es);

    free(ee); free(es); free(ep);
}
