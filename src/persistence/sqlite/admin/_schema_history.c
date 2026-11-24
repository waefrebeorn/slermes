/*
 * _schema_history.c - C home for persistence/sqlite/admin/_schema_history.py.
 * Faithful port of the declared-column history, the snapshot replay and the
 * reachable physical-layout graph walk (BFS over prefix chains).
 * PoP: w19_schemahist_ddcbcb_apply @ persistence/sqlite/admin/_schema_history.py:_apply
 * PoP: w19_schemahist_ddcbcb_declared_snapshots @ persistence/sqlite/admin/_schema_history.py:declared_snapshots
 * PoP: w19_schemahist_ddcbcb_current_declared_columns @ persistence/sqlite/admin/_schema_history.py:current_declared_columns
 * PoP: w19_schemahist_ddcbcb_reachable_physical_layouts @ persistence/sqlite/admin/_schema_history.py:reachable_physical_layouts
 * PoP: w19_schemahist_ddcbcb_schema_history @ persistence/sqlite/admin/_schema_history.py:SCHEMA_HISTORY
 */
#include "noc_w19_schemahist_ddcbcb.h"
#include <stdlib.h>
#include <string.h>

static char *sh_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

typedef struct { const char *op; const char *col; const char *after; } sh_edit;
typedef struct { const char *label; const sh_edit *edits; size_t nedits; } sh_event;
typedef struct {
    const char *name;
    const char *const *base;
    size_t nbase;
    const sh_event *events;
    size_t nevents;
} sh_table;

static const char *const SH_SESSIONS_BASE[] = {
    "id", "source", "user_id", "model", "model_config", "system_prompt",
    "parent_session_id", "started_at", "ended_at", "end_reason", "message_count",
    "tool_call_count", "input_tokens", "output_tokens"
};

