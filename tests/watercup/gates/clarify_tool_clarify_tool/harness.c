/* gate: clarify_tool_clarify_tool — traced ladder around the REAL
 * src/tools/clarify_tool.c::clarify_tool validation path (callback == NULL,
 * the deterministic no-UI path). Linked via extra_src; this wrapper mirrors
 * the exact branch order of clarify_tool()'s cb==NULL ladder:
 *
 *   B1  questions given?
 *       B2 normalize error -> error JSON
 *       B3 normalized non-empty (cb NULL) -> not-available
 *          (else fall through to single-question path)
 *   B4 !question
 *   B5 !stripped question
 *   B6 choices given?
 *       B7 not an array -> "choices must be a list of strings."
 *       LOOP flatten each choice (drop empties)
 *       B8 count > MAX_CHOICES -> trim
 *       B9 any choices kept?
 *   B10 callback NULL -> not-available
 *   V   result JSON string
 *
 * LIVE twin: tools/clarify_tool.py::clarify_tool(callback=None).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "watercup.h"
#include "json.h"
#include "clarify_tool.h"

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void str_strip_inplace(char *s) {
    char *a = s, *b = s + strlen(s);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b - 1))) b--;
    if (a != s) memmove(s, a, b - a);
    s[b - a] = '\0';
}

#define ERR_REQUIRED   "{\"error\": \"Question text is required.\"}"
#define ERR_NOT_LIST   "{\"error\": \"choices must be a list of strings.\"}"
#define ERR_NO_UI      "{\"error\": \"Clarify tool is not available in this execution context.\"}"

static const char *g_res = NULL;

static void wc_clarify_tool(const char *question, const char *choices_json,
                            const char *questions_json) {
    WCUP_BEGIN("clarify_tool");
    const char *res = NULL;

    /* batch path */
    if (questions_json) {                                  /* B1 */
        WCUP_BRANCH(1);
        char *normalized = clarify_normalize_questions(questions_json);
        int is_err = normalized && normalized[0] == '{' &&
                     normalized[1] == '\"' && normalized[2] == 'e';
        if (is_err) {                                      /* B2 */
            WCUP_BRANCH(1);
            res = normalized;
            goto out;
        }
        WCUP_BRANCH(0);
        if (normalized && strcmp(normalized, "null") != 0) {
            WCUP_BRANCH(1);                                /* B3 */
            free(normalized);
            res = ERR_NO_UI;
            goto out;
        }
        WCUP_BRANCH(0);                                    /* fall through */
        free(normalized);
    } else {
        WCUP_BRANCH(0);
    }

    if (!question || !question[0]) {                       /* B4 */
        WCUP_BRANCH(1);
        res = ERR_REQUIRED;
        goto out;
    }
    WCUP_BRANCH(0);
    char *q = xstrdup(question);
    str_strip_inplace(q);
    if (!q[0]) {                                           /* B5 */
        WCUP_BRANCH(1);
        free(q);
        res = ERR_REQUIRED;
        goto out;
    }
    WCUP_BRANCH(0);

    /* validate + flatten choices */
    bool has_choices = false;
    if (choices_json) {                                    /* B6 */
        WCUP_BRANCH(1);
        json_t *cn = json_parse(choices_json, NULL);
        if (!cn || cn->type != JSON_ARRAY) {               /* B7 */
            WCUP_BRANCH(1);
            if (cn) json_free(cn);
            free(q);
            res = ERR_NOT_LIST;
            goto out;
        }
        WCUP_BRANCH(0);
        size_t len = json_len(cn);
        size_t ccount = 0;
        for (size_t i = 0; i < len; i++) {                 /* LOOP flatten */
            WCUP_LOOP_TICK();
            json_t *c = json_get(cn, i);
            char *cj = c ? json_serialize(c) : xstrdup("null");
            char *flat = clarify_flatten_choice(cj);
            free(cj);
            bool keep = flat && flat[0];
            WCUP_BRANCH(keep);
            free(flat);
            if (keep)
                ccount++;
        }
        if (ccount > CLARIFY_MAX_CHOICES) {                /* B8 trim */
            WCUP_BRANCH(1);
            ccount = CLARIFY_MAX_CHOICES;
        } else {
            WCUP_BRANCH(0);
        }
        json_free(cn);
        has_choices = ccount > 0;
        WCUP_BRANCH(has_choices);                          /* B9 */
    } else {
        WCUP_BRANCH(0);
    }
    (void)has_choices;

    /* callback is NULL in this gate */                    /* B10 */
    free(q);
    res = ERR_NO_UI;

out:
    if (!res) res = "";
    {
        char cup[32];
        WCUP_VALSTR(res);
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("clarify_tool");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s clarify_tool <input-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        json_t *in = json_parse(argv[i], NULL);
        if (!in || in->type != JSON_OBJECT) {
            fprintf(stderr, "bad input: %s\n", argv[i]);
            if (in) json_free(in);
            return 2;
        }
        json_t *qn = json_obj_get(in, "question");
        json_t *ch = json_obj_get(in, "choices");
        json_t *qs = json_obj_get(in, "questions");
        wc_clarify_tool(
            (qn && qn->type == JSON_STRING) ? qn->str_val : NULL,
            ch->type == JSON_NULL ? NULL : json_serialize(ch),
            qs->type == JSON_NULL ? NULL : json_serialize(qs));
        json_free(in);
    }
    return 0;
}
