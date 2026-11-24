/*
 * sessions_repair_profiles.c - C11 port of upstream
 * hermes_cli/sessions_repair_profiles.py
 * (gates w34_sessionsre_3cc180_p1 / w34_sessionsre_3cc180_p2).
 *
 * PoP: w34_sessionsre_3cc180_p1_ledgered_lineages @ hermes_cli/sessions_repair_profiles.py:_ledgered_lineages
 * PoP: w34_sessionsre_3cc180_p1_scan_sessions @ hermes_cli/sessions_repair_profiles.py:RepairPlan._scan_sessions
 *      (with _would_move, _plan_foreign_row, _plan_legacy_main_row, Finding.as_dict, Finding.repairable)
 * PoP: w34_sessionsre_3cc180_p1_scan_topics @ hermes_cli/sessions_repair_profiles.py:RepairPlan._scan_topics
 * PoP: w34_sessionsre_3cc180_p1_scan_routing @ hermes_cli/sessions_repair_profiles.py:RepairPlan._scan_routing
 * PoP: w34_sessionsre_3cc180_p2_voice_modes @ hermes_cli/sessions_repair_profiles.py:RepairPlan._scan_voice_modes
 * PoP: w34_sessionsre_3cc180_p2_sessions_json @ hermes_cli/sessions_repair_profiles.py:RepairPlan._scan_sessions_json
 * PoP: w34_sessionsre_3cc180_p2_parents_first @ hermes_cli/sessions_repair_profiles.py:_parents_first
 * PoP: w34_sessionsre_3cc180_p2_rekey_voice @ hermes_cli/sessions_repair_profiles.py:_rekey_voice_mode_entry
 * PoP: w34_sessionsre_3cc180_p2_drop_entry @ hermes_cli/sessions_repair_profiles.py:_drop_sessions_json_entry
 * PoP: w34_sessionsre_3cc180_p2_move_batch @ hermes_cli/sessions_repair_profiles.py:_MoveBatch.run
 * PoP: w34_sessionsre_3cc180_p2_apply @ hermes_cli/sessions_repair_profiles.py:RepairPlan.apply
 * PoP (private): sr_key_profile @ persistence/core/keys.py:session_key_profile
 *
 * Seams, never ported and always caller-supplied: every state.db repair handle
 * (_Session.db -> find_crossed_profile_sessions, find_profile_less_telegram_
 * topic_rows, list_gateway_routing_rows, key_profiles_for_chat, and the move
 * primitives export/import/count/delete), Store.db_path
 * (persistence_assembly.store_location), the json files and atomic_json_write,
 * enumerate_stores, live-gateway pids, snapshots, and the gateway multiplex
 * verdict.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w34_sessionsre_3cc180_p1.h"
#include "noc_w34_sessionsre_3cc180_p2.h"

/* ── growable string ──────────────────────────────────────────────────────── */

typedef struct {
    char  *p;
    size_t n;
    size_t cap;
} sr_buf;

static void sr_init(sr_buf *b) { b->p = NULL; b->n = 0; b->cap = 0; }

static int sr_reserve(sr_buf *b, size_t extra)
{
    size_t need = b->n + extra + 1;
    size_t cap;
    char *np;

    if (need <= b->cap) return 1;
    cap = b->cap ? b->cap : 64;
    while (cap < need) cap *= 2;
    np = (char *)realloc(b->p, cap);
    if (!np) return 0;
    b->p = np;
    b->cap = cap;
    return 1;
}

static void sr_add(sr_buf *b, const char *s)
{
    size_t l;

    if (!s) return;
    l = strlen(s);
    if (!sr_reserve(b, l)) return;
    memcpy(b->p + b->n, s, l);
    b->n += l;
    b->p[b->n] = '\0';
}

static void sr_addc(sr_buf *b, char c)
{
    if (!sr_reserve(b, 1)) return;
    b->p[b->n++] = c;
    b->p[b->n] = '\0';
}

/* Detach the text; always a valid C string ("" when nothing was written). */
static char *sr_take(sr_buf *b)
{
    char *p = b->p;

    if (!p) {
        p = (char *)malloc(1);
        if (p) p[0] = '\0';
    }
    b->p = NULL;
    b->n = 0;
    b->cap = 0;
    return p;
}

static char *sr_dup(const char *s)
{
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);

    if (p) memcpy(p, s ? s : "", n);
    return p;
}

/* ── Python value views ───────────────────────────────────────────────────── */

/* Python truthiness of a JSON value. */
static int sr_truthy(const json_t *v)
{
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

/* str(int) / str(float): an integral value has no ".0" (json.loads made it an
 * int), the rest print at the shortest precision that round-trips. */
static void sr_num(char *buf, size_t cap, double v)
{
    int prec;

    if (v > -1e15 && v < 1e15) {
        long long iv = (long long)v;
        if ((double)iv == v) {
            snprintf(buf, cap, "%lld", iv);
            return;
        }
    }
    for (prec = 1; prec <= 17; prec++) {
        snprintf(buf, cap, "%.*g", prec, v);
        if (strtod(buf, NULL) == v) return;
    }
}

/* Python repr of a str, quote choice included. */
static void sr_repr_strlit(sr_buf *b, const char *s)
{
    char quote = '\'';

    if (!s) s = "";
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    sr_addc(b, quote);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == (unsigned char)quote || c == '\\') {
            sr_addc(b, '\\');
            sr_addc(b, (char)c);
        } else if (c == '\n') {
            sr_add(b, "\\n");
        } else if (c == '\r') {
            sr_add(b, "\\r");
        } else if (c == '\t') {
            sr_add(b, "\\t");
        } else if (c < 0x20 || c == 0x7f) {
            char t[8];
            snprintf(t, sizeof t, "\\x%02x", (unsigned)c);
            sr_add(b, t);
        } else {
            sr_addc(b, (char)c);
        }
    }
    sr_addc(b, quote);
}

