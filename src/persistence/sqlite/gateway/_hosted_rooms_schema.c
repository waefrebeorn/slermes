/*
 * _hosted_rooms_schema.c — C11 port of upstream Hermes persistence/sqlite/gateway/_hosted_rooms_schema.py
 * (gate w26_hostedroom_598018).
 * STATUS: PORTED — behavior verified by tests/oracle_w26_hostedroom_598018.py.
 * PoP: w26_hostedroom_598018_parse_ddl @ persistence/sqlite/gateway/_hosted_rooms_schema.py:_REQUIRED_COLUMNS
 * PoP: w26_hostedroom_598018_required_columns @ persistence/sqlite/gateway/_hosted_rooms_schema.py:_REQUIRED_COLUMNS
 * PoP: w26_hostedroom_598018_retire_sql @ persistence/sqlite/gateway/_hosted_rooms_schema.py:_RETIRE_FROM_ROOMS
 * PoP: w26_hostedroom_598018_event_bytes_backfill @ persistence/sqlite/gateway/_hosted_rooms_schema.py:_EVENT_BYTES_BACKFILL
 * PoP: w26_hostedroom_598018_legacy_columns @ persistence/sqlite/gateway/_hosted_rooms_schema.py:_LEGACY_COLUMNS
 * PoP: w26_hostedroom_598018_remote_run_current @ persistence/sqlite/gateway/_hosted_rooms_schema.py:_remote_run_schema_current
 *
 * Seams: every sqlite3.Connection (PRAGMA table_info, sqlite_master probes,
 * DDL/DML execution) is replaced by its rows.  What is ported is what the step
 * derives from values: the table/column regex parse of a DDL statement, the two
 * templated statements, the legacy column table and the primary-key receipt
 * check that fences legacy remote-run rows.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_hostedroom_598018.h"

typedef struct {
    const char *table;
    const char *const *cols;
    size_t n;
} hr_table;

static const char *const HR_ROOMS[] = {
    "authority_epoch", "authority_gateway_id", "created_at", "disbanded_at", "event_bytes",
    "members_json", "name", "next_seq", "revision", "room_id", "updated_at"};
static const char *const HR_EVENTS[] = {
    "actor_json", "authority_epoch", "created_at", "event_id", "kind", "payload_json",
    "room_id", "seq"};
static const char *const HR_RETIRED[] = {"retired_at", "room_id"};
static const char *const HR_LINKS[] = {
    "cancellation_scope_id", "catalog_json", "grant", "member_id", "room_id", "status",
    "target_profile", "target_url", "trace_id", "transport_security", "updated_at"};
static const char *const HR_REMOTE[] = {
    "authority_epoch", "authority_gateway_id", "created_at", "execution_generation",
    "home_install_id", "member_id", "room_id", "run_id", "session_id", "target_install_id",
    "target_profile", "task_id", "updated_at"};
static const char *const HR_REVOKED[] = {"expires_at", "revoked_before", "scope_key"};
static const char *const HR_RESERVATIONS[] = {
    "authority_epoch", "authority_gateway_id", "created_at", "expires_at", "member_id",
    "revoked_at", "room_id", "target_profile", "updated_at"};
static const char *const HR_OWED[] = {
    "attempts", "created_at", "expires_at", "grant", "grant_digest", "held", "member_id",
    "next_attempt_at", "room_id", "target_url"};

/* _REQUIRED_COLUMNS, in the order _schema_is_current probes them. */
static const hr_table HR_TABLES[8] = {
    {"hosted_rooms", HR_ROOMS, sizeof(HR_ROOMS) / sizeof(HR_ROOMS[0])},
    {"hosted_room_events", HR_EVENTS, sizeof(HR_EVENTS) / sizeof(HR_EVENTS[0])},
    {"hosted_room_retired_ids", HR_RETIRED, sizeof(HR_RETIRED) / sizeof(HR_RETIRED[0])},
    {"hosted_room_links", HR_LINKS, sizeof(HR_LINKS) / sizeof(HR_LINKS[0])},
    {"hosted_room_remote_runs", HR_REMOTE, sizeof(HR_REMOTE) / sizeof(HR_REMOTE[0])},
    {"hosted_room_revoked_grants", HR_REVOKED, sizeof(HR_REVOKED) / sizeof(HR_REVOKED[0])},
    {"hosted_room_peer_reservations", HR_RESERVATIONS, sizeof(HR_RESERVATIONS) / sizeof(HR_RESERVATIONS[0])},
    {"hosted_room_owed_revokes", HR_OWED, sizeof(HR_OWED) / sizeof(HR_OWED[0])},
};