static const sh_edit SH_S01[] = { {"+", "title", "output_tokens"} };
static const sh_edit SH_S02[] = {
    {"+", "cache_read_tokens", "output_tokens"},
    {"+", "cache_write_tokens", "cache_read_tokens"},
    {"+", "reasoning_tokens", "cache_write_tokens"},
    {"+", "billing_provider", "reasoning_tokens"},
    {"+", "billing_base_url", "billing_provider"},
    {"+", "billing_mode", "billing_base_url"},
    {"+", "estimated_cost_usd", "billing_mode"},
    {"+", "actual_cost_usd", "estimated_cost_usd"},
    {"+", "cost_status", "actual_cost_usd"},
    {"+", "cost_source", "cost_status"},
    {"+", "pricing_version", "cost_source"}
};
static const sh_edit SH_S03[] = { {"+", "api_call_count", "title"} };
static const sh_edit SH_S04[] = {
    {"+", "handoff_pending", "api_call_count"},
    {"+", "handoff_platform", "handoff_pending"}
};
static const sh_edit SH_S05[] = {
    {"-", "handoff_pending", NULL},
    {"+", "handoff_state", "api_call_count"},
    {"+", "handoff_error", "handoff_platform"}
};
static const sh_edit SH_S06[] = { {"+", "cwd", "reasoning_tokens"} };
static const sh_edit SH_S07[] = { {"+", "rewind_count", "handoff_error"} };
static const sh_edit SH_S08[] = { {"+", "archived", "rewind_count"} };
static const sh_edit SH_S09[] = {
    {"+", "git_branch", "cwd"},
    {"+", "git_repo_root", "git_branch"}
};
static const sh_edit SH_S10[] = {
    {"+", "session_key", "user_id"},
    {"+", "chat_id", "session_key"},
    {"+", "chat_type", "chat_id"},
    {"+", "thread_id", "chat_type"}
};
static const sh_edit SH_S11[] = {
    {"+", "compression_failure_cooldown_until", "handoff_error"},
    {"+", "compression_failure_error", "compression_failure_cooldown_until"}
};
static const sh_edit SH_S12[] = {
    {"+", "display_name", "thread_id"},
    {"+", "origin_json", "display_name"},
    {"+", "expiry_finalized", "origin_json"}
};
static const sh_edit SH_S13[] = { {"+", "compression_fallback_streak", "compression_failure_error"} };
static const sh_edit SH_S14[] = { {"+", "profile_name", "compression_fallback_streak"} };
static const sh_edit SH_S15[] = { {"+", "compression_ineffective_count", "compression_fallback_streak"} };
static const sh_edit SH_S16[] = { {"+", "pinned", "archived"} };
static const sh_edit SH_S17[] = {
    {"+", "last_activity_at", "title"},
    {"+", "last_activity_description", "last_activity_at"},
    {"+", "last_activity_provenance", "last_activity_description"}
};
static const sh_edit SH_S18[] = {
    {"-", "last_activity_at", NULL},
    {"-", "last_activity_description", NULL},
    {"-", "last_activity_provenance", NULL}
};
static const sh_edit SH_S19[] = {
    {"+", "last_activity_at", "title"},
    {"+", "last_activity_description", "last_activity_at"},
    {"+", "last_activity_provenance", "last_activity_description"}
};
static const sh_edit SH_S20[] = { {"+", "system_prompt_hash", "system_prompt"} };
static const sh_edit SH_S21[] = { {"+", "last_read_at", "pinned"} };
static const sh_edit SH_S22[] = { {"+", "title_source", "title"} };
static const sh_edit SH_S23[] = { {"+", "hidden", "pinned"} };
static const sh_edit SH_S24[] = { {"+", "git_metadata_generation", "git_repo_root"} };
static const sh_edit SH_S25[] = { {"+", "compression_recovery_deadline", "compression_ineffective_count"} };
static const sh_edit SH_S26[] = { {"+", "tool_names", "last_read_at"} };
static const sh_edit SH_S27[] = { {"+", "transport_profile", "profile_name"} };
static const sh_edit SH_S28[] = { {"+", "compression_overload_streak", "compression_recovery_deadline"} };
static const sh_edit SH_S29[] = { {"+", "created_source", "source"} };
static const sh_edit SH_S30[] = { {"+", "auto_archived", "archived"} };
static const sh_edit SH_S31[] = {
    {"+", "runtime_revision", "id"},
    {"+", "runtime_generation", "runtime_revision"}
};
static const sh_edit SH_S32[] = { {"+", "transcript_revision", "tool_names"} };
static const sh_edit SH_S33[] = { {"+", "prompt_cache_clock", "transcript_revision"} };

