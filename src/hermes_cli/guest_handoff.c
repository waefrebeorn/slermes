/*
 * PoP: w10_guesthando_b805fe_classify     @ hermes_cli/guest_handoff.py:_classify
 * PoP: w10_guesthando_b805fe_result_lines  @ hermes_cli/guest_handoff.py:result_lines
 * PoP: w10_guesthando_b805fe_status_lines  @ hermes_cli/guest_handoff.py:status_lines
 */
#include "hermes_json.h"
#include "noc_w10_guesthando_b805fe.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, const char *s) {
    if (!s) return;
    size_t l = strlen(s);
    if (b->n + l + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 128;
        while (nc < b->n + l + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, l); b->n += l; b->p[b->n] = 0;
}
static json_t *jk(json_t *o, const char *k) { return json_obj_get(o, k); }
static const char *js(json_t *v) { return (v && v->type == JSON_STRING) ? v->str_val : NULL; }
static int jeq(const char *a, const char *b) { return a && b && !strcmp(a, b); }

/* Python str(value or "") for the small scalar values used in error fields. */
static const char *py_str_or_empty(json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) return "";
    if (v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    if (v->type == JSON_BOOL) return v->bool_val ? "True" : "False";
    if (v->type == JSON_NUMBER) { snprintf(buf, cap, "%.0f", v->num_val); return buf; }
    return "";
}

json_t *w10_guesthando_b805fe_classify(int status, json_t *payload) {
    if (status == 200) {
        const char *st = js(jk(payload, "status"));
        return json_string(jeq(st, "claimed") ? "claimed" : "retry");
    }
    char buf[64];
    const char *err = py_str_or_empty(jk(payload, "error"), buf, sizeof(buf));
    if (!jeq(err, "unknown_token") && !jeq(err, "already_retired") &&
        !jeq(err, "client_not_allowed") && !jeq(err, "account_locked") &&
        !jeq(err, "invalid_request"))
        return json_string("retry");
    if (!jeq(err, "client_not_allowed") && !jeq(err, "account_locked") &&
        !jeq(err, "invalid_request"))
        return json_string("gone");
    return json_string("fail");
}

/* Faithful copy of tools.tool_labels.app_title (known slugs + Title Case fallback). */
static const char *const _KNOWN_SLUG[] = {
    "discord", "figma", "github", "gmail", "googlecalendar", "googledocs",
    "googledrive", "jira", "linear", "notion", "outlook", "slack", "stripe_mcp", "todoist"
};
static const char *const _KNOWN_TITLE[] = {
    "Discord", "Figma", "GitHub", "Gmail", "Google Calendar", "Google Docs",
    "Google Drive", "Jira", "Linear", "Notion", "Outlook", "Slack", "Stripe", "Todoist"
};
static char *gh_app_title(const char *slug) {
    if (!slug) slug = "";
    const char *a = slug, *b = slug + strlen(slug);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *key = (char *)malloc(n + 1);
    if (!key) return NULL;
    memcpy(key, a, n); key[n] = 0;
    for (size_t i = 0; i < n; i++) key[i] = (char)tolower((unsigned char)key[i]);
    for (size_t i = 0; i < sizeof(_KNOWN_SLUG)/sizeof(_KNOWN_SLUG[0]); i++) {
        if (!strcmp(key, _KNOWN_SLUG[i])) { free(key); return strdup(_KNOWN_TITLE[i]); }
    }
    /* words = key.replace('-',' ').replace('_',' ').split() */
    char *work = (char *)malloc(n + 1);
    if (!work) { free(key); return NULL; }
    memcpy(work, a, n); work[n] = 0;
    for (size_t i = 0; i < n; i++) if (work[i]=='-' || work[i]=='_') work[i]=' ';
    buf_t out = {0};
    size_t i = 0;
    while (i < n) {
        while (i < n && isspace((unsigned char)work[i])) i++;
        size_t s = i;
        while (i < n && !isspace((unsigned char)work[i])) i++;
        if (i == s) continue;
        char w[512]; size_t wl = i - s;
        if (wl >= sizeof(w)) wl = sizeof(w)-1;
        memcpy(w, work + s, wl); w[wl] = 0;
        if (out.n) bput(&out, " ");
        char cap[2] = { (char)toupper((unsigned char)w[0]), 0 };
        bput(&out, cap);
        bput(&out, w + 1);
    }
    free(work); free(key);
    if (!out.p) { out.p = strdup(""); }
    return out.p;
}

static json_t *gh_lines(const char *line1, const char *line2) {
    json_t *arr = json_array();
    json_append(arr, json_string(line1));
    if (line2) json_append(arr, json_string(line2));
    return arr;
}