static void sr_repr(sr_buf *b, const json_t *v);

/* Python str(): a str prints bare, everything else as its repr. */
static void sr_strv(sr_buf *b, const json_t *v)
{
    if (v && v->type == JSON_STRING) {
        sr_add(b, v->str_val ? v->str_val : "");
        return;
    }
    sr_repr(b, v);
}

static void sr_repr(sr_buf *b, const json_t *v)
{
    size_t i, n;
    char numbuf[48];

    if (!v || v->type == JSON_NULL) { sr_add(b, "None"); return; }
    switch (v->type) {
    case JSON_NULL:
        sr_add(b, "None");
        return;
    case JSON_BOOL:
        sr_add(b, v->bool_val ? "True" : "False");
        return;
    case JSON_NUMBER:
        sr_num(numbuf, sizeof numbuf, v->num_val);
        sr_add(b, numbuf);
        return;
    case JSON_STRING:
        sr_repr_strlit(b, v->str_val);
        return;
    case JSON_ARRAY:
        n = json_len(v);
        sr_addc(b, '[');
        for (i = 0; i < n; i++) {
            if (i) sr_add(b, ", ");
            sr_repr(b, json_get(v, i));
        }
        sr_addc(b, ']');
        return;
    case JSON_OBJECT:
        n = json_object_size(v);
        sr_addc(b, '{');
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(v, i);
            if (i) sr_add(b, ", ");
            sr_repr_strlit(b, k ? k : "");
            sr_add(b, ": ");
            sr_repr(b, json_object_get_at(v, i));
        }
        sr_addc(b, '}');
        return;
    }
}

/* py_str(obj[key]) as a fresh allocation, never NULL. */
static char *sr_keystr(const json_t *obj, const char *key)
{
    sr_buf b;
    char *out;

    sr_init(&b);
    sr_strv(&b, (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL);
    out = sr_take(&b);
    return out ? out : sr_dup("");
}

static const char *sr_field(const json_t *obj, const char *key, const char *def)
{
    const json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}

static int sr_in_list(const json_t *list, const char *s)
{
    size_t i;

    if (!list || !s) return 0;
    for (i = 0; i < json_len(list); i++) {
        const json_t *e = json_get(list, i);
        if (e && e->type == JSON_STRING && !strcmp(e->str_val ? e->str_val : "", s)) return 1;
    }
    return 0;
}

static void sr_push(json_t *arr, const char *s)
{
    if (!sr_in_list(arr, s)) json_append(arr, json_string(s));
}

/* ── the finding record ───────────────────────────────────────────────────── */

/* Takes ownership of detail/action/reason (any may be NULL). */
static json_t *sr_finding(const char *kind, const char *store, const char *subject,
                          char *detail, char *action, char *reason, int repairable)
{
    json_t *o = json_object();

    json_set(o, "kind", json_string(kind ? kind : ""));
    json_set(o, "store", json_string(store ? store : ""));
    json_set(o, "subject", json_string(subject ? subject : ""));
    json_set(o, "detail", json_string(detail ? detail : ""));
    if (action) json_set(o, "action", json_string(action)); else json_set(o, "action", json_null());
    if (reason) json_set(o, "reason", json_string(reason)); else json_set(o, "reason", json_null());
    json_set(o, "repairable", json_bool(repairable ? true : false));
    free(detail);
    free(action);
    free(reason);
    return o;
}

/* f"key {session_key!r} ({message_count} messages) sits in the {profile} store" */
static char *sr_sits_detail(const json_t *row, const char *store_profile)
{
    sr_buf b;

    sr_init(&b);
    sr_add(&b, "key ");
    sr_repr(&b, json_obj_get(row, "session_key"));
    sr_add(&b, " (");
    sr_strv(&b, json_obj_get(row, "message_count"));
    sr_add(&b, " messages) sits in the ");
    sr_add(&b, store_profile);
    sr_add(&b, " store");
    return sr_take(&b);
}

/* ── session_key_profile (persistence/core/keys.py) ───────────────────────── */

/* agent:<ns>:... -> profile id ("main" is the default profile, "main~" is a
 * profile literally named main); anything else names no profile. */
static char *sr_key_profile(const char *key)
{
    const char *c1, *c2, *ns;
    size_t len;

    if (!key) return NULL;
    c1 = strchr(key, ':');
    if (!c1) return NULL;
    c2 = strchr(c1 + 1, ':');
    if (!c2) return NULL;
    if ((size_t)(c1 - key) != 5 || strncmp(key, "agent", 5) != 0) return NULL;
    if (c2 == c1 + 1) return NULL;
    ns = c1 + 1;
    len = (size_t)(c2 - ns);
    if (len == 4 && !strncmp(ns, "main", 4)) return sr_dup("default");
    if (len == 5 && !strncmp(ns, "main~", 5)) return sr_dup("main");
    {
        char *out = (char *)malloc(len + 1);
        if (!out) return NULL;
        memcpy(out, ns, len);
        out[len] = '\0';
        return out;
    }
}

/* ── ledgered lineages ────────────────────────────────────────────────────── */

#define SR_REASON_SELF "has gateway admission/worker history, which a move cannot carry"

static size_t sr_root(size_t *uf, size_t i)
{
    while (uf[i] != i) {
        uf[i] = uf[uf[i]];
        i = uf[i];
    }
    return i;
}

/* {id: reason} for the rows a move must not carry.  Rows linked by
 * parent_session_id form one lineage; a lineage holding any ledgered row holds
 * all of it (a moved parent detaches the child left behind, and a moved child
 * loses its parent). */
static json_t *sr_ledgered_map(const json_t *foreign)
{
    json_t *out = json_object();
    const json_t **rows;
    char **ids;
    size_t *uf;
    size_t n, m = 0, i, k;

    if (!foreign || foreign->type != JSON_ARRAY) return out;
    n = json_len(foreign);
    if (!n) return out;
    rows = (const json_t **)calloc(n, sizeof *rows);
    ids = (char **)calloc(n, sizeof *ids);
    if (!rows || !ids) {
        free(rows);
        free(ids);
        return out;
    }
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(foreign, i);
        char *id;
        size_t hit = (size_t)-1, j;

        if (!row || row->type != JSON_OBJECT) continue;
        id = sr_keystr(row, "id");
        if (!id) continue;
        for (j = 0; j < m; j++) {
            if (!strcmp(ids[j], id)) { hit = j; break; }
        }
        if (hit != (size_t)-1) {
            rows[hit] = row;          /* last row for an id wins, first position kept */
            free(id);
        } else {
            ids[m] = id;
            rows[m] = row;
            m++;
        }
    }
    if (!m) {
        free(rows);
        free(ids);
        return out;
    }
    uf = (size_t *)malloc(m * sizeof *uf);
    if (!uf) {
        for (i = 0; i < m; i++) free(ids[i]);
        free(rows);
        free(ids);
        return out;
    }
    for (i = 0; i < m; i++) uf[i] = i;
    for (i = 0; i < m; i++) {
        const json_t *pv = json_obj_get(rows[i], "parent_session_id");
        char *par;
        size_t j = (size_t)-1;

        if (!pv || pv->type == JSON_NULL) continue;
        {
            sr_buf b;
            sr_init(&b);
            sr_strv(&b, pv);
            par = sr_take(&b);
        }
        if (!par) continue;
        for (k = 0; k < m; k++) {
            if (!strcmp(ids[k], par)) { j = k; break; }
        }
        free(par);
        if (j == (size_t)-1) continue;
        if (sr_root(uf, j) != sr_root(uf, i)) uf[sr_root(uf, j)] = sr_root(uf, i);
    }
    for (i = 0; i < m; i++) {
        size_t root = sr_root(uf, i);
        const char *anchor = NULL;

        for (k = 0; k < m; k++) {
            if (sr_root(uf, k) != root) continue;
            if (!sr_truthy(json_obj_get(rows[k], "ledgered"))) continue;
            if (!anchor || strcmp(ids[k], anchor) < 0) anchor = ids[k];
        }
        if (sr_truthy(json_obj_get(rows[i], "ledgered"))) {
            json_set(out, ids[i], json_string(SR_REASON_SELF));
        } else if (anchor) {
            sr_buf b;
            char *reason;
            sr_init(&b);
            sr_add(&b, "its lineage holds ");
            sr_add(&b, anchor);
            sr_add(&b, ", which has gateway admission/worker history");
            reason = sr_take(&b);
            json_set(out, ids[i], json_string(reason ? reason : ""));
            free(reason);
        }
    }
    for (i = 0; i < m; i++) free(ids[i]);
    free(uf);
    free(rows);
    free(ids);
    return out;
}

