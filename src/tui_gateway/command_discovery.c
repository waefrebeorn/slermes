/*
 * command_discovery.c — C home for upstream Hermes tui_gateway/command_discovery.py.
 *
 * PoP: w18_commanddis_8a156e_item @ tui_gateway/command_discovery.py:_item
 * PoP: w18_commanddis_8a156e_details_root_meta @ tui_gateway/command_discovery.py:_details_root_meta
 * PoP: w18_commanddis_8a156e_details_completions @ tui_gateway/command_discovery.py:_details_completions
 *
 * The three mirrored builders are pure (no import_module, no session runtime):
 * they only shape slash-catalog rows and the /details completion table.
 */
#include "noc_w18_commanddis_8a156e.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const DETAILS_SECTIONS[] = {"thinking", "tools", "subagents", "activity"};
static const char *const DETAILS_MODES[] = {"hidden", "collapsed", "expanded"};
#define N_SECTIONS 4
#define N_MODES 3

static char *dupstr(const char *s) {
    size_t n;
    char *d;
    if (!s) return NULL;
    n = strlen(s) + 1;
    d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static int in_list(const char *c, const char *const *list, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i)
        if (!strcmp(c, list[i])) return 1;
    return 0;
}

static char *lower_dup(const char *s) {
    size_t n = strlen(s), i;
    char *d = (char *)malloc(n + 1);
    if (!d) return NULL;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        d[i] = (char)(c >= 'A' && c <= 'Z' ? (c + 32) : c);
    }
    d[n] = '\0';
    return d;
}

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *strip_dup(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    char *d;
    while (a < b && is_ws((unsigned char)s[a])) a++;
    while (b > a && is_ws((unsigned char)s[b - 1])) b--;
    d = (char *)malloc(b - a + 1);
    if (!d) return NULL;
    memcpy(d, s + a, b - a);
    d[b - a] = '\0';
    return d;
}

/* Python str.split() with no args: runs of whitespace dropped. */
static size_t split_ws(const char *s, char ***tokens_out) {
    size_t n = strlen(s), i = 0, count = 0, cap = 8;
    char **toks = (char **)malloc(sizeof(char *) * cap);
    if (!toks) { *tokens_out = NULL; return 0; }
    while (i < n) {
        size_t j;
        char *t;
        while (i < n && is_ws((unsigned char)s[i])) i++;
        if (i >= n) break;
        j = i;
        while (j < n && !is_ws((unsigned char)s[j])) j++;
        if (count == cap) {
            size_t nc = cap * 2;
            char **nt = (char **)realloc(toks, sizeof(char *) * nc);
            if (!nt) break;
            toks = nt; cap = nc;
        }
        t = (char *)malloc(j - i + 1);
        if (!t) break;
        memcpy(t, s + i, j - i);
        t[j - i] = '\0';
        toks[count++] = t;
        i = j;
    }
    *tokens_out = toks;
    return count;
}

static void free_tokens(char **toks, size_t n) {
    size_t i;
    if (!toks) return;
    for (i = 0; i < n; ++i) free(toks[i]);
    free(toks);
}

static json_t *make_item(const char *text, const char *meta) {
    json_t *o = json_object();
    json_set(o, "text", json_string(text ? text : ""));
    json_set(o, "display", json_string(text ? text : ""));
    json_set(o, "meta", json_string(meta ? meta : ""));
    return o;
}

static const char *root_meta(const char *candidate) {
    if (in_list(candidate, DETAILS_SECTIONS, N_SECTIONS)) return "section override";
    if (!strcmp(candidate, "cycle")) return "cycle global mode";
    return "global mode";
}

json_t *w18_commanddis_8a156e_item(const char *text, const char *meta, const char *display) {
    json_t *o = json_object();
    json_set(o, "text", json_string(text ? text : ""));
    json_set(o, "display", json_string((display != NULL) ? display : (text ? text : "")));
    json_set(o, "meta", json_string(meta ? meta : ""));
    return o;
}

json_t *w18_commanddis_8a156e_details_root_meta(const char *candidate) {
    return json_string(root_meta(candidate ? candidate : ""));
}