static const sh_event SH_SESSIONS_EVENTS[] = {
    {"01 2026-03-08T23:32Z c5e8166c8b", SH_S01, 1},
    {"02 2026-03-17T10:44Z d417ba2a48", SH_S02, 11},
    {"03 2026-04-22T12:51Z 5fb143169b", SH_S03, 1},
    {"04 2026-05-10T20:06Z 878611a79d", SH_S04, 2},
    {"05 2026-05-10T20:06Z 00ce5f04d9", SH_S05, 3},
    {"06 2026-05-31T22:46Z 51c68d4ab1", SH_S06, 1},
    {"07 2026-06-01T08:22Z 3e59be0c41", SH_S07, 1},
    {"08 2026-06-02T01:41Z 85b65e29f0", SH_S08, 1},
    {"09 2026-06-25T22:49Z ffa3d3c811", SH_S09, 2},
    {"10 2026-06-28T22:10Z 86e64900b9", SH_S10, 4},
    {"11 2026-06-30T08:06Z f2ccb2859f", SH_S11, 2},
    {"12 2026-07-05T21:01Z 747386ecfa", SH_S12, 3},
    {"13 2026-07-13T20:49Z af7dceaf77", SH_S13, 1},
    {"14 2026-07-15T16:50Z e8b7ce8c19", SH_S14, 1},
    {"15 2026-07-23T15:08Z ec5835ab8b", SH_S15, 1},
    {"16 2026-07-24T16:15Z 951d606730", SH_S16, 1},
    {"17 2026-07-27T19:14Z cfb206fe2e", SH_S17, 3},
    {"18 2026-07-27T19:22Z 5646fed97e", SH_S18, 3},
    {"19 2026-08-02T23:16Z c2088efe9e", SH_S19, 3},
    {"20 2026-08-03T15:07Z 7d066c3c56", SH_S20, 1},
    {"21 2026-08-04T19:20Z d98287fe3c", SH_S21, 1},
    {"22 2026-08-08T22:17Z fe42097865", SH_S22, 1},
    {"23 2026-08-15T07:31Z fbaea9bddc", SH_S23, 1},
    {"24 2026-08-15T07:33Z e89532d97e", SH_S24, 1},
    {"25 2026-09-02T11:14Z 238b6c1ab9", SH_S25, 1},
    {"26 2026-09-02T14:22Z 8e4366d358", SH_S26, 1},
    {"27 2026-09-19T00:10Z 922a0c3c87", SH_S27, 1},
    {"28 2026-09-25T23:25Z 2941aadffa", SH_S28, 1},
    {"29 2026-09-27T00:29Z d75f29934b", SH_S29, 1},
    {"30 2026-09-28T00:00Z #117713", SH_S30, 1},
    {"31 2026-09-08T18:45Z 0695b12b4c", SH_S31, 2},
    {"32 2026-10-18T10:11Z 1fc1999769", SH_S32, 1},
    {"33 2026-11-15T12:00Z feat/prompt-cache-clock", SH_S33, 1}
};

static const char *const SH_MESSAGES_BASE[] = {
    "id", "session_id", "role", "content", "tool_call_id",
    "tool_calls", "tool_name", "timestamp", "token_count"
};

static const sh_edit SH_M01[] = { {"+", "finish_reason", "token_count"} };
static const sh_edit SH_M02[] = {
    {"+", "reasoning", "finish_reason"},
    {"+", "reasoning_details", "reasoning"},
    {"+", "codex_reasoning_items", "reasoning_details"}
};
static const sh_edit SH_M03[] = { {"+", "reasoning_content", "reasoning"} };
static const sh_edit SH_M04[] = { {"+", "codex_message_items", "codex_reasoning_items"} };
static const sh_edit SH_M05[] = { {"+", "platform_message_id", "codex_message_items"} };
static const sh_edit SH_M06[] = { {"+", "observed", "platform_message_id"} };
static const sh_edit SH_M07[] = { {"+", "active", "observed"} };
static const sh_edit SH_M08[] = { {"+", "anthropic_content_blocks", "reasoning_details"} };
static const sh_edit SH_M09[] = { {"-", "anthropic_content_blocks", NULL} };
static const sh_edit SH_M10[] = { {"+", "compacted", "active"} };
static const sh_edit SH_M11[] = { {"+", "effect_disposition", "tool_name"} };
static const sh_edit SH_M12[] = { {"+", "api_content", "compacted"} };
static const sh_edit SH_M13[] = {
    {"+", "display_kind", "api_content"},
    {"+", "display_metadata", "display_kind"}
};
static const sh_edit SH_M14[] = { {"+", "_compressed_summary", "observed"} };
static const sh_edit SH_M15[] = {
    {"+", "display_identity", "display_metadata"},
    {"+", "display_order", "display_identity"}
};
static const sh_edit SH_M16[] = {
    {"+", "message_uid", "display_order"},
    {"+", "absorbed_message_uids", "message_uid"},
    {"+", "tool_call_uids", "absorbed_message_uids"},
    {"+", "tool_call_uid", "tool_call_uids"}
};