json_t *w34_sessionsre_3cc180_p1_ledgered_lineages(const json_t *foreign)
{
    return sr_ledgered_map(foreign);
}

/* ── scan: sessions ───────────────────────────────────────────────────────── */

/* _would_move: does the plan copy this row to another store? */
static int sr_would_move(const json_t *row, const json_t *claimed, const char *store_profile,
                         const char *routing_store, const char *legacy_main)
{
    char *kp = sr_keystr(row, "key_profile");
    int move;

    if (!strcmp(kp, "default") && strcmp(store_profile, "default") != 0)
        move = !strcmp(legacy_main, "move") && routing_store != NULL;
    else
        move = sr_in_list(claimed, kp);
    free(kp);
    return move;
}

/* _plan_foreign_row (+ _plan_legacy_main_row): the verdict for one row that
 * sits in a store whose namespace its key does not belong to. */
static void sr_plan_foreign(json_t *out, json_t *moving, const json_t *claimed,
                            const char *sp, const char *routing_store, const char *legacy_main,
                            const json_t *row)
{
    char *sid = sr_keystr(row, "id");
    char *kp = sr_keystr(row, "key_profile");

    if (!strcmp(kp, "default") && strcmp(sp, "default") != 0) {
        sr_buf b;
        char *detail;
        sr_init(&b);
        sr_add(&b, "legacy default-namespace key ");
        sr_repr(&b, json_obj_get(row, "session_key"));
        sr_add(&b, " (");
        sr_strv(&b, json_obj_get(row, "message_count"));
        sr_add(&b, " messages) in the ");
        sr_add(&b, sp);
        sr_add(&b, " store");
        detail = sr_take(&b);
        if (!strcmp(legacy_main, "rekey")) {
            sr_buf a;
            char *action;
            sr_init(&a);
            sr_add(&a, "rekey to agent:");
            sr_add(&a, sp);
            sr_add(&a, ":");
            action = sr_take(&a);
            json_append(out, sr_finding("legacy_main", sp, sid, detail, action, NULL, 1));
        } else if (!strcmp(legacy_main, "move") && routing_store) {
            sr_push(moving, sid);
            json_append(out, sr_finding("legacy_main", sp, sid, detail,
                                        sr_dup("move to the default store"), NULL, 1));
        } else {
            json_append(out, sr_finding(
                "legacy_main", sp, sid, detail, NULL,
                sr_dup("either a standalone gateway's own history (--legacy-main rekey) or a "
                       "default chat that leaked in (--legacy-main move); the row cannot say which"),
                0));
        }
    } else if (!sr_in_list(claimed, kp)) {
        sr_buf b;
        char *reason;
        sr_init(&b);
        sr_add(&b, "no live profile named ");
        sr_repr_strlit(&b, kp);
        sr_add(&b, "; create it, then rerun, or `hermes profile migrate-identity ");
        sr_add(&b, kp);
        sr_add(&b, " <target>`");
        reason = sr_take(&b);
        json_append(out, sr_finding("unclaimed_namespace", sp, sid, sr_sits_detail(row, sp),
                                    NULL, reason, 0));
    } else {
        sr_buf a;
        char *action;
        sr_init(&a);
        sr_add(&a, "move to the ");
        sr_add(&a, kp);
        sr_add(&a, " store");
        action = sr_take(&a);
        sr_push(moving, sid);
        json_append(out, sr_finding("wrong_store", sp, sid, sr_sits_detail(row, sp), action, NULL, 1));
    }
    free(sid);
    free(kp);
}

