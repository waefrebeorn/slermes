/* gate: native_compaction_prune_pre_checkpoint_items — traced copy of
 * src/agent/native_compaction.c::native_compaction_prune_pre_checkpoint_items
 * (+ its static helpers _user_item_text / _approx_tokens, faithfully copied;
 * libjson is linked via extra_src for parsing).
 * LIVE twin: agent/native_compaction.py::prune_pre_checkpoint_items.
 *
 * Branch ladder (twin.py mirrors EXACTLY, same rung order):
 *   B1: items is a non-empty array          (else -> returned as-is)
 *   LOOP scan for checkpoints (tick per item)
 *   B2: checkpoint found                    (else -> returned as-is)
 *   LOOP backward contiguous-run extension (tick per step)
 *   LOOP retention walk, newest-first (tick per pre item):
 *     B3 item is an object
 *     B4 role == "user"
 *     B5 typed item is type=="message" (or untyped)
 *     B6 budget remaining > 0               (C breaks here; see notes)
 *     B7 measurable text extracted
 *     B8 token cost fits remaining budget
 *     B9 (overflow) content is a plain string -> head-truncate path
 *     B10 (truncate) kept prefix non-empty
 *
 * KNOWN DIVERGENCE (documented, inputs restricted around it):
 *  - The C port has NO summary retention (LIVE retains canonical compression
 *    summaries via _is_summary_item); inputs contain no summary items.
 *  - On budget exhaustion C BREAKS the retention walk while LIVE CONTINUES
 *    ticking; every input exhausts the budget only at the final pre item
 *    (or never).
 *  - C measures the JSON-serialized content string (2 quote chars included)
 *    while LIVE measures the raw text; input text lengths are chosen with
 *    len % 4 in {0, 1} so both token estimates agree.
 *  - Whitespace-only string contents are excluded (LIVE drops them as
 *    unmeasurable, C treats them as text).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"
#include <json.h>

/* faithful copy of native_compaction_approx_tokens */
static long approx_tokens(const char *text) {
    if (!text) return 1;
    long n = (long)strlen(text) / 4;
    return n < 1 ? 1 : n;
}

/* faithful copy of native_compaction_user_item_text (string-content path;
 * the traced inputs only use string content) */
static char *user_item_text(const json_t *item) {
    if (!item || item->type != JSON_OBJECT) return NULL;
    json_t *content = json_obj_get((json_t *)item, "content");
    if (!content) return NULL;
    if (content->type == JSON_STRING) {
        const char *s = content->str_val;
        if (s && s[0]) return json_serialize(content);
        return NULL;
    }
    if (content->type == JSON_ARRAY) {
        /* faithful copy of the input_text-part join in the C port */
        size_t cap = 0, len = 0;
        char *text = NULL;
        size_t n = json_len(content);
        for (size_t i = 0; i < n; i++) {
            json_t *part = json_get(content, i);
            if (!part || part->type != JSON_OBJECT) continue;
            json_t *ptype = json_obj_get(part, "type");
            if (!ptype || ptype->type != JSON_STRING ||
                strcmp(ptype->str_val, "input_text") != 0) continue;
            json_t *ptext = json_obj_get(part, "text");
            if (!ptext || ptext->type != JSON_STRING) continue;
            const char *s = ptext->str_val;
            size_t add = strlen(s);
            if (len + add + 1 > cap) {
                cap = len + add + 64;
                text = (char *)realloc(text, cap);
                if (text) text[0] = '\0';
            }
            if (!text) return NULL;
            if (len) text[len++] = ' ';
            memcpy(text + len, s, add);
            len += add;
        }
        if (!text) return NULL;
        text[len] = '\0';
        if (!text[0]) { free(text); return NULL; }
        return text;
    }
    return NULL;
}