static const sh_event SH_MESSAGES_EVENTS[] = {
    {"01 2026-02-21T08:05Z b33ed9176f", SH_M01, 1},
    {"02 2026-03-25T16:47Z 42fec19151", SH_M02, 3},
    {"03 2026-04-22T11:31Z a7d78d3bfd", SH_M03, 1},
    {"04 2026-04-26T01:22Z 81e01f6ee9", SH_M04, 1},
    {"05 2026-05-20T20:00Z 31a0100104", SH_M05, 1},
    {"06 2026-05-23T08:33Z 4a91e36495", SH_M06, 1},
    {"07 2026-06-01T08:22Z 3e59be0c41", SH_M07, 1},
    {"08 2026-06-11T03:45Z aaccaada28", SH_M08, 1},
    {"09 2026-06-11T03:45Z efcbbde48c", SH_M09, 1},
    {"10 2026-06-20T17:57Z 854d75723f", SH_M10, 1},
    {"11 2026-07-11T12:41Z a0a6cd80f5", SH_M11, 1},
    {"12 2026-07-19T02:55Z 7b3dcee928", SH_M12, 1},
    {"13 2026-07-23T18:46Z a4bc1ca502", SH_M13, 2},
    {"14 2026-08-25T10:55Z 1104ffe0b9", SH_M14, 1},
    {"15 2026-09-09T17:05Z 1c6683e8e0", SH_M15, 2},
    {"16 2026-09-28T08:42Z eed37d63ce", SH_M16, 4}
};

static const char *const SH_SMU_BASE[] = {
    "session_id", "model", "billing_provider", "billing_base_url",
    "api_call_count", "input_tokens", "output_tokens",
    "cache_read_tokens", "cache_write_tokens", "reasoning_tokens",
    "estimated_cost_usd", "first_seen", "last_seen"
};

static const sh_edit SH_U01[] = {
    {"+", "billing_mode", "billing_base_url"},
    {"+", "actual_cost_usd", "estimated_cost_usd"},
    {"+", "cost_status", "actual_cost_usd"},
    {"+", "cost_source", "cost_status"}
};
static const sh_edit SH_U02[] = { {"+", "task", "billing_mode"} };

static const sh_event SH_SMU_EVENTS[] = {
    {"01 2026-07-11T12:59Z 0d63c23f36", SH_U01, 4},
    {"02 2026-07-16T11:23Z eb6aa03609", SH_U02, 1}
};

static const sh_table SH_TABLES[] = {
    {"sessions", SH_SESSIONS_BASE, sizeof(SH_SESSIONS_BASE) / sizeof(SH_SESSIONS_BASE[0]),
     SH_SESSIONS_EVENTS, sizeof(SH_SESSIONS_EVENTS) / sizeof(SH_SESSIONS_EVENTS[0])},
    {"messages", SH_MESSAGES_BASE, sizeof(SH_MESSAGES_BASE) / sizeof(SH_MESSAGES_BASE[0]),
     SH_MESSAGES_EVENTS, sizeof(SH_MESSAGES_EVENTS) / sizeof(SH_MESSAGES_EVENTS[0])},
    {"session_model_usage", SH_SMU_BASE, sizeof(SH_SMU_BASE) / sizeof(SH_SMU_BASE[0]),
     SH_SMU_EVENTS, sizeof(SH_SMU_EVENTS) / sizeof(SH_SMU_EVENTS[0])}
};

static const sh_table *sh_find(const char *table) {
    size_t i;
    if (!table) return NULL;
    for (i = 0; i < sizeof(SH_TABLES) / sizeof(SH_TABLES[0]); i++)
        if (strcmp(SH_TABLES[i].name, table) == 0) return &SH_TABLES[i];
    return NULL;
}

/* ---------------- growable string list (a Python list[str] of columns) --------------- */

typedef struct { char **v; size_t n; size_t cap; } sh_list;

static int sh_list_push(sh_list *l, const char *s) {
    char *copy;
    if (l->n == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 16;
        char **nv = (char **)realloc(l->v, cap * sizeof(char *));
        if (!nv) return 0;
        l->v = nv;
        l->cap = cap;
    }
    copy = sh_dup(s);
    if (!copy) return 0;
    l->v[l->n++] = copy;
    return 1;
}