json_t *w34_sessionsre_3cc180_p1_scan_sessions(const json_t *req)
{
    const json_t *store = json_obj_get(req, "store");
    const json_t *crossed = json_obj_get(req, "crossed");
    const json_t *claimed = json_obj_get(req, "claimed");
    const json_t *foreign = json_obj_get(crossed, "foreign");
    const json_t *mis = json_obj_get(crossed, "mislabelled");
    const json_t *par = json_obj_get(crossed, "crossed_parents");
    const char *sp = sr_field(store, "profile", "default");
    const char *routing_store = sr_field(req, "routing_store", NULL);
    const char *legacy_main = sr_field(req, "legacy_main", "report");
    json_t *out = json_array();
    json_t *moving = json_array();
    json_t *held;
    json_t *filtered;
    size_t i;

    /* held = _ledgered_lineages([r for r in foreign if self._would_move(store, r)]) */
    filtered = json_array();
    for (i = 0; i < json_len(foreign); i++) {
        const json_t *row = json_get(foreign, i);
        if (row && row->type == JSON_OBJECT &&
            sr_would_move(row, claimed, sp, routing_store, legacy_main))
            json_append(filtered, json_copy(row));
    }
    held = sr_ledgered_map(filtered);
    json_free(filtered);

    for (i = 0; i < json_len(foreign); i++) {
        const json_t *row = json_get(foreign, i);
        char *sid;
        const json_t *reason;

        if (!row || row->type != JSON_OBJECT) continue;
        sid = sr_keystr(row, "id");
        reason = json_obj_get(held, sid);
        if (reason && reason->type == JSON_STRING) {
            sr_push(moving, sid);   /* neither relabelled nor severed in place: it is a stray */
            json_append(out, sr_finding("wrong_store", sp, sid, sr_sits_detail(row, sp), NULL,
                                        sr_dup(reason->str_val ? reason->str_val : ""), 0));
        } else {
            sr_plan_foreign(out, moving, claimed, sp, routing_store, legacy_main, row);
        }
        free(sid);
    }
    json_free(held);

    for (i = 0; i < json_len(mis); i++) {
        const json_t *row = json_get(mis, i);
        char *sid, *kp;
        sr_buf b, a;
        char *detail, *action;

        if (!row || row->type != JSON_OBJECT) continue;
        sid = sr_keystr(row, "id");
        if (sr_in_list(moving, sid)) { free(sid); continue; }   /* the move stamps the label */
        kp = sr_keystr(row, "key_profile");
        sr_init(&b);
        sr_add(&b, "profile_name=");
        sr_repr(&b, json_obj_get(row, "profile_name"));
        sr_add(&b, " but key ");
        sr_repr(&b, json_obj_get(row, "session_key"));
        sr_add(&b, " belongs to ");
        sr_repr_strlit(&b, kp);
        detail = sr_take(&b);
        sr_init(&a);
        sr_add(&a, "relabel to ");
        sr_repr_strlit(&a, kp);
        action = sr_take(&a);
        json_append(out, sr_finding("mislabelled", sp, sid, detail, action, NULL, 1));
        free(sid);
        free(kp);
    }

    for (i = 0; i < json_len(par); i++) {
        const json_t *row = json_get(par, i);
        char *sid, *kp, *pp;
        sr_buf b;
        char *detail;

        if (!row || row->type != JSON_OBJECT) continue;
        sid = sr_keystr(row, "id");
        if (sr_in_list(moving, sid)) { free(sid); continue; }   /* import drops a gone parent */
        kp = sr_keystr(row, "key_profile");
        pp = sr_keystr(row, "parent_profile");
        sr_init(&b);
        sr_repr_strlit(&b, kp);
        sr_add(&b, " row inherits from parent ");
        sr_strv(&b, json_obj_get(row, "parent_session_id"));
        sr_add(&b, " keyed under ");
        sr_repr_strlit(&b, pp);
        detail = sr_take(&b);
        json_append(out, sr_finding("crossed_parent", sp, sid, detail,
                                    sr_dup("sever parent_session_id"), NULL, 1));
        free(sid);
        free(kp);
        free(pp);
    }
    json_free(moving);
    return out;
}

/* ── scan: topics ─────────────────────────────────────────────────────────── */

