/* gate: anthropic_adapter_convert_assistant_message
 * Calls the REAL C port src/agent/provider_anthropic.c::
 * anthropic_convert_assistant_message against LIVE Python
 * agent/anthropic_adapter.py::_convert_assistant_message.
 * Input: one assistant-message JSON per argv entry.
 * Inputs avoid the LIVE-only anthropic_content_blocks replay fast path
 * (no C counterpart — documented narrowing in gate.json).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"

json_t *anthropic_convert_assistant_message(const json_t *m);

/* out-of-path externals pulled in by provider_anthropic.c */
void hermes_log(int lvl, const char *tag, const char *fmt, ...) {(void)lvl;(void)tag;(void)fmt;}
void apply_provider_profile(json_t *a, const char *b) {(void)a;(void)b;}
void crypto_pkce_challenge(char *a, size_t b, char *c, size_t d) {(void)a;(void)b;(void)c;(void)d;}
void crypto_pkce_verifier(char *a, size_t b) {(void)a;(void)b;}
int url_host_matches(const char *a, const char *b) {(void)a;(void)b; return 0;}
/* off-traced-path http stack */
void *http_new(void) { return NULL; }
int http_request(void *h) {(void)h; return -1;}
void http_resp_free(void *r) {(void)r;}
void http_free(void *h) {(void)h;}

/* canonical compact serialization matching python json.dumps(sep=(',',':')) */
static char *compact(const json_t *n) { return json_serialize(n); }

static void run_one(const char *msg_json) {
    WCUP_BEGIN("_convert_assistant_message");
    json_t *m = msg_json ? json_parse(msg_json, NULL) : NULL;
    if (!m) m = json_object();

    /* ---- trace LIVE's ladder (coarse rungs, same order both sides) ---- */
    /* replay fast path: anthropic_content_blocks list non-empty */
    json_t *ob = json_obj_get(m, "anthropic_content_blocks");
    WCUP_BRANCH(ob && ob->type == JSON_ARRAY && json_len(ob) > 0);

    /* preserved thinking blocks from reasoning_details */
    json_t *rd = json_obj_get(m, "reasoning_details");
    int rd_list = rd && rd->type == JSON_ARRAY;
    WCUP_BRANCH(rd_list);
    if (rd_list) {
        for (size_t i = 0; i < json_len(rd); i++) {
            json_t *d = json_get(rd, i);
            int ok = d && d->type == JSON_OBJECT;
            const char *bt = ok ? json_get_str(d, "type", "") : "";
            ok = ok && (strcmp(bt, "thinking") == 0 || strcmp(bt, "redacted_thinking") == 0);
            WCUP_BRANCH(ok);
            WCUP_LOOP_TICK();
        }
    }

    json_t *content = json_obj_get(m, "content");
    WCUP_BRANCH(content != NULL && !(content->type == JSON_STRING &&
                                     (!content->str_val || !content->str_val[0])));
    int content_is_array = content && content->type == JSON_ARRAY;
    WCUP_BRANCH(content_is_array);
    json_t *tcs = json_obj_get(m, "tool_calls");
    int has_tcs = tcs && tcs->type == JSON_ARRAY && json_len(tcs) > 0;
    WCUP_BRANCH(has_tcs);
    if (has_tcs) {
        for (size_t i = 0; i < json_len(tcs); i++) {
            json_t *tc = json_get(tcs, i);
            int good = tc && tc->type == JSON_OBJECT && json_obj_get(tc, "function");
            WCUP_BRANCH(good);      /* malformed tc skipped by C, kept-by-default in LIVE — inputs keep all good */
            WCUP_LOOP_TICK();
        }
    }
    json_t *rc = json_obj_get(m, "reasoning_content");
    int rc_str = rc && rc->type == JSON_STRING;
    WCUP_BRANCH(rc_str);
    if (rc_str) {
        /* already-has-thinking guard (inverted injection condition) */
        int already = 0;
        if (rd_list) {
            for (size_t i = 0; i < json_len(rd); i++) {
                json_t *d = json_get(rd, i);
                if (d && d->type == JSON_OBJECT) {
                    const char *bt = json_get_str(d, "type", "");
                    if (strcmp(bt, "thinking") == 0 || strcmp(bt, "redacted_thinking") == 0) { already = 1; break; }
                }
            }
        }
        WCUP_BRANCH(!already);
    }

    json_t *result = anthropic_convert_assistant_message(m);
    json_t *blocks = result ? json_obj_get(result, "content") : NULL;

    /* empty-guard rung: did we land on the (empty) placeholder? */
    int only_placeholder = 0;
    if (blocks && json_len(blocks) == 1) {
        json_t *b0 = json_get(blocks, 0);
        only_placeholder = b0 && b0->type == JSON_OBJECT &&
            strcmp(json_get_str(b0, "text", ""), "(empty)") == 0 &&
            strcmp(json_get_str(b0, "type", ""), "text") == 0;
    }
    WCUP_BRANCH(only_placeholder);

    json_t *cc = json_obj_get(m, "cache_control");
    WCUP_BRANCH(cc && cc->type == JSON_OBJECT);

    char *ser = blocks ? compact(blocks) : strdup("null");
    WCUP_VALSTR(ser ? ser : "null");

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_convert_assistant_message");
    free(ser);
    json_free(result);
    json_free(m);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _convert_assistant_message <msg-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
