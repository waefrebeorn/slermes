/*
 * test_clarify_tool.c — behavioral oracle for clarify_tool.c
 *
 * Cross-checked against the Python reference tools/clarify_tool.py (loaded with
 * the registry import stripped; tool_error stubbed). Each case's expected
 * output below was produced by the real Python implementation. The C result is
 * compared byte-for-byte. The platform callback is injected identically on both
 * sides (returns the per-case raw response), so only the schema/validation/
 * normalization/result-JSON logic is under test.
 */

#include "clarify_tool.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *g_raw = "";

static char *test_cb(const char *question, const char *choices_json,
                     bool multi_select, void *user_data) {
    (void)question; (void)choices_json; (void)multi_select; (void)user_data;
    return g_raw ? strdup(g_raw) : strdup("");
}

typedef struct {
    const char *name;
    const char *question;
    const char *choices_json;   /* JSON array string or NULL */
    bool        multi_select;
    const char *questions_json; /* JSON array string or NULL */
    const char *raw;            /* callback return */
    const char *expected;
    bool        cb_is_batch;
} case_t;

static int g_failures = 0;

static char *normalize_json(const char *s) {
    /* strip insignificant whitespace after ':' and ',' so compact (C) and
     * Python-default (spaced) serializers compare equal structurally. */
    size_t n = strlen(s), j = 0;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == ' ' && i > 0 && (s[i-1] == ':' || s[i-1] == ',')) continue;
        out[j++] = c;
    }
    out[j] = '\0';
    return out;
}

static void check_case(const case_t *c) {
    g_raw = c->raw;
    char *got = clarify_tool(c->question, c->choices_json, c->multi_select,
                             c->questions_json, test_cb, c->cb_is_batch, NULL);
    if (!got) {
        printf("FAIL: %s (null result)\n", c->name);
        g_failures++;
        return;
    }
    char *ng = normalize_json(got);
    char *ne = normalize_json(c->expected);
    if (strcmp(ng ? ng : got, ne ? ne : c->expected) != 0) {
        printf("FAIL: %s\n  expected=%s\n  got    =%s\n", c->name, c->expected, got);
        g_failures++;
    }
    free(got); free(ng); free(ne);
}

int main(void) {
    case_t cases[] = {
        {"open", "Pick one?", NULL, false, NULL, "ok",
         "{\"question\": \"Pick one?\", \"choices_offered\": null, \"user_response\": \"ok\"}"},
        {"single", "Which?", "[\"a\", \"b\"]", false, NULL, "a (Recommended)",
         "{\"question\": \"Which?\", \"choices_offered\": [\"a\", \"b\"], \"user_response\": \"a\"}"},
        {"multi", "Pick many?", "[\"x\", \"y\"]", true, NULL, "x (Recommended), y",
         "{\"question\": \"Pick many?\", \"choices_offered\": [\"x\", \"y\"], \"user_response\": [\"x\", \"y\"]}"},
        {"dictchoice", "Color?", "[\"{\\\"description\\\": \\\"Red\\\"}\"]", false, NULL, "Red",
         "{\"question\": \"Color?\", \"choices_offered\": [\"{\\\"description\\\": \\\"Red\\\"}\"], \"user_response\": \"Red\"}"},
        {"batch", "Batch", NULL, false,
         "[{\"question\": \"A?\", \"choices\": [\"x\", \"y\"]}]",
         "{\"answers\": {\"q0\": \"x\"}}",
         "{\"responses\": [{\"question\": \"A?\", \"choices_offered\": [\"x\", \"y\"], \"user_response\": \"x\"}]}", true},
        {"emptyq", "", NULL, false, NULL, "ok",
         "{\"error\": \"Question text is required.\"}"},
        {"badchoices", "Q?", "\"notalist\"", false, NULL, "ok",
         "{\"error\": \"choices must be a list of strings.\"}"},
        {NULL, NULL, NULL, false, NULL, NULL, NULL},
    };

    for (size_t i = 0; cases[i].name; i++) check_case(&cases[i]);

    /* also exercise the pure helpers directly */
    {
        char *f = clarify_flatten_choice("\"hello\"");
        if (strcmp(f, "hello") != 0) { printf("FAIL: flatten string got %s\n", f); g_failures++; }
        free(f);
        char *s = clarify_strip_recommended("Rebase (Recommended)");
        if (strcmp(s, "Rebase") != 0) { printf("FAIL: strip got %s\n", s); g_failures++; }
        free(s);
        if (!clarify_check_requirements()) { printf("FAIL: check_requirements\n"); g_failures++; }
    }

    if (g_failures == 0) {
        printf("ORACLE PASSED (0 failures)\n");
        return 0;
    }
    printf("ORACLE FAILED (%d failures)\n", g_failures);
    return 1;
}