json_t *w34_sessionsre_3cc180_p1_scan_topics(const json_t *req)
{
    const json_t *store = json_obj_get(req, "store");
    const json_t *rows = json_obj_get(req, "rows");
    const char *sp = sr_field(store, "profile", "default");
    json_t *out = json_array();
    size_t i;

    for (i = 0; i < json_len(rows); i++) {
        const json_t *row = json_get(rows, i);
        char *kp, *subject;
        sr_buf b, a;
        char *detail, *action;

        if (!row || row->type != JSON_OBJECT) continue;
        kp = sr_keystr(row, "key_profile");
        sr_init(&b);
        sr_add(&b, "chat ");
        sr_strv(&b, json_obj_get(row, "chat_id"));
        sr_add(&b, " thread ");
        sr_strv(&b, json_obj_get(row, "thread_id"));
        subject = sr_take(&b);
        sr_init(&b);
        sr_add(&b, "telegram topic binding labelled 'default' but keyed ");
        sr_repr(&b, json_obj_get(row, "session_key"));
        detail = sr_take(&b);
        sr_init(&a);
        sr_add(&a, "relabel to ");
        sr_repr_strlit(&a, kp);
        action = sr_take(&a);
        json_append(out, sr_finding("topic_profile_less", sp, subject, detail, action, NULL, 1));
        free(subject);
        free(kp);
    }
    return out;
}

/* ── scan: routing ────────────────────────────────────────────────────────── */

json_t *w34_sessionsre_3cc180_p1_scan_routing(const json_t *req)
{
    const json_t *store = json_obj_get(req, "store");
    const json_t *rows = json_obj_get(req, "rows");
    const json_t *claimed = json_obj_get(req, "claimed");
    const char *sp = sr_field(store, "profile", "default");
    const char *routing_store = sr_field(req, "routing_store", NULL);
    int routing = sr_truthy(json_obj_get(store, "routing"));
    int multiplexes = sr_truthy(json_obj_get(req, "multiplexes"));
    json_t *out = json_array();
    size_t i;

    for (i = 0; i < json_len(rows); i++) {
        const json_t *row = json_get(rows, i);
        char *kp, *subject;
        sr_buf b;
        const json_t *sk;

        if (!row || row->type != JSON_OBJECT) continue;
        sk = json_obj_get(row, "session_key");
        if (!sk || sk->type != JSON_STRING) continue;
        kp = sr_key_profile(sk->str_val);
        if (!kp) continue;                          /* keyless rows name no profile */
        sr_init(&b);
        sr_add(&b, sk->str_val ? sk->str_val : "");
        sr_add(&b, " (scope ");
        sr_repr(&b, json_obj_get(row, "scope"));
        sr_add(&b, ")");
        subject = sr_take(&b);

        if (routing) {
            if (!sr_in_list(claimed, kp)) {
                sr_buf d;
                char *detail;
                sr_init(&d);
                sr_add(&d, "routing row for a namespace no live profile claims (");
                sr_repr_strlit(&d, kp);
                sr_add(&d, "); every inbound event on it logs a missing-profile error");
                detail = sr_take(&d);
                json_append(out, sr_finding("routing_unclaimed", sp, subject, detail,
                                            sr_dup("delete"), NULL, 1));
            } else {
                free(subject);
            }
        } else if (!strcmp(kp, sp) && !multiplexes) {
            free(subject);                          /* a standalone gateway's own index */
        } else if (!routing_store) {
            free(subject);
        } else {
            sr_buf d;
            char *detail;
            sr_init(&d);
            sr_add(&d, "routing row in the ");
            sr_add(&d, sp);
            sr_add(&d, " store; the index lives in the default store");
            detail = sr_take(&d);
            json_append(out, sr_finding(
                "routing_stray", sp, subject, detail,
                sr_dup("move to the default store (existing row there wins)"), NULL, 1));
        }
        free(kp);
    }
    return out;
}

/* ── json file scans ──────────────────────────────────────────────────────── */

static int sr_cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Parse the seam-supplied file text: a missing or unparsable file is a store
 * with nothing to find. */
static json_t *sr_parse_content(const json_t *req)
{
    const json_t *content = json_obj_get(req, "content");

    if (!content || content->type != JSON_STRING || !content->str_val) return NULL;
    return json_parse(content->str_val, NULL);
}

