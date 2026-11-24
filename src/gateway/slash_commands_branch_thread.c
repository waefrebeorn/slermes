/*
 * slash_commands_branch_thread.c — C11 port of upstream Hermes
 * gateway/slash_commands_branch_thread.py (gate w25_slashcomma_2c9db9).
 * STATUS: real behaviour port of the pure helpers; the adapter that creates
 *         the hand-off thread is a seam and never exists here.
 * PoP: w25_slashcomma_2c9db9_parse_branch_args @ gateway/slash_commands_branch_thread.py:parse_branch_args
 * PoP: w25_slashcomma_2c9db9_branch_thread_parent @ gateway/slash_commands_branch_thread.py:branch_thread_parent
 * PoP: w25_slashcomma_2c9db9_branch_dest_source @ gateway/slash_commands_branch_thread.py:branch_dest_source
 * PoP: w25_slashcomma_2c9db9_format_thread_ref @ gateway/slash_commands_branch_thread.py:format_thread_ref
 * PoP: w25_slashcomma_2c9db9_constants @ gateway/slash_commands_branch_thread.py:BRANCH_THREAD_PLATFORMS
 *
 * A plain /branch opens a NEW sibling thread for the clone on Discord,
 * Telegram, Slack and Matrix; /branch --here keeps the in-place switch, and
 * platforms without threads branch in place.  The session source is a plain
 * value here: platform name, chat id/type/name, thread id, parent chat id.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_slashcomma_2c9db9.h"

/* BRANCH_THREAD_PLATFORMS (a frozenset; exposed sorted so it has one order) */
static const char *const SC_THREAD_PLATFORMS[] = {"discord", "matrix", "slack", "telegram"};
#define SC_THREAD_PLATFORMS_N (sizeof(SC_THREAD_PLATFORMS) / sizeof(SC_THREAD_PLATFORMS[0]))

/* BRANCH_HERE_FLAG */
static const char *const SC_HERE_FLAG = "--here";