static void sh_list_free(sh_list *l) {
    size_t i;
    for (i = 0; i < l->n; i++) free(l->v[i]);
    free(l->v);
    l->v = NULL;
    l->n = l->cap = 0;
}

static int sh_list_index(const sh_list *l, const char *name) {
    size_t i;
    for (i = 0; i < l->n; i++)
        if (strcmp(l->v[i], name) == 0) return (int)i;
    return -1;
}

static int sh_list_remove(sh_list *l, const char *name) {
    int at = sh_list_index(l, name);
    size_t i;
    if (at < 0) return 0;
    free(l->v[at]);
    for (i = (size_t)at; i + 1 < l->n; i++) l->v[i] = l->v[i + 1];
    l->n--;
    return 1;
}

static int sh_list_insert_at(sh_list *l, size_t at, const char *name) {
    size_t i;
    char *copy;
    if (at > l->n) at = l->n;
    if (l->n == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 16;
        char **nv = (char **)realloc(l->v, cap * sizeof(char *));
        if (!nv) return 0;
        l->v = nv;
        l->cap = cap;
    }
    copy = sh_dup(name);
    if (!copy) return 0;
    for (i = l->n; i > at; i--) l->v[i] = l->v[i - 1];
    l->v[at] = copy;
    l->n++;
    return 1;
}

static json_t *sh_list_to_json(const sh_list *l) {
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < l->n; i++) json_append(arr, json_string(l->v[i]));
    return arr;
}

/* _apply: mutates `columns` (a list) exactly like the Python helper. */
static void sh_apply_list(sh_list *columns, json_t *edits) {
    size_t i, n;
    if (!edits || edits->type != JSON_ARRAY) return;
    n = json_len(edits);
    for (i = 0; i < n; i++) {
        json_t *edit = json_get(edits, i);
        json_t *op_node, *col_node, *after_node;
        const char *op, *col;
        if (!edit || edit->type != JSON_ARRAY) continue;
        op_node = json_get(edit, 0);
        col_node = json_get(edit, 1);
        after_node = json_get(edit, 2);
        op = (op_node && op_node->type == JSON_STRING) ? op_node->str_val : NULL;
        col = (col_node && col_node->type == JSON_STRING) ? col_node->str_val : NULL;
        if (!op || !col) continue;
        if (op[0] == '-' && op[1] == '\0') {
            sh_list_remove(columns, col);
        } else if (!after_node || after_node->type == JSON_NULL) {
            sh_list_insert_at(columns, 0, col);
        } else if (after_node->type == JSON_STRING) {
            int at = sh_list_index(columns, after_node->str_val);
            if (at >= 0) sh_list_insert_at(columns, (size_t)at + 1, col);
        }
    }
}

json_t *w19_schemahist_ddcbcb_apply(json_t *columns, json_t *edits) {
    sh_list l = {NULL, 0, 0};
    size_t i;
    json_t *out;
    if (columns && columns->type == JSON_ARRAY)
        for (i = 0; i < json_len(columns); i++) {
            json_t *c = json_get(columns, i);
            if (c && c->type == JSON_STRING) sh_list_push(&l, c->str_val);
        }
    sh_apply_list(&l, edits);
    out = sh_list_to_json(&l);
    sh_list_free(&l);
    return out;
}

/* --------------- declared snapshots --------------- */

typedef struct { sh_list *v; size_t n; size_t cap; } sh_snaps;

static sh_list *sh_snaps_add(sh_snaps *s) {
    if (s->n == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 16;
        sh_list *nv = (sh_list *)realloc(s->v, cap * sizeof(sh_list));
        if (!nv) return NULL;
        s->v = nv;
        s->cap = cap;
    }
    memset(&s->v[s->n], 0, sizeof(sh_list));
    return &s->v[s->n++];
}

static void sh_snaps_free(sh_snaps *s) {
    size_t i;
    for (i = 0; i < s->n; i++) sh_list_free(&s->v[i]);
    free(s->v);
    s->v = NULL;
    s->n = s->cap = 0;
}