json_t *w34_sessionsre_3cc180_p2_voice_modes(const json_t *req)
{
    const json_t *store = json_obj_get(req, "store");
    const json_t *chats = json_obj_get(req, "chats");
    const char *sp = sr_field(store, "profile", "default");
    json_t *out = json_array();
    json_t *data = sr_parse_content(req);
    const char **keys = NULL;
    size_t nk = 0, i, cap;

    if (!data || data->type != JSON_OBJECT) {
        json_free(data);
        return out;
    }
    cap = json_object_size(data);
    if (cap) keys = (const char **)calloc(cap, sizeof *keys);
    if (keys) {
        for (i = 0; i < cap; i++) {
            const char *k = json_object_get_key_at(data, i);
            const char *p;
            int colons = 0;

            if (!k) continue;
            for (p = k; *p; p++) if (*p == ':') colons++;
            if (colons == 1) keys[nk++] = k;
        }
        qsort(keys, nk, sizeof *keys, sr_cmp_str);
    }

    for (i = 0; i < nk; i++) {
        const char *key = keys[i];
        const json_t *owners = chats ? json_obj_get(chats, key) : NULL;
        size_t on = json_len(owners);
        const char **names = on ? (const char **)calloc(on, sizeof *names) : NULL;
        size_t nn = 0, j;
        int has_default = 0;

        for (j = 0; j < on; j++) {
            const json_t *o = json_get(owners, j);
            const char *name;
            size_t q;
            int dup = 0;

            if (!o || o->type != JSON_STRING) continue;
            name = o->str_val ? o->str_val : "";
            for (q = 0; q < nn; q++) {
                if (!strcmp(names[q], name)) { dup = 1; break; }
            }
            if (dup) continue;
            names[nn++] = name;
            if (!strcmp(name, "default")) has_default = 1;
        }
        if (!nn || has_default) {          /* the default bot speaks there */
            free(names);
            continue;
        }
        if (nn > 1) {
            sr_buf d;
            char *detail;
            sr_init(&d);
            sr_add(&d, "voice mode entry without a profile; chat is held by [");
            qsort(names, nn, sizeof *names, sr_cmp_str);
            for (j = 0; j < nn; j++) {
                if (j) sr_add(&d, ", ");
                sr_repr_strlit(&d, names[j]);
            }
            sr_add(&d, "]");
            detail = sr_take(&d);
            json_append(out, sr_finding("voice_profile_less", sp, key, detail, NULL,
                                        sr_dup("more than one non-default bot speaks in this chat"), 0));
        } else {
            sr_buf d, a;
            char *detail, *action;
            sr_init(&d);
            sr_add(&d, "voice mode ");
            sr_repr(&d, json_obj_get(data, key));
            sr_add(&d, " without a profile; chat is held only by ");
            sr_repr_strlit(&d, names[0]);
            detail = sr_take(&d);
            sr_init(&a);
            sr_add(&a, "rekey to ");
            sr_add(&a, names[0]);
            sr_add(&a, ":");
            sr_add(&a, key);
            action = sr_take(&a);
            json_append(out, sr_finding("voice_profile_less", sp, key, detail, action, NULL, 1));
        }
        free(names);
    }
    free(keys);
    json_free(data);
    return out;
}

json_t *w34_sessionsre_3cc180_p2_sessions_json(const json_t *req)
{
    const json_t *store = json_obj_get(req, "store");
    const json_t *claimed = json_obj_get(req, "claimed");
    const char *sp = sr_field(store, "profile", "default");
    json_t *out = json_array();
    json_t *data = sr_parse_content(req);
    const char **keys = NULL;
    size_t nk = 0, i, cap;

    if (!data || data->type != JSON_OBJECT) {
        json_free(data);
        return out;
    }
    cap = json_object_size(data);
    if (cap) keys = (const char **)calloc(cap, sizeof *keys);
    if (keys) {
        for (i = 0; i < cap; i++) {
            const char *k = json_object_get_key_at(data, i);
            if (!k || k[0] == '_') continue;
            keys[nk++] = k;
        }
        qsort(keys, nk, sizeof *keys, sr_cmp_str);
    }
    for (i = 0; i < nk; i++) {
        char *kp = sr_key_profile(keys[i]);
        sr_buf d;
        char *detail;

        if (!kp) continue;                     /* keyless: names no namespace */
        if (sr_in_list(claimed, kp)) { free(kp); continue; }
        sr_init(&d);
        sr_add(&d, "sessions.json mirror entry for a namespace no live profile claims (");
        sr_repr_strlit(&d, kp);
        sr_add(&d, "); the legacy import re-injects it into routing on every boot");
        detail = sr_take(&d);
        json_append(out, sr_finding("sessions_json_unclaimed", sp, keys[i], detail,
                                    sr_dup("delete entry"), NULL, 1));
        free(kp);
    }
    free(keys);
    json_free(data);
    return out;
}

/* ── move order ───────────────────────────────────────────────────────────── */

typedef struct {
    const json_t *payload;
    const char   *id;
    const char   *parent;   /* NULL when the payload names no parent */
} sr_payload;

/* Is the parent still waiting in the batch? */
static int sr_parent_pending(const sr_payload *ps, size_t n, const char *alive, const sr_payload *p)
{
    size_t i;

    if (!p->parent) return 0;
    for (i = 0; i < n; i++) {
        if (alive[i] && !strcmp(ps[i].id, p->parent)) return 1;
    }
    return 0;
}

/* Order payload indices so every parent in the batch precedes its children; a
 * cycle (impossible for a well-formed store) falls back to insertion order. */
static size_t *sr_order(const sr_payload *ps, size_t n, size_t *out_n)
{
    size_t *order = (size_t *)malloc((n ? n : 1) * sizeof *order);
    char *alive = (char *)calloc(n ? n : 1, 1);
    char *ready = (char *)calloc(n ? n : 1, 1);
    size_t cnt = 0, done = 0, i;

    *out_n = 0;
    if (!order || !alive || !ready) {
        free(order);
        free(alive);
        free(ready);
        return NULL;
    }
    for (i = 0; i < n; i++) alive[i] = 1;
    while (done < n) {
        size_t pass = 0;

        memset(ready, 0, n);
        for (i = 0; i < n; i++) {
            if (!alive[i]) continue;
            if (!sr_parent_pending(ps, n, alive, &ps[i])) { ready[i] = 1; pass++; }
        }
        if (!pass) {
            for (i = 0; i < n; i++) {
                if (alive[i]) { ready[i] = 1; pass++; }
            }
        }
        for (i = 0; i < n; i++) {
            if (!ready[i]) continue;
            order[cnt++] = i;
            alive[i] = 0;
            done++;
        }
    }
    free(alive);
    free(ready);
    *out_n = cnt;
    return order;
}

