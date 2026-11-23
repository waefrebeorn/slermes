/*
 * relay_cwd.c — real C11 port of upstream Hermes agent/relay_cwd.py.
 *
 * PoP: relaycwd_clean @ agent/relay_cwd.py:_clean_cwd
 * PoP: relaycwd_resolve @ agent/relay_cwd.py:resolve_relay_scope_cwds
 */
#include "noc_w5_relaycwd_c03229.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dupj(const json_t *v) {
    if (v && v->type == JSON_STRING && v->str_val) return strdup(v->str_val);
    return strdup("");
}

/* Python str.strip() over ASCII whitespace, then sentinel folding. */
static char *clean_cstr(const char *in) {
    size_t a, b, n;
    const char *s = in ? in : "";
    char *out;
    n = strlen(s);
    a = 0;
    while (a < n && isspace((unsigned char)s[a])) a++;
    b = n;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    if (a == b) return strdup("");
    out = (char *)malloc(b - a + 1);
    if (!out) return strdup("");
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    /* case-insensitive sentinel set {"", ".", "./", "auto", "cwd"} */
    if (strcasecmp(out, ".") == 0 || strcasecmp(out, "./") == 0 ||
        strcasecmp(out, "auto") == 0 || strcasecmp(out, "cwd") == 0) {
        free(out);
        return strdup("");
    }
    return out;
}

static char *cleanj(const json_t *v) {
    char *raw = dupj(v);
    char *c = clean_cstr(raw);
    free(raw);
    return c;
}

json_t *relaycwd_clean(const json_t *value) {
    char *c = cleanj(value);
    json_t *out = json_string(c);
    free(c);
    return out;
}

json_t *relaycwd_resolve(const json_t *facts) {
    json_t *arr, *rec_map;
    char *task, *session, *backend, *turn, *session_key;
    const char *platform;
    json_t *v;
    int i, skip;

    if (!facts || facts->type != JSON_OBJECT) {
        arr = json_array();
        json_append(arr, json_string(""));
        json_append(arr, json_string(""));
        return arr;
    }
    platform = json_get_str(facts, "platform", "");
    if (!platform) platform = "";
    rec_map = json_obj_get(facts, "recorded");

    /* task_cwd = _clean_cwd(get_session_cwd(task_id)) */
    task = cleanj(json_obj_get(facts, "task_cwd"));
    /* session_cwd = _clean_cwd(scoped_session_cwd()) */
    session = cleanj(json_obj_get(facts, "scoped_cwd"));

    if (!session[0]) {
        /* for key in dict.fromkeys(k for k in (session_key, session_id) if k) */
        session_key = dupj(json_obj_get(facts, "session_key"));
        const char *candidates[2];
        int nc = 0;
        if (session_key[0]) candidates[nc++] = session_key;
        if (strcmp("session-1", "") != 0) candidates[nc++] = "session-1";
        for (i = 0; i < nc && !session[0]; i++) {
            const char *key = candidates[i];
            /* dict.fromkeys de-dupes: skip a repeated key */
            skip = 0;
            for (int j = 0; j < i; j++) if (strcmp(candidates[j], key) == 0) skip = 1;
            if (skip || !key[0]) continue;
            v = rec_map ? json_obj_get(rec_map, key) : NULL;
            {
                char *rec = cleanj(v);
                if (rec[0]) { free(session); session = rec; }
                else free(rec);
            }
        }
        free(session_key);
    }

    if (!session[0]) {
        free(session);
        session = cleanj(json_obj_get(facts, "agent_cwd"));
    }

    backend = strdup("");
    if (!session[0]) {
        json_t *tev = json_obj_get(facts, "terminal_env");
        char *te = (tev && tev->type == JSON_STRING && tev->str_val)
                 ? strdup(tev->str_val) : strdup("local");
        /* .strip().lower() */
        char *p, *q;
        size_t len;
        char *trim;
        len = strlen(te);
        p = te;
        while (*p && isspace((unsigned char)*p)) p++;
        q = te + len;
        while (q > p && isspace((unsigned char)q[-1])) q--;
        trim = (char *)malloc((size_t)(q - p) + 1);
        if (trim) {
            memcpy(trim, p, (size_t)(q - p));
            trim[q - p] = '\0';
            for (char *c = trim; *c; c++) *c = (char)tolower((unsigned char)*c);
            free(backend);
            backend = trim;
        }
        free(te);
        free(session);
        session = cleanj(json_obj_get(facts, "terminal_cwd"));
    }

    if (!session[0] &&
        (!platform[0] || strcmp(platform, "cli") == 0) &&
        (!backend[0] || strcmp(backend, "local") == 0)) {
        char *resolved = dupj(json_obj_get(facts, "resolved_cwd"));
        char *c = clean_cstr(resolved); /* resolved is already absolute in fixtures */
        free(resolved);
        free(session);
        session = c;
    }

    turn = task[0] ? strdup(task) : strdup(session);
    if ((strcmp(platform, "subagent") == 0 || strcmp(platform, "cron") == 0) && task[0]) {
        free(session);
        session = strdup(task);
    }

    arr = json_array();
    json_append(arr, json_string(session[0] ? session : turn));
    json_append(arr, json_string(turn));
    free(session);
    free(turn);
    free(task);
    free(backend);
    return arr;
}