/* _REMOTE_RUN_IDENTITY_COLUMNS: the receipt key in primary-key order. */
static const char *const HR_REMOTE_PK[] = {
    "room_id", "home_install_id", "authority_gateway_id", "authority_epoch", "member_id",
    "target_install_id", "target_profile", "task_id", "execution_generation"};

#define HR_REMOTE_PK_N (sizeof(HR_REMOTE_PK) / sizeof(HR_REMOTE_PK[0]))

static int hr_word(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

static int hr_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* r"^\s*(\w+) (?:TEXT|INTEGER|REAL)\b" over ddl.split("(", 1)[1], MULTILINE.
 * findall resumes after each match, exactly as re does. */
static json_t *hr_columns_of(const char *ddl) {
    json_t *cols = json_array();
    const char *body = strchr(ddl, '(');
    size_t len, i = 0;
    if (!body) return cols;
    body++;
    len = strlen(body);
    while (i < len) {
        if (i == 0 || body[i - 1] == '\n') {
            size_t j = i, w, t, te;
            size_t tlen = 0;
            while (j < len && hr_space((unsigned char)body[j])) j++;
            w = j;
            while (w < len && hr_word((unsigned char)body[w])) w++;
            if (w > j && w < len && body[w] == ' ') {
                t = w + 1;
                if (!strncmp(body + t, "TEXT", 4)) tlen = 4;
                else if (!strncmp(body + t, "INTEGER", 7)) tlen = 7;
                else if (!strncmp(body + t, "REAL", 4)) tlen = 4;
                te = t + tlen;
                if (tlen && (te >= len || !hr_word((unsigned char)body[te]))) {
                    char *name = (char *)malloc(w - j + 1);
                    if (name) {
                        memcpy(name, body + j, w - j);
                        name[w - j] = '\0';
                        json_append(cols, json_string(name));
                        free(name);
                    }
                    i = te;
                    continue;
                }
            }
        }
        i++;
    }
    return cols;
}

json_t *w26_hostedroom_598018_parse_ddl(const char *ddl) {
    json_t *out, *cols;
    const char *p, *start;
    char *table;
    size_t n;

    if (!ddl) {
        out = json_object();
        json_set(out, "error", json_string("AttributeError"));
        return out;
    }
    p = strstr(ddl, "EXISTS ");
    if (!p) {
        out = json_object();
        json_set(out, "error", json_string("AttributeError"));
        return out;
    }
    p += 7;
    start = p;
    while (*p && hr_word((unsigned char)*p)) p++;
    n = (size_t)(p - start);
    if (!n) {
        out = json_object();
        json_set(out, "error", json_string("AttributeError"));
        return out;
    }
    table = (char *)malloc(n + 1);
    if (!table) return json_null();
    memcpy(table, start, n);
    table[n] = '\0';
    if (!strchr(ddl, '(')) {
        free(table);
        out = json_object();
        json_set(out, "error", json_string("IndexError"));
        return out;
    }
    cols = hr_columns_of(ddl);
    out = json_object();
    json_set(out, "table", json_string(table));
    json_set(out, "columns", cols);
    free(table);
    return out;
}

json_t *w26_hostedroom_598018_required_columns(void) {
    json_t *arr = json_array();
    size_t i, j;
    for (i = 0; i < sizeof(HR_TABLES) / sizeof(HR_TABLES[0]); i++) {
        json_t *row = json_object(), *cols = json_array();
        for (j = 0; j < HR_TABLES[i].n; j++)
            json_append(cols, json_string(HR_TABLES[i].cols[j]));
        json_set(row, "table", json_string(HR_TABLES[i].table));
        json_set(row, "columns", cols);
        json_append(arr, row);
    }
    return arr;
}

static json_t *hr_format(const char *tmpl, const char *where) {
    size_t need = strlen(tmpl) + strlen(where ? where : "") + 1;
    char *buf = (char *)malloc(need);
    json_t *out;
    const char *brace;
    if (!buf) return json_null();
    brace = strstr(tmpl, "{where}");
    if (!brace) {
        snprintf(buf, need, "%s", tmpl);
    } else {
        size_t head = (size_t)(brace - tmpl);
        memcpy(buf, tmpl, head);
        buf[head] = '\0';
        strcat(buf, where ? where : "");
        strcat(buf, brace + 7);
    }
    out = json_string(buf);
    free(buf);
    return out;
}

json_t *w26_hostedroom_598018_retire_sql(const char *where) {
    static const char RETIRE[] =
        "INSERT OR IGNORE INTO hosted_room_retired_ids (room_id, retired_at)"
        " SELECT room_id, disbanded_at FROM hosted_rooms WHERE {where}";
    return hr_format(RETIRE, where);
}

json_t *w26_hostedroom_598018_event_bytes_backfill(const char *where) {
    static const char BACKFILL[] =
        "UPDATE hosted_rooms\n"
        "                  SET event_bytes=COALESCE((\n"
        "                      SELECT SUM(\n"
        "                          length(CAST(event_id AS BLOB)) +\n"
        "                          length(CAST(kind AS BLOB)) +\n"
        "                          length(CAST(actor_json AS BLOB)) +\n"
        "                          length(CAST(payload_json AS BLOB))\n"
        "                      )\n"
        "                      FROM hosted_room_events\n"
        "                      WHERE hosted_room_events.room_id=hosted_rooms.room_id\n"
        "                  ), 0) WHERE {where}";
    return hr_format(BACKFILL, where);
}

json_t *w26_hostedroom_598018_legacy_columns(void) {
    static const char *const ROWS[5][3] = {
        {"hosted_rooms", "authority_gateway_id", "TEXT NOT NULL"},
        {"hosted_rooms", "authority_epoch", "INTEGER NOT NULL"},
        {"hosted_rooms", "event_bytes", "INTEGER NOT NULL"},
        {"hosted_room_events", "actor_json", "TEXT NOT NULL"},
        {"hosted_room_events", "authority_epoch", "INTEGER"},
    };
    static const char *const DEFAULTS[5] = {
        "'legacy'", "1", "0", "'{\"id\":\"legacy\",\"kind\":\"system\"}'", NULL};
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < 5; i++) {
        json_t *row = json_object();
        json_set(row, "table", json_string(ROWS[i][0]));
        json_set(row, "column", json_string(ROWS[i][1]));
        json_set(row, "declaration", json_string(ROWS[i][2]));
        json_set(row, "default", DEFAULTS[i] ? json_string(DEFAULTS[i]) : json_null());
        json_append(arr, row);
    }
    return arr;
}