/* declared_snapshots(): oldest declared order first, one entry per shipped event. */
static int sh_build_snapshots(const sh_table *t, sh_snaps *out) {
    sh_list columns = {NULL, 0, 0};
    size_t i, j;
    sh_list *snap;
    for (i = 0; i < t->nbase; i++) sh_list_push(&columns, t->base[i]);
    snap = sh_snaps_add(out);
    if (!snap) { sh_list_free(&columns); return 0; }
    for (i = 0; i < columns.n; i++) sh_list_push(snap, columns.v[i]);
    for (i = 0; i < t->nevents; i++) {
        const sh_event *ev = &t->events[i];
        for (j = 0; j < ev->nedits; j++) {
            const sh_edit *e = &ev->edits[j];
            if (e->op[0] == '-' && e->op[1] == '\0') {
                sh_list_remove(&columns, e->col);
                continue;
            }
            if (!e->after) {
                sh_list_insert_at(&columns, 0, e->col);
            } else {
                int at = sh_list_index(&columns, e->after);
                if (at >= 0) sh_list_insert_at(&columns, (size_t)at + 1, e->col);
            }
        }
        snap = sh_snaps_add(out);
        if (!snap) { sh_list_free(&columns); return 0; }
        for (j = 0; j < columns.n; j++) sh_list_push(snap, columns.v[j]);
    }
    sh_list_free(&columns);
    return 1;
}

json_t *w19_schemahist_ddcbcb_declared_snapshots(const char *table) {
    const sh_table *t = sh_find(table);
    sh_snaps snaps = {NULL, 0, 0};
    json_t *out = json_array();
    size_t i, j;
    if (!t) return out;
    if (!sh_build_snapshots(t, &snaps)) { sh_snaps_free(&snaps); return out; }
    for (i = 0; i < snaps.n; i++) {
        json_t *row = json_array();
        for (j = 0; j < snaps.v[i].n; j++) json_append(row, json_string(snaps.v[i].v[j]));
        json_append(out, row);
    }
    sh_snaps_free(&snaps);
    return out;
}

json_t *w19_schemahist_ddcbcb_current_declared_columns(const char *table) {
    const sh_table *t = sh_find(table);
    sh_snaps snaps = {NULL, 0, 0};
    json_t *out;
    if (!t) return json_array();
    if (!sh_build_snapshots(t, &snaps) || snaps.n == 0) { sh_snaps_free(&snaps); return json_array(); }
    out = sh_list_to_json(&snaps.v[snaps.n - 1]);
    sh_snaps_free(&snaps);
    return out;
}

/* --------------- reachable physical layouts --------------- */

typedef struct {
    int has_max_len;
    long long max_len;
    int has_first_new;
    long long first_new;
    json_t *require;   /* borrowed */
} sh_accept;

static int sh_layout_has(const sh_list *l, const char *name) { return sh_list_index(l, name) >= 0; }

static int sh_accept_ok(const sh_accept *a, const sh_list *layout, size_t first_new) {
    size_t i;
    if (!a) return 1;
    if (a->has_max_len && (long long)layout->n > a->max_len) return 0;
    if (a->has_first_new && (long long)first_new > a->first_new) return 0;
    if (a->require && a->require->type == JSON_ARRAY) {
        for (i = 0; i < json_len(a->require); i++) {
            json_t *c = json_get(a->require, i);
            if (c && c->type == JSON_STRING && !sh_layout_has(layout, c->str_val)) return 0;
        }
    }
    return 1;
}

static char *sh_layout_key(const sh_list *l) {
    size_t i, total = 1;
    char *key, *p;
    for (i = 0; i < l->n; i++) total += strlen(l->v[i]) + 1;
    key = (char *)malloc(total);
    if (!key) return NULL;
    p = key;
    for (i = 0; i < l->n; i++) {
        size_t n = strlen(l->v[i]);
        memcpy(p, l->v[i], n);
        p += n;
        *p++ = '\x1f';
    }
    *p = '\0';
    return key;
}