/* payloads -> [{payload, id, parent}] carrying only what the ordering needs;
 * Python's dict-by-id collapses a repeated id to its last payload in its first
 * position. */
static size_t sr_collect_payloads(const json_t *payloads, sr_payload *ps, size_t cap)
{
    size_t n = 0, i;

    for (i = 0; i < json_len(payloads); i++) {
        const json_t *p = json_get(payloads, i);
        const json_t *sess;
        const json_t *pv;
        char *id, *parent;
        size_t hit = (size_t)-1, q;

        if (!p || p->type != JSON_OBJECT) continue;
        sess = json_obj_get(p, "session");
        if (!sess || sess->type != JSON_OBJECT) continue;
        id = sr_keystr(sess, "id");
        if (!id) continue;
        pv = json_obj_get(sess, "parent_session_id");
        parent = (pv && pv->type != JSON_NULL) ? sr_keystr(sess, "parent_session_id") : NULL;
        for (q = 0; q < n; q++) {
            if (!strcmp(ps[q].id, id)) { hit = q; break; }
        }
        if (hit != (size_t)-1) {
            free((char *)ps[hit].id);
            free((char *)ps[hit].parent);
            ps[hit].payload = p;
            ps[hit].id = id;
            ps[hit].parent = parent;
            continue;
        }
        if (n >= cap) { free(id); free(parent); break; }
        ps[n].payload = p;
        ps[n].id = id;
        ps[n].parent = parent;
        n++;
    }
    return n;
}

static void sr_free_payloads(sr_payload *ps, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        free((char *)ps[i].id);
        free((char *)ps[i].parent);
    }
}

json_t *w34_sessionsre_3cc180_p2_parents_first(const json_t *req)
{
    const json_t *payloads = json_obj_get(req, "payloads");
    size_t cap = json_len(payloads), n, cnt = 0, i;
    sr_payload *ps;
    size_t *order;
    json_t *out = json_array();

    if (!cap) return out;
    ps = (sr_payload *)calloc(cap, sizeof *ps);
    if (!ps) return out;
    n = sr_collect_payloads(payloads, ps, cap);
    order = sr_order(ps, n, &cnt);
    for (i = 0; i < cnt; i++) json_append(out, json_string(ps[order[i]].id));
    free(order);
    sr_free_payloads(ps, n);
    free(ps);
    return out;
}

/* ── json rewrites ────────────────────────────────────────────────────────── */

json_t *w34_sessionsre_3cc180_p2_rekey_voice(const json_t *req)
{
    const char *key = sr_field(req, "key", "");
    const char *owner = sr_field(req, "owner", "");
    json_t *data = sr_parse_content(req);
    json_t *out = json_object();
    json_t *result = json_object();
    const json_t *val;

    json_set(out, "result", result);
    if (!data || data->type != JSON_OBJECT) {
        json_free(data);
        json_set(out, "data", json_null());
        return out;
    }
    val = json_obj_get(data, key);
    if (!val) {                                  /* nothing to rekey: no write */
        json_free(data);
        json_set(out, "data", json_null());
        return out;
    }
    {
        sr_buf b;
        char *newkey;
        sr_init(&b);
        sr_add(&b, owner);
        sr_add(&b, ":");
        sr_add(&b, key);
        newkey = sr_take(&b);
        if (newkey && !json_obj_get(data, newkey))
            json_set(data, newkey, json_copy(val));   /* setdefault: first writer wins */
        json_obj_del(data, key);
        free(newkey);
    }
    json_set(result, "voice_rekeyed", json_number(1));
    json_set(out, "data", data);
    return out;
}

json_t *w34_sessionsre_3cc180_p2_drop_entry(const json_t *req)
{
    const char *key = sr_field(req, "key", "");
    json_t *data = sr_parse_content(req);
    json_t *out = json_object();
    json_t *result = json_object();

    json_set(out, "result", result);
    if (!data || data->type != JSON_OBJECT) {
        json_free(data);
        json_set(out, "data", json_null());
        return out;
    }
    if (!json_obj_get(data, key)) {               /* nothing to drop: no write */
        json_free(data);
        json_set(out, "data", json_null());
        return out;
    }
    json_obj_del(data, key);
    json_set(result, "sessions_json_dropped", json_number(1));
    json_set(out, "data", data);
    return out;
}

/* ── move batch ───────────────────────────────────────────────────────────── */

