/*
 * hosted_room_service.c — C11 port of upstream Hermes
 * tui_gateway/hosted_room_service.py (the same-gateway hosted Discussion room
 * coordinator: room/session state transitions, capability/status derivation,
 * payload and envelope shaping, predicates and coercion).
 *
 * STATUS: PURE CORE PORTED — every symbol below is a 1:1 port of a pure
 *         function/method of that module and is checked by three differential
 *         gates (w28_hostedroom_e01c0e_p1 / _p2 / _p3) against the live Python.
 *         The store, the peer transports, the worker runtime, the clocks and
 *         the locks are seams: fixtures feed their answers in, nothing here
 *         opens a socket, a database or a thread.
 *
 * PoP (proof of provenance: C symbol @ upstream definition)
 *   hrs_task_member_id          @ tui_gateway/hosted_room_service.py:task_member_id
 *   hrs_member_of               @ tui_gateway/hosted_room_service.py:_member_of
 *   hrs_attempt_action          @ tui_gateway/hosted_room_service.py:attempt_action
 *   hrs_approvable_attempts     @ tui_gateway/hosted_room_service.py:_approvable_attempts
 *   hrs_attempt_actions         @ tui_gateway/hosted_room_service.py:_attempt_actions
 *   hrs_member_is_peer          @ tui_gateway/hosted_room_service.py:_member_is_peer
 *   hrs_grant_revoke_is_terminal@ tui_gateway/hosted_room_service.py:_grant_revoke_is_terminal
 *   hrs_route_statuses          @ tui_gateway/hosted_room_service.py:_route_statuses
 *   hrs_live_status_sig         @ tui_gateway/hosted_room_service.py:live_status_sig
 *   hrs_turn_timeout_seconds    @ tui_gateway/hosted_room_service.py:_hosted_room_turn_timeout_seconds
 *   hrs_constants               @ tui_gateway/hosted_room_service.py:<module> constants
 *   hrs_authority               @ tui_gateway/hosted_room_service.py:_authority
 *   hrs_activity_event          @ tui_gateway/hosted_room_service.py:_append_room_status
 *   hrs_continue_event          @ tui_gateway/hosted_room_service.py:_append_continue
 *   hrs_stop_room_tally         @ tui_gateway/hosted_room_service.py:_stop_room
 *   hrs_retry_room_task         @ tui_gateway/hosted_room_service.py:retry_room_task
 *   hrs_owed_revokes            @ tui_gateway/hosted_room_service.py:retry_owed_revokes
 *
 * Python semantics honoured here on purpose:
 *   - truthiness (`x or y`) over JSON null/false/0/""/[]/{}
 *   - str() of the scalars these ids are built from
 *   - dict/set identity: attempts are a set (deduped) and sorted; task rows keep
 *     insertion order; `sorted(rows)` on status signatures is a tuple sort.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hermes_json.h"

/* ------------------------------------------------------------------ utils */

static char *hrs_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python truthiness of a JSON value. */
static int hrs_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

/* str() of a JSON scalar; arrays/objects collapse to "" (never used by the
 * ported ids, which are all strings or small integers). */
static char *hrs_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return hrs_dup("None");
    switch (v->type) {
        case JSON_STRING: return hrs_dup(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return hrs_dup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return hrs_dup(buf);
        default: return hrs_dup("");
    }
}

static int hrs_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }

static const char *hrs_s(const json_t *o, const char *k) {
    const json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static double hrs_num(const json_t *o, const char *k, double def) {
    const json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

static long long hrs_ll(const json_t *o, const char *k, long long def) {
    const json_t *v = json_obj_get(o, k);
    if (!v || v->type != JSON_NUMBER) return def;
    return (long long)v->num_val;
}

static int hrs_status_in(const json_t *task, const char *const *set) {
    const char *s = hrs_s(task, "status");
    if (!s) return 0;
    for (; *set; set++)
        if (!strcmp(s, *set)) return 1;
    return 0;
}

/* python-visible status families (tui_gateway/hosted_room_service.py) */
static const char *const HRS_LIVE[] = {"queued", "running", "stopping", NULL};
static const char *const HRS_STOPPABLE[] = {"queued", "running", "indeterminate", "deferred", "stopping", NULL};
static const char *const HRS_RETRYABLE[] = {"indeterminate", "deferred", NULL};
static const char *const HRS_APPROVABLE[] = {"running", "indeterminate", "deferred", NULL};
static const char *const HRS_TERMINAL[] = {"deferred", "settled", "failed", "cancelled", NULL};
static const char *const HRS_PEER_RUN_ENDED[] = {"cancelled", "completed", "failed", "interrupted", NULL};

/* ---- a tiny string builder, for Python's repr() of a pending action ---- */

typedef struct {
    char *buf;
    size_t len, cap;
} hrs_sb;

static void hrs_sb_init(hrs_sb *sb) { sb->buf = NULL; sb->len = sb->cap = 0; }

static void hrs_sb_puts(hrs_sb *sb, const char *s) {
    size_t n = strlen(s);
    if (sb->len + n + 1 > sb->cap) {
        size_t cap = sb->cap ? sb->cap : 64;
        char *nb;
        while (cap < sb->len + n + 1) cap *= 2;
        nb = (char *)realloc(sb->buf, cap);
        if (!nb) return;
        sb->buf = nb;
        sb->cap = cap;
    }
    memcpy(sb->buf + sb->len, s, n + 1);
    sb->len += n;
}

static void hrs_sb_printf(hrs_sb *sb, const char *fmt, ...) {
    char tmp[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    hrs_sb_puts(sb, tmp);
}

/* repr('text'): single quotes unless the text itself holds one. */
static void hrs_sb_repr_str(hrs_sb *sb, const char *s) {
    char q = strchr(s, '\'') && !strchr(s, '"') ? '"' : '\'';
    hrs_sb_puts(sb, q == '\'' ? "'" : "\"");
    for (; *s; s++) {
        if (*s == '\\' || *s == q) hrs_sb_puts(sb, "\\");
        hrs_sb_printf(sb, "%c", *s);
    }
    hrs_sb_puts(sb, q == '\'' ? "'" : "\"");
}

static void hrs_sb_repr_value(hrs_sb *sb, const json_t *v) {
    char tmp[64];
    if (!v || v->type == JSON_NULL) { hrs_sb_puts(sb, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: hrs_sb_puts(sb, v->bool_val ? "True" : "False"); return;
        case JSON_STRING: hrs_sb_repr_str(sb, v->str_val ? v->str_val : ""); return;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(tmp, sizeof tmp, "%lld", (long long)v->num_val);
            else
                snprintf(tmp, sizeof tmp, "%.17g", v->num_val);
            hrs_sb_puts(sb, tmp);
            return;
        default: {
            char *s = json_dumps(v, 0);
            hrs_sb_puts(sb, s ? s : "None");
            free(s);
            return;
        }
    }
}

/* repr(sorted(action.items())) */
static char *hrs_repr_sorted_items(const json_t *action) {
    hrs_sb sb;
    size_t n, i, j;
    const char **keys;
    hrs_sb_init(&sb);
    hrs_sb_puts(&sb, "[");
    if (!hrs_is_obj(action)) { hrs_sb_puts(&sb, "]"); return sb.buf ? sb.buf : hrs_dup("[]"); }
    n = json_object_size(action);
    keys = (const char **)malloc(sizeof(char *) * (n ? n : 1));
    if (!keys) { free(sb.buf); return hrs_dup("[]"); }
    for (i = 0; i < n; i++) keys[i] = json_object_get_key_at(action, i);
    for (i = 1; i < n; i++) { /* insertion sort: tiny maps, stable order by key */
        const char *k = keys[i];
        for (j = i; j > 0 && strcmp(keys[j - 1], k) > 0; j--) keys[j] = keys[j - 1];
        keys[j] = k;
    }
    for (i = 0; i < n; i++) {
        if (i) hrs_sb_puts(&sb, ", ");
        hrs_sb_puts(&sb, "(");
        hrs_sb_repr_str(&sb, keys[i]);
        hrs_sb_puts(&sb, ", ");
        hrs_sb_repr_value(&sb, json_obj_get(action, keys[i]));
        hrs_sb_puts(&sb, ")");
    }
    hrs_sb_puts(&sb, "]");
    free((void *)keys);
    return sb.buf ? sb.buf : hrs_dup("[]");
}

/* ------------------------------------------------------- gate p1: identity */

/* task_member_id(task): str(payload.target_member_id or payload.target_profile) */
static json_t *hrs_member_id_of(const json_t *task) {
    const json_t *payload = json_obj_get(task, "payload");
    const json_t *tm = json_obj_get(payload, "target_member_id");
    const json_t *v = hrs_truthy(tm) ? tm : json_obj_get(payload, "target_profile");
    char *s = hrs_pystr(v);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

json_t *hrs_task_member_id(const json_t *task) { return hrs_member_id_of(task); }

/* _member_of(task): str(payload.get("target_member_id") or payload.get("target_profile") or "") */
json_t *hrs_member_of(const json_t *task) {
    const json_t *payload = json_obj_get(task, "payload");
    const json_t *tm, *v;
    char *s;
    json_t *out;
    if (!hrs_truthy(payload)) payload = NULL;
    tm = json_obj_get(payload, "target_member_id");
    v = hrs_truthy(tm) ? tm : json_obj_get(payload, "target_profile");
    if (!hrs_truthy(v)) return json_string("");
    s = hrs_pystr(v);
    out = json_string(s ? s : "");
    free(s);
    return out;
}

/* attempt_action(kind, task) */
json_t *hrs_attempt_action(const char *kind, const json_t *task) {
    const json_t *identity = json_obj_get(task, "identity");
    const char *task_id = hrs_s(identity, "task_id");
    json_t *o = json_object();
    json_set(o, "kind", json_string(kind ? kind : ""));
    json_set(o, "member_id", hrs_member_id_of(task));
    json_set(o, "task_id", json_string(task_id ? task_id : ""));
    json_set(o, "execution_generation", json_number(hrs_num(task, "execution_generation", 0.0)));
    return o;
}

typedef struct {
    char *member;
    char *task_id;
    long long gen;
} hrs_attempt;

static int hrs_attempt_cmp(const void *pa, const void *pb) {
    const hrs_attempt *a = (const hrs_attempt *)pa, *b = (const hrs_attempt *)pb;
    int c = strcmp(a->member, b->member);
    if (c) return c;
    c = strcmp(a->task_id, b->task_id);
    if (c) return c;
    if (a->gen < b->gen) return -1;
    if (a->gen > b->gen) return 1;
    return 0;
}

/* _approvable_attempts(tasks): a set of (member_id, task_id, execution_generation) */
json_t *hrs_approvable_attempts(const json_t *tasks) {
    size_t n = json_len(tasks), i, k, kept = 0;
    hrs_attempt *rows = (hrs_attempt *)calloc(n ? n : 1, sizeof *rows);
    json_t *out = json_array();
    if (!rows) return out;
    for (i = 0; i < n; i++) {
        const json_t *task = json_get(tasks, i);
        const json_t *identity = json_obj_get(task, "identity");
        const char *task_id = hrs_s(identity, "task_id");
        json_t *mid;
        if (!hrs_is_obj(task) || !hrs_status_in(task, HRS_APPROVABLE)) continue;
        mid = hrs_member_id_of(task);
        rows[kept].member = hrs_dup(mid && mid->type == JSON_STRING ? mid->str_val : "");
        json_free(mid);
        rows[kept].task_id = hrs_dup(task_id ? task_id : "");
        rows[kept].gen = hrs_ll(task, "execution_generation", 0);
        kept++;
    }
    /* the upstream value is a set: duplicates collapse */
    for (i = 0; i < kept; i++)
        for (k = i + 1; k < kept;) {
            if (!strcmp(rows[i].member, rows[k].member) && !strcmp(rows[i].task_id, rows[k].task_id)
                && rows[i].gen == rows[k].gen) {
                free(rows[k].member);
                free(rows[k].task_id);
                memmove(&rows[k], &rows[k + 1], (kept - k - 1) * sizeof *rows);
                kept--;
            } else {
                k++;
            }
        }
    if (kept > 1) qsort(rows, kept, sizeof *rows, hrs_attempt_cmp);
    for (i = 0; i < kept; i++) {
        json_t *row = json_array();
        json_append(row, json_string(rows[i].member));
        json_append(row, json_string(rows[i].task_id));
        json_append(row, json_number((double)rows[i].gen));
        json_append(out, row);
        free(rows[i].member);
        free(rows[i].task_id);
    }
    free(rows);
    return out;
}

/* _attempt_actions(room_id, tasks_shown): Retry each retryable row, in order */
json_t *hrs_attempt_actions(const json_t *tasks) {
    size_t n = json_len(tasks), i;
    json_t *out = json_array();
    for (i = 0; i < n; i++) {
        const json_t *task = json_get(tasks, i);
        if (!hrs_is_obj(task) || !hrs_status_in(task, HRS_RETRYABLE)) continue;
        json_append(out, hrs_attempt_action("retry", task));
    }
    return out;
}

/* _member_is_peer(room_id, member_id) over an already-read room state */
json_t *hrs_member_is_peer(const json_t *room, const char *member_id) {
    const json_t *members = json_obj_get(room, "members");
    size_t n, i;
    if (!hrs_truthy(members)) return json_bool(false);
    n = json_len(members);
    for (i = 0; i < n; i++) {
        const json_t *m = json_get(members, i);
        const json_t *mid, *target;
        char *s;
        int eq;
        if (!hrs_is_obj(m)) continue;
        mid = json_obj_get(m, "member_id");
        if (!hrs_truthy(mid)) mid = json_obj_get(m, "profile");
        s = hrs_truthy(mid) ? hrs_pystr(mid) : hrs_dup("");
        eq = (member_id && s && !strcmp(s, member_id));
        free(s);
        if (!eq) continue;
        target = json_obj_get(m, "target");
        return json_bool(hrs_is_obj(target) && hrs_s(target, "kind") && !strcmp(hrs_s(target, "kind"), "peer"));
    }
    return json_bool(false);
}

/* ------------------------------------------ gate p2: status & predicates */

/* _grant_revoke_is_terminal(exc) */
json_t *hrs_grant_revoke_is_terminal(const json_t *status_code, const char *error_code) {
    double code = (status_code && status_code->type == JSON_NUMBER) ? status_code->num_val : 0.0;
    int status_ok = (status_code && status_code->type == JSON_NUMBER && (code == 401.0 || code == 403.0));
    int error_ok = error_code && (!strcmp(error_code, "invalid_room_grant")
                                  || !strcmp(error_code, "room_reauthorization_required"));
    return json_bool(status_ok && error_ok);
}

typedef struct {
    const char *room;
    const char *member;
    const char *status;
} hrs_route_row;

static int hrs_route_cmp(const void *pa, const void *pb) {
    const hrs_route_row *a = (const hrs_route_row *)pa, *b = (const hrs_route_row *)pb;
    int c = strcmp(a->room, b->room);
    if (c) return c;
    return strcmp(a->member, b->member);
}

/* _route_statuses(room_id=None): [{"room_id","member_id","status"}] sorted by key */
json_t *hrs_route_statuses(const json_t *routes, const char *room_id) {
    size_t n = json_len(routes), i, kept = 0;
    hrs_route_row *rows = (hrs_route_row *)calloc(n ? n : 1, sizeof *rows);
    json_t *out = json_array();
    if (!rows) return out;
    for (i = 0; i < n; i++) {
        const json_t *r = json_get(routes, i);
        const char *rid, *mid, *st;
        if (!hrs_is_obj(r)) continue;
        rid = hrs_s(r, "room_id");
        mid = hrs_s(r, "member_id");
        st = hrs_s(r, "status");
        rid = rid ? rid : "";
        mid = mid ? mid : "";
        st = st ? st : "";
        if (room_id && strcmp(rid, room_id)) continue;
        rows[kept].room = rid;
        rows[kept].member = mid;
        rows[kept].status = st;
        kept++;
    }
    if (kept > 1) qsort(rows, kept, sizeof *rows, hrs_route_cmp);
    for (i = 0; i < kept; i++) {
        json_t *row = json_object();
        json_set(row, "room_id", json_string(rows[i].room));
        json_set(row, "member_id", json_string(rows[i].member));
        json_set(row, "status", json_string(rows[i].status));
        json_append(out, row);
    }
    free(rows);
    return out;
}

/* one row of a live_status_sig room tuple */
typedef struct {
    int tag;       /* 0 "action", 1 "blocked", 2 "prepare", 3 "route" */
    char *member;  /* action/route only */
    char *text;    /* status, repr(...) or last error */
    int flag;      /* prepare: stalled */
} hrs_sig_row;

typedef struct {
    char *room;
    hrs_sig_row *rows;
    size_t n, cap;
} hrs_sig_group;

static int hrs_sig_cmp(const void *pa, const void *pb) {
    const hrs_sig_row *a = (const hrs_sig_row *)pa, *b = (const hrs_sig_row *)pb;
    int c;
    if (a->tag != b->tag) return a->tag - b->tag;
    if (a->tag == 2 && a->flag != b->flag) return a->flag - b->flag;
    if (a->tag != 1) {
        c = strcmp(a->member ? a->member : "", b->member ? b->member : "");
        if (c) return c;
    }
    return strcmp(a->text ? a->text : "", b->text ? b->text : "");
}

static hrs_sig_group *hrs_sig_group_for(hrs_sig_group **groups, size_t *count, const char *room) {
    size_t i;
    for (i = 0; i < *count; i++)
        if (!strcmp((*groups)[i].room, room ? room : "")) return &(*groups)[i];
    {
        hrs_sig_group *ng = (hrs_sig_group *)realloc(*groups, (*count + 1) * sizeof **groups);
        if (!ng) return NULL;
        *groups = ng;
        ng[*count].room = hrs_dup(room ? room : "");
        ng[*count].rows = NULL;
        ng[*count].n = ng[*count].cap = 0;
        return &ng[(*count)++];
    }
}

static void hrs_sig_add(hrs_sig_group *g, int tag, const char *member, char *owned_text, int flag) {
    if (!g) { free(owned_text); return; }
    if (g->n == g->cap) {
        size_t cap = g->cap ? g->cap * 2 : 4;
        hrs_sig_row *nr = (hrs_sig_row *)realloc(g->rows, cap * sizeof *nr);
        if (!nr) { free(owned_text); return; }
        g->rows = nr;
        g->cap = cap;
    }
    g->rows[g->n].tag = tag;
    g->rows[g->n].member = hrs_dup(member ? member : "");
    g->rows[g->n].text = owned_text;
    g->rows[g->n].flag = flag;
    g->n++;
}

/* live_status_sig(): per room the memory-only parts of status(room_id), plus
 * key "" for whether the worker runs. */
json_t *hrs_live_status_sig(const json_t *fixture) {
    const json_t *runtime = json_obj_get(fixture, "runtime");
    const json_t *actions = json_obj_get(fixture, "pending_actions");
    const json_t *routes = json_obj_get(fixture, "routes");
    const json_t *stalls = json_obj_get(fixture, "prepare_stalls");
    hrs_sig_group *groups = NULL;
    size_t count = 0, i, j;
    json_t *out = json_object();
    int running = hrs_truthy(json_obj_get(runtime, "running")) && !hrs_truthy(json_obj_get(runtime, "stopping"));

    for (i = 0; i < json_len(actions); i++) {
        const json_t *a = json_get(actions, i);
        const json_t *action;
        char *text;
        if (!hrs_is_obj(a)) continue;
        action = json_obj_get(a, "action");
        text = hrs_repr_sorted_items(action);
        hrs_sig_add(hrs_sig_group_for(&groups, &count, hrs_s(a, "room_id")), 0, hrs_s(a, "member_id"), text, 0);
    }
    for (i = 0; i < json_len(routes); i++) {
        const json_t *r = json_get(routes, i);
        const char *st;
        if (!hrs_is_obj(r)) continue;
        st = hrs_s(r, "status");
        hrs_sig_add(hrs_sig_group_for(&groups, &count, hrs_s(r, "room_id")), 3, hrs_s(r, "member_id"),
                    hrs_dup(st ? st : ""), 0);
    }
    for (i = 0; i < json_len(json_obj_get(runtime, "blocked_rooms")); i++) {
        const json_t *rid = json_get(json_obj_get(runtime, "blocked_rooms"), i);
        char *room = hrs_pystr(rid);
        hrs_sig_add(hrs_sig_group_for(&groups, &count, room), 1, NULL, hrs_dup(""), 0);
        free(room);
    }
    for (i = 0; i < json_len(stalls); i++) {
        const json_t *s = json_get(stalls, i);
        const json_t *err;
        char *text;
        int stalled;
        if (!hrs_is_obj(s)) continue;
        err = json_obj_get(s, "error");
        stalled = hrs_truthy(json_obj_get(s, "stalled"));
        text = hrs_truthy(err) ? hrs_pystr(err) : hrs_dup("");
        hrs_sig_add(hrs_sig_group_for(&groups, &count, hrs_s(s, "room_id")), 2, NULL, text, stalled);
    }

    for (i = 0; i < count; i++) {
        json_t *arr = json_array();
        if (groups[i].n > 1) qsort(groups[i].rows, groups[i].n, sizeof *groups[i].rows, hrs_sig_cmp);
        for (j = 0; j < groups[i].n; j++) {
            json_t *row = json_array();
            switch (groups[i].rows[j].tag) {
                case 0:
                    json_append(row, json_string("action"));
                    json_append(row, json_string(groups[i].rows[j].member));
                    json_append(row, json_string(groups[i].rows[j].text));
                    break;
                case 1:
                    json_append(row, json_string("blocked"));
                    break;
                case 2:
                    json_append(row, json_string("prepare"));
                    json_append(row, json_bool(groups[i].rows[j].flag != 0));
                    json_append(row, json_string(groups[i].rows[j].text));
                    break;
                default:
                    json_append(row, json_string("route"));
                    json_append(row, json_string(groups[i].rows[j].member));
                    json_append(row, json_string(groups[i].rows[j].text));
                    break;
            }
            json_append(arr, row);
            free(groups[i].rows[j].member);
            free(groups[i].rows[j].text);
        }
        json_set(out, groups[i].room, arr);
        free(groups[i].rows);
        free(groups[i].room);
    }
    free(groups);
    {
        json_t *worker = json_array();
        json_append(worker, json_bool(running != 0));
        json_set(out, "", worker);
    }
    return out;
}

/* _hosted_room_turn_timeout_seconds(): (float(HERMES_AGENT_TIMEOUT or 1800) or 1800) + 30 */
json_t *hrs_turn_timeout_seconds(const json_t *env_value) {
    double agent_timeout = 1800.0;
    if (env_value && env_value->type == JSON_STRING && env_value->str_val) {
        char *end = NULL;
        double d = strtod(env_value->str_val, &end);
        int consumed = end && end != env_value->str_val;
        while (end && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end++;
        if (!consumed || (end && *end)) agent_timeout = 0.0;
        else agent_timeout = d;
        if (isnan(agent_timeout)) agent_timeout = 0.0;
    }
    return json_number((agent_timeout > 0 ? agent_timeout : 1800.0) + 30.0);
}

static json_t *hrs_str_array(const char *const *set) {
    json_t *a = json_array();
    for (; *set; set++) json_append(a, json_string(*set));
    return a;
}

/* module constants the room coordinator is tuned with */
json_t *hrs_constants(void) {
    json_t *o = json_object();
    json_set(o, "idle_fallback_seconds", json_number(5.0));
    json_set(o, "active_poll_seconds", json_number(0.25));
    json_set(o, "terminal_grace_seconds", json_number(30.0));
    json_set(o, "fenced_append_attempts", json_number(3));
    json_set(o, "owed_revokes_per_cycle", json_number(1));
    json_set(o, "owed_revoke_timeout_seconds", json_number(3.0));
    json_set(o, "owed_revoke_first_retry_seconds", json_number(5.0));
    json_set(o, "owed_revoke_max_retry_seconds", json_number(3600.0));
    json_set(o, "terminal_statuses", hrs_str_array(HRS_TERMINAL));
    json_set(o, "live_statuses", hrs_str_array(HRS_LIVE));
    json_set(o, "stoppable_statuses", hrs_str_array(HRS_STOPPABLE));
    json_set(o, "retryable_statuses", hrs_str_array(HRS_RETRYABLE));
    json_set(o, "approvable_statuses", hrs_str_array(HRS_APPROVABLE));
    json_set(o, "peer_run_ended", hrs_str_array(HRS_PEER_RUN_ENDED));
    return o;
}

/* _authority(room): (str(room["authority_gateway_id"]), int(room["authority_epoch"])) */
json_t *hrs_authority(const json_t *room) {
    const json_t *gw = json_obj_get(room, "authority_gateway_id");
    const json_t *epoch = json_obj_get(room, "authority_epoch");
    char *s = hrs_pystr(gw);
    json_t *out = json_array();
    json_append(out, json_string(s ? s : ""));
    json_append(out, json_number((double)(epoch && epoch->type == JSON_NUMBER ? (long long)epoch->num_val : 0)));
    free(s);
    return out;
}

/* ------------------------------------ gate p3: envelopes, tallies, guards */

/* _append_room_status(room, decision) shaped as its append_event kwargs */
json_t *hrs_activity_event(const json_t *room, const json_t *decision) {
    const json_t *deid = json_obj_get(decision, "discussion_event_id");
    const json_t *continues;
    const char *reason, *gw;
    char *deid_s, event_id[512];
    size_t used;
    json_t *event, *actor, *payload;

    if (!deid || deid->type == JSON_NULL) return json_null();
    deid_s = hrs_pystr(deid);
    reason = hrs_s(decision, "reason");
    reason = reason ? reason : "";
    continues = json_obj_get(decision, "continues");
    used = (size_t)snprintf(event_id, sizeof event_id, "dactivity:%s:%s", deid_s ? deid_s : "", reason);
    if (hrs_truthy(continues) && used < sizeof event_id)
        snprintf(event_id + used, sizeof event_id - used, ":c%lld", (long long)hrs_num(decision, "continues", 0));
    free(deid_s);

    gw = hrs_s(room, "authority_gateway_id");
    event = json_object();
    json_set(event, "room_id", json_string(hrs_s(room, "room_id") ? hrs_s(room, "room_id") : ""));
    json_set(event, "event_id", json_string(event_id));
    json_set(event, "kind", json_string("room.activity"));
    actor = json_object();
    json_set(actor, "kind", json_string("gateway"));
    json_set(actor, "id", json_string(gw ? gw : ""));
    json_set(event, "actor", actor);
    payload = json_object();
    json_set(payload, "status", json_string(hrs_s(decision, "status") ? hrs_s(decision, "status") : ""));
    json_set(payload, "reason_code", json_string(reason));
    json_set(payload, "thread_id", json_string(hrs_s(decision, "thread_id") ? hrs_s(decision, "thread_id") : ""));
    json_set(payload, "discussion_event_id",
             json_string(hrs_s(decision, "discussion_event_id") ? hrs_s(decision, "discussion_event_id") : ""));
    json_set(event, "payload", payload);
    json_set(event, "authority_gateway_id", json_string(gw ? gw : ""));
    json_set(event, "authority_epoch", json_number(hrs_num(room, "authority_epoch", 0.0)));
    return event;
}

/* _append_continue(...) shaped as its append_event kwargs, or the refusal */
json_t *hrs_continue_event(const char *room_id, const char *discussion_event_id, const char *gateway_id,
                           double epoch, const json_t *paused, double max_continues,
                           const json_t *event_seqs) {
    json_t *out = json_object();
    long long grant, latest = 0;
    char event_id[512];
    json_t *event, *actor, *payload;
    size_t i;

    if (!paused || paused->type == JSON_NULL) {
        json_set(out, "outcome", json_string("not_paused"));
        return out;
    }
    grant = (long long)hrs_num(paused, "grant", 0.0);
    if ((double)grant > max_continues) {
        json_set(out, "outcome", json_string("exhausted"));
        return out;
    }
    for (i = 0; i < json_len(event_seqs); i++) {
        const json_t *s = json_get(event_seqs, i);
        const json_t *v = hrs_is_obj(s) ? json_obj_get(s, "seq") : s;
        long long seq = (v && v->type == JSON_NUMBER) ? (long long)v->num_val : 0;
        if (seq > latest) latest = seq;
    }
    snprintf(event_id, sizeof event_id, "dcontinue:%s:%lld", discussion_event_id ? discussion_event_id : "", grant);
    event = json_object();
    json_set(event, "room_id", json_string(room_id ? room_id : ""));
    json_set(event, "event_id", json_string(event_id));
    json_set(event, "kind", json_string("room.continued"));
    actor = json_object();
    json_set(actor, "kind", json_string("gateway"));
    json_set(actor, "id", json_string(gateway_id ? gateway_id : ""));
    json_set(event, "actor", actor);
    payload = json_object();
    json_set(payload, "discussion_event_id", json_string(discussion_event_id ? discussion_event_id : ""));
    json_set(payload, "thread_id", json_string(hrs_s(paused, "thread_id") ? hrs_s(paused, "thread_id") : ""));
    json_set(payload, "grant", json_number((double)grant));
    json_set(event, "payload", payload);
    json_set(event, "authority_gateway_id", json_string(gateway_id ? gateway_id : ""));
    json_set(event, "authority_epoch", json_number(epoch));
    json_set(event, "expected_latest_seq", json_number((double)latest));
    json_set(out, "outcome", json_string("append"));
    json_set(out, "event", event);
    return out;
}

/* _stop_room(...): which cancel each attempt gets, and how many settle */
json_t *hrs_stop_room_tally(const json_t *fixture) {
    const json_t *tasks = json_obj_get(fixture, "tasks");
    const char *cancel_id = hrs_s(fixture, "cancel_id");
    double fence = hrs_num(fixture, "fence_seq", 0.0);
    int require_ack = hrs_truthy(json_obj_get(fixture, "require_acknowledged"));
    size_t n = json_len(tasks), i, selected = 0;
    json_t *cancels = json_array();
    json_t *out = json_object();
    int pending = 0, unconfirmed = 0;

    cancel_id = cancel_id ? cancel_id : "";
    for (i = 0; i < n; i++) {
        const json_t *task = json_get(tasks, i);
        const json_t *identity, *own;
        const char *status, *task_id, *result;
        double source_seq;
        char *own_s;
        json_t *row;

        if (!hrs_is_obj(task)) continue;
        status = hrs_s(task, "status");
        source_seq = hrs_num(task, "source_event_seq", 0.0);
        if (!(require_ack || source_seq < fence)) continue;
        selected++;
        identity = json_obj_get(task, "identity");
        task_id = hrs_s(identity, "task_id");
        own = json_obj_get(task, "cancel_id");
        own_s = (status && !strcmp(status, "stopping")) && hrs_truthy(own) ? hrs_pystr(own) : hrs_dup("");
        row = json_object();
        json_set(row, "task_id", json_string(task_id ? task_id : ""));
        json_set(row, "cancel_id", json_string(own_s && own_s[0] ? own_s : cancel_id));
        json_append(cancels, row);
        result = hrs_s(task, "cancel_result");
        if (result && !strcmp(result, "stopping")) pending++;
        else if (result && !strcmp(result, "deferred")) unconfirmed++;
        free(own_s);
    }
    json_set(out, "cancels", cancels);
    if (require_ack && (pending || unconfirmed)) {
        json_set(out, "outcome", json_string("refused"));
        json_set(out, "count", json_null());
    } else {
        json_set(out, "outcome", json_string("ok"));
        json_set(out, "count", json_number((double)selected - (double)unconfirmed));
    }
    return out;
}

/* retry_room_task(...): which attempt the explicit Retry lands on */
json_t *hrs_retry_room_task(const json_t *fixture) {
    const char *task_id = hrs_s(fixture, "task_id");
    const json_t *tasks = json_obj_get(fixture, "tasks");
    const json_t *member = json_obj_get(fixture, "member_id");
    const json_t *generation = json_obj_get(fixture, "execution_generation");
    const char *room_id = hrs_s(fixture, "room_id");
    json_t *out = json_object();
    size_t i;

    task_id = task_id ? task_id : "";
    room_id = room_id ? room_id : "";
    if (!strncmp(task_id, "prepare:", 8)) {
        char wanted[512];
        int binding = hrs_truthy(json_obj_get(fixture, "binding_present"));
        int stalled = hrs_truthy(json_obj_get(fixture, "stalled"));
        snprintf(wanted, sizeof wanted, "prepare:%s", room_id);
        if (!binding || strcmp(task_id, wanted) || !stalled) {
            json_set(out, "outcome", json_string("invalid"));
            return out;
        }
        if (!hrs_truthy(json_obj_get(fixture, "prepare_ok"))) {
            const char *name = hrs_s(fixture, "prepare_error");
            json_set(out, "outcome", json_string("error"));
            json_set(out, "error", json_string(name ? name : "RuntimeError"));
            return out;
        }
        json_set(out, "outcome", json_string("prepared"));
        json_set(out, "task_id", json_string(task_id));
        json_set(out, "execution_generation", json_number(1));
        return out;
    }
    for (i = 0; i < json_len(tasks); i++) {
        const json_t *task = json_get(tasks, i);
        const json_t *identity;
        const char *cand_id;
        json_t *cand_member;
        int member_ok, gen_ok;
        if (!hrs_is_obj(task) || !hrs_status_in(task, HRS_RETRYABLE)) continue;
        identity = json_obj_get(task, "identity");
        cand_id = hrs_s(identity, "task_id");
        if (!cand_id || strcmp(cand_id, task_id)) continue;
        cand_member = hrs_member_id_of(task);
        member_ok = (!member || member->type == JSON_NULL)
                    || (cand_member && cand_member->type == JSON_STRING && member->type == JSON_STRING
                        && !strcmp(cand_member->str_val, member->str_val));
        gen_ok = (!generation || generation->type == JSON_NULL)
                 || (generation->type == JSON_NUMBER
                     && generation->num_val == hrs_num(task, "execution_generation", 0.0));
        json_free(cand_member);
        if (!member_ok || !gen_ok) continue;
        json_set(out, "outcome", json_string("retry"));
        json_set(out, "task_id", json_string(cand_id));
        json_set(out, "execution_generation", json_number(hrs_num(task, "execution_generation", 0.0)));
        return out;
    }
    json_set(out, "outcome", json_string("invalid"));
    return out;
}

/* retry_owed_revokes(): one due revoke per cycle, backoff from the attempt end */
json_t *hrs_owed_revokes(const json_t *fixture) {
    json_t *out = json_object();
    json_t *settles = json_array();
    json_t *errors = json_array();
    const json_t *owed = json_obj_get(fixture, "owed");
    double clock = hrs_num(fixture, "clock", 0.0);
    size_t i;
    int settled = 0;

    if (!hrs_truthy(json_obj_get(fixture, "may_owe"))) {
        json_set(out, "settled", json_number(0));
        json_set(out, "settles", settles);
        json_set(out, "errors", errors);
        json_set(out, "may_owe_after", json_bool(hrs_truthy(json_obj_get(fixture, "owes_after"))));
        return out;
    }
    for (i = 0; i < json_len(owed); i++) {
        const json_t *o = json_get(owed, i);
        const char *result, *room_id, *member_id, *digest;
        json_t *row;
        if (!hrs_is_obj(o)) continue;
        result = hrs_s(o, "result");
        result = result ? result : "ok";
        if (!strcmp(result, "fault")) {
            double delay = 5.0 * pow(2.0, (double)((hrs_ll(o, "attempts", 0) > 20) ? 20 : hrs_ll(o, "attempts", 0)));
            if (delay > 3600.0) delay = 3600.0;
            row = json_object();
            json_set(row, "room_id", json_string(hrs_s(o, "room_id") ? hrs_s(o, "room_id") : ""));
            json_set(row, "member_id", json_string(hrs_s(o, "member_id") ? hrs_s(o, "member_id") : ""));
            json_set(row, "grant_digest", json_string(hrs_s(o, "grant_digest") ? hrs_s(o, "grant_digest") : ""));
            json_set(row, "retry_at", json_number(clock + delay));
            json_append(settles, row);
            {
                char msg[512];
                const json_t *text = json_obj_get(o, "fault_text");
                char *text_s = hrs_truthy(text) ? hrs_pystr(text) : hrs_dup("boom");
                snprintf(msg, sizeof msg, "revoke owed for member %s of room %s is pending: %s",
                         hrs_s(o, "member_id") ? hrs_s(o, "member_id") : "", hrs_s(o, "room_id") ? hrs_s(o, "room_id") : "",
                         text_s ? text_s : "");
                free(text_s);
                json_append(errors, json_string(msg));
            }
            continue;
        }
        room_id = hrs_s(o, "room_id");
        member_id = hrs_s(o, "member_id");
        digest = hrs_s(o, "grant_digest");
        row = json_object();
        json_set(row, "room_id", json_string(room_id ? room_id : ""));
        json_set(row, "member_id", json_string(member_id ? member_id : ""));
        json_set(row, "grant_digest", json_string(digest ? digest : ""));
        json_set(row, "retry_at", json_null());
        json_append(settles, row);
        settled++;
    }
    json_set(out, "settled", json_number((double)settled));
    json_set(out, "settles", settles);
    json_set(out, "errors", errors);
    json_set(out, "may_owe_after", json_bool(hrs_truthy(json_obj_get(fixture, "owes_after"))));
    return out;
}
