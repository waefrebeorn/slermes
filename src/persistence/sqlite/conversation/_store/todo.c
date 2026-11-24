/*
 * todo.c -- real C11 port of the resume-display decisions of upstream Hermes
 *   persistence/sqlite/conversation/_store/todo.py.
 *
 * PoP: w21_todo_138ed8_is_todo_call @ persistence/sqlite/conversation/_store/todo.py:is_todo_call
 * PoP: w21_todo_138ed8_paired @ persistence/sqlite/conversation/_store/todo.py:get_latest_todo_result
 * PoP: w21_todo_138ed8_valid_result @ persistence/sqlite/conversation/_store/todo.py:get_latest_todo_result
 * PoP: w21_todo_138ed8_copy_ids @ persistence/sqlite/conversation/_store/todo.py:_todo_copies
 * PoP: w21_todo_138ed8_removed_map @ persistence/sqlite/conversation/_store/todo.py:_removed_todo_copies
 * PoP: w21_todo_138ed8_copy_removed @ persistence/sqlite/conversation/_store/todo.py:_todo_copy_removed
 *
 * The paged sqlite walk itself (the holder, the `INDEXED BY` statements, the
 * `_display_dedupe_key` hash) is the DB seam and stays upstream; every branch of
 * that walk is a pure decision and lives here.
 */
#include "hermes_json.h"
#include "noc_w21_todo_138ed8.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int td_is_name(const json_t *v, const char *name) {
    return v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, name);
}

static int td_is_todo_name(const json_t *v) {
    return td_is_name(v, "todo_list") || td_is_name(v, "todo");
}

/*
 * Python `int(x or 0)` for the `revision` field: falsy -> 0; a bool/number is
 * an int (float truncates); a string must parse as an optionally signed decimal
 * with Python's single-underscore-between-digits rule; anything else raises
 * TypeError. Returns 0 on success, -1 on the raise.
 */
static int td_int_or_zero(const json_t *v) {
    const char *s;
    int digits = 0;

    if (!v || v->type == JSON_NULL) return 0;                 /* None or 0 */
    switch (v->type) {
    case JSON_BOOL:
        return 0;
    case JSON_NUMBER:
        if (!isfinite(v->num_val)) return -1;
        return 0;
    case JSON_STRING:
        break;
    default:
        return -1;                                            /* list/dict: TypeError */
    }
    s = v->str_val ? v->str_val : "";
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '+' || *s == '-') s++;
    while (*s) {
        if (*s == '_') {
            if (!digits || !isdigit((unsigned char)s[1])) return -1;
            s++;
            continue;
        }
        if (!isdigit((unsigned char)*s)) {
            const char *rest = s;
            while (*rest && isspace((unsigned char)*rest)) rest++;
            return *rest ? -1 : 0;
        }
        digits = 1;
        s++;
    }
    return digits ? 0 : -1;
}

int w21_todo_138ed8_is_todo_call(const json_t *call, const char *call_id) {
    const json_t *fn, *name, *arguments, *inner;
    size_t i;

    if (!call || call->type != JSON_OBJECT) return 0;
    {
        const json_t *id = json_obj_get(call, "id");
        if (!id || id->type != JSON_STRING || !call_id || strcmp(id->str_val, call_id)) return 0;
    }
    fn = json_obj_get(call, "function");
    if (!fn || fn->type != JSON_OBJECT) return 0;
    name = json_obj_get(fn, "name");
    if (td_is_todo_name(name)) return 1;
    if (!td_is_name(name, "tool_call")) return 0;

    arguments = json_obj_get(fn, "arguments");
    if (arguments && arguments->type == JSON_STRING) {
        json_t *decoded = json_parse(arguments->str_val ? arguments->str_val : "", NULL);
        int hit = 0;
        if (!decoded) {
            /* json.loads raises ValueError inside is_todo_call; the caller's
             * `except (ValueError, TypeError)` skips the row. */
            return -1;
        }
        if (decoded->type == JSON_OBJECT) {
            inner = json_obj_get(decoded, "calls");
            if (inner && inner->type == JSON_ARRAY) {
                for (i = 0; i < inner->c.count; i++) {
                    const json_t *one = inner->c.items[i];
                    if (one && one->type == JSON_OBJECT &&
                        td_is_todo_name(json_obj_get(one, "name"))) { hit = 1; break; }
                }
            }
        }
        json_free(decoded);
        return hit;
    }
    if (!arguments || arguments->type != JSON_OBJECT) return 0;
    inner = json_obj_get(arguments, "calls");
    if (!inner || inner->type != JSON_ARRAY) return 0;
    for (i = 0; i < inner->c.count; i++) {
        const json_t *one = inner->c.items[i];
        if (one && one->type == JSON_OBJECT && td_is_todo_name(json_obj_get(one, "name"))) return 1;
    }
    return 0;
}

int w21_todo_138ed8_paired(const char *tool_calls_json, const char *call_id) {
    const char *text = (tool_calls_json && tool_calls_json[0]) ? tool_calls_json : NULL;
    json_t *calls;
    size_t i;

    if (!text) return 0;                     /* `prior["tool_calls"]` falsy -> [] */
    calls = json_parse(text, NULL);
    if (!calls) return 0;                    /* json.loads raised -> row skipped */
    if (calls->type != JSON_ARRAY) { json_free(calls); return 0; }
    for (i = 0; i < calls->c.count; i++) {   /* any(): in order, short-circuit */
        int r = w21_todo_138ed8_is_todo_call(calls->c.items[i], call_id);
        if (r == 1) { json_free(calls); return 1; }
        if (r < 0) { json_free(calls); return 0; }  /* predicate raised -> skip */
    }
    json_free(calls);
    return 0;
}

