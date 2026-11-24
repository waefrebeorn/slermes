/*
 * session_messaging_picks.c - C11 port of upstream
 * gateway/session_messaging_picks.py (gate w26_sessionmes_de1f3f).
 *
 * PoP: w26_sessionmes_de1f3f_is_chat_pick @ gateway/session_messaging_picks.py:is_chat_pick
 * PoP: w26_sessionmes_de1f3f_platform_key @ gateway/session_messaging_picks.py:_platform_key
 * PoP: w26_sessionmes_de1f3f_constants @ gateway/session_messaging_picks.py:CHAT_PICKS
 *
 * The admission half of the module: a chat pick is ``model`` or ``reasoning`` on
 * a live session whose platform is not LOCAL, and it resolves the platform's
 * config.yaml key.  The authority, the runner, the runtime row, the model switch
 * and the receipt write are seams and stay in the caller.
 */
#include <stddef.h>
#include <string.h>
#include "noc_w26_sessionmes_de1f3f.h"

/* CHAT_PICKS = frozenset({'model', 'reasoning'}) */
static const char *const SM_PICKS[2] = {"model", "reasoning"};

static const char *sm_str(const json_t *v) {
    if (!v || v->type != JSON_STRING || !v->str_val) return NULL;
    return v->str_val;
}

static int sm_flag(const json_t *v, int def) {
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_NULL) return 0;
    return def;
}

json_t *w26_sessionmes_de1f3f_is_chat_pick(const json_t *operation, const json_t *platform,
                                           const json_t *live) {
    const char *op = sm_str(operation), *plat = sm_str(platform);
    size_t i;
    int is_pick = 0;

    if (!op || !plat) return json_bool(0);
    for (i = 0; i < 2; i++)
        if (!strcmp(op, SM_PICKS[i])) is_pick = 1;
    if (!is_pick) return json_bool(0);
    if (!sm_flag(live, 1)) return json_bool(0);
    /* Platform.LOCAL is the one platform a chat pick is refused on. */
    return json_bool(strcmp(plat, "local") != 0);
}

json_t *w26_sessionmes_de1f3f_platform_key(const json_t *platform) {
    const char *plat = sm_str(platform);

    if (!plat) return json_null();
    if (!strcmp(plat, "local")) return json_string("cli");
    return json_string(plat);
}

json_t *w26_sessionmes_de1f3f_constants(void) {
    json_t *o = json_array();
    json_append(o, json_string(SM_PICKS[0]));
    json_append(o, json_string(SM_PICKS[1]));
    return o;
}
