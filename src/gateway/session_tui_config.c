/*
 * session_tui_config.c — C port of upstream Hermes gateway/session_tui_config.py.
 *
 * PoP: w13_sessiontui_41eb5b_word @ gateway/session_tui_config.py:_word
 * PoP: w13_sessiontui_41eb5b_statusbar @ gateway/session_tui_config.py:_statusbar
 * PoP: w13_sessiontui_41eb5b_profile_leaf @ gateway/session_tui_config.py:_set_profile_leaf
 *
 * Config IO / authorization are seams; the pure word coercion, statusbar
 * normalization and dotted-leaf edit (with AuthorityRefusal semantics) are
 * ported exactly.
 */
#include "noc_w13_sessiontui_41eb5b.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Python str(value).strip().lower() for the JSON scalars used here, into out. */
static void py_word(const struct json_t *v, char *out, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(out, cap, ""); return; }
    if (v->type == JSON_BOOL) { snprintf(out, cap, v->bool_val ? "true" : "false"); return; }
    if (v->type == JSON_NUMBER) {
        long long i = (long long)v->num_val;
        if ((double)i == v->num_val) snprintf(out, cap, "%.0f", v->num_val);
        else snprintf(out, cap, "%g", v->num_val);
        return;
    }
    if (v->type == JSON_STRING) {
        snprintf(out, cap, "%s", v->str_val ? v->str_val : "");
        /* strip() then lower(), in place; NUL rewritten at the shortened length. */
        size_t n = strlen(out), a = 0, b = n;
        while (a < b && isspace((unsigned char)out[a])) a++;
        while (b > a && isspace((unsigned char)out[b - 1])) b--;
        size_t len = b - a;
        for (size_t i = a; i < b; i++) out[i] = (char)tolower((unsigned char)out[i]);
        if (a) memmove(out, out + a, len);
        out[len] = '\0';
        return;
    }
    snprintf(out, cap, "");
}

struct json_t *w13_sessiontui_41eb5b_word(const struct json_t *value) {
    char buf[8192];
    py_word(value, buf, sizeof(buf));
    return json_string(buf);
}

struct json_t *w13_sessiontui_41eb5b_statusbar(const struct json_t *raw) {
    /* 'off' if raw is False else raw.strip().lower() if ... in modes else 'top' */
    if (raw && raw->type == JSON_BOOL && !raw->bool_val) return json_string("off");
    if (raw && raw->type == JSON_STRING) {
        char buf[8192];
        py_word(raw, buf, sizeof(buf));
        if (!strcmp(buf, "off") || !strcmp(buf, "top") || !strcmp(buf, "bottom"))
            return json_string(buf);
    }
    return json_string("top");
}

static struct json_t *raise_named(const char *name) {
    struct json_t *e = json_object();
    json_set(e, "__raise__", json_string(name));
    return e;
}

struct json_t *w13_sessiontui_41eb5b_profile_leaf(const struct json_t *cfg,
                                                   const struct json_t *key,
                                                   const struct json_t *value) {
    if (!json_is_string(key) || !key->str_val || !strchr(key->str_val, '.'))
        return raise_named("ValueError");  /* key.split('.', 1) needs one dot */
    const char *k = key->str_val;
    const char *dot = strchr(k, '.');
    size_t slen = (size_t)(dot - k);
    char section[4096], leaf[4096];
    if (slen >= sizeof(section)) slen = sizeof(section) - 1;
    memcpy(section, k, slen); section[slen] = '\0';
    snprintf(leaf, sizeof(leaf), "%s", dot + 1);

    char w[8192];
    py_word(value, w, sizeof(w));
    struct json_t *newv = NULL;
    if (!strcmp(k, "voice.voice_chat_mode")) {
        if (strcmp(w, "chained") && strcmp(w, "gpt-live")) return raise_named("AuthorityRefusal");
        newv = json_string(w);
    } else {
        if (!strcmp(w, "on") || !strcmp(w, "true") || !strcmp(w, "yes") || !strcmp(w, "1"))
            newv = json_bool(1);
        else if (!strcmp(w, "off") || !strcmp(w, "false") || !strcmp(w, "no") || !strcmp(w, "0"))
            newv = json_bool(0);
        else
            return raise_named("AuthorityRefusal");
    }

    /* Work on a copy: never mutate the parsed input node. */
    struct json_t *out = json_is_object(cfg) ? json_copy(cfg) : json_object();
    struct json_t *sec = json_obj_get(out, section);
    if (!json_is_object(sec)) {
        sec = json_object();
        json_set(out, section, sec);
    }
    json_set(sec, leaf, newv);
    return out;
}

