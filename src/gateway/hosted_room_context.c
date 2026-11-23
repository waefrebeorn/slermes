/*
 * hosted_room_context.c — faithful C11 port of the pure actor classifier in
 * gateway/hosted_room_context.py (gate w14_hostedroom_3724b3).
 * PoP: w14_hostedroom_3724b3_scheduled_post_kind @ gateway/hosted_room_context.py:scheduled_post_kind
 */
#include "noc_w14_hostedroom_3724b3.h"

#include <stdio.h>
#include <string.h>

/* Live module constants (SCHEDULED_POST_PREFIX / SCHEDULED_NOTICE_PREFIX). */
static const char *const SCHEDULED_POST_PREFIX = "cron:";
static const char *const SCHEDULED_NOTICE_PREFIX = "cron-notice:";

/* Python `str(actor.get("id") or "")` for the scalar JSON values the actor id
 * can take. Falsy values (None, "", 0, False, empty containers) become "". */
static void py_id_str(json_t *v, char *buf, size_t cap) {
    buf[0] = '\0';
    if (!v) return;
    switch (v->type) {
    case JSON_STRING:
        if (v->str_val) snprintf(buf, cap, "%s", v->str_val);
        break;
    case JSON_NUMBER:
        if (v->num_val == 0.0) break; /* 0 is falsy -> "" */
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, cap, "%lld", (long long)v->num_val);
        else
            snprintf(buf, cap, "%g", v->num_val);
        break;
    case JSON_BOOL:
        if (v->bool_val) snprintf(buf, cap, "%s", "True");
        break;
    default:
        break; /* null / empty containers -> "" */
    }
}

json_t *w14_hostedroom_3724b3_scheduled_post_kind(json_t *actor) {
    /* `if not isinstance(actor, Mapping) or actor.get("kind") != "user": None` */
    if (!actor || actor->type != JSON_OBJECT) return json_null();
    json_t *kind = json_obj_get(actor, "kind");
    if (!kind || kind->type != JSON_STRING || strcmp(kind->str_val, "user") != 0)
        return json_null();

    char actor_id[512];
    py_id_str(json_obj_get(actor, "id"), actor_id, sizeof(actor_id));

    /* The notice prefix is checked first in the live module. */
    if (strncmp(actor_id, SCHEDULED_NOTICE_PREFIX, strlen(SCHEDULED_NOTICE_PREFIX)) == 0)
        return json_string("notice");
    if (strncmp(actor_id, SCHEDULED_POST_PREFIX, strlen(SCHEDULED_POST_PREFIX)) == 0)
        return json_string("post");
    return json_null();
}
