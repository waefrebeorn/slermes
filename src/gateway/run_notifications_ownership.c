/*
 * run_notifications_ownership.c - C home for gateway/run_notifications_ownership.py.
 * Faithful port of the completion-ownership decisions: the provenance walk, the
 * compression-lineage acceptance rule and the closed-local-target rule. The store is
 * data in, verdict out; the awaits themselves remain the Python boundary.
 * PoP: w19_runnotific_f48e0a_lookup_owner @ gateway/run_notifications_ownership.py:GatewayNotificationOwnershipMixin._lookup_completion_owner
 * PoP: w19_runnotific_f48e0a_compression_target @ gateway/run_notifications_ownership.py:GatewayNotificationOwnershipMixin._resolve_compression_lineage_target
 * PoP: w19_runnotific_f48e0a_closed_local_target @ gateway/run_notifications_ownership.py:GatewayNotificationOwnershipMixin._closed_local_target
 */
#include "noc_w19_runnotific_f48e0a.h"
#include <stdlib.h>
#include <string.h>

#define MAX_DELEGATE_PROVENANCE_HOPS 16
#define MAX_SEEN (MAX_DELEGATE_PROVENANCE_HOPS + 2)

static const char *const USER_BOUNDARY_END_REASONS[] = {
    "session_reset", "user_exit", "session_switch", "new_session"};

static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

