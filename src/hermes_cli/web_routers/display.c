/*
 * display.c - C11 port of upstream hermes_cli/web_routers/display.py
 * (gate w26_display_756d88).
 *
 * PoP: w26_display_756d88_should_evict @ hermes_cli/web_routers/display.py:_should_evict
 * PoP: w26_display_756d88_constants @ hermes_cli/web_routers/display.py:<close codes and budgets>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_display_756d88.h"

/* The WebSocket, the RFB socket, the lease file and the activity stamp are all
 * seams.  Ported here: who gets kicked when control changes hands (the input
 * gate's per-connection memory) and the codes/budgets the bridge is tuned with.
 */

/* tools/bot_desktop/lease.py:HUMAN */
#define WD_HUMAN "human"

static const char *wd_safe(const char *s) { return s ? s : ""; }

/* Python truthiness over the shapes `held["ever"]` can hold. */
static int wd_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

json_t *w26_display_756d88_should_evict(const json_t *req) {
    const json_t *held, *lease, *holder, *lviewer, *viewer;
    json_t *out;
    int ever;

    held = json_obj_get(req, "held");
    lease = json_obj_get(req, "lease");
    viewer = json_obj_get(req, "viewer_id");

    if (!held || held->type != JSON_OBJECT) return json_null();
    if (!lease || lease->type != JSON_OBJECT) return json_null();
    ever = wd_truthy(json_obj_get(held, "ever"));

    out = json_object();
    holder = json_obj_get(lease, "holder");
    lviewer = json_obj_get(lease, "viewer_id");

    /* A lease not held by a human is a hand-back: forget that this viewer ever held. */
    if (!(holder && holder->type == JSON_STRING &&
          strcmp(wd_safe(holder->str_val), WD_HUMAN) == 0)) {
        json_set(out, "evict", json_bool(0));
        json_set(out, "ever", json_bool(0));
        return out;
    }

    /* `lease.viewer_id == viewer_id`: strings compare by value, None equals nothing here. */
    if (lviewer && viewer && lviewer->type == JSON_STRING && viewer->type == JSON_STRING &&
        strcmp(wd_safe(lviewer->str_val), wd_safe(viewer->str_val)) == 0) {
        json_set(out, "evict", json_bool(0));
        json_set(out, "ever", json_bool(1));
        return out;
    }

    json_set(out, "evict", json_bool(ever ? 1 : 0));
    /* Only this branch leaves `held["ever"]` untouched, so echo it as it was. */
    {
        json_t *src = json_obj_get(held, "ever");
        json_set(out, "ever", src ? json_copy(src) : json_null());
    }
    return out;
}

json_t *w26_display_756d88_constants(void) {
    json_t *o = json_object();
    json_t *clean = json_array();
    json_set(o, "activity_stamp_s", json_number(60.0));
    json_set(o, "read_chunk", json_number(65536));
    json_set(o, "close_control_taken", json_number(4000));
    json_append(clean, json_number(1000));
    json_append(clean, json_number(1001));
    json_set(o, "clean_close", clean);
    json_set(o, "close_desktop_gone", json_number(4001));
    json_set(o, "close_bad_ticket", json_number(4401));
    json_set(o, "close_not_allowed", json_number(4403));
    json_set(o, "close_protocol", json_number(1003));
    json_set(o, "lease_refresh_s", json_number(0.25));
    return o;
}
