/*
 * telegram_live_location.c - C11 port of upstream
 * plugins/platforms/telegram/telegram_live_location.py (gate w25_telegramli_06412b).
 *
 * PoP: w25_telegramli_06412b_meters_between @ plugins/platforms/telegram/telegram_live_location.py:_meters_between
 * PoP: w25_telegramli_06412b_is_redundant @ plugins/platforms/telegram/telegram_live_location.py:is_redundant_live_refresh
 * PoP: w25_telegramli_06412b_constants @ plugins/platforms/telegram/telegram_live_location.py:<module constants>
 *
 * PTB dispatch, the per-chat handler queue and the wall clock are seams. What is
 * ported is what the values decide: whether an edited live-location message is a
 * redundant refresh of a share this process already answered, and what the LRU
 * table of tracked shares looks like afterwards.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_telegramli_06412b.h"

#define LL_MAX_SHARES W25_TELEGRAMLI_06412B_MAX_TRACKED_SHARES
#define LL_MIN_METERS W25_TELEGRAMLI_06412B_MIN_METERS
#define LL_MIN_SECONDS W25_TELEGRAMLI_06412B_MIN_SECONDS
#define LL_EARTH_M W25_TELEGRAMLI_06412B_EARTH_RADIUS_M

/* math.radians: x * (pi / 180), the same rounded constant CPython uses. */
#define LL_DEG2RAD (3.141592653589793238462643383279502884 / 180.0)

