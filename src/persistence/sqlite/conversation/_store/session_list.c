/*
 * session_list.c -- real C11 port of the session-list page projection of
 * upstream Hermes persistence/sqlite/conversation/_store/session_list.py.
 *
 * PoP: w21_sessionlis_08fd81_project_compression_tips @ persistence/sqlite/conversation/_store/session_list.py:project_compression_tips
 *
 * The two DB reads the Python function makes
 *   get_compression_chain(db, s["id"])                        (from .compression)
 *   _get_session_rich_rows_batch(db, tips, compact_rows=...)  (from .portability)
 * are the only non-pure seams; the C port takes their results as `chains` and
 * `tip_rows` maps.  Everything else -- which roots rotate, which fields the
 * tip overrides, the title carry-back, the `_lineage_*` bookkeeping and the
 * `continuation_kind` label -- is ported verbatim.
 */
#include "hermes_json.h"
#include "noc_w21_sessionlis_08fd81.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fields the tip row replaces on the root row, in the Python tuple's order. */
static const char *const SL08_REPLACE_KEYS[] = {
    "id", "ended_at", "end_reason", "message_count", "tool_call_count", "title",
    "last_active", "preview", "model", "system_prompt", "cwd", "git_branch",
    "git_repo_root"
};

static json_t *sl08_error(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("error"));
    json_set(out, "type", json_string(type ? type : "Exception"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

/* The Python type name a decoded JSON value would have, for exception text. */
static const char *sl08_py_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val) return "int";
        return "float";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

json_t *w21_sessionlis_08fd81_project_compression_tips(const json_t *sessions,
                                                       const json_t *chains,
                                                       const json_t *tip_rows) {
    json_t *projected;
    size_t i;

    if (!sessions || sessions->type == JSON_NULL) {
        return sl08_error("TypeError", "'NoneType' object is not iterable");
    }
    if (sessions->type != JSON_ARRAY) {
        /* A string/dict iterates to str keys, which then fail on .get(); any other
         * type is simply not iterable. */
        const char *name = sl08_py_type_name(sessions);
        int iterates = (sessions->type == JSON_STRING && sessions->str_val && sessions->str_val[0]) ||
                       (sessions->type == JSON_OBJECT && sessions->c.count > 0);
        if (iterates) {
            return sl08_error("AttributeError", "'str' object has no attribute 'get'");
        }
        if (sessions->type == JSON_STRING || sessions->type == JSON_OBJECT) {
            return json_array();                     /* empty iterable -> [] */
        }
        {
            char msg[96];
            snprintf(msg, sizeof(msg), "'%s' object is not iterable", name);
            return sl08_error("TypeError", msg);
        }
    }

    projected = json_array();
    for (i = 0; i < sessions->c.count; i++) {
        json_t *s = sessions->c.items[i];
        json_t *end_reason, *root_id, *chain = NULL, *tip_row = NULL;
        json_t *merged;
        const char *root_text;
        const char *tip_text;
        size_t k;

        if (!s || s->type != JSON_OBJECT) {
            char msg[96];
            json_free(projected);
            snprintf(msg, sizeof(msg), "'%s' object has no attribute 'get'",
                     sl08_py_type_name(s));
            return sl08_error("AttributeError", msg);
        }
        if (!json_obj_get(s, "id")) {
            json_free(projected);
            return sl08_error("KeyError", "'id'");
        }

        end_reason = json_obj_get(s, "end_reason");
        if (end_reason && end_reason->type == JSON_STRING &&
            !strcmp(end_reason->str_val, "compression")) {
            root_id = json_obj_get(s, "id");
            root_text = (root_id && root_id->type == JSON_STRING) ? root_id->str_val : NULL;
            chain = root_text ? json_obj_get(chains, root_text) : NULL;
            if (chain && chain->type == JSON_ARRAY && chain->c.count > 0) {
                json_t *last = chain->c.items[chain->c.count - 1];
                tip_text = (last && last->type == JSON_STRING) ? last->str_val : NULL;
                /* chain[-1] != s["id"] -- a self-tipped chain needs no projection */
                if (tip_text && root_text && strcmp(tip_text, root_text)) {
                    tip_row = json_obj_get(tip_rows, tip_text);
                    if (tip_row && tip_row->type != JSON_OBJECT) tip_row = NULL;
                }
            } else {
                chain = NULL;
            }
        }

        if (!tip_row) {
            json_append(projected, json_copy(s));
            continue;
        }

        merged = json_copy(s);
        for (k = 0; k < sizeof(SL08_REPLACE_KEYS) / sizeof(SL08_REPLACE_KEYS[0]); k++) {
            json_t *v = json_obj_get(tip_row, SL08_REPLACE_KEYS[k]);
            if (v) json_set(merged, SL08_REPLACE_KEYS[k], json_copy(v));
        }
        {
            json_t *title = json_obj_get(merged, "title");
            if (!title || title->type == JSON_NULL) {
                /* the title is carried root->tip AFTER the publish transaction */
                json_t *root_title = json_obj_get(s, "title");
                json_set(merged, "title", root_title ? json_copy(root_title) : json_null());
            }
        }
        {
            json_t *root_copy = json_obj_get(s, "id");
            json_set(merged, "_lineage_root_id",
                     root_copy ? json_copy(root_copy) : json_null());
        }
        json_set(merged, "_lineage_ids", json_copy(chain));
        json_set(merged, "continuation_kind", json_string("compression"));
        json_append(projected, merged);
    }
    return projected;
}