/* Python truthiness of a JSON value. */
static int sc_truthy(const json_t *v) {
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

/* ASCII case-insensitive compare: str.lower() == literal. */
static int sc_ieq(const char *a, const char *b) {
    size_t i;
    for (i = 0; a[i] && b[i]; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
    }
    return a[i] == b[i];
}

static int sc_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static json_t *sc_str_range(const char *s, size_t n) {
    char *buf = (char *)malloc(n + 1);
    json_t *out;
    if (!buf) return json_null();
    if (n) memcpy(buf, s, n);
    buf[n] = '\0';
    out = json_string(buf);
    free(buf);
    return out;
}

/* str(value) for the scalar shapes a source field can hold. */
static json_t *sc_str_of(const json_t *v) {
    char buf[64];
    if (!v) return json_string("None");
    switch (v->type) {
    case JSON_STRING: return json_string(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return json_string(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return json_string(buf);
    default: return json_string("None");
    }
}

/* A source field as a string, or NULL when absent/null/non-string. */
static const char *sc_field(const json_t *source, const char *key) {
    const json_t *v;
    if (!source || source->type != JSON_OBJECT) return NULL;
    v = json_obj_get(source, key);
    if (!v || v->type == JSON_NULL) return NULL;
    if (v->type != JSON_STRING) return NULL;
    return v->str_val ? v->str_val : "";
}

static const json_t *sc_get(const json_t *source, const char *key) {
    if (!source || source->type != JSON_OBJECT) return NULL;
    return json_obj_get(source, key);
}

/* A copy that keeps an absent field present as null, like the dataclass field. */
static json_t *sc_copy(const json_t *v) {
    json_t *c = v ? json_copy(v) : NULL;
    return c ? c : json_null();
}

json_t *w25_slashcomma_2c9db9_parse_branch_args(const json_t *raw) {
    const char *s;
    size_t n, b, e, h;
    char *head;
    json_t *out, *title;
    int stay_here = 0;

    out = json_object();
    if (!raw || raw->type == JSON_NULL) s = "";
    else if (raw->type == JSON_STRING) s = raw->str_val ? raw->str_val : "";
    else s = "";
    n = strlen(s);
    b = 0;
    e = n;
    while (b < e && sc_ws(s[b])) b++;
    while (e > b && sc_ws(s[e - 1])) e--;

    /* head, _, rest = text.partition(" ") -- a single space, not a ws run */
    h = b;
    while (h < e && s[h] != ' ') h++;
    head = (char *)malloc(h - b + 1);
    if (!head) return out;
    if (h > b) memcpy(head, s + b, h - b);
    head[h - b] = '\0';
    if (sc_ieq(head, SC_HERE_FLAG)) {  /* head.lower() == "--here" */
        size_t rs = h < e ? h + 1 : e;
        size_t re = e;
        stay_here = 1;
        while (rs < re && sc_ws(s[rs])) rs++;
        title = sc_str_range(s + rs, re - rs);
    } else {
        title = sc_str_range(s + b, e - b);
    }
    free(head);
    json_set(out, "stay_here", json_bool(stay_here));
    json_set(out, "title", title);
    return out;
}

json_t *w25_slashcomma_2c9db9_branch_thread_parent(const json_t *source) {
    const char *platform = sc_field(source, "platform");
    const char *chat_type = sc_field(source, "chat_type");
    const json_t *chat_id = sc_get(source, "chat_id");
    const json_t *thread_id = sc_get(source, "thread_id");
    const json_t *parent_chat_id = sc_get(source, "parent_chat_id");
    int known = 0, is_discord = 0;
    size_t i;

    if (!platform) return json_null();
    for (i = 0; i < SC_THREAD_PLATFORMS_N; i++)
        if (strcmp(platform, SC_THREAD_PLATFORMS[i]) == 0) { known = 1; break; }
    if (!known) return json_null();
    is_discord = strcmp(platform, "discord") == 0;
    if (is_discord) {
        if (chat_type && strcmp(chat_type, "dm") == 0) return json_null();
        /* Inbound Discord threads carry chat_id == thread_id; the sibling goes
         * under the real text channel, which only parent_chat_id names. */
        if (sc_truthy(thread_id) || (chat_type && strcmp(chat_type, "thread") == 0))
            return sc_truthy(parent_chat_id) ? sc_str_of(parent_chat_id) : json_null();
    }
    return sc_truthy(chat_id) ? sc_str_of(chat_id) : json_null();
}

json_t *w25_slashcomma_2c9db9_branch_dest_source(const json_t *source, const json_t *parent_id,
                                                 const json_t *thread_id, const json_t *title) {
    const char *platform = sc_field(source, "platform");
    const json_t *chat_type = sc_get(source, "chat_type");
    const char *chat_type_s = sc_field(source, "chat_type");
    json_t *out = json_object();

    if (!out) return json_null();
    /* common: thread_id=str(thread_id), message_id=None, prospective_thread_id=None,
     * auto_thread_created=False, auto_thread_initial_name=None */
    json_set(out, "thread_id", sc_str_of(thread_id));
    json_set(out, "message_id", json_null());
    json_set(out, "prospective_thread_id", json_null());
    json_set(out, "auto_thread_created", json_bool(0));
    json_set(out, "auto_thread_initial_name", json_null());

    if (platform && strcmp(platform, "discord") == 0) {
        /* Discord keys an in-thread message on the thread's OWN id as chat_id. */
        json_set(out, "chat_id", sc_str_of(thread_id));
        json_set(out, "chat_name",
                 sc_truthy(title) ? sc_copy(title) : sc_copy(sc_get(source, "chat_name")));
        json_set(out, "chat_type", json_string("thread"));
        json_set(out, "parent_chat_id", sc_str_of(parent_id));
        return out;
    }
    /* Telegram/Slack/Matrix key a thread reply on the PARENT chat's type. */
    json_set(out, "chat_id", sc_str_of(parent_id));
    json_set(out, "chat_name", sc_copy(sc_get(source, "chat_name")));
    if (chat_type_s && strcmp(chat_type_s, "thread") == 0)
        json_set(out, "chat_type", json_string("group"));
    else
        json_set(out, "chat_type", sc_copy(chat_type));
    json_set(out, "parent_chat_id", sc_copy(sc_get(source, "parent_chat_id")));
    return out;
}

json_t *w25_slashcomma_2c9db9_format_thread_ref(const json_t *platform, const json_t *thread_id) {
    json_t *id = sc_str_of(thread_id);
    const char *id_s = id && id->type == JSON_STRING ? id->str_val : "";
    json_t *out;
    char *buf;

    if (platform && platform->type == JSON_STRING && platform->str_val &&
        strcmp(platform->str_val, "discord") == 0) {
        size_t n = strlen(id_s);
        buf = (char *)malloc(n + 4);
        if (!buf) { json_free(id); return json_null(); }
        buf[0] = '<';
        buf[1] = '#';
        if (n) memcpy(buf + 2, id_s, n);
        buf[2 + n] = '>';
        buf[3 + n] = '\0';
        out = json_string(buf);
        free(buf);
    } else {
        size_t n = strlen(id_s);
        buf = (char *)malloc(n + 3);
        if (!buf) { json_free(id); return json_null(); }
        buf[0] = '`';
        if (n) memcpy(buf + 1, id_s, n);
        buf[1 + n] = '`';
        buf[2 + n] = '\0';
        out = json_string(buf);
        free(buf);
    }
    json_free(id);
    return out;
}

json_t *w25_slashcomma_2c9db9_constants(void) {
    json_t *o = json_object();
    json_t *set = json_array();
    size_t i;
    for (i = 0; i < SC_THREAD_PLATFORMS_N; i++)
        json_append(set, json_string(SC_THREAD_PLATFORMS[i]));
    json_set(o, "branch_thread_platforms", set);
    json_set(o, "branch_here_flag", json_string(SC_HERE_FLAG));
    return o;
}
