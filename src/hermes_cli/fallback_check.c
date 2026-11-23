/*
 * fallback_check.c — C home for upstream Hermes hermes_cli/fallback_check.py.
 *
 * PoP: w12_fallbackch_b8adfb_canonical_model_id @ hermes_cli/fallback_check.py:_canonical_model_id
 * PoP: w12_fallbackch_b8adfb_answered          @ hermes_cli/fallback_check.py:answered_by_requested_model
 * PoP: w12_fallbackch_b8adfb_check_status      @ hermes_cli/fallback_check.py:EntryCheck.ready/.status
 * PoP: w12_fallbackch_b8adfb_secret_values     @ hermes_cli/fallback_check.py:_secret_values
 *
 * Pure model-id / verdict helpers. No network / provider resolution.
 */
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w12_fallbackch_b8adfb.h"

/* _VERTEX_PUBLISHER_PATH.sub(r"\1/", model.strip().lower()) */
static void canonical(const char *raw, char *out, size_t cap) {
    const char *s = raw ? raw : "";
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char buf[4096];
    size_t k = 0;
    for (size_t i = 0; i < n && k + 1 < sizeof buf; ++i)
        buf[k++] = (char)tolower((unsigned char)s[i]);
    buf[k] = 0;

    size_t p = 0;
    if (strncmp(buf, "projects/", 9) == 0) {
        size_t a = 9;
        while (buf[a] && buf[a] != '/') a++;
        if (a > 9 && buf[a] == '/' && strncmp(buf + a + 1, "locations/", 10) == 0) {
            size_t b = a + 1 + 10;
            size_t c = b;
            while (buf[c] && buf[c] != '/') c++;
            if (c > b && buf[c] == '/') p = c + 1;
        }
    }
    if (strncmp(buf + p, "publishers/", 11) == 0) {
        size_t g = p + 11, e = g;
        while (buf[e] && buf[e] != '/') e++;
        if (e > g && buf[e] == '/' && strncmp(buf + e, "/models/", 8) == 0) {
            snprintf(out, cap, "%.*s/%s", (int)(e - g), buf + g, buf + e + 8);
            return;
        }
    }
    snprintf(out, cap, "%s", buf);
}

json_t *w12_fallbackch_b8adfb_canonical_model_id(const char *model) {
    char out[4096];
    canonical(model, out, sizeof out);
    return json_string(out);
}

static const char *after_last_slash(const char *s) {
    const char *p = strrchr(s, '/');
    return p ? p + 1 : s;
}

static int take_digits(const char **p, int max) {
    int n = 0;
    while (**p >= '0' && **p <= '9' && n < max) { (*p)++; n++; }
    return n;
}

/* fullmatch(re.escape(want) + SNAPSHOT_SUFFIX, got). */
static int snap_match(const char *want, const char *got) {
    size_t wl = strlen(want), gl = strlen(got);
    if (gl < wl || strncmp(got, want, wl) != 0) return 0;
    const char *r = got + wl;
    if (!*r) return 1;
    if (!strcmp(r, "-latest")) return 1;
    if (*r != '-') return 0;
    const char *d = r + 1;
    if (take_digits(&d, 4) != 4) return 0;
    if (*d == 0) return 1;                          /* -\d{4} */
    if (*d == '-') d++;
    if (take_digits(&d, 2) != 2) return 0;
    if (*d == '-') d++;
    if (take_digits(&d, 2) != 2) return 0;
    return *d == 0;                                 /* -\d{4}-?\d{2}-?\d{2} */
}

json_t *w12_fallbackch_b8adfb_answered(const char *requested, const char *returned) {
    if (!returned || !*returned) return json_bool(1);
    char want[4096], got[4096];
    canonical(requested, want, sizeof want);
    canonical(returned, got, sizeof got);

    /* partition(":"): compare tags, then compare the ids WITHOUT the tag. */
    char want_tag[1024] = "", got_tag[1024] = "";
    char *wc = strchr(want, ':');
    if (wc) { snprintf(want_tag, sizeof want_tag, "%s", wc + 1); *wc = 0; }
    char *gc = strchr(got, ':');
    if (gc) { snprintf(got_tag, sizeof got_tag, "%s", gc + 1); *gc = 0; }
    if (*got_tag && strcmp(got_tag, want_tag) != 0) return json_bool(0);

    if (!strchr(want, '/') || !strchr(got, '/')) {
        char wtmp[4096], gtmp[4096];
        snprintf(wtmp, sizeof wtmp, "%s", after_last_slash(want));
        snprintf(gtmp, sizeof gtmp, "%s", after_last_slash(got));
        snprintf(want, sizeof want, "%s", wtmp);
        snprintf(got, sizeof got, "%s", gtmp);
    }
    return json_bool(snap_match(want, got) ? 1 : 0);
}

json_t *w12_fallbackch_b8adfb_check_status(int configured, int auth_ready,
                                           int inference, const char *error_class) {
    int inf_false = (inference == 2);
    int ready = configured && auth_ready && !inf_false;
    char status[1024];
    if (ready) snprintf(status, sizeof status, "%s", (inference == 1) ? "ready" : "ready (static)");
    else snprintf(status, sizeof status, "%s", error_class ? error_class : "");
    json_t *out = json_object();
    json_set(out, "ready", json_bool(ready));
    json_set(out, "status", json_string(status));
    return out;
}

json_t *w12_fallbackch_b8adfb_secret_values(const json_t *candidates) {
    json_t *out = json_array();
    if (!candidates || candidates->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(candidates); ++i) {
        const json_t *v = json_get(candidates, i);
        if (v && v->type == JSON_STRING && v->str_val && strlen(v->str_val) >= 8)
            json_append(out, json_copy(v));
    }
    return out;
}
