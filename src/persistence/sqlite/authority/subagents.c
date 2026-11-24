/*
 * subagents.c - C11 port of upstream persistence/sqlite/authority/subagents.py
 * (gates w32_subagents_a171ba_p1 and w32_subagents_a171ba_p2).
 *
 * PoP: w32_subagents_a171ba_p1_record_shape @ persistence/sqlite/authority/subagents.py:_record
 * PoP: w32_subagents_a171ba_p1_route_shape @ persistence/sqlite/authority/subagents.py:SubagentLedgerVerbs.get_delegation_route
 * PoP: w32_subagents_a171ba_p1_upsert @ persistence/sqlite/authority/subagents.py:_upsert
 * PoP: w32_subagents_a171ba_p1_apply_limits @ persistence/sqlite/authority/subagents.py:_apply_limits
 * PoP: w32_subagents_a171ba_p1_insert_completion @ persistence/sqlite/authority/subagents.py:_insert_completion
 * PoP: w32_subagents_a171ba_p1_recover_guard @ persistence/sqlite/authority/subagents.py:recover_subagents
 * PoP: w32_subagents_a171ba_p1_constants @ persistence/sqlite/authority/subagents.py:<_COLUMNS,_RUN_ONLY,retention>
 * PoP: w32_subagents_a171ba_p2_note @ persistence/sqlite/authority/subagents.py:_note
 * PoP: w32_subagents_a171ba_p2_cancel_expired @ persistence/sqlite/authority/subagents.py:_cancel_expired
 * PoP: w32_subagents_a171ba_p2_cancel_manual_claim @ persistence/sqlite/authority/subagents.py:_cancel_manual_claim
 * PoP: w32_subagents_a171ba_p2_parent_note @ persistence/authority/subagents.py:parent_superseded_note
 * PoP: w32_subagents_a171ba_p2_predicates @ persistence/authority/subagents.py:SubagentRecord.owed
 *                                       + subagents.py:_unstarted_auto_claim, _attempts
 * PoP: w32_subagents_a171ba_p2_hand_back @ persistence/sqlite/authority/subagents.py:SubagentLedgerVerbs.hand_back_subagent
 * PoP: w32_subagents_a171ba_p2_refusal @ persistence/sqlite/authority/subagents.py:SubagentLedgerVerbs.settle_auto_refusal
 * PoP: w32_subagents_a171ba_p2_constants @ persistence/sqlite/authority/subagents.py:<OWED_SQL,CANCEL_DEBT,...>
 *
 * Seams left in the caller: the ledger writer/reader connections, the owner
 * scan's cursor, the compression-chain walk (only its tip note is ported), and
 * the clock (``_at`` -- ``now`` is passed in).  What is ported is the
 * value->value core: the shaping of a typed ``resumable_subagents`` row and of
 * a delegation row, the exact statement and parameter tuple of each write
 * verb, the TTL / retention / pin-cap arithmetic, and the predicates the
 * automatic-resume decisions are made from.
 *
 * Every SQL literal below is byte-for-byte the upstream string (the triple
 * quoted ones keep their newlines and indentation).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w32_subagents_a171ba_p1.h"
#include "noc_w32_subagents_a171ba_p2.h"

/* ── module literals ────────────────────────────────────────────────────── */

/* _COLUMNS */
static const char *const COLUMNS_SQL =
    "subagent_id, state, pinned, stop_cause, owner_pid, owner_started_at, record_json, created_at, "
    "updated_at, expires_at";

/* _RUN_ONLY */
static const char *const RUN_ONLY_KEYS[] = {"state", "stop_cause", "auto_claim", "auto_cancel"};
#define RUN_ONLY_N 4

/* _CANCEL_STATES */
static const char *const CANCEL_STATES[] = {"superseded", "stopped"};
#define CANCEL_STATES_N 2

/* AUTOMATIC_END_REASONS (persistence/conversation/sessions.py) */
static const char *const AUTOMATIC_END_REASONS[] = {
    "agent_close", "idle_timeout", "lru_evict", "startup_orphan_reap",
    "superseded_by_resume", "tui_shutdown", "ws_disconnect", "ws_orphan_reap",
};
#define AUTOMATIC_END_N 8

/* OWED_SQL */
#define OWED_SQL \
    "json_extract(record_json, '$.auto_promised') AND json_extract(record_json, '$.auto_gave_up') IS NULL"