typedef struct {
    long long pk;
    const char *name;
} hr_pk;

static int hr_pk_cmp(const void *pa, const void *pb) {
    const hr_pk *a = (const hr_pk *)pa;
    const hr_pk *b = (const hr_pk *)pb;
    if (a->pk < b->pk) return -1;
    return a->pk > b->pk ? 1 : 0;
}

json_t *w26_hostedroom_598018_remote_run_current(const json_t *columns,
                                                const json_t *pk_rows) {
    size_t i, j, n = 0, cnt;
    hr_pk rows[64];

    /* Every remote-run column must be present. */
    for (i = 0; i < sizeof(HR_REMOTE) / sizeof(HR_REMOTE[0]); i++) {
        int found = 0;
        for (j = 0; j < json_len(columns); j++) {
            const json_t *c = json_get(columns, j);
            if (c && c->type == JSON_STRING && c->str_val && !strcmp(c->str_val, HR_REMOTE[i])) {
                found = 1;
                break;
            }
        }
        if (!found) return json_bool(0);
    }

    cnt = json_len(pk_rows);
    for (i = 0; i < cnt && n < 64; i++) {
        const json_t *row = json_get(pk_rows, i);
        const json_t *pk, *name;
        if (!row || row->type != JSON_ARRAY || json_len(row) < 6) continue;
        pk = json_get(row, 5);
        name = json_get(row, 1);
        if (!pk || pk->type != JSON_NUMBER || pk->num_val == 0.0) continue;
        rows[n].pk = (long long)pk->num_val;
        rows[n].name = (name && name->type == JSON_STRING) ? name->str_val : "";
        n++;
    }
    if (n != HR_REMOTE_PK_N) return json_bool(0);
    qsort(rows, n, sizeof(hr_pk), hr_pk_cmp);
    for (i = 0; i < n; i++)
        if (strcmp(rows[i].name, HR_REMOTE_PK[i])) return json_bool(0);
    return json_bool(1);
}
