/* gate: goals_parse_judge_response — faithful traced copy of
 * src/hermes_cli/goals_manager.c::goal_judge_parse_response +
 * goal_judge_extract_json_object + extract_json (verbatim logic).
 * LIVE twin: hermes_cli/goals.py::_parse_judge_response.
 *
 * Branch ladder per input (must match twin.py EXACTLY, same rung order):
 *   B1: raw empty
 *   B2: JSON object extracted from reply
 *   B3: parsed doc is an object
 *   B4: reason field non-empty
 *   B5: explicit "verdict" string field present
 *   B6: legacy "done" value is a string
 *   B7: legacy done truthy
 *   B8: normalized verdict valid {done,continue,wait}
 *   B9: verdict == wait
 *   B10: wait_on_session/session_id/wait_session present
 *   B11: pid > 0
 *   B12: seconds > 0
 *   B13: usable wait target found
 *   VSTR(final verdict); VSTR(directive key or "none")
 * The cup hashes control flow + verdict label + directive key, NOT the
 * formatted reason strings (LIVE repr/truncates them differently).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "hermes_json.h"
#include "watercup.h"

/* ---- verbatim from src/hermes_cli/goals_manager.c ---- */
static char *extract_json(const char *text) {
    if (!text) return NULL;
    char *dup = strdup(text);
    char *p = dup;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (p[0] == '`' && p[1] == '`') {
        char *nl = strchr(p, '\n');
        if (nl) p = nl + 1;
    }
    char *json = NULL;
    char *err = NULL;
    json_t *doc = json_parse(p, &err);
    if (err) { free(err); }
    else if (doc) {
        char *tmp = json_serialize(doc);
        json_free(doc);
        json = tmp;
    }
    if (!json) {
        const char *start = strchr(p, '{');
        if (start) {
            int depth = 0;
            const char *q = start;
            for (; *q; q++) {
                if (*q == '{') depth++;
                else if (*q == '}') { depth--; if (depth == 0) { q++; break; } }
            }
            if (depth == 0 && q > start) {
                size_t len = (size_t)(q - start);
                json = malloc(len + 1);
                memcpy(json, start, len);
                json[len] = '\0';
            }
        }
    }
    free(dup);
    return json;
}

static char *wc_extract_json_object(const char *raw) {
    if (!raw || !raw[0]) return NULL;
    char *text = strdup(raw);
    char *p = text;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t' || p[n-1] == '\n' || p[n-1] == '\r')) n--;
    p[n] = '\0';
    if (p[0] == '`' && p[1] == '`') {
        char *nl = strchr(p, '\n');
        if (nl) { memmove(p, nl + 1, strlen(nl + 1) + 1); }
    }
    char *json = extract_json(p);
    free(text);
    return json;
}

static void decode_escapes(char *s) {
    char *o = s;
    for (char *p = s; *p; p++) {
        if (p[0] == '\\' && p[1] == 'n') { *o++ = '\n'; p++; }
        else if (p[0] == '\\' && p[1] == 't') { *o++ = '\t'; p++; }
        else *o++ = *p;
    }
    *o = '\0';
}