/* CANCEL_DEBT */
#define CANCEL_DEBT "'$.auto_promised', json('false')"

/* KEEP_WAITING_UNITS_SQL (built from OWED_SQL upstream; same bytes) */
#define KEEP_WAITING_UNITS_SQL                                                      \
    " AND delegation_id NOT IN (SELECT json_extract(record_json, '$.unit_id') FROM " \
    "resumable_subagents WHERE state IN ('resumable','expired') AND "                \
    "json_extract(record_json, '$.auto_promised') AND json_extract(record_json, "    \
    "'$.auto_gave_up') IS NULL AND json_extract(record_json, '$.unit_id') IS NOT NULL)"

/* SUBAGENT_EXPIRED_RETENTION_SECONDS = 7 * 24 * 3600.0 */
#define EXPIRED_RETENTION_SECONDS 604800.0

/* _upsert */
static const char *const SQL_UPSERT =
    "INSERT INTO resumable_subagents (subagent_id, state, pinned, stop_cause, owner_pid, owner_started_at,\n"
    "               record_json, created_at, updated_at, expires_at) VALUES (?,?,?,?,?,?,?,?,?,?)\n"
    "           ON CONFLICT(subagent_id) DO UPDATE SET state=excluded.state, pinned=excluded.pinned,\n"
    "               stop_cause=excluded.stop_cause, owner_pid=excluded.owner_pid,\n"
    "               owner_started_at=excluded.owner_started_at, record_json=excluded.record_json,\n"
    "               updated_at=excluded.updated_at, expires_at=excluded.expires_at";

/* _apply_limits */
static const char *const SQL_LIMIT_EXPIRE =
    "UPDATE resumable_subagents SET pinned=0, state='expired' WHERE expires_at IS NOT NULL "
    "AND expires_at <= ? AND state IN ('resumable','completed','stopped','superseded')";

static const char *const SQL_LIMIT_DELETE_HEAD =
    "DELETE FROM resumable_subagents WHERE state='expired' AND updated_at < ? AND NOT COALESCE((";
static const char *const SQL_LIMIT_DELETE_TAIL = "), 0)";

static const char *const SQL_LIMIT_PIN =
    "UPDATE resumable_subagents SET pinned=0 WHERE pinned=1 AND subagent_id NOT IN (\n"
    "                        SELECT subagent_id FROM resumable_subagents WHERE pinned=1\n"
    "                        ORDER BY updated_at DESC LIMIT ?)";

/* _insert_completion */
static const char *const SQL_INSERT_COMPLETION =
    "INSERT OR IGNORE INTO async_delegations (delegation_id, origin_session, origin_ui_session_id, "
    "parent_session_id, state, dispatched_at, completed_at, updated_at, event_json, result_json, "
    "delivery_state, delivery_attempts, owner_pid, owner_started_at, task_json, origin_session_id) "
    "VALUES (?, ?, ?, ?, 'failed', ?, ?, ?, ?, ?, 'pending', 0, ?, ?, ?, ?)";

/* the task columns a give-up completion's row carries */
static const char *const COMPLETION_TASK_KEYS[] = {
    "goal", "goals", "context", "toolsets", "role", "model", "is_batch",
};
#define COMPLETION_TASK_N 7

/* _cancel_expired */
static const char *const SQL_CANCEL_EXPIRED_HEAD =
    "UPDATE resumable_subagents SET record_json=json_set(record_json, '$.auto_note', ?, ";
static const char *const SQL_CANCEL_EXPIRED_MID =
    "), updated_at=? WHERE subagent_id=? AND state='expired' AND ";

/* _cancel_manual_claim */
static const char *const SQL_CANCEL_MANUAL =
    "UPDATE resumable_subagents SET record_json=json_set(record_json, '$.auto_cancel', "
    "json_object('state', ?, 'note', ?)), updated_at=? WHERE subagent_id=? AND state='claimed' "
    "AND json_extract(record_json, '$.auto_claim') IS NULL";

/* ── value helpers (Python semantics) ───────────────────────────────────── */

static int sa_truthy(const json_t *v) {
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

/* Python int(): bools/numbers truncate toward zero, integer strings parse. */
static long sa_int_of(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (long)v->num_val;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        long got = strtol(s, &end, 10);
        return end == s ? 0 : got;
    }
    return 0;
}

/* int(x or 0) */
static long sa_int_or_zero(const json_t *v) { return sa_truthy(v) ? sa_int_of(v) : 0; }