typedef struct { char *key; long long index; } sh_seen;
typedef struct { sh_seen *v; size_t n, cap; } sh_seen_set;

static int sh_seen_find(sh_seen_set *s, const char *key) {
    size_t i;
    for (i = 0; i < s->n; i++)
        if (strcmp(s->v[i].key, key) == 0) return (int)i;
    return -1;
}

static void sh_seen_put(sh_seen_set *s, const char *key, long long index) {
    int at = sh_seen_find(s, key);
    if (at >= 0) { s->v[at].index = index; return; }
    if (s->n == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 64;
        sh_seen *nv = (sh_seen *)realloc(s->v, cap * sizeof(sh_seen));
        if (!nv) return;
        s->v = nv;
        s->cap = cap;
    }
    s->v[s->n].key = sh_dup(key);
    s->v[s->n].index = index;
    s->n++;
}

static void sh_seen_free(sh_seen_set *s) {
    size_t i;
    for (i = 0; i < s->n; i++) free(s->v[i].key);
    free(s->v);
    s->v = NULL;
    s->n = s->cap = 0;
}

typedef struct { sh_list layout; long long index; } sh_front;

typedef struct { sh_front *v; size_t n, cap; } sh_frontier;

static sh_front *sh_frontier_add(sh_frontier *f) {
    if (f->n == f->cap) {
        size_t cap = f->cap ? f->cap * 2 : 32;
        sh_front *nv = (sh_front *)realloc(f->v, cap * sizeof(sh_front));
        if (!nv) return NULL;
        f->v = nv;
        f->cap = cap;
    }
    memset(&f->v[f->n], 0, sizeof(sh_front));
    return &f->v[f->n++];
}

static void sh_frontier_free(sh_frontier *f) {
    size_t i;
    for (i = 0; i < f->n; i++) sh_list_free(&f->v[i].layout);
    free(f->v);
    f->v = NULL;
    f->n = f->cap = 0;
}

