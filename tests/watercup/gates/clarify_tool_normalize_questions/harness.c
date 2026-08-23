/* gate: clarify_tool_normalize_questions — faithful traced copy of
 * src/tools/clarify_tool.c::clarify_normalize_questions
 * (LIVE twin: tools/clarify_tool.py::_normalize_questions).
 *
 * Branch ladder (must match twin.py EXACTLY):
 *   B1 unparseable JSON            -> V(error)
 *   B2 not an array                -> V(error)
 *   B3 empty array                 -> V("null")
 *   B4 more than MAX_QUESTIONS(5)  -> V(error)
 *   per item: LOOP, then
 *   B5 item is a bare string       -> treated as {"question": item}
 *   B6 item not an object          -> V(error)
 *   B7 blank question text         -> V(error)
 *   B8 "choices" key present -> {
 *        B9 choices not a list     -> V(error)
 *        per choice: LOOP + B(skip empty flatten) }
 *   B10 all choices flattened away -> no choices
 *   B11 model "id" present (stripped)
 *   V per-item summary "qid|id|question|nchoices|multiselect"
 *
 * Input restriction (documented): choice elements are JSON strings only, so
 * _flatten_choice is the identity on them; dict/list choice coercion and the
 * (Recommended)-marking path are NOT exercised by this gate.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"
#include "json.h"

#define MAXQ 5
#define MAXC 4

static char *xstrdup_(const char *s) { size_t n = strlen(s) + 1; char *r = malloc(n); memcpy(r, s, n); return r; }

static void strip_inplace(char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    if (a) memmove(s, a ? s + a : s, b - a);
    s[b - a] = '\0';
}

/* flatten one already-parsed choice node; string-choices only exercised */
static void flatten_choice_str(const json_t *c, char *out, size_t outsz) {
    out[0] = '\0';
    if (!c) return;
    if (c->type == JSON_STRING && c->str_val) {
        snprintf(out, outsz, "%s", c->str_val);
        strip_inplace(out);
    }
}

static void emit_err(wcup_t *wc, const char *msg) {
    wcup_valstr(wc, msg);
}

static void wc_trace_one(const char *questions_json) {
    WCUP_BEGIN("normalize_questions");
    json_t *node = json_parse(questions_json ? questions_json : "", NULL);
    if (!node || node->type != JSON_ARRAY) {           /* B1/B2 */
        WCUP_BRANCH(1);
        if (node && node->type != JSON_ARRAY)
            emit_err(&_wc, "questions must be an array of question objects.");
        else
            emit_err(&_wc, "questions must be an array of question objects.");
        goto out;
    }
    WCUP_BRANCH(0);
    {
        size_t n = json_len(node);
        if (n == 0) {                                   /* B3 */
            WCUP_BRANCH(1);
            emit_err(&_wc, "null");
            goto out;
        }
        WCUP_BRANCH(0);
        if (n > MAXQ) {                                 /* B4 */
            WCUP_BRANCH(1);
            emit_err(&_wc, "questions supports at most 5 items.");
            goto out;
        }
        WCUP_BRANCH(0);
        for (size_t index = 0; index < n; index++) {
            WCUP_LOOP_TICK();
            json_t *item = json_get(node, index);
            char qtext[512] = "";
            const char *model_id = "";
            bool has_choices = false;
            const char *choice_strs[16]; size_t ccount = 0;
            bool ms = false;

            if (item && item->type == JSON_STRING) {    /* B5 */
                WCUP_BRANCH(1);
                snprintf(qtext, sizeof qtext, "%s", item->str_val ? item->str_val : "");
                strip_inplace(qtext);
            } else {
                WCUP_BRANCH(0);
                if (!item || item->type != JSON_OBJECT) { /* B6 */
                    WCUP_BRANCH(1);
                    char buf[128];
                    snprintf(buf, sizeof buf,
                             "{\"error\": \"questions[%zu] must be an object with a 'question'.\"}",
                             index);
                    emit_err(&_wc, buf);
                    goto out;
                }
                WCUP_BRANCH(0);
                json_t *qnode = json_obj_get(item, "question");
                if (qnode && qnode->type == JSON_STRING && qnode->str_val)
                    snprintf(qtext, sizeof qtext, "%s", qnode->str_val);
                strip_inplace(qtext);
            }
            if (!qtext[0]) {                            /* B7 */
                WCUP_BRANCH(1);
                char buf[128];
                snprintf(buf, sizeof buf,
                         "{\"error\": \"questions[%zu].question must be non-empty text.\"}", index);
                emit_err(&_wc, buf);
                goto out;
            }
            WCUP_BRANCH(0);

            json_t *choices_node = json_obj_get(item, "choices");
            if (choices_node) {                          /* B8 */
                WCUP_BRANCH(1);
                if (choices_node->type != JSON_ARRAY) {  /* B9 */
                    WCUP_BRANCH(1);
                    char buf[128];
                    snprintf(buf, sizeof buf,
                             "{\"error\": \"questions[%zu].choices must be a list.\"}", index);
                    emit_err(&_wc, buf);
                    goto out;
                }
                WCUP_BRANCH(0);
                size_t cn = json_len(choices_node);
                for (size_t i = 0; i < cn && ccount < 16; i++) {
                    WCUP_LOOP_TICK();
                    char flat[512];
                    flatten_choice_str(json_get(choices_node, i), flat, sizeof flat);
                    if (!flat[0]) {                      /* skip-empty */
                        WCUP_BRANCH(1);
                        continue;
                    }
                    WCUP_BRANCH(0);
                    choice_strs[ccount++] = xstrdup_(flat);
                }
                if (ccount > MAXC) ccount = MAXC;
                if (ccount == 0) {                       /* B10 */
                    WCUP_BRANCH(1);
                    has_choices = false;
                } else {
                    WCUP_BRANCH(0);
                    has_choices = true;
                }
            } else {
                WCUP_BRANCH(0);
                WCUP_BRANCH(0);                          /* B9 not taken */
                WCUP_BRANCH(0);                          /* B10 not taken */
            }

            json_t *id_node = json_obj_get(item, "id");
            char idbuf[256] = "";
            if (id_node && id_node->type == JSON_STRING && id_node->str_val) {
                snprintf(idbuf, sizeof idbuf, "%s", id_node->str_val);
                strip_inplace(idbuf);
            }
            model_id = idbuf;
            bool has_id = model_id[0] != '\0';
            WCUP_BRANCH(has_id);                         /* B11 */

            json_t *ms_node = json_obj_get(item, "multi_select");
            ms = ms_node && ms_node->type == JSON_BOOL && ms_node->bool_val && has_choices;

            char summ[1024];
            snprintf(summ, sizeof summ, "q%zu|%s|%s|%zu|%s",
                     index, has_id ? model_id : "", qtext,
                     (size_t)(has_choices ? ccount : 0), ms ? "True" : "False");
            WCUP_VALSTR(summ);
            for (size_t i = 0; i < ccount; i++) free((void *)choice_strs[i]);
        }
    }
out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("normalize_questions");
    }
    if (node) json_free(node);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_clarify <questions-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        wc_trace_one(argv[i]);
    return 0;
}
