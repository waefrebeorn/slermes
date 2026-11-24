/*
 * worker_lifecycle.c -- real C11 port of the write-transaction body of upstream
 * Hermes persistence/sqlite/conversation/_store/worker_lifecycle.py.
 *
 * PoP: w20_workerlife_dacc9a_insert @ persistence/sqlite/conversation/_store/worker_lifecycle.py:insert_session_row_in_transaction
 *
 * The connection, the holder, _own_profile_name, _store_system_prompt and the
 * three post-insert helpers are seams: their answers (and the two tombstone
 * SELECT results) arrive in the spec. The port keeps every branch the helper
 * owns -- the retired-session guard, the profile default, the _delegate_from
 * strip on a /new continuation, the 19-value INSERT parameter tuple and the
 * steps that follow it.
 *
 * params[9] reproduces Python's json.dumps(model_config) with default
 * separators and ensure_ascii; parameter strings are ASCII in practice, and
 * non-ASCII is emitted as \uXXXX like ensure_ascii does.
 */
#include "hermes_json.h"
#include "noc_w20_workerlife_dacc9a.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Python json.dumps(model_config) -------------------------------------- */

typedef struct {
    char buf[8192];
    size_t len;
    int overflow;
} wl20_sb;

static void wl20_sb_init(wl20_sb *sb) {
    sb->len = 0;
    sb->buf[0] = '\0';
    sb->overflow = 0;
}

static void wl20_sb_raw(wl20_sb *sb, const char *s, size_t n) {
    if (sb->len + n >= sizeof(sb->buf)) {
        sb->overflow = 1;
        n = sizeof(sb->buf) - 1 - sb->len;
    }
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0'; /* NUL-terminate after shortening */
}

static void wl20_sb_add(wl20_sb *sb, const char *s) {
    if (s) wl20_sb_raw(sb, s, strlen(s));
}

/* The UTF-8 sequence at *p decoded to its codepoint, advancing *p. */
static unsigned long wl20_utf8(const unsigned char **p) {
    const unsigned char *s = *p;
    unsigned long cp;
    int extra, i;
    if (s[0] < 0x80) {
        *p = s + 1;
        return s[0];
    }
    if ((s[0] & 0xE0) == 0xC0) { cp = s[0] & 0x1F; extra = 1; }
    else if ((s[0] & 0xF0) == 0xE0) { cp = s[0] & 0x0F; extra = 2; }
    else if ((s[0] & 0xF8) == 0xF0) { cp = s[0] & 0x07; extra = 3; }
    else { *p = s + 1; return 0xFFFD; }
    for (i = 1; i <= extra; i++) {
        if (!s[i]) { *p = s + i; return 0xFFFD; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *p = s + 1 + extra;
    return cp;
}

/* json.dumps(str): quoted, ensure_ascii (non-ASCII -> \uXXXX, surrogate pairs). */
static void wl20_dumps_str(wl20_sb *sb, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    char tmp[16];
    wl20_sb_add(sb, "\"");
    while (*p) {
        unsigned long cp = wl20_utf8(&p);
        if (cp == '"') { wl20_sb_add(sb, "\\\""); continue; }
        if (cp == '\\') { wl20_sb_add(sb, "\\\\"); continue; }
        if (cp == '\n') { wl20_sb_add(sb, "\\n"); continue; }
        if (cp == '\r') { wl20_sb_add(sb, "\\r"); continue; }
        if (cp == '\t') { wl20_sb_add(sb, "\\t"); continue; }
        if (cp == '\b') { wl20_sb_add(sb, "\\b"); continue; }
        if (cp == '\f') { wl20_sb_add(sb, "\\f"); continue; }
        if (cp < 0x20 || cp > 0x7E) {
            if (cp > 0xFFFF) {
                unsigned long v = cp - 0x10000;
                snprintf(tmp, sizeof(tmp), "\\u%04x\\u%04x",
                         0xD800 + (unsigned)(v >> 10), 0xDC00 + (unsigned)(v & 0x3FF));
            } else {
                snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)cp);
            }
            wl20_sb_add(sb, tmp);
            continue;
        }
        {
            char ch[2];
            ch[0] = (char)cp;
            ch[1] = '\0';
            wl20_sb_add(sb, ch);
        }
    }
    wl20_sb_add(sb, "\"");
}