/* Python truthiness of a JSON value. */
static int ll_truthy(const json_t *v) {
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

/* A numeric value, or 0 when the fixture did not give one. */
static int ll_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python str() over the shapes a chat/message id can hold. */
static void ll_str(const json_t *v, char *out, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(out, cap, "None"); return; }
    if (v->type == JSON_BOOL) { snprintf(out, cap, "%s", v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(out, cap, "%.0f", v->num_val);
        else snprintf(out, cap, "%.17g", v->num_val);
        return;
    }
    if (v->type == JSON_STRING) { snprintf(out, cap, "%s", v->str_val ? v->str_val : ""); return; }
    snprintf(out, cap, "%s", "");
}

static double ll_meters(double lat1, double lon1, double lat2, double lon2) {
    double p1 = lat1 * LL_DEG2RAD, p2 = lat2 * LL_DEG2RAD;
    double dp = p2 - p1, dl = (lon2 - lon1) * LL_DEG2RAD;
    double sdp = sin(dp / 2.0), sdl = sin(dl / 2.0);
    double h = pow(sdp, 2.0) + cos(p1) * cos(p2) * pow(sdl, 2.0);
    if (h > 1.0) h = 1.0;
    return 2.0 * LL_EARTH_M * asin(sqrt(h));
}

json_t *w25_telegramli_06412b_meters_between(const json_t *req) {
    double lat1 = 0.0, lon1 = 0.0, lat2 = 0.0, lon2 = 0.0;
    if (req) {
        ll_number(json_obj_get(req, "lat1"), &lat1);
        ll_number(json_obj_get(req, "lon1"), &lon1);
        ll_number(json_obj_get(req, "lat2"), &lat2);
        ll_number(json_obj_get(req, "lon2"), &lon2);
    }
    return json_number(ll_meters(lat1, lon1, lat2, lon2));
}

/* OrderedDict[str, tuple[ts|None, lat, lon]] restricted to the ported shape. */
typedef struct {
    char chat[128];
    char mid[128];
    int has_ts;
    double ts;
    double lat;
    double lon;
} ll_share;

typedef struct {
    ll_share v[LL_MAX_SHARES + 2];
    size_t n;
} ll_shares;

static int ll_same_key(const ll_share *a, const char *chat, const char *mid) {
    return !strcmp(a->chat, chat) && !strcmp(a->mid, mid);
}

static int ll_index(const ll_shares *s, const char *chat, const char *mid) {
    size_t i;
    for (i = 0; i < s->n; i++)
        if (ll_same_key(&s->v[i], chat, mid)) return (int)i;
    return -1;
}

static void ll_drop(ll_shares *s, size_t at) {
    size_t i;
    for (i = at; i + 1 < s->n; i++) s->v[i] = s->v[i + 1];
    s->n--;
}

/* shares[key] = value; shares.move_to_end(key) */
static void ll_put(ll_shares *s, const char *chat, const char *mid,
                   int has_ts, double ts, double lat, double lon) {
    int at = ll_index(s, chat, mid);
    ll_share e;
    if (at >= 0) ll_drop(s, (size_t)at);
    if (s->n >= LL_MAX_SHARES) ll_drop(s, 0);          /* bounded table */
    snprintf(e.chat, sizeof(e.chat), "%s", chat);
    snprintf(e.mid, sizeof(e.mid), "%s", mid);
    e.has_ts = has_ts; e.ts = ts; e.lat = lat; e.lon = lon;
    s->v[s->n++] = e;
}

static void ll_load(const json_t *raw, ll_shares *s) {
    size_t i, n;
    s->n = 0;
    if (!raw || raw->type != JSON_ARRAY) return;
    n = json_len(raw);
    for (i = 0; i < n && s->n < LL_MAX_SHARES; i++) {
        const json_t *e = json_get(raw, i);
        ll_share out;
        if (!e || e->type != JSON_ARRAY || json_len(e) < 5) continue;
        ll_str(json_get(e, 0), out.chat, sizeof(out.chat));
        ll_str(json_get(e, 1), out.mid, sizeof(out.mid));
        out.has_ts = ll_number(json_get(e, 2), &out.ts);
        ll_number(json_get(e, 3), &out.lat);
        ll_number(json_get(e, 4), &out.lon);
        s->v[s->n++] = out;
    }
}

json_t *w25_telegramli_06412b_is_redundant(const json_t *req) {
    const json_t *msg = req ? json_obj_get(req, "msg") : NULL;
    const json_t *loc = req ? json_obj_get(req, "location") : NULL;
    const json_t *msgobj = (msg && msg->type == JSON_OBJECT) ? msg : NULL;
    const json_t *chat_id = msgobj ? json_obj_get(msgobj, "chat_id") : NULL;
    const json_t *live_period = (loc && loc->type == JSON_OBJECT) ? json_obj_get(loc, "live_period") : NULL;
    char chat[128], mid[128];
    ll_shares shares;
    double lat = 0.0, lon = 0.0, edited_at = 0.0, stamp = 0.0;
    int has_edited, has_stamp, live, redundant = 0;
    int prev;
    json_t *out = json_object(), *arr;

    ll_str(chat_id, chat, sizeof(chat));
    ll_str(msgobj ? json_obj_get(msgobj, "message_id") : NULL, mid, sizeof(mid));
    has_edited = ll_number(msgobj ? json_obj_get(msgobj, "edit_date") : NULL, &edited_at);
    has_stamp = ll_number(msgobj ? json_obj_get(msgobj, "date") : NULL, &stamp);
    live = live_period != NULL && live_period->type != JSON_NULL;
    ll_load(req ? json_obj_get(req, "shares") : NULL, &shares);
    ll_number(req ? json_obj_get(req, "lat") : NULL, &lat);
    ll_number(req ? json_obj_get(req, "lon") : NULL, &lon);

    /* An edit without live_period is the share stopping: nothing new. */
    if (has_edited && !live) {
        int at = ll_index(&shares, chat, mid);
        if (at >= 0) ll_drop(&shares, (size_t)at);
        json_set(out, "redundant", json_bool(1));
        arr = json_array();
        json_set(out, "shares", arr);
        return out;
    }
    if (!live) {
        redundant = 0;
    } else {
        double now_ts = 0.0;
        int has_now;
        prev = ll_index(&shares, chat, mid);
        has_now = has_edited || has_stamp;
        now_ts = has_edited ? edited_at : stamp;
        redundant = has_edited;
        if (redundant) {
            if (prev < 0) {
                redundant = 1;
            } else if (!has_now || !shares.v[prev].has_ts) {
                redundant = 0;
            } else if (now_ts - shares.v[prev].ts < LL_MIN_SECONDS
                       && ll_meters(shares.v[prev].lat, shares.v[prev].lon, lat, lon) < LL_MIN_METERS) {
                redundant = 1;
            } else {
                redundant = 0;
            }
        }
        if (!redundant || prev < 0)
            ll_put(&shares, chat, mid, has_now, has_now ? now_ts : 0.0, lat, lon);
    }

    json_set(out, "redundant", json_bool(redundant ? 1 : 0));
    arr = json_array();
    for (size_t i = 0; i < shares.n; i++) {
        json_t *row = json_array();
        json_append(row, json_string(shares.v[i].chat));
        json_append(row, json_string(shares.v[i].mid));
        json_append(row, shares.v[i].has_ts ? json_number(shares.v[i].ts) : json_null());
        json_append(row, json_number(shares.v[i].lat));
        json_append(row, json_number(shares.v[i].lon));
        json_append(arr, row);
    }
    json_set(out, "shares", arr);
    return out;
}

json_t *w25_telegramli_06412b_constants(void) {
    json_t *o = json_object();
    json_set(o, "live_refresh_min_meters", json_number(LL_MIN_METERS));
    json_set(o, "live_refresh_min_seconds", json_number(LL_MIN_SECONDS));
    json_set(o, "max_tracked_shares", json_number((double)LL_MAX_SHARES));
    return o;
}
