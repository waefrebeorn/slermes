/*
 * methods_session_seed.c -- real C11 port of the seeded-branch write of upstream
 * Hermes tui_gateway/methods_session_seed.py.
 *
 * _persist_branch does three things in order and compensates when the third
 * fails: it creates the child row (inheriting the parent's system prompt when
 * the parent is still readable), copies the parent transcript into it, then
 * writes the title. The transaction boundaries are the point -- a committed row
 * whose transcript never landed would defeat the lazy INSERT-OR-IGNORE seed --
 * so the port keeps the ordering and the compensation rule exactly, over a
 * scripted persistence handle instead of the live one.
 *
 * What stays upstream: _session_db(), the real _persistence_for() handle,
 * _branch_title(), _is_remote_launch_cwd(), _current_profile_name(),
 * _session_default_route(), _session_auth_user_id(), the NewMessage builder and
 * message_identity()'s uid minting -- each needs live room/session config.
 *
 * PoP: w20_methodsses_a61454_branch_rows @ tui_gateway/methods_session_seed.py:_persist_branch
 * PoP: w20_methodsses_a61454_branch_persist @ tui_gateway/methods_session_seed.py:_persist_branch
 * PoP: w20_methodsses_a61454_seed_title_source @ tui_gateway/methods_session_seed.py:_persist_branch
 */
#include "hermes_json.h"
#include "noc_w20_methodsses_a61454.h"

#include <stdlib.h>
#include <string.h>

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int ms61_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* msg.get(key) as a fresh value, NULL (Python None) when absent. */
static json_t *ms61_get(const json_t *obj, const char *key) {
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    return v ? json_copy(v) : json_null();
}

json_t *w20_methodsses_a61454_branch_rows(const json_t *history, const json_t *copy_fields,
                                          const json_t *identities) {
    json_t *rows = json_array();
    size_t h, i;

    for (h = 0; history && history->type == JSON_ARRAY && h < history->c.count; h++) {
        json_t *msg = history->c.items[h];
        json_t *row = json_object();
        const json_t *ident = (identities && identities->type == JSON_ARRAY &&
                               h < identities->c.count) ? identities->c.items[h] : NULL;

        /* "role": msg.get("role", "user") */
        json_set(row, "role",
                 (msg && msg->type == JSON_OBJECT && json_obj_get(msg, "role"))
                     ? json_copy(json_obj_get(msg, "role"))
                     : json_string("user"));
        /* "content": msg.get("content") */
        json_set(row, "content", ms61_get(msg, "content"));
        /* **{field: msg.get(field) for field in copy_fields} */
        for (i = 0; copy_fields && copy_fields->type == JSON_ARRAY &&
                   i < copy_fields->c.count; i++) {
            json_t *f = copy_fields->c.items[i];
            if (!f || f->type != JSON_STRING || !f->str_val) continue;
            json_set(row, f->str_val, ms61_get(msg, f->str_val));
        }
        /* **message_identity(msg): the seam supplies the identity mapping. */
        if (ident && ident->type == JSON_OBJECT) {
            for (i = 0; i < ident->c.count; i++)
                json_set(row, ident->c.keys[i], json_copy(ident->c.items[i]));
        }
        json_append(rows, row);
    }
    return rows;
}

int w20_methodsses_a61454_seed_title_source(const char *title_source) {
    return title_source && !strcmp(title_source, "user");
}

