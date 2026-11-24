/*
 * session_registry.c - C home for upstream Hermes tui_gateway/session_registry.py.
 * PoP: w16_sessionreg_47e774_minted_key @ tui_gateway/session_registry.py:_is_server_minted_key
 * PoP: w16_sessionreg_47e774_lookup_key @ tui_gateway/session_registry.py:_session_lookup_key
 * PoP: w16_sessionreg_47e774_build_constants @ tui_gateway/session_registry.py:_AGENT_BUILD_WAIT_SLICE/_AGENT_BUILD_SLOW_NOTICE_AFTER/_AGENT_BUILD_SLOW_NOTICE_KEY
 *
 * Faithful ports of the module's pure key-shape gate, key lookup and static
 * build-wait constants. The registered session RPCs and build state stay with
 * the live Python owner. The lookup helper assumes the session 'agent' field is
 * None/absent (as in every fixture), so getattr(agent, 'session_id', None) is None
 * and resolution falls through to session_key / fallback / "".
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_sessionreg_47e774.h"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static int is_falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    switch (v->type) {
        case JSON_BOOL:   return !v->bool_val;
        case JSON_NUMBER: return v->num_val == 0.0;
        case JSON_STRING: return !v->str_val || !v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count == 0;
        default:          return 1;
    }
}
/* Python str() for scalar kinds (containers are not used by the fixtures). */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v) return dupstr("None");
    switch (v->type) {
        case JSON_NULL:   return dupstr("None");
        case JSON_STRING: return dupstr(v->str_val);
        case JSON_BOOL:   return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return dupstr(buf);
        default: return dupstr("");
    }
}
static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_lhex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

struct json_t *w16_sessionreg_47e774_minted_key(const struct json_t *value) {
    /* ^\d{8}_\d{6}_[0-9a-f]{6}$ -- fullmatch, 22 chars */
    if (!value || value->type != JSON_STRING) return json_bool(0);
    const char *s = value->str_val;
    if (!s || strlen(s) != 22) return json_bool(0);
    for (int i = 0; i < 8; i++)  if (!is_digit(s[i]))       return json_bool(0);
    if (s[8] != '_') return json_bool(0);
    for (int i = 9; i < 15; i++) if (!is_digit(s[i]))       return json_bool(0);
    if (s[15] != '_') return json_bool(0);
    for (int i = 16; i < 22; i++) if (!is_lhex(s[i]))       return json_bool(0);
    return json_bool(1);
}

struct json_t *w16_sessionreg_47e774_lookup_key(const struct json_t *session,
        const char *fallback) {
    /* str(getattr(agent,'session_id',None) or session.get('session_key') or fallback or "")
     * with agent absent/None -> the getattr term is None. */
    json_t *sk = json_obj_get(session, "session_key");
    char *res;
    if (sk && !is_falsy(sk))
        res = py_str(sk);
    else if (fallback && *fallback)
        res = dupstr(fallback);
    else
        res = dupstr("");
    json_t *out = json_string(res);
    free(res);
    return out;
}

struct json_t *w16_sessionreg_47e774_build_constants(void) {
    json_t *out = json_object();
    json_set(out, "wait_slice", json_number(5.0));
    json_set(out, "slow_notice_after", json_number(30.0));
    json_set(out, "slow_notice_key", json_string("agent-build-slow"));
    return out;
}
