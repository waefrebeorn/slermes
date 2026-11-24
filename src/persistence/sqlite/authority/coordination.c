/*
 * coordination.c - C11 port of upstream
 * persistence/sqlite/authority/coordination.py (gate w23_coordinati_5a8f30).
 *
 * PoP: w23_coordinati_5a8f30_claim_lease @ persistence/sqlite/authority/coordination.py:_claim_lease_row
 * PoP: w23_coordinati_5a8f30_replay_pending @ persistence/sqlite/authority/coordination.py:_replay_pending
 * PoP: w23_coordinati_5a8f30_past_replay_age @ persistence/sqlite/authority/coordination.py:_past_replay_age
 * PoP: w23_coordinati_5a8f30_json_object @ persistence/sqlite/authority/coordination.py:_json_object
 * PoP: w23_coordinati_5a8f30_admitted_completion @ persistence/sqlite/authority/coordination.py:get_admitted_completion
 * PoP: w23_coordinati_5a8f30_sweep @ persistence/sqlite/authority/coordination.py:sweep_orphaned_completions
 * PoP: w23_coordinati_5a8f30_finished @ persistence/sqlite/authority/coordination.py:list_finished_delegations
 *
 * The connection and the store holder are seams: the caller supplies the rows
 * the queries returned.  What lives here is the decision over those rows, the
 * statements it would issue, and the exact tokens of both -- the delivery
 * ledger's contract with the store.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_coordinati_5a8f30.h"

#define CO_DROP_SQL \
    "UPDATE async_delegations SET delivery_state='dropped', delivery_claim=NULL, " \
    "delivery_claimed_at=NULL, updated_at=? WHERE delegation_id=? AND " \
    "delivery_state='pending'"

#define CO_ADMITTED_SQL \
    "SELECT event_json, completed_at, dispatched_at, delivery_attempts FROM " \
    "async_delegations WHERE delegation_id=? AND delivery_state='delivered' AND " \
    "event_json IS NOT NULL"

#define CO_ORPHAN_SQL \
    "SELECT delegation_id, event_json, completed_at, dispatched_at, owner_pid, " \
    "owner_started_at, delivery_attempts FROM async_delegations WHERE " \
    "state!='running' AND delivery_state='pending' AND event_json IS NOT NULL AND " \
    "updated_at < ? AND (delivery_claim IS NULL OR delivery_claimed_at < ?) " \
    "ORDER BY completed_at, delegation_id"

#define CO_FINISHED_SQL \
    "SELECT delegation_id, state, dispatched_at, completed_at, task_json, " \
    "result_json FROM async_delegations WHERE (%s) AND state!='running' AND " \
    "completed_at >= ? ORDER BY completed_at DESC LIMIT ?"

/* -- small JSON helpers ------------------------------------------------- */

/* False for Python: None, False, 0, "", [], {}. */
static int co_falsy(const json_t *v)
{
    if (!v) return 1;
    switch (v->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return v->bool_val ? 0 : 1;
        case JSON_NUMBER: return v->num_val == 0.0;
        case JSON_STRING: return !v->str_val || v->str_val[0] == '\0';
        case JSON_ARRAY: return json_len(v) == 0;
        case JSON_OBJECT: return json_object_size(v) == 0;
        default: return 1;
    }
}

static double co_num(const json_t *obj, const char *key, double dflt)
{
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    return (v && v->type == JSON_NUMBER) ? v->num_val : dflt;
}

/* Python `x or y` over two optional numbers: the first truthy, else the second. */
static int co_age_basis(const json_t *completed, const json_t *dispatched, double *out)
{
    if (completed && completed->type == JSON_NUMBER && !co_falsy(completed)) {
        *out = completed->num_val;
        return 1;
    }
    if (dispatched && dispatched->type == JSON_NUMBER && !co_falsy(dispatched)) {
        *out = dispatched->num_val;
        return 1;
    }
    return 0;
}

static int co_past_replay_age(double now, const json_t *completed, const json_t *dispatched)
{
    double basis;
    if (!co_age_basis(completed, dispatched, &basis)) return 0;
    return (now - basis) > W23_COORDINATI_5A8F30_REPLAY_MAX_AGE;
}

