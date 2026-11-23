/*
 * cli_shutdown.c — C port of upstream Hermes hermes_cli/cli_shutdown.py.
 *
 * PoP: w9_clishutdown_5f546e_float_env @ hermes_cli/cli_shutdown.py:_float_env
 * PoP: w9_clishutdown_5f546e_should_emit_cleanup_session_finalize @ hermes_cli/cli_shutdown.py:_should_emit_cleanup_session_finalize
 * PoP: w9_clishutdown_5f546e_oneshot_session @ hermes_cli/cli_shutdown.py:_oneshot_agent_and_session
 *
 * Signal/watchdog/procedure bodies are intentionally excluded; only the
 * deterministic decision helpers are ported.
 */
#include "noc_w9_clishutdown_5f546e.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

double w9_clishutdown_5f546e_float_env(const json_t *env_value, double def) {
    if (!env_value || env_value->type == JSON_NULL) return def; /* os.getenv default */
    const char *p = (env_value->type == JSON_STRING && env_value->str_val)
                    ? env_value->str_val : NULL;
    if (!p) return def;
    const char *s = p;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return def;                    /* float("") raises */
    errno = 0;
    char *endp = NULL;
    double v = strtod(s, &endp);
    if (endp == s) return def;
    while (*endp && isspace((unsigned char)*endp)) endp++;
    if (*endp) return def;
    if (isnan(v) || isinf(v)) return def;   /* keep fixtures finite */
    return v;
}

static int str_in_array(const json_t *arr, const json_t *value) {
    if (!value || value->type != JSON_STRING || !value->str_val) return 0;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = json_get(arr, i);
        if (it && it->type == JSON_STRING && it->str_val && !strcmp(it->str_val, value->str_val))
            return 1;
    }
    return 0;
}

int w9_clishutdown_5f546e_should_emit_cleanup_session_finalize(const json_t *session_id,
                                                              int gateway_owns,
                                                              const json_t *handed_off,
                                                              const json_t *attempted) {
    if (gateway_owns) return 0;
    int is_none = !session_id || session_id->type == JSON_NULL;
    if (!is_none && str_in_array(handed_off, session_id)) return 0;
    int attempted_nonempty = (attempted && attempted->type == JSON_ARRAY && attempted->c.count > 0);
    if (!attempted_nonempty) return 1;
    if (is_none) return 0;
    return str_in_array(attempted, session_id) ? 0 : 1;
}

struct json_t *w9_clishutdown_5f546e_oneshot_session(const json_t *agent_session,
                                                    const json_t *cli_session) {
    const json_t *chosen = NULL;
    if (agent_session && agent_session->type == JSON_STRING && agent_session->str_val && agent_session->str_val[0])
        chosen = agent_session;             /* truthy agent.session_id wins */
    else
        chosen = cli_session;               /* `a or b` -> b (possibly None) */
    if (!chosen || chosen->type == JSON_NULL) return json_null();
    if (chosen->type == JSON_STRING && chosen->str_val) return json_string(chosen->str_val);
    return json_null();
}