static void run_one(const char *input) {
    char raw[4096];
    snprintf(raw, sizeof raw, "%s", input);
    decode_escapes(raw);

    WCUP_BEGIN("goal_judge_parse_response");

    char *verdict = strdup("continue");
    char *directive_key = strdup("none");
    bool parse_failed = false;

    WCUP_BRANCH(!raw[0]);                                    /* B1 */
    char *json = wc_extract_json_object(raw);
    WCUP_BRANCH(json != NULL);                               /* B2 */
    if (!json) {
        parse_failed = true;
        goto out;
    }
    {
        char *err = NULL;
        json_t *doc = json_parse(json, &err);
        if (err) free(err);
        free(json);
        WCUP_BRANCH(doc && doc->type == JSON_OBJECT);         /* B3 */
        if (!doc || doc->type != JSON_OBJECT) {
            parse_failed = true;
            if (doc) json_free(doc);
            goto out;
        }
        const char *r = json_get_str(doc, "reason", NULL);
        WCUP_BRANCH(r && r[0]);                               /* B4 */

        const char *vr = json_get_str(doc, "verdict", NULL);
        WCUP_BRANCH(vr != NULL);                              /* B5 */
        if (vr) {
            free(verdict);
            verdict = strdup(vr);
            for (char *c = verdict; *c; c++)
                if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 'a');
        } else {
            json_t *done_v = json_obj_get(doc, "done");
            WCUP_BRANCH(done_v && done_v->type == JSON_STRING); /* B6 */
            bool done = false;
            if (done_v) {
                if (done_v->type == JSON_BOOL) done = done_v->bool_val;
                else if (done_v->type == JSON_STRING) {
                    const char *ds = done_v->str_val;
                    done = (strcmp(ds, "true") == 0 || strcmp(ds, "yes") == 0 ||
                            strcmp(ds, "1") == 0 || strcmp(ds, "done") == 0);
                }
            }
            WCUP_BRANCH(done);                                /* B7 */
            free(verdict);
            verdict = strdup(done ? "done" : "continue");
        }
        WCUP_BRANCH(!strcmp(verdict, "done") || !strcmp(verdict, "continue") ||
                    !strcmp(verdict, "wait"));                /* B8 */
        if (strcmp(verdict, "done") && strcmp(verdict, "continue") && strcmp(verdict, "wait")) {
            free(verdict);
            verdict = strdup("continue");
        }

        WCUP_BRANCH(!strcmp(verdict, "wait"));                /* B9 */
        if (!strcmp(verdict, "wait")) {
            const char *sess = json_get_str(doc, "wait_on_session", NULL);
            if (!sess) sess = json_get_str(doc, "session_id", NULL);
            if (!sess) sess = json_get_str(doc, "wait_session", NULL);
            WCUP_BRANCH(sess && sess[0]);                     /* B10 */
            if (sess && sess[0]) {
                free(directive_key); directive_key = strdup("session_id");
            } else {
                long pid = 0;
                json_t *pv = json_obj_get(doc, "wait_on_pid");
                if (!pv) pv = json_obj_get(doc, "pid");
                if (!pv) pv = json_obj_get(doc, "wait_pid");
                if (pv && pv->type == JSON_NUMBER) pid = (long)pv->num_val;
                if (pid <= 0 && pv && pv->type == JSON_STRING) pid = atol(pv->str_val);
                WCUP_BRANCH(pid > 0);                         /* B11 */
                if (pid > 0) {
                    free(directive_key); directive_key = strdup("pid");
                } else {
                    long sec = 0;
                    json_t *sv = json_obj_get(doc, "wait_for_seconds");
                    if (!sv) sv = json_obj_get(doc, "seconds");
                    if (!sv) sv = json_obj_get(doc, "wait_seconds");
                    if (sv && sv->type == JSON_NUMBER) sec = (long)sv->num_val;
                    if (sec <= 0 && sv && sv->type == JSON_STRING) sec = atol(sv->str_val);
                    WCUP_BRANCH(sec > 0);                     /* B12 */
                    if (sec > 0) {
                        free(directive_key); directive_key = strdup("seconds");
                    }
                }
            }
            WCUP_BRANCH(strcmp(directive_key, "none") != 0);  /* B13 */
            if (!strcmp(directive_key, "none")) {
                free(verdict); verdict = strdup("continue");
            }
        }
        json_free(doc);
    }

out:
    WCUP_VALSTR(verdict);
    WCUP_VALSTR(directive_key);
    WCUP_BRANCH(parse_failed);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    free(verdict); free(directive_key);
    WCUP_END("goal_judge_parse_response");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s goal_judge_parse_response <inputs...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