json_t *w20_methodsses_a61454_branch_persist(const json_t *spec) {
    json_t *out = json_object(), *session = json_object(), *rows, *title = NULL;
    json_t *parent, *parent_prompt = NULL, *model_config;
    const char *new_key = json_get_str(spec, "new_key", "");
    const char *parent_key = json_get_str(spec, "parent_key", "");
    const char *title_source = json_get_str(spec, "title_source", "derived");
    const char *fail = json_get_str(spec, "fail", "none");
    const char *fail_message = json_get_str(spec, "fail_message", "transcript write failed");
    json_t *annotations = json_obj_get(spec, "annotations");
    int compensated = 0, parent_read_failed = 0;
    const char *raised = NULL;
    size_t i;

    /* parent_prompt = None; try: parent = sessions.get(parent_key) ... except: log */
    parent = json_obj_get(spec, "parent");
    if (parent && parent->type == JSON_OBJECT) {
        if (ms61_truthy(json_obj_get(parent, "error"))) {
            parent_read_failed = 1;
        } else {
            json_t *sp = json_obj_get(parent, "system_prompt");
            if (ms61_truthy(sp)) parent_prompt = json_copy(sp);
        }
    }

    /* persistence.sessions.create(NewSession(...system_prompt=parent_prompt or None)) */
    json_set(session, "session_id", json_string(new_key));
    json_set(session, "source", json_string(json_get_str(spec, "source", "")));
    /* model=_session_default_route(record)[0] -- the caller's slot, always a str here */
    json_set(session, "model", json_string(json_get_str(spec, "model", "")));
    model_config = json_object();
    json_set(model_config, "_branched_from", json_string(parent_key));
    json_set(session, "model_config", model_config);
    json_set(session, "parent_session_id", json_string(parent_key));
    json_set(session, "cwd", ms61_get(spec, "cwd"));
    json_set(session, "profile_name", ms61_get(spec, "profile_name"));
    json_set(session, "user_id", ms61_get(spec, "user_id"));
    json_set(session, "system_prompt", parent_prompt ? parent_prompt : json_null());

    rows = w20_methodsses_a61454_branch_rows(json_obj_get(spec, "history"),
                                             json_obj_get(spec, "copy_fields"),
                                             json_obj_get(spec, "identities"));

    if (!strcmp(fail, "append")) {
        raised = fail_message;
        goto out_of_try;
    }

    /* for row, outcome in zip(rows, batch.outcomes): row.update({k: v ... if v is not None}) */
    for (i = 0; annotations && annotations->type == JSON_ARRAY &&
                i < annotations->c.count && i < rows->c.count; i++) {
        json_t *ann = annotations->c.items[i];
        size_t k;
        if (!ann || ann->type != JSON_OBJECT) continue;
        for (k = 0; k < ann->c.count; k++) {
            if (!ann->c.items[k] || ann->c.items[k]->type == JSON_NULL) continue;
            json_set(rows->c.items[i], ann->c.keys[k], json_copy(ann->c.items[k]));
        }
    }

    if (!strcmp(fail, "title")) {
        raised = fail_message;          /* the one scripted failure text */
        goto out_of_try;
    }

    /* title_source == "user" -> set_title, else set_auto_title(source=TitleSource(...)) */
    title = json_object();
    if (!strcmp(title_source, "user")) {
        json_set(title, "kind", json_string("user"));
        json_set(title, "title", json_string(json_get_str(spec, "title", "")));
    } else {
        json_set(title, "kind", json_string("auto"));
        json_set(title, "title", json_string(json_get_str(spec, "title", "")));
        json_set(title, "source", json_string(title_source));
    }

out_of_try:
    json_set(out, "session", session);
    json_set(out, "rows", rows);
    json_set(out, "title", title ? title : json_null());
    json_set(out, "parent_read_failed", json_bool(parent_read_failed));
    if (raised) {
        /* except Exception: if compensate and not is_disk_full_error(exc): discard */
        if (ms61_truthy(json_obj_get(spec, "compensate")) &&
            !ms61_truthy(json_obj_get(spec, "disk_full"))) {
            /* _discard_unadmitted(...) -- its own failure is swallowed, never
             * masking the original raise (the fixture scripts that failure). */
            compensated = 1;
        }
        json_set(out, "raised", json_string(raised));
    } else {
        json_set(out, "raised", json_null());
    }
    json_set(out, "compensated", json_bool(compensated));
    return out;
}