static void wl20_dumps(wl20_sb *sb, const json_t *v) {
    size_t i;
    if (!v) { wl20_sb_add(sb, "null"); return; }
    switch (v->type) {
    case JSON_NULL: wl20_sb_add(sb, "null"); break;
    case JSON_BOOL: wl20_sb_add(sb, v->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: {
        char tmp[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e16)
            snprintf(tmp, sizeof(tmp), "%.0f", v->num_val);
        else
            snprintf(tmp, sizeof(tmp), "%g", v->num_val);
        wl20_sb_add(sb, tmp);
        break;
    }
    case JSON_STRING: wl20_dumps_str(sb, v->str_val); break;
    case JSON_ARRAY:
        wl20_sb_add(sb, "[");
        for (i = 0; i < v->c.count; i++) {
            if (i) wl20_sb_add(sb, ", ");
            wl20_dumps(sb, v->c.items[i]);
        }
        wl20_sb_add(sb, "]");
        break;
    case JSON_OBJECT:
        wl20_sb_add(sb, "{");
        for (i = 0; i < v->c.count; i++) {
            if (i) wl20_sb_add(sb, ", ");
            wl20_dumps_str(sb, v->c.keys[i]);
            wl20_sb_add(sb, ": ");
            wl20_dumps(sb, v->c.items[i]);
        }
        wl20_sb_add(sb, "}");
        break;
    }
}

/* --- helpers -------------------------------------------------------------- */

static int wl20_truthy(const json_t *v) {
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

/* `(profile_name or "").strip()` is non-empty. */
static int wl20_strip_has_text(const char *s) {
    size_t i;
    if (!s) return 0;
    for (i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v')
            return 1;
    }
    return 0;
}

static json_t *wl20_raised(const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string("AuthorityRefusal"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

static void wl20_append_copy(json_t *arr, const json_t *v) {
    json_append(arr, v ? json_copy(v) : json_null());
}

/* --- insert_session_row_in_transaction ----------------------------------- */

json_t *w20_workerlife_dacc9a_insert(const json_t *spec) {
    static const char *const MID_FIELDS[] = {
        "user_id", "session_key", "chat_id", "chat_type", "thread_id", "model", NULL
    };
    static const char *const TAIL_FIELDS[] = {
        "transport_profile", "git_repo_root", "origin_json", "display_name", NULL
    };
    json_t *session_id = json_obj_get(spec, "session_id");
    json_t *source = json_obj_get(spec, "source");
    json_t *model_config = json_obj_get(spec, "model_config");
    json_t *parent = json_obj_get(spec, "parent_session_id");
    json_t *session_key = json_obj_get(spec, "session_key");
    json_t *prompt_hash = json_obj_get(spec, "prompt_hash");
    json_t *profile_name = json_obj_get(spec, "profile_name");
    json_t *final_config, *steps, *params, *value, *out;
    int row_exists = wl20_truthy(json_obj_get(spec, "row_exists"));
    int tombstone_exists = wl20_truthy(json_obj_get(spec, "tombstone_exists"));
    int tombstone_queries = 1;
    int i;

    /* if (sessions row is None and state_meta tombstone exists): raise AuthorityRefusal */
    if (!row_exists) {
        tombstone_queries = 2;
        if (tombstone_exists) return wl20_raised("not_found");
    }

    /* if not (profile_name or "").strip(): profile_name = _own_profile_name(holder) */
    if (!profile_name || profile_name->type != JSON_STRING ||
        !wl20_strip_has_text(profile_name->str_val)) {
        profile_name = json_obj_get(spec, "own_profile_name");
    }

    /* a reset row is the chat itself: drop the delegate marker riding the config */
    if (model_config && model_config->type == JSON_OBJECT) {
        json_t *reset = json_obj_get(model_config, "_reset_from");
        json_t *deleg = json_obj_get(model_config, "_delegate_from");
        if (wl20_truthy(session_key) && reset && reset->type != JSON_NULL &&
            deleg && deleg->type != JSON_NULL) {
            json_t *stripped = json_object();
            size_t k;
            for (k = 0; k < model_config->c.count; k++) {
                if (!strcmp(model_config->c.keys[k], "_delegate_from")) continue;
                json_set(stripped, model_config->c.keys[k],
                         json_copy(model_config->c.items[k]));
            }
            model_config = stripped;
        } else {
            model_config = json_copy(model_config);
        }
    } else if (model_config) {
        model_config = json_copy(model_config);
    } else {
        model_config = json_null();
    }
    final_config = model_config;

    params = json_array();
    wl20_append_copy(params, session_id);
    wl20_append_copy(params, source);
    wl20_append_copy(params, source);        /* created_source */
    for (i = 0; MID_FIELDS[i]; i++) wl20_append_copy(params, json_obj_get(spec, MID_FIELDS[i]));
    /* params[9]: json.dumps(model_config) if model_config else None */
    if (wl20_truthy(model_config)) {
        wl20_sb sb;
        wl20_sb_init(&sb);
        wl20_dumps(&sb, model_config);
        json_append(params, json_string(sb.buf));
    } else {
        json_append(params, json_null());
    }
    wl20_append_copy(params, prompt_hash);   /* system_prompt_hash */
    wl20_append_copy(params, parent);        /* parent_session_id */
    wl20_append_copy(params, json_obj_get(spec, "cwd"));
    wl20_append_copy(params, profile_name);
    for (i = 0; TAIL_FIELDS[i]; i++) wl20_append_copy(params, json_obj_get(spec, TAIL_FIELDS[i]));
    json_append(params, json_number(json_get_num(spec, "now", 0.0)));

    steps = json_array();
    if (prompt_hash && prompt_hash->type != JSON_NULL)
        json_append(steps, json_string("delete_system_prompts"));
    if (wl20_truthy(parent)) json_append(steps, json_string("inherit_parent"));
    json_append(steps, json_string("stamp"));

    value = json_object();
    json_set(value, "params", params);
    json_set(value, "steps", steps);
    json_set(value, "profile_name", json_copy(profile_name));
    json_set(value, "model_config", wl20_truthy(final_config) ? json_copy(final_config) : json_null());
    json_set(value, "tombstone_queries", json_number((double)tombstone_queries));

    out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value);
    return out;
}