json_t *w34_sessionsre_3cc180_p2_move_batch(const json_t *req)
{
    const json_t *ids = json_obj_get(req, "ids");
    const json_t *payloads = json_obj_get(req, "payloads");
    const json_t *export_errors = json_obj_get(req, "export_errors");
    const json_t *import_errors = json_obj_get(req, "import_errors");
    const json_t *import_outcome = json_obj_get(req, "import_outcome");
    const json_t *counts = json_obj_get(req, "message_counts");
    const json_t *delete_errors = json_obj_get(req, "delete_errors");
    const json_t *delete_result = json_obj_get(req, "delete_result");
    json_t *result = json_object();
    json_t *errd = json_object();
    json_t *awaited = json_object();
    size_t cap = json_len(ids), i, n = 0, cnt = 0;
    sr_payload *ps;
    size_t *order;

    if (!cap) return result;
    ps = (sr_payload *)calloc(cap, sizeof *ps);
    if (!ps) return result;

    /* export every id; a row that gained ledger rows since the scan is not copied */
    for (i = 0; i < cap; i++) {
        const json_t *sid = json_get(ids, i);
        const json_t *p;
        char *key;

        if (!sid || sid->type != JSON_STRING) continue;
        key = sr_dup(sid->str_val ? sid->str_val : "");
        if (!key) continue;
        if (sr_in_list(export_errors, key)) {
            json_set(errd, key, json_bool(true));
            free(key);
            continue;
        }
        p = payloads ? json_obj_get(payloads, key) : NULL;
        if (!p || p->type != JSON_OBJECT) {
            json_t *miss = json_object();
            json_set(miss, "missing", json_number(1));
            json_set(result, key, miss);
            free(key);
            continue;
        }
        ps[n].payload = p;
        ps[n].id = key;
        {
            const json_t *sess = json_obj_get(p, "session");
            const json_t *pv = (sess && sess->type == JSON_OBJECT)
                                   ? json_obj_get(sess, "parent_session_id") : NULL;
            ps[n].parent = (pv && pv->type != JSON_NULL)
                               ? sr_keystr(sess, "parent_session_id") : NULL;
        }
        n++;
    }

    order = sr_order(ps, n, &cnt);

    for (i = 0; i < cnt; i++) {
        sr_payload *p = &ps[order[i]];
        const json_t *outcome;
        const char *oc;
        double have;
        double want = (double)json_len(json_obj_get(p->payload, "messages"));

        if (sr_truthy(json_obj_get(errd, p->parent))) {
            /* imported now it would land without its parent and never regain the link */
            json_set(errd, p->id, json_bool(true));
            continue;
        }
        if (sr_in_list(import_errors, p->id)) {
            json_set(errd, p->id, json_bool(true));
            continue;
        }
        have = json_get_num(counts, p->id, 1e18);
        if (have < want) {
            json_t *r = json_object();
            json_set(r, "copied_incomplete", json_number(1));
            json_set(result, p->id, r);
            continue;
        }
        outcome = import_outcome ? json_obj_get(import_outcome, p->id) : NULL;
        oc = (outcome && outcome->type == JSON_STRING) ? outcome->str_val : "copied";
        {
            json_t *r = json_object();
            json_set(r, oc ? oc : "", json_number(1));
            json_set(result, p->id, r);
        }
    }

    /* a child that did not move must still find its parent in the source */
    for (i = 0; i < cnt; i++) {
        sr_payload *p = &ps[order[i]];
        if (sr_truthy(json_obj_get(errd, p->id)) && p->parent)
            json_set(awaited, p->parent, json_bool(true));
    }

    for (i = 0; i < cnt; i++) {
        sr_payload *p = &ps[order[i]];
        json_t *r = json_obj_get(result, p->id);
        const json_t *del;

        if (!r) continue;
        if (sr_truthy(json_obj_get(awaited, p->id))) continue;
        if (json_obj_get(r, "copied_incomplete")) continue;
        if (sr_in_list(delete_errors, p->id)) {
            json_set(errd, p->id, json_bool(true));
            continue;
        }
        del = delete_result ? json_obj_get(delete_result, p->id) : NULL;
        if (!del || sr_truthy(del)) json_set(r, "moved", json_number(1));
    }

    free(order);
    sr_free_payloads(ps, n);
    free(ps);
    json_free(errd);
    json_free(awaited);
    return result;
}

/* ── apply report ─────────────────────────────────────────────────────────── */

json_t *w34_sessionsre_3cc180_p2_apply(const json_t *req)
{
    const json_t *findings = json_obj_get(req, "findings");
    const json_t *stores = json_obj_get(req, "stores");
    const json_t *labels = json_obj_get(req, "snapshot_labels");
    json_t *out = json_object();
    json_t *snapshots = json_object();
    json_t *totals = json_object();
    json_t *failures = json_array();
    size_t i, j;

    for (i = 0; i < json_len(stores); i++) {
        const json_t *st = json_get(stores, i);
        const char *prof;
        const json_t *lab;

        if (!st || st->type != JSON_OBJECT) continue;
        if (!sr_truthy(json_obj_get(st, "db_exists"))) continue;
        prof = sr_field(st, "profile", "default");
        lab = labels ? json_obj_get(labels, prof) : NULL;
        json_set(snapshots, prof, lab ? json_copy(lab) : json_null());
    }

    for (i = 0; i < json_len(findings); i++) {
        const json_t *f = json_get(findings, i);
        const json_t *fix;
        const json_t *err;

        if (!f || f->type != JSON_OBJECT) continue;
        fix = json_obj_get(f, "fix");
        if (!fix) continue;                       /* report only */
        err = json_obj_get(fix, "error");
        if (err && err->type == JSON_OBJECT) {
            json_t *fo = json_object();
            sr_buf b;
            char *msg;

            sr_init(&b);
            sr_add(&b, "RuntimeError: ");
            sr_strv(&b, json_obj_get(err, "message"));
            msg = sr_take(&b);
            json_set(fo, "kind", json_string(sr_field(f, "kind", "")));
            json_set(fo, "subject", json_string(sr_field(f, "subject", "")));
            json_set(fo, "error", json_string(msg ? msg : ""));
            json_append(failures, fo);
            free(msg);
        } else {
            const json_t *c = json_obj_get(fix, "counts");

            for (j = 0; j < json_object_size(c); j++) {
                const char *k = json_object_get_key_at(c, j);
                const json_t *v = json_object_get_at(c, j);
                double add = (v && v->type == JSON_NUMBER) ? (double)(long long)v->num_val
                                                           : (sr_truthy(v) ? 1.0 : 0.0);
                json_set(totals, k, json_number(json_get_num(totals, k, 0.0) + add));
            }
        }
    }

    json_set(out, "snapshots", snapshots);
    json_set(out, "totals", totals);
    json_set(out, "failures", failures);
    return out;
}
