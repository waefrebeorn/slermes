/*
 * hosted_room_lead.c — C home for upstream Hermes gateway/hosted_room_lead.py.
 *
 * PoP: w10_hostedroom_1cff4f_parse_assignment @ gateway/hosted_room_lead.py:parse_assignment
 * PoP: w10_hostedroom_1cff4f_assignment @ gateway/hosted_room_lead.py:assignment
 * PoP: w10_hostedroom_1cff4f_is_routing_task @ gateway/hosted_room_lead.py:is_routing_task
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_hostedroom_1cff4f.h"

/* Mirrors _first_assign_object: scan each '{', decode the balanced JSON object
 * there, and return the first one carrying an "assign" key. String literals and
 * escapes are honored so braces inside text do not end the object early. */
static json_t *first_assign_object(const char *text) {
    if (!text) return NULL;
    const char *p = text;
    while ((p = strchr(p, '{')) != NULL) {
        int depth = 0, in_str = 0, esc = 0;
        const char *q;
        size_t end = 0;
        for (q = p; *q; q++) {
            char c = *q;
            if (in_str) {
                if (esc) { esc = 0; continue; }
                if (c == '\\') { esc = 1; continue; }
                if (c == '"') in_str = 0;
                continue;
            }
            if (c == '"') { in_str = 1; continue; }
            if (c == '{') depth++;
            else if (c == '}') {
                depth--;
                if (depth == 0) { end = (size_t)(q - p) + 1; break; }
            }
        }
        if (end > 0) {
            char *buf = (char *)malloc(end + 1);
            if (buf) {
                memcpy(buf, p, end);
                buf[end] = '\0';
                json_t *v = json_parse(buf, NULL);
                free(buf);
                if (v && v->type == JSON_OBJECT && json_obj_get(v, "assign")) return v;
                json_free(v);
            }
        }
        p++;
    }
    return NULL;
}

/* Mirrors parse_assignment: normalize each string pick with
 * str(item).strip().lstrip("@").casefold(), then keep roster members whose
 * casefolded handle is in that set, in roster order. */
json_t *w10_hostedroom_1cff4f_parse_assignment(const json_t *in) {
    json_t *out = json_array();
    const char *text = json_get_str(in, "text", "");
    json_t *answer = first_assign_object(text ? text : "");
    if (!answer) return out;

    const json_t *picked = json_obj_get(answer, "assign");
    json_t *handles = json_array();
    if (picked && picked->type == JSON_ARRAY) {
        size_t n = json_len(picked);
        for (size_t i = 0; i < n; i++) {
            const json_t *it = json_get(picked, i);
            if (!it || it->type != JSON_STRING || !it->str_val) continue; /* isinstance(str) */
            const char *s = it->str_val;
            const char *e = s + strlen(s);
            while (s < e && isspace((unsigned char)*s)) s++;     /* .strip() */
            while (e > s && isspace((unsigned char)e[-1])) e--;
            while (s < e && *s == '@') s++;                      /* .lstrip("@") */
            size_t len = (size_t)(e - s);
            char *norm = (char *)malloc(len + 1);
            if (!norm) continue;
            for (size_t k = 0; k < len; k++) norm[k] = (char)tolower((unsigned char)s[k]);
            norm[len] = '\0';
            int dup = 0;
            for (size_t j = 0; j < json_len(handles); j++) {
                const json_t *h = json_get(handles, j);
                if (h && h->str_val && !strcmp(h->str_val, norm)) { dup = 1; break; }
            }
            if (!dup) json_append(handles, json_string(norm));
            free(norm);
        }
    }
    json_free(answer);

    const json_t *members = json_obj_get(in, "members");
    if (members && members->type == JSON_ARRAY) {
        size_t n = json_len(members);
        for (size_t i = 0; i < n; i++) {
            const json_t *m = json_get(members, i);
            if (!m || m->type != JSON_OBJECT) continue;
            const char *handle = json_get_str(m, "handle", "");
            const json_t *midv = json_obj_get(m, "member_id");
            if (!handle || !midv || midv->type != JSON_STRING || !midv->str_val) continue;
            size_t hl = strlen(handle);
            char *lh = (char *)malloc(hl + 1);
            if (!lh) continue;
            for (size_t k = 0; k < hl; k++) lh[k] = (char)tolower((unsigned char)handle[k]);
            lh[hl] = '\0';
            int match = 0;
            for (size_t j = 0; j < json_len(handles); j++) {
                const json_t *h = json_get(handles, j);
                if (h && h->str_val && !strcmp(h->str_val, lh)) { match = 1; break; }
            }
            free(lh);
            if (match) json_append(out, json_string(midv->str_val));
        }
    }
    json_free(handles);
    return out;
}

/* Mirrors assignment: first event with kind == "room.assignment" and
 * payload["discussion_event_id"] == target; returns its id (or null). */
json_t *w10_hostedroom_1cff4f_assignment(const json_t *in) {
    const char *target = json_get_str(in, "discussion_event_id", "");
    const json_t *events = json_obj_get(in, "events");
    if (events && events->type == JSON_ARRAY) {
        size_t n = json_len(events);
        for (size_t i = 0; i < n; i++) {
            const json_t *e = json_get(events, i);
            if (!e || e->type != JSON_OBJECT) continue;
            const char *kind = json_get_str(e, "kind", "");
            if (!kind || strcmp(kind, "room.assignment")) continue;
            const json_t *p = json_obj_get(e, "payload");
            const char *deid = p ? json_get_str(p, "discussion_event_id", "") : "";
            if (deid && !strcmp(deid, target)) {
                const json_t *idv = json_obj_get(e, "id");
                if (idv && idv->type == JSON_STRING && idv->str_val)
                    return json_string(idv->str_val);
                return json_null();
            }
        }
    }
    return json_null();
}

/* Mirrors: task.payload.get("lead_routing") is True */
json_t *w10_hostedroom_1cff4f_is_routing_task(const json_t *in) {
    const json_t *task = json_obj_get(in, "payload");
    const json_t *lr = task ? json_obj_get(task, "lead_routing") : NULL;
    return json_bool(lr && lr->type == JSON_BOOL && lr->bool_val);
}