int w21_todo_138ed8_valid_result(const char *content) {
    json_t *data;
    const json_t *todos;
    if (!content) return 0;
    data = json_parse(content, NULL);
    if (!data) return 0;                     /* json.loads raised -> skip */
    if (data->type != JSON_OBJECT) { json_free(data); return 0; }
    todos = json_obj_get(data, "todos");
    if (!todos || todos->type != JSON_ARRAY) { json_free(data); return 0; }
    if (td_int_or_zero(json_obj_get(data, "revision")) < 0) { json_free(data); return 0; }
    json_free(data);
    return 1;
}

/* --- copy identities ------------------------------------------------------ */

/* `_uid_list`: unique non-empty strings, in order (JSON text accepted). */
static json_t *td_uid_list(const json_t *value) {
    json_t *arr = json_array();
    json_t *decoded = NULL;
    const json_t *list = value;
    size_t i, j;

    if (value && value->type == JSON_STRING) {
        decoded = json_parse(value->str_val ? value->str_val : "", NULL);
        list = decoded;                      /* a failure falls back to [] */
    }
    if (list && list->type == JSON_ARRAY) {
        for (i = 0; i < list->c.count; i++) {
            const json_t *u = list->c.items[i];
            int seen = 0;
            if (!u || u->type != JSON_STRING || !u->str_val || !u->str_val[0]) continue;
            for (j = 0; j < arr->c.count; j++) {
                if (!strcmp(arr->c.items[j]->str_val, u->str_val)) { seen = 1; break; }
            }
            if (!seen) json_append(arr, json_string(u->str_val));
        }
    }
    json_free(decoded);
    return arr;
}

static json_t *td_render_identity(const json_t *identity) {
    char buf[64];
    if (!identity) return json_string("");
    switch (identity->type) {
    case JSON_STRING:
        return json_string(identity->str_val ? identity->str_val : "");
    case JSON_NUMBER:
        snprintf(buf, sizeof(buf), "%.17g", identity->num_val);
        return json_string(buf);
    default:
        return json_string("");
    }
}

static void td_mark_seen(json_t *arr, const char *text) {
    size_t j;
    for (j = 0; j < arr->c.count; j++) {
        if (!strcmp(arr->c.items[j]->str_val, text)) return;
    }
    json_append(arr, json_string(text));
}

/* `_todo_copies` with the display hash supplied by the caller. */
static json_t *td_copy_ids(const json_t *identity, const char *message_uid,
                           const json_t *absorbed) {
    json_t *rendered = json_array();
    json_t *uids = td_uid_list(absorbed);
    size_t i;

    {
        json_t *id = td_render_identity(identity);
        td_mark_seen(rendered, id->str_val);
        json_free(id);
    }
    if (message_uid && message_uid[0]) {
        char *tag = (char *)malloc(strlen(message_uid) + 5);
        if (tag) {
            sprintf(tag, "uid:%s", message_uid);
            td_mark_seen(rendered, tag);
            free(tag);
        }
    }
    for (i = 0; i < uids->c.count; i++) {
        const char *uid = uids->c.items[i]->str_val;
        char *tag = (char *)malloc(strlen(uid) + 5);
        if (!tag) continue;
        sprintf(tag, "uid:%s", uid);
        td_mark_seen(rendered, tag);
        free(tag);
    }
    json_free(uids);
    return rendered;
}

json_t *w21_todo_138ed8_copy_ids(const json_t *identity, const char *message_uid,
                                 const json_t *absorbed) {
    return td_copy_ids(identity, message_uid, absorbed);
}

/* A row id as int64: decimal string (the JSON-safe form) or a number. */
static long long td_id_ll(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val ? v->str_val : "0", NULL, 10);
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    return 0;
}

json_t *w21_todo_138ed8_removed_map(const json_t *rows) {
    json_t *out = json_object();
    size_t i, k;

    if (!rows || rows->type != JSON_ARRAY) return out;
    for (i = 0; i < rows->c.count; i++) {
        const json_t *row = rows->c.items[i];
        json_t *ids;
        long long id;
        char buf[32];
        if (!row || row->type != JSON_OBJECT) continue;
        id = td_id_ll(json_obj_get(row, "id"));
        snprintf(buf, sizeof(buf), "%lld", id);
        ids = td_copy_ids(json_obj_get(row, "display_identity"),
                          json_get_str(row, "message_uid", NULL),
                          json_obj_get(row, "absorbed_message_uids"));
        for (k = 0; k < ids->c.count; k++) {
            const char *copy = ids->c.items[k]->str_val;
            const json_t *prior = json_obj_get(out, copy);
            long long best = td_id_ll(prior);
            if (!prior || id > best) json_set(out, copy, json_string(buf));
        }
        json_free(ids);
    }
    return out;
}

int w21_todo_138ed8_copy_removed(const json_t *removed, const json_t *copy_ids,
                                 const json_t *row_id) {
    size_t i;
    long long rid = td_id_ll(row_id);
    if (!removed || removed->type != JSON_OBJECT || removed->c.count == 0) return 0;
    if (!copy_ids || copy_ids->type != JSON_ARRAY) return 0;
    for (i = 0; i < copy_ids->c.count; i++) {
        const json_t *id = copy_ids->c.items[i];
        const json_t *hit;
        if (!id || id->type != JSON_STRING) continue;
        hit = json_obj_get(removed, id->str_val);
        if (hit && td_id_ll(hit) > rid) return 1;
    }
    return 0;
}
