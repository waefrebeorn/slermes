/*
 * hosted_room_router.c — C port of upstream Hermes gateway/hosted_room_router.py.
 *
 * PoP: w13_hostedroom_d92a93_routed_event_id @ gateway/hosted_room_router.py:routed_event_id
 * PoP: w13_hostedroom_d92a93_continue_count @ gateway/hosted_room_router.py:continue_count
 * PoP: w13_hostedroom_d92a93_routed_payload @ gateway/hosted_room_router.py:routed_payload
 * PoP: w13_hostedroom_d92a93_one_line @ gateway/hosted_room_router.py:_one_line
 * PoP: w13_hostedroom_d92a93_parse_router_answer @ gateway/hosted_room_router.py:parse_router_answer
 *
 * Pure decision helpers. Event/room objects are replaced by JSON seams from the
 * fixture; nothing is routed or sent.
 */
#include "noc_w13_hostedroom_d92a93.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUTED_EVENT_KIND "room.continued"
#define ROUTER_MESSAGE_CHARS 600

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* Python str.strip() over ASCII whitespace. */
static char *py_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

json_t *w13_hostedroom_d92a93_routed_event_id(const struct json_t *discussion_event_id) {
    const char *id = jstr(discussion_event_id);
    size_t n = id ? strlen(id) : 0;
    char *buf = (char *)malloc(n + sizeof("droute:"));
    if (!buf) return json_null();
    memcpy(buf, "droute:", 7);
    if (n) memcpy(buf + 7, id, n);
    buf[7 + n] = '\0';
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

json_t *w13_hostedroom_d92a93_continue_count(const struct json_t *events,
                                              const struct json_t *discussion_event_id) {
    long long count = 0;
    const char *want = jstr(discussion_event_id);
    if (json_is_array(events)) {
        size_t n = json_len(events);
        for (size_t i = 0; i < n; i++) {
            const json_t *ev = json_get(events, i);
            if (!json_is_object(ev)) continue;
            const char *kind = jstr(json_obj_get(ev, "kind"));
            if (!kind || strcmp(kind, ROUTED_EVENT_KIND) != 0) continue;
            const char *eid = jstr(json_obj_get(json_obj_get(ev, "payload"), "discussion_event_id"));
            if ((!want && !eid) || (want && eid && !strcmp(want, eid))) count++;
        }
    }
    return json_number((double)count);
}

json_t *w13_hostedroom_d92a93_routed_payload(const struct json_t *discussion_event_id,
                                              const struct json_t *thread_id,
                                              const struct json_t *member_ids) {
    json_t *out = json_object();
    json_set(out, "discussion_event_id", json_copy(discussion_event_id));
    json_set(out, "thread_id", json_copy(thread_id));
    json_t *ids = json_array();
    int has_ids = 0;
    if (json_is_array(member_ids)) {
        size_t n = json_len(member_ids);
        for (size_t i = 0; i < n; i++) json_append(ids, json_copy(json_get(member_ids, i)));
        has_ids = n > 0;
    }
    json_set(out, "member_ids", ids);
    json_set(out, "everyone", json_bool(!has_ids));
    json_set(out, "fallback", json_bool(member_ids == NULL || member_ids->type == JSON_NULL));
    return out;
}

json_t *w13_hostedroom_d92a93_one_line(const struct json_t *text) {
    const char *raw = jstr(text);
    if (!raw) raw = "";
    size_t n = strlen(raw);
    char *flat = (char *)malloc(n + 2);
    if (!flat) return json_string("");
    size_t m = 0;
    int pending_space = 0;
    for (size_t i = 0; i < n; i++) {
        if (isspace((unsigned char)raw[i])) {
            if (m > 0) pending_space = 1;
        } else {
            if (pending_space) flat[m++] = ' ';
            pending_space = 0;
            flat[m++] = raw[i];
        }
    }
    flat[m] = '\0';
    json_t *out;
    if (m <= ROUTER_MESSAGE_CHARS) {
        out = json_string(flat);
    } else {
        char *cut = (char *)malloc(ROUTER_MESSAGE_CHARS + 4);
        if (!cut) { free(flat); return json_string(""); }
        memcpy(cut, flat, ROUTER_MESSAGE_CHARS);
        cut[ROUTER_MESSAGE_CHARS] = '.';
        cut[ROUTER_MESSAGE_CHARS + 1] = '.';
        cut[ROUTER_MESSAGE_CHARS + 2] = '.';
        cut[ROUTER_MESSAGE_CHARS + 3] = '\0'; /* NUL at the NEW length */
        out = json_string(cut);
        free(cut);
    }
    free(flat);
    return out;
}

/* ASCII casefold: lower-case A-Z only (fixtures are ASCII handles). */
static char *ascii_casefold(const char *s) {
    char *t = py_strip(s);
    if (!t) return NULL;
    size_t n = strlen(t), a = 0;
    while (a < n && t[a] == '@') a++;
    size_t k = 0;
    for (size_t i = a; i < n; i++) t[k++] = (char)tolower((unsigned char)t[i]);
    t[k] = '\0'; /* NUL at the shortened length */
    return t;
}

json_t *w13_hostedroom_d92a93_parse_router_answer(const struct json_t *text,
                                                   const struct json_t *handles) {
    const char *raw = jstr(text);
    if (!raw) return json_null();
    char *s = py_strip(raw);
    if (!s) return json_null();
    if (strncmp(s, "```", 3) == 0) {
        char *q = py_strip(s);
        free(s);
        if (!q) return json_null();
        size_t qn = strlen(q);
        /* strip('`') strips leading and trailing backticks */
        size_t a = 0, b = qn;
        while (a < b && q[a] == '`') a++;
        while (b > a && q[b - 1] == '`') b--;
        char *core = (char *)malloc(b - a + 1);
        if (!core) { free(q); return json_null(); }
        memcpy(core, q + a, b - a);
        core[b - a] = '\0';
        free(q);
        /* removeprefix("json") */
        const char *body = core;
        if (strncmp(body, "json", 4) == 0) body += 4;
        s = py_strip(body);
        free(core);
        if (!s) return json_null();
    }
    json_t *answer = json_parse(s, NULL);
    free(s);
    if (!json_is_object(answer) || json_object_size(answer) != 1) {
        json_free(answer);
        return json_null();
    }
    const char *key = json_object_get_key_at(answer, 0);
    if (!key || strcmp(key, "members") != 0) { json_free(answer); return json_null(); }
    const json_t *picked = json_object_get_at(answer, 0);
    if (jstr(picked) && !strcmp(jstr(picked), "everyone")) {
        json_free(answer);
        return json_array();
    }
    if (!json_is_array(picked) || json_len(picked) == 0) { json_free(answer); return json_null(); }
    size_t n = json_len(picked);
    char **keys = (char **)calloc(n, sizeof(char *));
    if (!keys) { json_free(answer); return json_null(); }
    for (size_t i = 0; i < n; i++) {
        const char *item = jstr(json_get(picked, i));
        if (!item) { for (size_t j = 0; j < i; j++) free(keys[j]); free(keys); json_free(answer); return json_null(); }
        keys[i] = ascii_casefold(item);
    }
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < i; j++) {
            if (!strcmp(keys[i], keys[j])) {
                for (size_t k = 0; k < n; k++) free(keys[k]);
                free(keys); json_free(answer); return json_null();
            }
        }
        if (!json_is_object(handles) || !json_obj_get(handles, keys[i])) {
            for (size_t k = 0; k < n; k++) free(keys[k]);
            free(keys); json_free(answer); return json_null();
        }
    }
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) {
        json_append(out, json_copy(json_obj_get(handles, keys[i])));
        free(keys[i]);
    }
    free(keys);
    json_free(answer);
    return out;
}

