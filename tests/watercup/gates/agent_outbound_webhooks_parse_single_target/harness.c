/* gate: agent_outbound_webhooks_parse_single_target — traced copy of
 * src/agent/outbound_webhooks.c::ow_parse_single_target (+ helpers
 * is_tool_scoped_event / ow_resolve_secret, transcribed rung-for-rung).
 * LIVE twin: agent/outbound_webhooks.py::_parse_single_target (+ LIVE
 * _resolve_secret / DEFAULT_TIMEOUT_SECONDS / MAX_TIMEOUT_SECONDS).
 *
 * Input encoding (argv-safe, fields split on ';'):
 *   "<url>;<events>;<matcher>;<timeout>;<senv>;<secret>;<name>"
 *   url    : literal URL, "-" = absent, "@nodict" = raw is NOT a mapping,
 *            "@blank" = empty-string url
 *   events : comma-separated event names, "-" = absent, "@" = empty list,
 *            "@nonlist" = a non-list raw value
 *   matcher: literal, "-" = absent, "@num" = non-string value
 *   timeout: integer, "-" = absent (default), "@bad" = non-int
 *   senv   : secret_env var NAME to look up, "-" = absent (never set in walk)
 *   secret : inline secret literal, "-" = absent
 *   name   : literal string, "-" = absent/non-string (excluded divergence:
 *            LIVE strips names, C does not — walk uses unpadded names)
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: raw not a mapping -> None
 *   B2: url missing/blank -> None
 *   B3: url not http(s) -> None
 *   B4: plain http:// -> warning path
 *   B5: events missing/not-a-list/empty -> None
 *   LOOP copy events (string check per item)
 *   B6: no usable events -> None
 *   LOOP tool-scoped scan
 *   B7: matcher present but no tool-scoped event -> dropped
 *   secret: senv-taken / env-found / inline-found branches
 *   B8: timeout < 1 -> clamp to 1
 *   B9: timeout > max -> clamp to max
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include "watercup.h"

#define OW_DEFAULT_TIMEOUT_SECONDS 10
#define OW_MAX_TIMEOUT_SECONDS     60

static bool is_tool_scoped_event(const char *event) {
    return strcmp(event, "pre_tool_call") == 0 ||
           strcmp(event, "post_tool_call") == 0;
}

static const char *dash(const char *s) { return strcmp(s, "-") == 0 ? NULL : s; }

static void wc_parse_single_target(const char *url_f, const char *events_f,
                                   const char *matcher_f, const char *timeout_f,
                                   const char *senv_f, const char *secret_f,
                                   const char *name_f) {
    WCUP_BEGIN("parse_single_target");
    char res[1024];
    bool is_dict = strcmp(url_f, "@nodict") != 0;

    /* Decode events field into a string list (NULL semantics folded into
     * events_is_list). */
    const char *evlist[32];
    int evcount = 0;
    bool events_is_list;
    if (strcmp(events_f, "-") == 0 || strcmp(events_f, "@nonlist") == 0) {
        events_is_list = false;
    } else if (strcmp(events_f, "@") == 0) {
        events_is_list = true;
    } else {
        events_is_list = true;
        char evbuf[512];
        snprintf(evbuf, sizeof evbuf, "%s", events_f);
        char *save = NULL;
        for (char *tok = strtok_r(evbuf, ",", &save);
             tok && evcount < 32; tok = strtok_r(NULL, ",", &save))
            evlist[evcount++] = tok;
    }

    /* Decode timeout: "@bad"/non-int falls back to the default (LIVE warns
     * then defaults; C json_get_num defaults). */
    bool timeout_bad = timeout_f && strcmp(timeout_f, "@bad") == 0;
    int timeout = (timeout_bad || !timeout_f || strcmp(timeout_f, "-") == 0)
                      ? OW_DEFAULT_TIMEOUT_SECONDS : atoi(timeout_f);

    const char *url = dash(url_f);
    if (url && strcmp(url_f, "@blank") == 0) url = "";
    bool url_blank = url && url[0] == '\0';

    if (!is_dict) {                                    /* B1 */
        WCUP_BRANCH(1);
        snprintf(res, sizeof res, "none");
        WCUP_VALSTR(res);
        goto out;
    }
    WCUP_BRANCH(0);

    if (!url || url_blank) {                           /* B2 */
        WCUP_BRANCH(1);
        snprintf(res, sizeof res, "none");
        WCUP_VALSTR(res);
        goto out;
    }
    WCUP_BRANCH(0);

    if (strncasecmp(url, "http://", 7) != 0 &&
        strncasecmp(url, "https://", 8) != 0) {        /* B3 */
        WCUP_BRANCH(1);
        snprintf(res, sizeof res, "none");
        WCUP_VALSTR(res);
        goto out;
    }
    WCUP_BRANCH(0);

    if (strncasecmp(url, "http://", 7) == 0) {         /* B4 plain http */
        WCUP_BRANCH(1);
    } else {
        WCUP_BRANCH(0);
    }

    if (!events_is_list || evcount == 0) {             /* B5 */
        WCUP_BRANCH(1);
        snprintf(res, sizeof res, "none");
        WCUP_VALSTR(res);
        goto out;
    }
    WCUP_BRANCH(0);

    {
        int kept = 0;
        for (int i = 0; i < evcount; i++) {            /* LOOP copy events */
            WCUP_LOOP_TICK();
            WCUP_BRANCH(true);                         /* item is a string */
            kept++;
        }
        if (kept == 0) {                               /* B6 */
            WCUP_BRANCH(1);
            snprintf(res, sizeof res, "none");
            WCUP_VALSTR(res);
            goto out;
        }
        WCUP_BRANCH(0);

        bool has_tool_scoped = false;
        for (int i = 0; i < evcount; i++) {            /* LOOP scoped scan */
            WCUP_LOOP_TICK();
            if (is_tool_scoped_event(evlist[i])) {
                WCUP_BRANCH(1);
                has_tool_scoped = true;
                break;
            }
            WCUP_BRANCH(0);
        }

        const char *mraw = dash(matcher_f);
        bool matcher_is_str = mraw != NULL && strcmp(matcher_f, "@num") != 0;
        char matcher_clean[256];
        bool have_matcher = false;
        if (matcher_is_str && mraw) {
            const char *p = mraw;
            while (*p == ' ' || *p == '\t') p++;
            if (*p) {
                snprintf(matcher_clean, sizeof matcher_clean, "%s", p);
                have_matcher = true;
            }
        }
        bool matcher_kept = false;
        if (have_matcher && !has_tool_scoped) {        /* B7 dropped */
            WCUP_BRANCH(1);
        } else {
            WCUP_BRANCH(0);
            matcher_kept = have_matcher;
        }

        /* ow_resolve_secret traced */
        const char *sec = NULL;
        const char *env_name = dash(senv_f);
        if (env_name && env_name[0]) {                 /* senv branch */
            WCUP_BRANCH(1);
            const char *env_val = getenv(env_name);
            if (env_val && env_val[0]) {
                WCUP_BRANCH(1);
                sec = env_val;
            } else {
                WCUP_BRANCH(0);                        /* unset -> UNSIGNED */
            }
        } else {
            WCUP_BRANCH(0);
            const char *inline_sec = dash(secret_f);
            if (inline_sec && inline_sec[0]) {
                WCUP_BRANCH(1);
                sec = inline_sec;
            } else {
                WCUP_BRANCH(0);
            }
        }

        if (timeout < 1) {                             /* B8 */
            WCUP_BRANCH(1);
            timeout = 1;
        } else {
            WCUP_BRANCH(0);
        }
        if (timeout > OW_MAX_TIMEOUT_SECONDS) {        /* B9 */
            WCUP_BRANCH(1);
            timeout = OW_MAX_TIMEOUT_SECONDS;
        } else {
            WCUP_BRANCH(0);
        }

        const char *name = dash(name_f);
        if (!name) name = "";

        {
            char evs[512], mbuf[64], sbuf[128];
            evs[0] = '\0';
            for (int i = 0; i < evcount; i++) {
                if (i) strncat(evs, "+", sizeof evs - strlen(evs) - 1);
                strncat(evs, evlist[i], sizeof evs - strlen(evs) - 1);
            }
            snprintf(mbuf, sizeof mbuf, "%s", matcher_kept ? matcher_clean : "~");
            snprintf(sbuf, sizeof sbuf, "%s", sec ? sec : "~");
            snprintf(res, sizeof res, "ok|%s|%s|%s|%s|%d|%s",
                     url, evs, name, mbuf, timeout, sbuf);
        }
        WCUP_VALSTR(res);
    }

out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("parse_single_target");
    }
}

static void run_input(const char *arg) {
    char buf[2048];
    snprintf(buf, sizeof buf, "%s", arg);
    char *fields[7];
    int nf = 0;
    char *save = NULL;
    for (char *tok = strtok_r(buf, ";", &save); tok && nf < 7;
         tok = strtok_r(NULL, ";", &save))
        fields[nf++] = tok;
    while (nf < 7) fields[nf++] = (char *)"-";
    wc_parse_single_target(fields[0], fields[1], fields[2], fields[3],
                           fields[4], fields[5], fields[6]);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s parse_single_target <spec>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) run_input(argv[i]);
    return 0;
}
