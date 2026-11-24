/*
 * methods_subagents.c — C home for upstream Hermes tui_gateway/methods_subagents.py.
 *
 * PoP: w18_methodssub_378aa1_child_session_id @ tui_gateway/methods_subagents.py:_child_session_id
 * PoP: w18_methodssub_378aa1_stored_owner @ tui_gateway/methods_subagents.py:_stored_owner
 *
 * Both helpers are pure: they read the supplied record/store snapshot and never
 * touch the live delegate registry or process state.
 */
#include "noc_w18_methodssub_378aa1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *str_of(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}

static int in_array(const json_t *arr, const char *want) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (i = 0; i < arr->c.count; ++i)
        if (!strcmp(str_of(arr->c.items[i]), want)) return 1;
    return 0;
}

json_t *w18_methodssub_378aa1_child_session_id(const json_t *record) {
    const json_t *agent = record ? json_obj_get(record, "agent") : NULL;
    const char *child;
    if (!agent || agent->type != JSON_OBJECT) return json_null();
    child = str_of(json_obj_get(agent, "session_id"));
    return *child ? json_string(child) : json_null();
}

json_t *w18_methodssub_378aa1_stored_owner(const json_t *db, const json_t *owner_ids,
                                           const json_t *namespaces) {
    const char *stored_id = str_of(owner_ids ? json_get(owner_ids, 0) : NULL);
    const char *route = str_of(owner_ids && json_len(owner_ids) > 1 ? json_get(owner_ids, 1) : NULL);
    const char *parts[16];
    size_t np = 0, i, start = 0, n;
    const json_t *sessions, *routing, *found;

    if (*stored_id) {
        sessions = db ? json_obj_get(db, "sessions") : NULL;
        return json_string(in_array(sessions, stored_id) ? stored_id : "");
    }

    n = strlen(route);
    for (i = 0; i <= n; ++i) {
        if (route[i] == ':' || route[i] == '\0') {
            if (np < 16) {
                char *p = (char *)malloc(i - start + 1);
                if (!p) return json_string("");
                memcpy(p, route + start, i - start);
                p[i - start] = '\0';
                parts[np++] = p;
            }
            start = i + 1;
        }
    }
    if (np < 3 || strcmp(parts[0], "agent") || !in_array(namespaces, parts[1])) {
        for (i = 0; i < np; ++i) free((void *)parts[i]);
        return json_string("");
    }
    routing = db ? json_obj_get(db, "routing") : NULL;
    found = routing ? json_obj_get(routing, route) : NULL;
    {
        const char *sid = str_of(found);
        json_t *out = json_string(*sid ? sid : "");
        for (i = 0; i < np; ++i) free((void *)parts[i]);
        return out;
    }
}

