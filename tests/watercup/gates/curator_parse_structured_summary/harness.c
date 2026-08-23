/* gate: curator_parse_structured_summary
 * Calls the REAL C port src/agent/curator_ports.c::cur_parse_structured_summary
 * (compiled from source, real libyaml) against LIVE Python agent/curator.py::
 * _parse_structured_summary. Input argv: the LLM final response text.
 *
 * Derived-shape ladder over the returned JSON (must match twin.py):
 *   B1: no input / NULL result
 *   B2: "consolidations" array present and non-empty -> per-item LOOP + V row
 *   B3: "prunings" array present and non-empty       -> per-item LOOP + V row
 *   V rows are canonicalized as "from>into|reason" / "name|reason".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"

char *cur_parse_structured_summary(const char *final_response);

/* out-of-path externals from curator_ports.c (LLM/db paths never traced) */
void apply_automatic_transitions(void){}
char *llm_resolve_provider_env(void){return NULL;}
void *message_new(void){return NULL;}
void message_free(void *p){(void)p;}
char *llm_chat_completion(const char *a, const char *b){(void)a;(void)b;return NULL;}
void llm_response_free(void *p){(void)p;}

static void trace_rows(wcup_t *_wc, const json_t *arr, int kind) {
    size_t n = (arr && arr->type == JSON_ARRAY) ? json_len(arr) : 0;
    wcup_branch(_wc, n > 0);
    for (size_t i = 0; i < n; i++) {
        wcup_loop(_wc);
        json_t *it = json_get(arr, i);
        const char *a = "", *b = "";
        if (it && it->type == JSON_OBJECT) {
            json_t *ja = json_obj_get(it, kind == 0 ? "from" : "name");
            json_t *jb = json_obj_get(it, kind == 0 ? "into" : "reason");
            if (ja && ja->type == JSON_STRING && ja->str_val) a = ja->str_val;
            if (jb && jb->type == JSON_STRING && jb->str_val) b = jb->str_val;
        }
        char row[1024];
        snprintf(row, sizeof row, "%s>%s", a, b);
        wcup_valstr(_wc, row);
    }
}

static void run_one(const char *input) {
    WCUP_BEGIN("parse_structured_summary");
    char *res = cur_parse_structured_summary(input);
    json_t *out = res ? json_parse(res, NULL) : NULL;
    free(res);
    if (!out || out->type != JSON_OBJECT) {              /* B1 */
        WCUP_BRANCH(1);
    } else {
        WCUP_BRANCH(0);
        trace_rows(&_wc, json_obj_get(out, "consolidations"), 0);   /* B2 */
        trace_rows(&_wc, json_obj_get(out, "prunings"), 1);         /* B3 */
    }
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("parse_structured_summary");
    if (out) json_free(out);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_curator <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        run_one(argv[i]);
    return 0;
}
