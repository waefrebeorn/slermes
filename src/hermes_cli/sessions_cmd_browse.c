/*
 * sessions_cmd_browse.c -- faithful C11 port of the deterministic row helpers
 * in hermes_cli/sessions_cmd_browse.py (wave18 gate w18_sessionscm_da8730).
 *
 * PoP: w18_sessionscm_da8730_status_tag @ hermes_cli/sessions_cmd_browse.py:_session_status_tag
 * PoP: w18_sessionscm_da8730_label @ hermes_cli/sessions_cmd_browse.py:_label
 * PoP: w18_sessionscm_da8730_clip @ hermes_cli/sessions_cmd_browse.py:_clip
 * PoP: w18_sessionscm_da8730_msgs_str @ hermes_cli/sessions_cmd_browse.py:_msgs_str
 * PoP: w18_sessionscm_da8730_match @ hermes_cli/sessions_cmd_browse.py:_match
 *
 * The curses/numbered-picker UI stays in Python; only the pure text helpers
 * are ported. All char* results are heap allocated and owned by the caller.
 */
#include "noc_w18_sessionscm_da8730.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s)
{
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* Python str.strip(): drop ASCII whitespace at both ends. */
static char *strip_dup(const char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';                                  /* NUL at new length */
    return out;
}

/* Python str.lower() for the ASCII space the picker filters over. */
static char *lower_dup(const char *s)
{
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
    }
    out[n] = '\0';
    return out;
}

char *w18_sessionscm_da8730_status_tag(const char *status)
{
    const char *s = status ? status : "";           /* status or "" */
    if (!strcmp(s, "complete")) return dupstr("done");
    if (!strcmp(s, "interrupted")) return dupstr("intr");
    if (!strcmp(s, "error")) return dupstr("err");
    if (!strcmp(s, "empty")) return dupstr("empty");
    return dupstr("-");
}

char *w18_sessionscm_da8730_label(json_t *session)
{
    char *title = strip_dup(json_get_str(session, "title", ""));
    if (*title) return title;
    free(title);
    char *preview = strip_dup(json_get_str(session, "preview", ""));
    if (*preview) return preview;
    free(preview);
    return dupstr(json_get_str(session, "id", ""));
}

char *w18_sessionscm_da8730_clip(const char *text, int n)
{
    const char *s = text ? text : "";
    size_t len = strlen(s);
    if ((int)len <= n) return dupstr(s);
    int k = n - 3;
    int take;
    if (k >= 0) {
        take = k;
    } else {
        take = (int)len + k;                        /* Python negative slice */
        if (take < 0) take = 0;
    }
    if (take > (int)len) take = (int)len;
    char *out = (char *)malloc((size_t)take + 4);
    if (!out) return NULL;
    memcpy(out, s, (size_t)take);
    memcpy(out + take, "...", 3);
    out[take + 3] = '\0';
    return out;
}

char *w18_sessionscm_da8730_msgs_str(json_t *session)
{
    json_t *m = json_obj_get(session, "message_count");
    if (m && m->type == JSON_BOOL)
        return dupstr(m->bool_val ? "True" : "False");  /* isinstance(True, int) */
    if (m && m->type == JSON_NUMBER) {
        double d = m->num_val;
        if (isfinite(d) && d == floor(d)) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.0f", d);
            return dupstr(buf);
        }
        return dupstr("-");                          /* non-integral float */
    }
    return dupstr("-");
}

bool w18_sessionscm_da8730_match(json_t *session, const char *query)
{
    char *q = lower_dup(query ? query : "");
    int found = 0;
    static const char *fields[] = {"title", "preview", "source"};
    for (size_t i = 0; i < 3; i++) {
        char *f = lower_dup(json_get_str(session, fields[i], ""));
        if (f && strstr(f, q)) found = 1;
        free(f);
    }
    char *id = lower_dup(json_get_str(session, "id", ""));
    if (id && strstr(id, q)) found = 1;
    free(id);
    free(q);
    return found ? true : false;
}