json_t *w18_commanddis_8a156e_details_completions(const char *text) {
    const char *s = text ? text : "";
    char *low, *stripped = NULL, *body = NULL, *fst = NULL, *fstlow = NULL;
    char **parts = NULL;
    size_t nparts = 0, slen, i;
    int trailing;
    json_t *out;

    low = lower_dup(s);
    if (!low) return json_null();
    if (strncmp(low, "/details", 8) != 0) { free(low); return json_null(); }

    stripped = strip_dup(s);
    if (stripped && *stripped) {
        size_t flen = 0;
        while (stripped[flen] && !is_ws((unsigned char)stripped[flen])) flen++;
        fst = (char *)malloc(flen + 1);
        if (!fst) { free(stripped); free(low); return json_null(); }
        memcpy(fst, stripped, flen);
        fst[flen] = '\0';
        fstlow = lower_dup(fst);
        if (fstlow && (strlen(fstlow) > 8 || strncmp("/details", fstlow, strlen(fstlow)) != 0)) {
            free(fstlow); free(fst); free(stripped); free(low);
            return json_null();
        }
    }
    free(fstlow); free(fst); free(stripped); free(low);

    slen = strlen(s);
    trailing = (slen > 0 && s[slen - 1] == ' ');
    body = dupstr(s + 8);
    if (!body) return json_null();
    if (body[0] == ' ') memmove(body, body + 1, strlen(body)); /* removeprefix(" ") */
    nparts = split_ws(body, &parts);
    out = json_array();

    if (body[0] == '\0' || (nparts == 0 && trailing)) {
        const char *lead = trailing ? "" : " ";
        char *buf;
        size_t k;
        for (i = 0; i < N_MODES; ++i) {
            buf = (char *)malloc(strlen(lead) + strlen(DETAILS_MODES[i]) + 1);
            if (buf) { snprintf(buf, strlen(lead) + strlen(DETAILS_MODES[i]) + 1, "%s%s", lead, DETAILS_MODES[i]);
                       json_append(out, make_item(buf, root_meta(DETAILS_MODES[i]))); free(buf); }
        }
        buf = (char *)malloc(strlen(lead) + 6);
        if (buf) { snprintf(buf, strlen(lead) + 6, "%scycle", lead);
                   json_append(out, make_item(buf, root_meta("cycle"))); free(buf); }
        for (k = 0; k < N_SECTIONS; ++k) {
            buf = (char *)malloc(strlen(lead) + strlen(DETAILS_SECTIONS[k]) + 1);
            if (buf) { snprintf(buf, strlen(lead) + strlen(DETAILS_SECTIONS[k]) + 1, "%s%s", lead, DETAILS_SECTIONS[k]);
                       json_append(out, make_item(buf, root_meta(DETAILS_SECTIONS[k]))); free(buf); }
        }
    } else if (nparts == 1 && !trailing) {
        char *prefix = lower_dup(parts[0]);
        for (i = 0; i < N_MODES; ++i)
            if (!strncmp(DETAILS_MODES[i], prefix, strlen(prefix)) && strcmp(DETAILS_MODES[i], prefix))
                json_append(out, make_item(DETAILS_MODES[i], root_meta(DETAILS_MODES[i])));
        if (!strncmp("cycle", prefix, strlen(prefix)) && strcmp("cycle", prefix))
            json_append(out, make_item("cycle", root_meta("cycle")));
        for (i = 0; i < N_SECTIONS; ++i)
            if (!strncmp(DETAILS_SECTIONS[i], prefix, strlen(prefix)) && strcmp(DETAILS_SECTIONS[i], prefix))
                json_append(out, make_item(DETAILS_SECTIONS[i], root_meta(DETAILS_SECTIONS[i])));
        free(prefix);
    } else {
        char *section = nparts > 0 ? lower_dup(parts[0]) : dupstr("");
        if (in_list(section, DETAILS_SECTIONS, N_SECTIONS)) {
            char *modes[4];
            size_t m, nmodes = 0;
            char *meta_buf;
            for (i = 0; i < N_MODES; ++i) modes[nmodes++] = (char *)DETAILS_MODES[i];
            modes[nmodes++] = "reset";
            if (nparts == 1) {
                for (m = 0; m < nmodes; ++m) {
                    meta_buf = (char *)malloc(strlen(section) + 32);
                    if (!strcmp(modes[m], "reset")) snprintf(meta_buf, strlen(section) + 32, "clear %s override", section);
                    else snprintf(meta_buf, strlen(section) + 32, "set %s", section);
                    json_append(out, make_item(modes[m], meta_buf));
                    free(meta_buf);
                }
            } else if (nparts == 2 && !trailing) {
                char *prefix = lower_dup(parts[1]);
                for (m = 0; m < nmodes; ++m) {
                    if (!strncmp(modes[m], prefix, strlen(prefix)) && strcmp(modes[m], prefix)) {
                        meta_buf = (char *)malloc(strlen(section) + 32);
                        if (!strcmp(modes[m], "reset")) snprintf(meta_buf, strlen(section) + 32, "clear %s override", section);
                        else snprintf(meta_buf, strlen(section) + 32, "set %s", section);
                        json_append(out, make_item(modes[m], meta_buf));
                        free(meta_buf);
                    }
                }
                free(prefix);
            }
        }
        free(section);
    }
    free_tokens(parts, nparts);
    free(body);
    return out;
}