static void trace_prune(const char *arg) {
    WCUP_BEGIN("prune_pre_checkpoint_items");
    json_t *result_desc = NULL;

    json_t *root = arg ? json_parse(arg, NULL) : NULL;
    json_t *items = NULL;
    long budget = 200;
    if (root && root->type == JSON_OBJECT) {
        json_t *iv = json_obj_get(root, "items");
        if (iv && iv->type == JSON_ARRAY) items = iv;
        json_t *bv = json_obj_get(root, "budget");
        if (bv && bv->type == JSON_NUMBER) budget = (long)bv->num_val;
    }
    bool is_arr = items && json_len(items) > 0;

    if (!is_arr) {                                    /* B1 */
        WCUP_BRANCH(0);
        goto out;
    }
    WCUP_BRANCH(1);
    {
        size_t n = json_len(items);
        long last_cp = -1;
        for (size_t i = 0; i < n; i++) {              /* LOOP scan */
            WCUP_LOOP_TICK();
            json_t *it = json_get(items, i);
            if (it && it->type == JSON_OBJECT) {
                json_t *t = json_obj_get(it, "type");
                if (t && t->type == JSON_STRING &&
                    strcmp(t->str_val, "compaction") == 0)
                    last_cp = (long)i;
            }
        }
        if (last_cp < 0) {                            /* B2 */
            WCUP_BRANCH(0);
            goto out;
        }
        WCUP_BRANCH(1);

        long first_cp = last_cp;
        while (first_cp > 0) {                        /* LOOP extension */
            json_t *prev = json_get(items, (size_t)(first_cp - 1));
            bool is_cp = false;
            if (prev && prev->type == JSON_OBJECT) {
                json_t *t = json_obj_get(prev, "type");
                is_cp = t && t->type == JSON_STRING &&
                        strcmp(t->str_val, "compaction") == 0;
            }
            if (!is_cp) break;
            WCUP_LOOP_TICK();
            first_cp--;
        }

        size_t pre_count = (size_t)first_cp;
        size_t post_count = n - (size_t)(last_cp + 1);
        long remaining = budget > 0 ? budget : 0;
        (void)post_count;

        for (long i = (long)pre_count - 1; i >= 0; i--) {  /* LOOP retention */
            WCUP_LOOP_TICK();
            json_t *it = json_get(items, (size_t)i);
            if (!it || it->type != JSON_OBJECT) {     /* B3 */
                WCUP_BRANCH(0);
                continue;
            }
            WCUP_BRANCH(1);
            json_t *role = json_obj_get(it, "role");
            if (!role || role->type != JSON_STRING ||
                strcmp(role->str_val, "user") != 0) {  /* B4 */
                WCUP_BRANCH(0);
                continue;
            }
            WCUP_BRANCH(1);
            json_t *typ = json_obj_get(it, "type");
            if (typ && typ->type == JSON_STRING &&
                strcmp(typ->str_val, "message") != 0) {/* B5 */
                WCUP_BRANCH(0);
                continue;
            }
            WCUP_BRANCH(1);
            if (remaining <= 0) {                     /* B6 (break in C) */
                WCUP_BRANCH(0);
                break;
            }
            WCUP_BRANCH(1);
            char *text = user_item_text(it);
            if (!text) {                              /* B7 */
                WCUP_BRANCH(0);
                continue;
            }
            WCUP_BRANCH(1);
            long cost = approx_tokens(text);
            free(text);
            if (cost <= remaining) {                  /* B8 fits */
                WCUP_BRANCH(1);
                remaining -= cost;
                continue;
            }
            WCUP_BRANCH(0);
            {
                json_t *c = json_obj_get(it, "content");
                if (c && c->type == JSON_STRING) {    /* B9 string truncate */
                    WCUP_BRANCH(1);
                    long keep = remaining * 4;
                    const char *s = c->str_val;
                    long slen = (long)strlen(s);
                    if (keep > slen) keep = slen;
                    if (keep > 0) {                   /* B10 non-empty prefix */
                        WCUP_BRANCH(1);
                    } else {
                        WCUP_BRANCH(0);
                    }
                    remaining = 0;
                } else {
                    WCUP_BRANCH(0);
                }
            }
        }
    }
    result_desc = NULL;

out:
    WCUP_VALSTR("done");
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("prune_pre_checkpoint_items");
    }
    (void)result_desc;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s prune_pre_checkpoint_items "
                        "<{\"items\":[...],\"budget\":N}>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) trace_prune(argv[i]);
    return 0;
}
