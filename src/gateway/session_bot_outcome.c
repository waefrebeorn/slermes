/*
 * session_bot_outcome.c - C port of upstream Hermes gateway/session_bot_outcome.py
 *
 * PoP: w14_sessionbot_859d7d_owner_stopped @ gateway/session_bot_outcome.py:owner_stopped
 * PoP: w14_sessionbot_859d7d_reply_attempt @ gateway/session_bot_outcome.py:_reply_attempt
 * PoP: w14_sessionbot_859d7d_last_words @ gateway/session_bot_outcome.py:_last_words
 *
 * The forgotten-reply relay: interrupt classification and the walk back through
 * a turn's assistant messages to the last words addressed to the sender.
 */
#include "noc_w14_sessionbot_859d7d.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr_l(const char *s);
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const json_t *og(const json_t *o, const char *k) {
    return json_is_object(o) ? json_obj_get(o, k) : NULL;
}
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    return v->c.count > 0;
}
/* Python str(v) for the scalar shapes the fixture uses; falsy -> "" via caller. */
static char *py_str(const json_t *v) {
    if (!v) return dupstr_l("");
    if (v->type == JSON_STRING) return v->str_val ? _strdup(v->str_val) : dupstr_l("");
    if (v->type == JSON_BOOL) return dupstr_l(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof b, "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof b, "%.17g", v->num_val);
        return _strdup(b);
    }
    return dupstr_l("");
}
static char *dupstr_l(const char *s) { return _strdup(s ? s : ""); }
/* strip() then lstrip('@') then lower(); result is a fresh NUL-terminated buffer. */
static char *norm_target(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    while (a < b && s[a] == '@') a++;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return dupstr_l("");
    for (size_t i = a; i < b; i++) out[i - a] = (char)tolower((unsigned char)s[i]);
    out[b - a] = '\0';
    return out;
}
static char *strip_dup(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return dupstr_l("");
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
static int is_silence(const char *s) {
    static const char *marks[] = {"[SILENT]", "SILENT", "NO_REPLY", "NO REPLY",
                                  "[静默]", "静默", "[沉默]", "沉默"};
    for (size_t i = 0; i < sizeof marks / sizeof marks[0]; i++)
        if (!strcmp(s, marks[i])) return 1;
    return 0;
}

json_t *w14_sessionbot_859d7d_owner_stopped(const struct json_t *result) {
    const char *msg = jstr(og(result, "interrupt_message"));
    int yes = msg && (!strcmp(msg, "Gateway restarting") || !strcmp(msg, "Gateway shutting down"));
    return json_bool(yes);
}

json_t *w14_sessionbot_859d7d_reply_attempt(const struct json_t *call, const struct json_t *sender) {
    const json_t *fn = og(call, "function");
    if (fn && !json_is_object(fn)) return json_null(); /* Python would raise; fixtures avoid */
    if (!fn || !truthy(fn)) fn = NULL;
    const char *name = jstr(og(fn, "name"));
    if (!name || strcmp(name, "message_agent")) return json_null();

    const json_t *av = og(fn, "arguments");
    json_t *args = NULL;
    if (!av || (av->type == JSON_STRING && !*(av->str_val ? av->str_val : "")) || !truthy(av))
        args = json_parse("{}", NULL);
    else if (av->type == JSON_STRING)
        args = json_parse(jstr(av), NULL);
    else
        args = json_parse("{}", NULL);
    if (!json_is_object(args)) { json_free(args); return json_null(); }

    const char *snd = jstr(sender) ? jstr(sender) : "";
    const json_t *tv = og(args, "target");
    char *traw = truthy(tv) ? py_str(tv) : dupstr_l("");
    char *tgt = norm_target(traw);
    free(traw);
    int match = !strcmp(tgt, snd);
    free(tgt);
    if (!match) { json_free(args); return json_null(); }

    const json_t *mv = og(args, "message");
    char *mraw = truthy(mv) ? py_str(mv) : dupstr_l("");
    char *msg = strip_dup(mraw);
    free(mraw);
    json_free(args);
    if (!*msg) { free(msg); return json_null(); }
    json_t *out = json_string(msg);
    free(msg);
    return out;
}

json_t *w14_sessionbot_859d7d_last_words(const struct json_t *messages, const struct json_t *sender) {
    if (!json_is_array(messages)) return json_null();
    for (size_t i = json_len(messages); i > 0; i--) {
        const json_t *m = json_get(messages, i - 1);
        if (!json_is_object(m)) continue;
        const char *role = jstr(og(m, "role"));
        if (!role || strcmp(role, "assistant")) continue;
        const json_t *tcs = og(m, "tool_calls");
        if (json_is_array(tcs)) {
            for (size_t j = json_len(tcs); j > 0; j--) {
                json_t *txt = w14_sessionbot_859d7d_reply_attempt(json_get(tcs, j - 1), sender);
                if (txt && txt->type == JSON_STRING && txt->str_val && *txt->str_val) return txt;
                json_free(txt);
            }
        }
        const json_t *cv = og(m, "content");
        const char *c = jstr(cv);
        if (!c) continue;
        char *trim = strip_dup(c);
        /* content.strip() is returned */
        if (*trim && !is_silence(trim)) { json_t *o = json_string(trim); free(trim); return o; }
        free(trim);
    }
    return json_null();
}