static double sa_num_or(const json_t *v, double fallback) {
    if (!v) return fallback;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return fallback;
}

static int sa_str_in(const char *s, const char *const *set, size_t n) {
    size_t i;
    if (!s) return 0;
    for (i = 0; i < n; i++)
        if (!strcmp(s, set[i])) return 1;
    return 0;
}

static int sa_str_eq(const json_t *v, const char *s) {
    return v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s);
}

/* Python '==' over the scalars a tuple of columns holds (None == None). */
static int sa_same(const json_t *a, const json_t *b) {
    double x, y;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
    return 0;
}

/* obj[key] = copy(v), or null when the key is absent (X.get(k) is None) */
static void sa_set_copy(json_t *obj, const char *key, const json_t *v) {
    json_set(obj, key, v ? json_copy(v) : json_null());
}

/* arr.append(copy(v) or null) */
static void sa_push_copy(json_t *arr, const json_t *v) {
    json_append(arr, v ? json_copy(v) : json_null());
}

static char *sa_dup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── bounded string builder for the composed statements ─────────────────── */

typedef struct {
    char *p;
    size_t n;
    size_t cap;
} sa_sb;

static void sb_init(sa_sb *b) {
    b->cap = 512;
    b->n = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void sb_add(sa_sb *b, const char *s) {
    size_t len;
    if (!b->p || !s) return;
    len = strlen(s);
    if (b->n + len + 1 > b->cap) {
        size_t want = b->cap;
        char *grown;
        while (want < b->n + len + 1) want *= 2;
        grown = (char *)realloc(b->p, want);
        if (!grown) return;
        b->p = grown;
        b->cap = want;
    }
    memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
}

static char *sb_take(sa_sb *b) {
    char *out = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return out;
}

/* {sql, params} */
static json_t *sa_stmt(const char *sql, json_t *params) {
    json_t *o = json_object();
    json_set(o, "sql", json_string(sql ? sql : ""));
    json_set(o, "params", params ? params : json_array());
    return o;
}

static long sa_req_rowcount(const json_t *req) {
    const json_t *rc = json_obj_get(req, "rowcount");
    return rc ? sa_int_of(rc) : 1;
}

static int sa_req_flag(const json_t *req, const char *key) {
    return sa_truthy(json_obj_get(req, key));
}

static double sa_req_now(const json_t *req) { return sa_num_or(json_obj_get(req, "now"), 0.0); }

/* ══════════════════════════════════════════════════════════════════════════
 * Gate w32_subagents_a171ba_p1 -- row shaping + write statement builders
 * ══════════════════════════════════════════════════════════════════════════ */

/* _record(row): the ten typed columns plus the decoded run record. */
json_t *w32_subagents_a171ba_p1_record_shape(const json_t *row) {
    json_t *o, *doc = NULL;
    const json_t *raw;
    if (!row || row->type != JSON_ARRAY || json_len(row) < 10) return json_null();

    raw = json_get(row, 6); /* record_json */
    if (raw && raw->type == JSON_STRING && raw->str_val && raw->str_val[0] != '\0')
        doc = json_parse(raw->str_val, NULL);

    o = json_object();
    sa_set_copy(o, "subagent_id", json_get(row, 0));
    sa_set_copy(o, "state", json_get(row, 1));
    json_set(o, "pinned", json_bool(sa_truthy(json_get(row, 2))));
    sa_set_copy(o, "stop_cause", json_get(row, 3));
    sa_set_copy(o, "owner_pid", json_get(row, 4));
    sa_set_copy(o, "owner_started_at", json_get(row, 5));
    json_set(o, "document", doc ? doc : json_object()); /* json.loads(raw or "{}") */
    sa_set_copy(o, "created_at", json_get(row, 7));
    sa_set_copy(o, "updated_at", json_get(row, 8));
    sa_set_copy(o, "expires_at", json_get(row, 9));
    return o;
}

/* get_delegation_route's shaping: route columns, decoded task/result, times. */
json_t *w32_subagents_a171ba_p1_route_shape(const json_t *req) {
    const json_t *row = json_obj_get(req, "row");
    const json_t *v;
    json_t *o, *doc;
    if (!req || req->type != JSON_OBJECT) return json_null();
    if (!row || row->type != JSON_ARRAY || json_len(row) < 8) return json_null();

    o = json_object();
    sa_set_copy(o, "delegation_id", json_obj_get(req, "delegation_id"));
    sa_set_copy(o, "origin_session", json_get(row, 0));
    sa_set_copy(o, "origin_ui_session_id", json_get(row, 1));
    sa_set_copy(o, "parent_session_id", json_get(row, 2));

    v = json_get(row, 3); /* origin_session_id=origin_sid or "" */
    if (sa_truthy(v)) sa_set_copy(o, "origin_session_id", v);
    else json_set(o, "origin_session_id", json_string(""));

    v = json_get(row, 4); /* task=json.loads(task_json) if task_json else None */
    doc = (sa_truthy(v) && v->type == JSON_STRING) ? json_parse(v->str_val, NULL) : NULL;
    json_set(o, "task", doc ? doc : json_null());

    v = json_get(row, 5);
    doc = (sa_truthy(v) && v->type == JSON_STRING) ? json_parse(v->str_val, NULL) : NULL;
    json_set(o, "result", doc ? doc : json_null());

    sa_set_copy(o, "dispatched_at", json_get(row, 6));
    sa_set_copy(o, "updated_at", json_get(row, 7));
    return o;
}

/* _upsert: the document without _RUN_ONLY, and the row's ten parameters.
 * req = {subagent_id, write:{state,pinned,stop_cause,owner_pid,owner_started_at,
 *                            document,expires_at}, now} -> {sql, params} */
json_t *w32_subagents_a171ba_p1_upsert(const json_t *req) {
    const json_t *write = json_obj_get(req, "write");
    const json_t *doc = write ? json_obj_get(write, "document") : NULL;
    json_t *params, *filtered;
    double now = sa_req_now(req);
    size_t i;

    filtered = (doc && doc->type == JSON_OBJECT) ? json_copy(doc) : json_object();
    for (i = 0; i < RUN_ONLY_N; i++) (void)json_obj_del(filtered, RUN_ONLY_KEYS[i]);

    params = json_array();
    sa_push_copy(params, json_obj_get(req, "subagent_id"));
    sa_push_copy(params, write ? json_obj_get(write, "state") : NULL);
    json_append(params, json_number(sa_truthy(write ? json_obj_get(write, "pinned") : NULL) ? 1.0 : 0.0));
    sa_push_copy(params, write ? json_obj_get(write, "stop_cause") : NULL);
    sa_push_copy(params, write ? json_obj_get(write, "owner_pid") : NULL);
    sa_push_copy(params, write ? json_obj_get(write, "owner_started_at") : NULL);
    json_append(params, filtered); /* json.dumps(record, ensure_ascii=False, default=str) */
    json_append(params, json_number(now)); /* created_at (insert) */
    json_append(params, json_number(now)); /* updated_at */
    sa_push_copy(params, write ? json_obj_get(write, "expires_at") : NULL);
    return sa_stmt(SQL_UPSERT, params);
}

/* _apply_limits: TTL retire, retention delete, pin cap.
 * req = {limits:{ttl_hours,max_resumable}, now} -> {stmts:[{sql,params}, ...]} */
json_t *w32_subagents_a171ba_p1_apply_limits(const json_t *req) {
    const json_t *limits = json_obj_get(req, "limits");
    double now = sa_req_now(req);
    double ttl = sa_num_or(limits ? json_obj_get(limits, "ttl_hours") : NULL, 0.0);
    long cap = sa_int_of(limits ? json_obj_get(limits, "max_resumable") : NULL);
    json_t *out = json_object(), *stmts = json_array(), *params;
    sa_sb b;
    char *sql;

    /* 1. a passed TTL unpins and retires the record */
    params = json_array();
    json_append(params, json_number(now));
    json_append(stmts, sa_stmt(SQL_LIMIT_EXPIRE, params));

    /* 2. now - SUBAGENT_EXPIRED_RETENTION_SECONDS - ttl_hours * 3600 */
    params = json_array();
    json_append(params, json_number(now - EXPIRED_RETENTION_SECONDS - ttl * 3600.0));
    sb_init(&b);
    sb_add(&b, SQL_LIMIT_DELETE_HEAD);
    sb_add(&b, OWED_SQL);
    sb_add(&b, SQL_LIMIT_DELETE_TAIL);
    sql = sb_take(&b);
    json_append(stmts, sa_stmt(sql, params));
    free(sql);

    /* 3. beyond the cap the oldest pins drop: max(limits.max_resumable, 0) */
    params = json_array();
    json_append(params, json_number((double)(cap > 0 ? cap : 0)));
    json_append(stmts, sa_stmt(SQL_LIMIT_PIN, params));

    json_set(out, "stmts", stmts);
    return out;
}

/* _insert_completion: the give-up's one failed completion, pending, never
 * replacing a row.  req = {completion:{unit_id,unit_updated_at,event,result,
 * owner_pid,owner_started_at}, now} -> {sql, params} */
json_t *w32_subagents_a171ba_p1_insert_completion(const json_t *req) {
    const json_t *comp = json_obj_get(req, "completion");
    const json_t *evt = comp ? json_obj_get(comp, "event") : NULL;
    const json_t *res = comp ? json_obj_get(comp, "result") : NULL;
    double now = sa_req_now(req);
    json_t *params, *task;
    const json_t *v;
    size_t i;

    params = json_array();
    sa_push_copy(params, evt ? json_obj_get(evt, "delegation_id") : NULL);

    v = evt ? json_obj_get(evt, "session_key") : NULL; /* evt.get(k) or "" */
    if (sa_truthy(v)) sa_push_copy(params, v); else json_append(params, json_string(""));
    v = evt ? json_obj_get(evt, "origin_ui_session_id") : NULL;
    if (sa_truthy(v)) sa_push_copy(params, v); else json_append(params, json_string(""));

    sa_push_copy(params, evt ? json_obj_get(evt, "parent_session_id") : NULL);
    v = evt ? json_obj_get(evt, "dispatched_at") : NULL; /* evt.get(k) or now */
    json_append(params, json_number(sa_truthy(v) ? sa_num_or(v, now) : now));
    v = evt ? json_obj_get(evt, "completed_at") : NULL;
    json_append(params, json_number(sa_truthy(v) ? sa_num_or(v, now) : now));
    json_append(params, json_number(now));

    json_append(params, evt ? json_copy(evt) : json_object()); /* json.dumps(evt) */
    json_append(params, res ? json_copy(res) : json_null());   /* json.dumps(result) */
    sa_push_copy(params, comp ? json_obj_get(comp, "owner_pid") : NULL);
    sa_push_copy(params, comp ? json_obj_get(comp, "owner_started_at") : NULL);

    /* task = {k: evt.get(k) for k in (...)}: every key present, missing -> null */
    task = json_object();
    for (i = 0; i < COMPLETION_TASK_N; i++)
        sa_set_copy(task, COMPLETION_TASK_KEYS[i],
                    evt ? json_obj_get(evt, COMPLETION_TASK_KEYS[i]) : NULL);
    json_append(params, task);

    v = evt ? json_obj_get(evt, "origin_session_id") : NULL;
    if (sa_truthy(v)) sa_push_copy(params, v); else json_append(params, json_string(""));

    return sa_stmt(SQL_INSERT_COMPLETION, params);
}

/* recover_subagents' gate: a record is written only while the row is still
 * held by the same dead owner (and still running/claimed).
 * req = {row:[state,owner_pid,owner_started_at]|null,
 *        recovery:{owner_pid,owner_started_at}} -> bool */
json_t *w32_subagents_a171ba_p1_recover_guard(const json_t *req) {
    const json_t *row = json_obj_get(req, "row");
    const json_t *rec = json_obj_get(req, "recovery");
    const json_t *state;
    if (!row || row->type != JSON_ARRAY || json_len(row) < 3 || !rec) return json_bool(false);
    state = json_get(row, 0);
    if (!(sa_str_eq(state, "running") || sa_str_eq(state, "claimed"))) return json_bool(false);
    /* (got[1], got[2]) != (recovery.owner_pid, recovery.owner_started_at) */
    return json_bool(sa_same(json_get(row, 1), json_obj_get(rec, "owner_pid")) &&
                     sa_same(json_get(row, 2), json_obj_get(rec, "owner_started_at")));
}

json_t *w32_subagents_a171ba_p1_constants(void) {
    json_t *o = json_object(), *run_only = json_array();
    size_t i;
    for (i = 0; i < RUN_ONLY_N; i++) json_append(run_only, json_string(RUN_ONLY_KEYS[i]));
    json_set(o, "columns", json_string(COLUMNS_SQL));
    json_set(o, "run_only", run_only);
    json_set(o, "expired_retention_seconds", json_number(EXPIRED_RETENTION_SECONDS));
    return o;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Gate w32_subagents_a171ba_p2 -- decisions, notes and predicates
 * ══════════════════════════════════════════════════════════════════════════ */

/* _note: auto_note set (the debt cleared too when the row moves to a cancel
 * state), the claim dropped, the row moved/unpinned when asked, an unstarted
 * automatic claim included.
 * req = {subagent_id, note, now, state|null, from_states[] (default
 *        ["resumable"]), unpin, unstarted_claim, rowcount (default 1)} */
json_t *w32_subagents_a171ba_p2_note(const json_t *req) {
    const json_t *state_v = json_obj_get(req, "state");
    const json_t *from = json_obj_get(req, "from_states");
    const char *state = (state_v && state_v->type == JSON_STRING) ? state_v->str_val : NULL;
    int has_state = state && state[0] != '\0';
    int unpin = sa_req_flag(req, "unpin");
    int unstarted = sa_req_flag(req, "unstarted_claim");
    long rowcount = sa_req_rowcount(req);
    json_t *params = json_array(), *out;
    sa_sb b;
    char *sql;
    size_t i, n;

    sb_init(&b);
    sb_add(&b,
           "UPDATE resumable_subagents SET record_json=json_remove(json_set(record_json, '$.auto_note', ?");
    if (has_state && sa_str_in(state, CANCEL_STATES, CANCEL_STATES_N)) {
        sb_add(&b, ", ");
        sb_add(&b, CANCEL_DEBT);
    }
    sb_add(&b, "), '$.auto_claim'), updated_at=?");
    if (has_state) {
        sb_add(&b, ", state=?");
        if (unpin) sb_add(&b, ", pinned=0");
    }
    sb_add(&b, " WHERE subagent_id=? AND (state IN (");
    /* from_states defaults to (RESUMABLE,); an explicit [] keeps state IN () */
    n = (from && from->type == JSON_ARRAY) ? json_len(from) : 1;
    for (i = 0; i < n; i++) {
        if (i) sb_add(&b, ",");
        sb_add(&b, "?");
    }
    sb_add(&b, ")");
    if (unstarted)
        sb_add(&b, " OR (state='claimed' AND json_extract(record_json, '$.auto_claim') IS NOT NULL)");
    sb_add(&b, ")");
    sql = sb_take(&b);

    sa_push_copy(params, json_obj_get(req, "note"));
    json_append(params, json_number(sa_req_now(req)));
    if (has_state) json_append(params, json_string(state));
    sa_push_copy(params, json_obj_get(req, "subagent_id"));
    for (i = 0; i < n; i++) {
        if (from && from->type == JSON_ARRAY) sa_push_copy(params, json_get(from, i));
        else json_append(params, json_string("resumable"));
    }

    out = sa_stmt(sql, params);
    free(sql);
    json_set(out, "ok", json_bool(rowcount == 1));
    return out;
}

/* _cancel_expired: a cancel that lands after the TTL retired an owed row but
 * before a tick reported it.  req = {subagent_id, note, now, rowcount} */
json_t *w32_subagents_a171ba_p2_cancel_expired(const json_t *req) {
    json_t *params, *out, *tmp;
    sa_sb b;
    char *sql;

    sb_init(&b);
    sb_add(&b, SQL_CANCEL_EXPIRED_HEAD);
    sb_add(&b, CANCEL_DEBT);
    sb_add(&b, SQL_CANCEL_EXPIRED_MID);
    sb_add(&b, OWED_SQL);
    sql = sb_take(&b);

    params = json_array();
    sa_push_copy(params, json_obj_get(req, "note"));
    json_append(params, json_number(sa_req_now(req)));
    sa_push_copy(params, json_obj_get(req, "subagent_id"));

    tmp = sa_stmt(sql, params);
    free(sql);
    out = tmp;
    json_set(out, "ok", json_bool(sa_req_rowcount(req) == 1));
    return out;
}

/* _cancel_manual_claim: the row the parent's own continue holds records the
 * cancel as auto_cancel {state, note} for its hand-back.
 * req = {subagent_id, state, note, now, rowcount} */
json_t *w32_subagents_a171ba_p2_cancel_manual_claim(const json_t *req) {
    json_t *params = json_array(), *out;

    sa_push_copy(params, json_obj_get(req, "state"));
    sa_push_copy(params, json_obj_get(req, "note"));
    json_append(params, json_number(sa_req_now(req)));
    sa_push_copy(params, json_obj_get(req, "subagent_id"));

    out = sa_stmt(SQL_CANCEL_MANUAL, params);
    json_set(out, "ok", json_bool(sa_req_rowcount(req) == 1));
    return out;
}

/* parent_superseded_note: gone, or ended for a deliberate (not automatic)
 * reason; None while the parent still wants the child.
 * req = {tip_end_reason: str|null, tip_exists: bool} -> str|null
 * (the call site passes the sessions row's end reason: a str or None.) */
json_t *w32_subagents_a171ba_p2_parent_note(const json_t *req) {
    const json_t *reason = json_obj_get(req, "tip_end_reason");
    sa_sb b;
    char *buf;
    json_t *out;

    if (!sa_truthy(json_obj_get(req, "tip_exists")))
        return json_string("its parent session no longer exists");
    if (!sa_truthy(reason)) return json_null(); /* not ended: still wanted */
    if (reason->type != JSON_STRING) return json_null();
    if (sa_str_in(reason->str_val ? reason->str_val : "", AUTOMATIC_END_REASONS, AUTOMATIC_END_N))
        return json_null();

    sb_init(&b);
    sb_add(&b, "its parent session ended (");
    sb_add(&b, reason->str_val ? reason->str_val : "");
    sb_add(&b, ")");
    buf = sb_take(&b);
    out = json_string(buf ? buf : "");
    free(buf);
    return out;
}

/* SubagentRecord.owed + _unstarted_auto_claim + _attempts over one record.
 * req = {record:{state,document,...}} -> {owed, unstarted_claim, attempts} */
json_t *w32_subagents_a171ba_p2_predicates(const json_t *req) {
    const json_t *rec = json_obj_get(req, "record");
    const json_t *doc = rec ? json_obj_get(rec, "document") : NULL;
    const json_t *state = rec ? json_obj_get(rec, "state") : NULL;
    const json_t *gave_up = doc ? json_obj_get(doc, "auto_gave_up") : NULL;
    json_t *o = json_object();

    /* promised and no give-up delivered (a missing key and a null one both count) */
    json_set(o, "owed",
             json_bool(sa_truthy(doc ? json_obj_get(doc, "auto_promised") : NULL) &&
                       (!gave_up || gave_up->type == JSON_NULL)));
    json_set(o, "unstarted_claim",
             json_bool(sa_str_eq(state, "claimed") &&
                       sa_truthy(doc ? json_obj_get(doc, "auto_claim") : NULL)));
    json_set(o, "attempts",
             json_number((double)sa_int_or_zero(doc ? json_obj_get(doc, "auto_attempts") : NULL)));
    return o;
}

/* hand_back_subagent's statement: a stop that landed during the claim
 * (auto_cancel) wins over handing a promise back.
 * req = {subagent_id, state, promised|null, owner_pid|null,
 *        found:[auto_cancel.state, auto_cancel.note]|null}
 * -> null when no claimed row of this owner answered, else {sql, params} */
json_t *w32_subagents_a171ba_p2_hand_back(const json_t *req) {
    const json_t *found = json_obj_get(req, "found");
    const json_t *promised = json_obj_get(req, "promised");
    const json_t *owner = json_obj_get(req, "owner_pid");
    const json_t *cancel_state_v, *cancel_note_v, *state_param = NULL;
    const char *target, *cancel_state;
    int has_owner, write_promised, took_cancel_state = 0;
    const char *mark = "false";
    json_t *params = json_array(), *out;
    sa_sb b;
    char *sql, *rexpr = NULL;

    if (!found || found->type != JSON_ARRAY || json_len(found) < 2) return json_null();
    cancel_state_v = json_get(found, 0);
    cancel_note_v = json_get(found, 1);
    has_owner = owner && owner->type != JSON_NULL;
    write_promised = promised && promised->type != JSON_NULL;
    if (write_promised) mark = sa_truthy(promised) ? "true" : "false";
    target = json_get_str(req, "state", "");
    cancel_state =
        (cancel_state_v && cancel_state_v->type == JSON_STRING) ? cancel_state_v->str_val : NULL;

    if (sa_truthy(cancel_state_v)) { /* the parent cancelled it during the claim */
        write_promised = 1;          /* hand_promised = False: still written, no report */
        mark = "false";
        if (!strcmp(target, "resumable")) {
            target = cancel_state ? cancel_state : "";
            state_param = cancel_state_v;
            took_cancel_state = 1;
            rexpr = sa_dup("json_set(record_json, '$.auto_note', ?)");
            sa_push_copy(params, cancel_note_v);
        }
    }
    if (write_promised) {
        sa_sb wrapped;
        sb_init(&wrapped);
        sb_add(&wrapped, "json_set(");
        sb_add(&wrapped, rexpr ? rexpr : "record_json");
        sb_add(&wrapped, ", '$.auto_promised', json(?))");
        free(rexpr);
        rexpr = sb_take(&wrapped);
        json_append(params, json_string(mark));
    }

    sb_init(&b);
    sb_add(&b, "UPDATE resumable_subagents SET record_json=json_remove(");
    sb_add(&b, rexpr ? rexpr : "record_json");
    sb_add(&b, ", '$.auto_claim', '$.auto_cancel'), state=?");
    if (took_cancel_state && !strcmp(target, "stopped")) sb_add(&b, ", pinned=0");
    sb_add(&b, " WHERE subagent_id=? AND state='claimed'");
    if (has_owner) sb_add(&b, " AND owner_pid=?");
    sql = sb_take(&b);
    free(rexpr);

    if (state_param) json_append(params, json_copy(state_param));
    else json_append(params, json_string(target ? target : ""));
    sa_push_copy(params, json_obj_get(req, "subagent_id"));
    if (has_owner) sa_push_copy(params, owner);

    out = sa_stmt(sql, params);
    free(sql);
    return out;
}

/* settle_auto_refusal: which attempt/off fields the refusal writes.
 * req = {subagent_id, kind: cancelled|transient|permanent, attempts, note,
 *        owner_pid} -> {sql, params} */
json_t *w32_subagents_a171ba_p2_refusal(const json_t *req) {
    const char *kind = json_get_str(req, "kind", "");
    const char *note = json_get_str(req, "note", "");
    json_t *params = json_array(), *out;
    sa_sb inner, b;
    char *isql, *sql;

    sb_init(&inner);
    if (!strcmp(kind, "cancelled")) { /* the parent's decision already wrote the row */
        sb_add(&inner, "json_set(record_json, '$.auto_attempts', ?)");
        sa_push_copy(params, json_obj_get(req, "attempts"));
    } else if (!strcmp(kind, "transient")) {
        sb_add(&inner, "json_set(record_json, '$.auto_attempts', ?, '$.auto_note', ?)");
        sa_push_copy(params, json_obj_get(req, "attempts"));
        json_append(params, json_string(note));
    } else { /* permanent: automatic resumes of the record end */
        sb_add(&inner, "json_set(record_json, '$.auto_off', ?, '$.auto_note', ?)");
        json_append(params, json_string(note));
        json_append(params, json_string(note));
    }
    isql = sb_take(&inner);

    sb_init(&b);
    sb_add(&b, "UPDATE resumable_subagents SET record_json=json_remove(");
    sb_add(&b, isql ? isql : "record_json");
    sb_add(&b,
           ", '$.auto_claim'), state=CASE WHEN state='claimed' AND owner_pid=? "
           "THEN 'resumable' ELSE state END WHERE subagent_id=?");
    sql = sb_take(&b);
    free(isql);

    sa_push_copy(params, json_obj_get(req, "owner_pid"));
    sa_push_copy(params, json_obj_get(req, "subagent_id"));

    out = sa_stmt(sql, params);
    free(sql);
    return out;
}

json_t *w32_subagents_a171ba_p2_constants(void) {
    json_t *o = json_object(), *states = json_array();
    size_t i;
    for (i = 0; i < CANCEL_STATES_N; i++) json_append(states, json_string(CANCEL_STATES[i]));
    json_set(o, "owed_sql", json_string(OWED_SQL));
    json_set(o, "cancel_debt", json_string(CANCEL_DEBT));
    json_set(o, "keep_waiting_units_sql", json_string(KEEP_WAITING_UNITS_SQL));
    json_set(o, "cancel_states", states);
    json_set(o, "auto_resume_max_attempts", json_number(2.0));
    json_set(o, "gateway_shutdown", json_string("gateway_shutdown"));
    json_set(o, "auto_resume_cancelled",
             json_string("Not continued: its parent cancelled the automatic resume"));
    return o;
}
