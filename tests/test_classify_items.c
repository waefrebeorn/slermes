/*
 * test_classify_items.c — unit tests for the pure cron/scripts/classify_items.py
 * helpers. Invariants derived from a Python oracle.
 */

#include "classify_items_helpers.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

static void eq(const char *label, const char *got, const char *exp)
{
    if (!got || strcmp(got, exp) != 0) {
        printf("FAIL: %s\n  expected=[%s]\n  got    =[%s]\n", label, exp, got ? got : "(null)");
        g_fail++;
    } else printf("ok: %s -> [%s]\n", label, got ? got : "");
}

static void check_parse(const char *label, const char *content, int n_items,
                        int exp_count, int exp_idx0, int exp_score0, const char *exp_reason0)
{
    int cnt = 0;
    classify_score_t *s = cron_classify_parse_scores(content, n_items, &cnt);
    if (cnt != exp_count) {
        printf("FAIL: %s count=%d want %d\n", label, cnt, exp_count);
        g_fail++;
    } else if (exp_count > 0) {
        if (s[0].index != exp_idx0 || s[0].score != exp_score0 ||
            (exp_reason0 && strcmp(s[0].reason, exp_reason0) != 0)) {
            printf("FAIL: %s [idx=%d score=%d reason=%s]\n", label, s[0].index, s[0].score, s[0].reason);
            g_fail++;
        } else printf("ok: %s -> [%d]=%d/%s\n", label, s[0].index, s[0].score, s[0].reason);
    } else printf("ok: %s -> empty\n", label);
    cron_classify_free_scores(s, cnt);
}

int main(void)
{
    char *id;

    /* _item_id */
    json_t *o = json_object();
    json_set(o, "id", json_string("abc"));
    id = cron_classify_item_id(o, 3); eq("id(id)", id, "abc"); free(id);
    json_free(o);

    o = json_object();
    json_set(o, "url", json_string("http://x"));
    id = cron_classify_item_id(o, 0); eq("id(url)", id, "http://x"); free(id);
    json_free(o);

    id = cron_classify_item_id(NULL, 7); eq("id(none)", id, "item-7"); free(id);

    o = json_object();
    json_set(o, "guid", json_string("G1"));
    json_set(o, "id", json_string("I1"));
    id = cron_classify_item_id(o, 0); eq("id(prefer id)", id, "I1"); free(id);
    json_free(o);

    /* _parse_scores */
    check_parse("p1", "[{\"index\":0,\"score\":9,\"reason\":\"boss\"}]", 2, 1, 0, 9, "boss");
    check_parse("p2", "```json\n[{\"index\":1,\"score\":3,\"reason\":\"low\"}]\n```", 3, 1, 1, 3, "low");
    check_parse("p3", "garbage [{\"index\":0,\"score\":5,\"reason\":\"x\"}] trailing", 1, 1, 0, 5, "x");
    check_parse("p4", "[{\"index\":99,\"score\":5}]", 2, 0, 0, 0, NULL);     /* out-of-range dropped */
    check_parse("p5", "not json at all", 2, 0, 0, 0, NULL);                 /* invalid -> empty */
    check_parse("p6", "[{\"index\":0,\"score\":2,\"reason\":\"a\"},{\"index\":1,\"score\":8,\"reason\":\"b\"}]", 5, 2, 0, 2, "a");

    /* _build_prompt (substring invariants) */
    json_t *items = json_array();
    json_t *it = json_object();
    json_set(it, "title", json_string("Inbox zero broken"));
    json_set(it, "from", json_string("boss@corp"));
    json_append(items, it);
    char *prompt = cron_classify_build_prompt(items, "Urgent if from manager");
    if (!prompt || !strstr(prompt, "USER IMPORTANCE CRITERIA:") ||
        !strstr(prompt, "Urgent if from manager") || !strstr(prompt, "ITEMS:") ||
        !strstr(prompt, "[0]") || !strstr(prompt, "Inbox zero broken") ||
        !strstr(prompt, "Return the JSON array of scores now")) {
        printf("FAIL: build_prompt invariants\n  [%s]\n", prompt ? prompt : "(null)");
        g_fail++;
    } else printf("ok: build_prompt invariants\n");
    free(prompt);
    json_free(items);

    if (g_fail) { printf("\n%d FAIL\n", g_fail); return 1; }
    printf("\nALL PASSED\n");
    return 0;
}