/* Append one statement (text + parameter list) to the recorded log. */
static void co_log(json_t *sql, json_t *params, const char *stmt, json_t *args)
{
    json_append(sql, json_string(stmt));
    json_append(params, args);
}

/* Text of a string-typed cell, or "". */
static const char *co_text(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* -- constants --------------------------------------------------------- */

json_t *w23_coordinati_5a8f30_consts(void)
{
    json_t *o = json_object();
    json_t *deliveries = json_array();
    json_set(o, "replay_max_age", json_number(W23_COORDINATI_5A8F30_REPLAY_MAX_AGE));
    json_set(o, "orphan_stale", json_number(W23_COORDINATI_5A8F30_ORPHAN_STALE));
    json_set(o, "claim_stale", json_number(W23_COORDINATI_5A8F30_CLAIM_STALE));
    json_set(o, "retention", json_number(W23_COORDINATI_5A8F30_RETENTION));
    json_set(o, "failure_window", json_number(W23_COORDINATI_5A8F30_FAILURE_WINDOW));
    json_set(o, "turn_lease_sweep_grace",
             json_number(W23_COORDINATI_5A8F30_TURN_LEASE_SWEEP_GRACE));
    json_set(o, "max_delivery_attempts",
             json_number(W23_COORDINATI_5A8F30_MAX_DELIVERY_ATTEMPTS));
    json_set(o, "max_retained_terminal",
             json_number(W23_COORDINATI_5A8F30_MAX_RETAINED_TERMINAL));
    json_set(o, "max_durable_pending", json_number(W23_COORDINATI_5A8F30_MAX_DURABLE_PENDING));
    json_append(deliveries, json_string("pending"));
    json_append(deliveries, json_string("delivered"));
    json_set(o, "completion_deliveries", deliveries);
    return o;
}

json_t *w23_coordinati_5a8f30_past_replay_age(const json_t *now, const json_t *completed_at,
                                              const json_t *dispatched_at)
{
    double n = (now && now->type == JSON_NUMBER) ? now->num_val : 0.0;
    return json_bool(co_past_replay_age(n, completed_at, dispatched_at) ? true : false);
}

json_t *w23_coordinati_5a8f30_json_object(const char *raw)
{
    json_t *parsed, *out;
    if (!raw || raw[0] == '\0') return json_object();
    parsed = json_parse(raw, NULL);
    if (!parsed) return json_object();
    if (parsed->type != JSON_OBJECT) {
        json_free(parsed);
        return json_object();
    }
    out = json_copy(parsed);
    json_free(parsed);
    return out;
}

/* _claim_lease_row: SELECT the row, DELETE it when the caller's predicate
 * calls it stale, INSERT OR IGNORE ours, then SELECT to confirm ownership
 * (INSERT OR IGNORE gives no rowcount signal). */
json_t *w23_coordinati_5a8f30_claim_lease(const json_t *req)
{
    const char *table = json_get_str(req, "table", "");
    const char *key_col = json_get_str(req, "key_col", "");
    const char *holder = json_get_str(req, "holder", "");
    const char *mode = json_get_str(req, "mode", "lt");
    double now = co_num(req, "now", 0.0);
    double expires = co_num(req, "expires_at", 0.0);
    json_t *lease = json_obj_get(req, "lease");   /* [holder, expires_at] or null */
    json_t *dead = json_obj_get(req, "dead");
    json_t *key = json_obj_get(req, "key");
    json_t *out = json_object();
    json_t *sql = json_array();
    json_t *params = json_array();
    json_t *args;
    char stmt[512];
    const char *owner;
    const char *reclaimed = NULL;
    int inclusive = mode && strcmp(mode, "le") == 0;
    int stale = 0;
    int acquired;

    if (!table) table = "";
    if (!key_col) key_col = "";
    if (!holder) holder = "";
    if (lease && (lease->type != JSON_ARRAY || json_len(lease) < 2)) lease = NULL;

    snprintf(stmt, sizeof(stmt), "SELECT holder, expires_at FROM %s WHERE %s = ?",
             table, key_col);
    args = json_array();
    json_append(args, key ? json_copy(key) : json_null());
    co_log(sql, params, stmt, args);

    if (lease) {
        json_t *expires_at = json_get(lease, 1);
        double e = (expires_at && expires_at->type == JSON_NUMBER) ? expires_at->num_val : 0.0;
        size_t i, n = (dead && dead->type == JSON_ARRAY) ? json_len(dead) : 0;
        const char *row_holder = co_text(json_get(lease, 0));
        if (inclusive ? (e <= now) : (e < now)) stale = 1;
        for (i = 0; i < n && !stale; i++)
            if (strcmp(co_text(json_get(dead, i)), row_holder) == 0) stale = 1;
        if (stale) {
            snprintf(stmt, sizeof(stmt), "DELETE FROM %s WHERE %s = ? AND holder = ?",
                     table, key_col);
            args = json_array();
            json_append(args, key ? json_copy(key) : json_null());
            json_append(args, json_string(row_holder));
            co_log(sql, params, stmt, args);
            reclaimed = row_holder;
        }
    }

    snprintf(stmt, sizeof(stmt),
             "INSERT OR IGNORE INTO %s (%s, holder, acquired_at, expires_at) "
             "VALUES (?, ?, ?, ?)", table, key_col);
    args = json_array();
    json_append(args, key ? json_copy(key) : json_null());
    json_append(args, json_string(holder));
    json_append(args, json_number(now));
    json_append(args, json_number(expires));
    co_log(sql, params, stmt, args);

    snprintf(stmt, sizeof(stmt), "SELECT holder FROM %s WHERE %s = ?", table, key_col);
    args = json_array();
    json_append(args, key ? json_copy(key) : json_null());
    co_log(sql, params, stmt, args);

    /* A deleted stale row leaves ours in place; a live row keeps its holder. */
    owner = (lease && !stale) ? co_text(json_get(lease, 0)) : holder;
    if (!owner) owner = "";
    acquired = (strcmp(owner, holder) == 0);

    json_set(out, "acquired", json_bool(acquired ? true : false));
    json_set(out, "reclaimed", reclaimed ? json_string(reclaimed) : json_null());
    json_set(out, "sql", sql);
    json_set(out, "params", params);
    return out;
}

/* One replay-eligible row: past the age cap it is dropped terminally,
 * otherwise it is offered with its payload decoded. */
static void co_offer_row(json_t *offered, json_t *dropped, json_t *sql, json_t *params,
                         const json_t *id_node, const json_t *payload, const json_t *completed,
                         const json_t *dispatched, double now)
{
    const char *id = co_text(id_node);

    if (co_past_replay_age(now, completed, dispatched)) {
        json_t *args = json_array();
        json_append(args, json_number(now));
        json_append(args, json_string(id));
        co_log(sql, params, CO_DROP_SQL, args);
        json_append(dropped, json_string(id));
        return;
    }
    {
        json_t *o = json_object();
        json_set(o, "delegation_id", json_string(id));
        json_set(o, "event",
                 w23_coordinati_5a8f30_json_object(
                     (payload && payload->type == JSON_STRING) ? payload->str_val : NULL));
        json_set(o, "completed_at", completed ? json_copy(completed) : json_null());
        json_set(o, "dispatched_at", dispatched ? json_copy(dispatched) : json_null());
        json_append(offered, o);
    }
}

json_t *w23_coordinati_5a8f30_replay_pending(const json_t *req)
{
    json_t *out = json_object();
    json_t *offered = json_array();
    json_t *dropped = json_array();
    json_t *sql = json_array();
    json_t *params = json_array();
    json_t *rows = req ? json_obj_get(req, "rows") : NULL;
    double now = co_num(req, "now", 0.0);
    size_t i, n = (rows && rows->type == JSON_ARRAY) ? json_len(rows) : 0;

    for (i = 0; i < n; i++) {
        json_t *row = json_get(rows, i);
        if (!row || row->type != JSON_ARRAY || json_len(row) < 4) continue;
        co_offer_row(offered, dropped, sql, params, json_get(row, 0), json_get(row, 1),
                     json_get(row, 2), json_get(row, 3), now);
    }
    json_set(out, "offered", offered);
    json_set(out, "dropped", dropped);
    json_set(out, "sql", sql);
    json_set(out, "params", params);
    return out;
}

json_t *w23_coordinati_5a8f30_admitted_completion(const json_t *req)
{
    json_t *out = json_object();
    json_t *sql = json_array();
    json_t *params = json_array();
    json_t *args = json_array();
    json_t *row = req ? json_obj_get(req, "row") : NULL;
    const char *id = req ? json_get_str(req, "delegation_id", "") : "";
    double now = co_num(req, "now", 0.0);
    json_t *result = json_null();

    if (!id) id = "";
    json_append(args, json_string(id));
    co_log(sql, params, CO_ADMITTED_SQL, args);

    if (row && row->type == JSON_ARRAY && json_len(row) >= 4) {
        json_t *attempts = json_get(row, 3);
        json_t *completed = json_get(row, 1);
        json_t *dispatched = json_get(row, 2);
        json_t *payload = json_get(row, 0);
        double a = (attempts && attempts->type == JSON_NUMBER) ? attempts->num_val : 0.0;
        if (a < (double)W23_COORDINATI_5A8F30_MAX_DELIVERY_ATTEMPTS &&
            !co_past_replay_age(now, completed, dispatched)) {
            result = json_object();
            json_set(result, "delegation_id", json_string(id));
            json_set(result, "event",
                     w23_coordinati_5a8f30_json_object(
                         (payload && payload->type == JSON_STRING) ? payload->str_val : NULL));
            json_set(result, "completed_at", completed ? json_copy(completed) : json_null());
            json_set(result, "dispatched_at", dispatched ? json_copy(dispatched) : json_null());
        }
    }
    json_set(out, "result", result);
    json_set(out, "sql", sql);
    json_set(out, "params", params);
    return out;
}

/* Is this row's recorded owner one the caller proved dead? */
static int co_owner_dead(const json_t *row, const json_t *dead_owners)
{
    json_t *pid = json_get(row, 4);
    json_t *started = json_get(row, 5);
    size_t i, n = (dead_owners && dead_owners->type == JSON_ARRAY) ? json_len(dead_owners) : 0;
    if (!pid || !started || pid->type != JSON_NUMBER || started->type != JSON_NUMBER) return 0;
    for (i = 0; i < n; i++) {
        json_t *pair = json_get(dead_owners, i);
        json_t *p, *s;
        if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
        p = json_get(pair, 0);
        s = json_get(pair, 1);
        if (p && s && p->type == JSON_NUMBER && s->type == JSON_NUMBER &&
            p->num_val == pid->num_val && s->num_val == started->num_val)
            return 1;
    }
    return 0;
}

static int co_in_list(const json_t *list, const char *id)
{
    size_t i, n = (list && list->type == JSON_ARRAY) ? json_len(list) : 0;
    for (i = 0; i < n; i++)
        if (strcmp(co_text(json_get(list, i)), id) == 0) return 1;
    return 0;
}

json_t *w23_coordinati_5a8f30_sweep(const json_t *req)
{
    json_t *offered = json_array();
    json_t *dropped = json_array();
    json_t *sql = json_array();
    json_t *params = json_array();
    json_t *out = json_object();
    json_t *args;
    json_t *rows = req ? json_obj_get(req, "rows") : NULL;
    json_t *exclude = req ? json_obj_get(req, "exclude_ids") : NULL;
    json_t *lost = req ? json_obj_get(req, "lost_ids") : NULL;
    json_t *dead_owners = req ? json_obj_get(req, "dead_owners") : NULL;
    double now = co_num(req, "now", 0.0);
    size_t i, n = (rows && rows->type == JSON_ARRAY) ? json_len(rows) : 0;

    args = json_array();
    json_append(args, json_number(now - W23_COORDINATI_5A8F30_ORPHAN_STALE));
    json_append(args, json_number(now - W23_COORDINATI_5A8F30_CLAIM_STALE));
    co_log(sql, params, CO_ORPHAN_SQL, args);

    for (i = 0; i < n; i++) {
        json_t *row = json_get(rows, i);
        const char *id;
        json_t *attempts;
        double a;
        if (!row || row->type != JSON_ARRAY || json_len(row) < 7) continue;
        id = co_text(json_get(row, 0));
        /* Only a lost copy or a proven-dead owner (and never an excluded id)
         * makes a pending completion an orphan. */
        if (!co_in_list(lost, id) && (co_in_list(exclude, id) || !co_owner_dead(row, dead_owners)))
            continue;
        attempts = json_get(row, 6);
        a = (attempts && attempts->type == JSON_NUMBER) ? attempts->num_val : 0.0;
        if (a >= (double)W23_COORDINATI_5A8F30_MAX_DELIVERY_ATTEMPTS) {
            /* Its last claimant died holding the final attempt: converge like
             * release_delivery instead of offering a ninth try. */
            json_t *dargs = json_array();
            json_append(dargs, json_number(now));
            json_append(dargs, json_string(id));
            co_log(sql, params, CO_DROP_SQL, dargs);
            json_append(dropped, json_string(id));
            continue;
        }
        co_offer_row(offered, dropped, sql, params, json_get(row, 0), json_get(row, 1),
                     json_get(row, 2), json_get(row, 3), now);
    }
    json_set(out, "offered", offered);
    json_set(out, "dropped", dropped);
    json_set(out, "sql", sql);
    json_set(out, "params", params);
    return out;
}

json_t *w23_coordinati_5a8f30_finished(const json_t *req)
{
    json_t *out = json_object();
    json_t *sql = json_array();
    json_t *params = json_array();
    json_t *rows_out = json_array();
    const char *origin_ui = req ? json_get_str(req, "origin_ui_session_id", "") : "";
    const char *parent = req ? json_get_str(req, "parent_session_id", "") : "";
    json_t *rows = req ? json_obj_get(req, "rows") : NULL;
    double now = co_num(req, "now", 0.0);
    double limit = co_num(req, "limit", 20.0);
    char owner_sql[128];
    char stmt[512];
    json_t *args;
    size_t i, n;

    if (!origin_ui) origin_ui = "";
    if (!parent) parent = "";

    /* selectors: (col, value) pairs kept only while the value is truthy. */
    owner_sql[0] = '\0';
    if (origin_ui[0] && parent[0])
        snprintf(owner_sql, sizeof(owner_sql), "origin_ui_session_id=? OR parent_session_id=?");
    else if (origin_ui[0])
        snprintf(owner_sql, sizeof(owner_sql), "origin_ui_session_id=?");
    else if (parent[0])
        snprintf(owner_sql, sizeof(owner_sql), "parent_session_id=?");

    if (owner_sql[0]) {
        snprintf(stmt, sizeof(stmt), CO_FINISHED_SQL, owner_sql);
        args = json_array();
        if (origin_ui[0]) json_append(args, json_string(origin_ui));
        if (parent[0]) json_append(args, json_string(parent));
        json_append(args, json_number(now - W23_COORDINATI_5A8F30_FAILURE_WINDOW));
        json_append(args, json_number(limit));
        co_log(sql, params, stmt, args);

        n = (rows && rows->type == JSON_ARRAY) ? json_len(rows) : 0;
        for (i = 0; i < n; i++) {
            json_t *r = json_get(rows, i);
            json_t *o, *task, *result_r;
            if (!r || r->type != JSON_ARRAY || json_len(r) < 6) continue;
            o = json_object();
            task = json_get(r, 4);
            result_r = json_get(r, 5);
            json_set(o, "delegation_id", json_copy(json_get(r, 0)));
            json_set(o, "state", json_copy(json_get(r, 1)));
            json_set(o, "dispatched_at", json_copy(json_get(r, 2)));
            json_set(o, "completed_at", json_copy(json_get(r, 3)));
            json_set(o, "task", w23_coordinati_5a8f30_json_object(
                                     (task && task->type == JSON_STRING) ? task->str_val : NULL));
            json_set(o, "result", w23_coordinati_5a8f30_json_object(
                                       (result_r && result_r->type == JSON_STRING)
                                           ? result_r->str_val : NULL));
            json_append(rows_out, o);
        }
    }
    json_set(out, "rows", rows_out);
    json_set(out, "sql", sql);
    json_set(out, "params", params);
    return out;
}