static int py_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *py_strip_dup(const char *s) {
    const char *a;
    const char *b;
    size_t n;
    char *out;
    if (!s) return dupstr("");
    a = s;
    while (*a && py_ws((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && py_ws((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

static const char *jstr(const json_t *obj, const char *key) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static int jbool(const json_t *obj, const char *key) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    if (!v) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    return 0;
}

static int seq_has(const char *const *list, size_t n, const char *value) {
    size_t i;
    if (!value) return 0;
    for (i = 0; i < n; i++)
        if (!strcmp(list[i], value)) return 1;
    return 0;
}

/* store.aio.sessions.get(session_id) over the modeled table */
static json_t *find_row(json_t *sessions, const char *sid) {
    size_t i;
    if (!sessions || sessions->type != JSON_ARRAY || !sid) return NULL;
    for (i = 0; i < json_len(sessions); i++) {
        json_t *row = json_get(sessions, i);
        const char *id = jstr(row, "session_id");
        if (id && !strcmp(id, sid)) return row;
    }
    return NULL;
}

static int list_has(json_t *arr, const char *sid) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY || !sid) return 0;
    for (i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, sid)) return 1;
    }
    return 0;
}

static const char *tip_of(json_t *tips, const char *sid) {
    json_t *v = (tips && tips->type == JSON_OBJECT && sid) ? json_obj_get(tips, sid) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

/* ---- _lookup_completion_owner(session_id, current_session_id) ---- */
json_t *w19_runnotific_f48e0a_lookup_owner(json_t *sessions, json_t *tips,
                                           json_t *get_raises, json_t *tip_raises,
                                           const char *session_id,
                                           const char *current_session_id,
                                           int store_none) {
    json_t *out = json_object();
    char *seen[MAX_SEEN];
    size_t nseen = 0;
    size_t si;
    char *cur = dupstr(session_id ? session_id : "");
    char *current = dupstr(current_session_id ? current_session_id : "");
    const char *verdict = NULL;
    json_t *row = NULL;
    int hops;

    if (!cur || !current) { free(cur); free(current); return json_null(); }
    if (store_none) {
        json_set(out, "verdict", json_string("retry"));
        json_set(out, "session", json_string(cur));
        json_set(out, "row", json_null());
        free(cur);
        free(current);
        return out;
    }

    for (hops = 0; hops <= MAX_DELEGATE_PROVENANCE_HOPS; hops++) {
        size_t i;
        int dup = 0;
        for (i = 0; i < nseen; i++)
            if (seen[i] && !strcmp(seen[i], cur)) { dup = 1; break; }
        if (dup) { verdict = "terminal"; row = NULL; break; }
        if (nseen < MAX_SEEN) {
            seen[nseen] = dupstr(cur);  /* owned copy: `cur` is replaced on the next hop */
            if (seen[nseen]) nseen++;
        }

        if (list_has(get_raises, cur)) { verdict = "retry"; row = NULL; break; }
        row = find_row(sessions, cur);
        if (!row) { verdict = "terminal"; break; }

        if (jbool(row, "ended_at")) {
            const char *reason = jstr(row, "end_reason");
            if (reason && seq_has(USER_BOUNDARY_END_REASONS,
                                  sizeof(USER_BOUNDARY_END_REASONS) / sizeof(USER_BOUNDARY_END_REASONS[0]),
                                  reason)) {
                verdict = "terminal";
                break;
            }
        }
        if (current[0] && !strcmp(cur, current)) { verdict = "deliver"; break; }

        if (current[0]) {
            const char *reason = jstr(row, "end_reason");
            if (reason && !strcmp(reason, "compression")) {
                const char *tip;
                if (list_has(tip_raises, cur)) { verdict = "retry"; row = NULL; break; }
                tip = tip_of(tips, cur);
                if (tip && !strcmp(tip, current)) { verdict = "deliver"; break; }
            }
        }

        {
            json_t *config = (row->type == JSON_OBJECT) ? json_obj_get(row, "model_config") : NULL;
            if (!config || config->type != JSON_OBJECT) {
                /* `"_delegate_from" not in None` raises inside the try -> retry */
                verdict = "retry";
                row = NULL;
                break;
            }
            {
                json_t *parent_node = json_obj_get(config, "_delegate_from");
                const char *parent = (parent_node && parent_node->type == JSON_STRING)
                                   ? parent_node->str_val : NULL;
                char *trimmed;
                if (parent_node && parent_node->type != JSON_NULL &&
                    (!parent_node || parent_node->type != JSON_STRING)) {
                    verdict = "terminal";  /* not isinstance(parent, str) */
                    row = NULL;
                    break;
                }
                if (!parent) {
                    const char *source = jstr(row, "source");
                    verdict = (source && !strcmp(source, "subagent")) ? "terminal" : "deliver";
                    break;
                }
                trimmed = py_strip_dup(parent);
                if (!trimmed || !trimmed[0] || hops == MAX_DELEGATE_PROVENANCE_HOPS) {
                    free(trimmed);
                    verdict = "terminal";
                    row = NULL;
                    break;
                }
                free(trimmed);
                free(cur);
                cur = dupstr(parent);
                if (!cur) { verdict = "terminal"; row = NULL; break; }
            }
        }
    }
    if (!verdict) { verdict = "terminal"; row = NULL; }
    json_set(out, "verdict", json_string(verdict));
    json_set(out, "session", json_string(cur ? cur : ""));
    json_set(out, "row", row ? json_copy(row) : json_null());
    for (si = 0; si < nseen; si++) free(seen[si]);
    free(cur);
    free(current);
    return out;
}

/* ---- _resolve_compression_lineage_target(store_aio, session_entry, pinned) ---- */
json_t *w19_runnotific_f48e0a_compression_target(json_t *sessions, json_t *tips,
                                                 json_t *entry, const char *pinned_session_id,
                                                 int get_raises, int tip_raises) {
    json_t *out = json_object();
    const char *entry_id = (entry && entry->type == JSON_OBJECT) ? jstr(entry, "session_id") : NULL;
    const char *pinned = pinned_session_id ? pinned_session_id : "";
    const char *verdict = "terminal";
    const char *target = NULL;

    if (tip_raises) {
        verdict = "retry";
    } else {
        const char *tip = tip_of(tips, pinned);
        if (!tip || !strcmp(tip, pinned)) {
            verdict = "retry";
        } else if (get_raises) {
            verdict = "retry";
        } else {
            json_t *tip_row = find_row(sessions, tip);
            if (!tip_row || jbool(tip_row, "ended_at")) {
                verdict = "retry";
            } else if (!entry || !entry_id || !strcmp(entry_id, pinned) || !strcmp(entry_id, tip)) {
                verdict = "deliver";
                target = tip;
            } else {
                json_t *route_row = find_row(sessions, entry_id);
                if (!route_row) {
                    verdict = "retry";
                } else {
                    const char *reason = jstr(route_row, "end_reason");
                    if (jbool(route_row, "ended_at") && reason && !strcmp(reason, "compression")) {
                        const char *route_tip = tip_of(tips, entry_id);
                        if (!route_tip || !strcmp(route_tip, entry_id)) {
                            verdict = "retry";
                        } else if (!strcmp(route_tip, tip)) {
                            verdict = "deliver";
                            target = tip;
                        } else {
                            verdict = "terminal";
                        }
                    } else {
                        verdict = "terminal";
                    }
                }
            }
        }
    }
    json_set(out, "verdict", json_string(verdict));
    json_set(out, "target", target ? json_string(target) : json_null());
    return out;
}

/* ---- _closed_local_target(store_aio, entry, pinned, pinned_row) ---- */
json_t *w19_runnotific_f48e0a_closed_local_target(json_t *sessions, json_t *tips,
                                                  json_t *entry, const char *pinned_session_id,
                                                  json_t *pinned_row, int tip_raises) {
    const char *entry_id = (entry && entry->type == JSON_OBJECT) ? jstr(entry, "session_id") : NULL;
    const char *platform = (entry && entry->type == JSON_OBJECT) ? jstr(entry, "platform") : NULL;
    const char *pinned = pinned_session_id ? pinned_session_id : "";
    const char *target = pinned;
    json_t *row = pinned_row;

    if (!entry_id || !platform || strcmp(platform, "local") != 0) return json_string("");
    if (!pinned_row || pinned_row->type != JSON_OBJECT) return json_string("");

    {
        const char *reason = jstr(pinned_row, "end_reason");
        if (reason && !strcmp(reason, "compression")) {
            const char *tip;
            if (tip_raises) return json_string("");  /* STORE_FAILURES: the compression path defers it */
            tip = tip_of(tips, pinned);
            target = tip ? tip : "";
            row = NULL;
            if (tip && entry_id && !strcmp(tip, entry_id)) row = find_row(sessions, tip);
        }
    }
    if (!entry_id || strcmp(target, entry_id) != 0 || !row || row->type != JSON_OBJECT)
        return json_string("");
    if (!jbool(row, "ended_at")) return json_string("");
    {
        const char *reason = jstr(row, "end_reason");
        if (reason && !strcmp(reason, "compression")) return json_string("");
    }
    return json_string(target);
}