json_t *w19_schemahist_ddcbcb_reachable_physical_layouts(const char *table, json_t *accept, double limit) {
    const sh_table *t = sh_find(table);
    sh_snaps snaps = {NULL, 0, 0};
    sh_seen_set seen = {NULL, 0, 0};
    sh_frontier frontier = {NULL, 0, 0};
    sh_accept acc;
    json_t *out = json_object();
    json_t *layouts = json_array();
    size_t yielded = 0;
    size_t cap = (limit > 0) ? (size_t)limit : 1000;
    int truncated = 0;
    size_t i, j, k;

    memset(&acc, 0, sizeof(acc));
    if (accept && accept->type == JSON_OBJECT) {
        json_t *ml = json_obj_get(accept, "max_len");
        json_t *fn = json_obj_get(accept, "max_first_new");
        if (ml && ml->type == JSON_NUMBER) { acc.has_max_len = 1; acc.max_len = (long long)ml->num_val; }
        if (fn && fn->type == JSON_NUMBER) { acc.has_first_new = 1; acc.first_new = (long long)fn->num_val; }
        acc.require = json_obj_get(accept, "require");
    }

    if (!t || !sh_build_snapshots(t, &snaps)) {
        json_set(out, "layouts", layouts);
        json_set(out, "count", json_number(0));
        json_set(out, "truncated", json_bool(0));
        sh_snaps_free(&snaps);
        return out;
    }

    /* pass 1: every declared snapshot, oldest first */
    for (i = 0; i < snaps.n && !truncated; i++) {
        char *key;
        sh_front *slot;
        if (snaps.v[i].n == 0) continue;
        key = sh_layout_key(&snaps.v[i]);
        if (!key) continue;
        if (sh_seen_find(&seen, key) >= 0) { free(key); continue; }
        if (!sh_accept_ok(&acc, &snaps.v[i], 0)) { free(key); continue; }
        sh_seen_put(&seen, key, (long long)i);
        free(key);
        slot = sh_frontier_add(&frontier);
        if (!slot) continue;
        for (j = 0; j < snaps.v[i].n; j++) sh_list_push(&slot->layout, snaps.v[i].v[j]);
        slot->index = (long long)i;
        if (yielded >= cap) { truncated = 1; break; }
        json_append(layouts, sh_list_to_json(&slot->layout));
        yielded++;
    }

    /* BFS: extend each layout with the columns later snapshots added and it lacks */
    while (frontier.n > 0 && !truncated) {
        sh_frontier next = {NULL, 0, 0};
        for (i = 0; i < frontier.n && !truncated; i++) {
            sh_list *layout = &frontier.v[i].layout;
            for (j = (size_t)frontier.v[i].index + 1; j < snaps.n; j++) {
                sh_list added_cols = {NULL, 0, 0};
                sh_list candidate = {NULL, 0, 0};
                char *key;
                int seen_at;
                sh_front *slot;
                for (k = 0; k < snaps.v[j].n; k++)
                    if (!sh_layout_has(layout, snaps.v[j].v[k]))
                        sh_list_push(&added_cols, snaps.v[j].v[k]);
                if (added_cols.n == 0) { sh_list_free(&added_cols); continue; }
                /* candidate = layout + added */
                for (k = 0; k < layout->n; k++) sh_list_push(&candidate, layout->v[k]);
                for (k = 0; k < added_cols.n; k++) sh_list_push(&candidate, added_cols.v[k]);
                sh_list_free(&added_cols);
                key = sh_layout_key(&candidate);
                if (!key) { sh_list_free(&candidate); continue; }
                seen_at = sh_seen_find(&seen, key);
                if (seen_at >= 0 && seen.v[seen_at].index <= (long long)j) {
                    free(key);
                    sh_list_free(&candidate);
                    continue;
                }
                if (!sh_accept_ok(&acc, &candidate, layout->n)) {
                    free(key);
                    sh_list_free(&candidate);
                    continue;
                }
                sh_seen_put(&seen, key, (long long)j);
                free(key);
                slot = sh_frontier_add(&next);
                if (!slot) { sh_list_free(&candidate); continue; }
                slot->layout = candidate;
                slot->index = (long long)j;
                if (yielded >= cap) { truncated = 1; break; }
                json_append(layouts, sh_list_to_json(&candidate));
                yielded++;
            }
        }
        sh_frontier_free(&frontier);
        frontier = next;
    }

    json_set(out, "layouts", layouts);
    json_set(out, "count", json_number((double)yielded));
    json_set(out, "truncated", json_bool(truncated));
    sh_frontier_free(&frontier);
    sh_seen_free(&seen);
    sh_snaps_free(&snaps);
    return out;
}

json_t *w19_schemahist_ddcbcb_schema_history(const char *table) {
    const sh_table *t = sh_find(table);
    json_t *out = json_object();
    json_t *base = json_array();
    json_t *events = json_array();
    size_t i, j;
    if (!t) return json_null();
    for (i = 0; i < t->nbase; i++) json_append(base, json_string(t->base[i]));
    for (i = 0; i < t->nevents; i++) {
        json_t *row = json_array();
        json_t *edits = json_array();
        json_append(row, json_string(t->events[i].label));
        for (j = 0; j < t->events[i].nedits; j++) {
            json_t *edit = json_array();
            json_append(edit, json_string(t->events[i].edits[j].op));
            json_append(edit, json_string(t->events[i].edits[j].col));
            if (t->events[i].edits[j].after)
                json_append(edit, json_string(t->events[i].edits[j].after));
            else
                json_append(edit, json_null());
            json_append(edits, edit);
        }
        json_append(row, edits);
        json_append(events, row);
    }
    json_set(out, "base", base);
    json_set(out, "events", events);
    return out;
}

json_t *w19_schemahist_ddcbcb_tables(void) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < sizeof(SH_TABLES) / sizeof(SH_TABLES[0]); i++)
        json_append(out, json_string(SH_TABLES[i].name));
    return out;
}