static json_t *gh_result_lines(json_t *st) {
    const char *state = js(jk(st, "state"));
    json_t *emailv = jk(st, "account_email");
    const char *where = (emailv && emailv->type == JSON_STRING && emailv->str_val && *emailv->str_val)
                        ? emailv->str_val : "your account";
    buf_t moved = {0}, dropped = {0};
    json_t *mt = jk(st, "moved_toolkits");
    if (mt && mt->type == JSON_ARRAY)
        for (size_t i = 0; i < json_array_size(mt); i++) {
            char *t = gh_app_title(js(json_array_get(mt, i)));
            if (i) bput(&moved, ", ");
            bput(&moved, t ? t : ""); free(t);
        }
    json_t *dt = jk(st, "dropped_toolkits");
    if (dt && dt->type == JSON_ARRAY)
        for (size_t i = 0; i < json_array_size(dt); i++) {
            char *t = gh_app_title(js(json_array_get(dt, i)));
            if (i) bput(&dropped, ", ");
            bput(&dropped, t ? t : ""); free(t);
        }
    int have_moved = moved.n > 0, have_dropped = dropped.n > 0;
    json_t *arr;
    char line[1024];
    if (jeq(state, "pending") || jeq(state, "retrying")) {
        free(moved.p); free(dropped.p);
        return gh_lines("Moving your free-tier apps to this account; Hermes keeps trying in the background.", NULL);
    }
    if (jeq(state, "failed")) {
        char buf[64];
        const char *err = py_str_or_empty(jk(st, "error"), buf, sizeof(buf));
        if (jeq(err, "client_not_allowed"))
            snprintf(line, sizeof(line), "Your free-tier apps could not be moved. This sign-in can't receive them. Sign in again with `hermes auth add nous`.");
        else if (jeq(err, "account_locked"))
            snprintf(line, sizeof(line), "Your free-tier apps could not be moved. This Nous account is locked.");
        else
            snprintf(line, sizeof(line), "Your free-tier apps could not be moved.");
        free(moved.p); free(dropped.p);
        return gh_lines(line, NULL);
    }
    if (jeq(state, "gone")) {
        json_t *pm = jk(st, "probably_moved");
        int probably = pm && pm->type == JSON_BOOL && pm->bool_val;
        free(moved.p); free(dropped.p);
        return gh_lines(probably ? "Your free-tier apps were probably moved already."
                                 : "Nothing left to move from the free tier.", NULL);
    }
    if (!jeq(state, "claimed")) {
        free(moved.p); free(dropped.p);
        return json_array();
    }
    const char *kind = js(jk(st, "connectors_kind"));
    char line2[1024];
    if ((jeq(kind, "moved") || jeq(kind, "swapped")) && have_moved) {
        snprintf(line, sizeof(line), "Your apps moved to %s: %s.", where, moved.p);
    } else if (jeq(kind, "conflict")) {
        snprintf(line, sizeof(line), "%s already had its own apps, so the free-tier apps stayed behind.", where);
    } else if (jeq(kind, "none") || jeq(kind, "moved") || jeq(kind, "swapped")) {
        snprintf(line, sizeof(line), "There were no free-tier apps to move.");
    } else {
        snprintf(line, sizeof(line), "Any free-tier apps are in this account now.");
    }
    if (have_dropped) snprintf(line2, sizeof(line2), "Reconnect in %s: %s.", where, dropped.p);
    else line2[0] = 0;
    arr = gh_lines(line, have_dropped ? line2 : NULL);
    free(moved.p); free(dropped.p);
    return arr;
}

json_t *w10_guesthando_b805fe_result_lines(json_t *status) { return gh_result_lines(status); }

json_t *w10_guesthando_b805fe_status_lines(json_t *status) {
    json_t *lines = gh_result_lines(status);
    const char *kind = js(jk(status, "connectors_kind"));
    int moved_some = 0;
    if ((jeq(kind, "moved") || jeq(kind, "swapped"))) {
        json_t *mt = jk(status, "moved_toolkits");
        moved_some = (mt && mt->type == JSON_ARRAY && json_array_size(mt) > 0) ? 1 : 0;
    }
    if (jeq(js(jk(status, "state")), "claimed") && !moved_some && !jeq(kind, "conflict")) {
        json_t *emailv = jk(status, "account_email");
        const char *where = (emailv && emailv->type == JSON_STRING && emailv->str_val && *emailv->str_val)
                            ? emailv->str_val : "your account";
        const char *old = (json_array_size(lines) > 0) ? js(json_array_get(lines, 0)) : "";
        char prefixed[2048];
        snprintf(prefixed, sizeof(prefixed), "Signed in as %s. %s", where, old ? old : "");
        json_array_set(lines, 0, json_string(prefixed));
    }
    return lines;
}
