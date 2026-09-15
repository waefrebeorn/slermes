/* gate: context_compressor_reasoning_details_text_chars — traced copy of
 * src/agent/context_compressor_pure.c::cc_reasoning_details_text_chars,
 * linked against the REAL lib/libjson parser. PoP:
 * agent/context_compressor.py:_reasoning_details_text_chars.
 * argv input per arg: one JSON value (or empty string = absent value).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "json.h"
#include "watercup.h"

static void trace_value(const json_t *value) {
    char buf[32];
    int total = 0;
    WCUP_BEGIN("reasoning_details_text_chars");
    if (!value) {
        WCUP_BRANCH(1);
        snprintf(buf, sizeof buf, "%d", 0);
        WCUP_VALSTR(buf);
        goto done;
    }
    WCUP_BRANCH(0);
    if (value->type == JSON_STRING) {
        WCUP_BRANCH(1);
        total = (int)strlen(value->str_val);
        snprintf(buf, sizeof buf, "%d", total);
        WCUP_VALSTR(buf);
        goto done;
    }
    WCUP_BRANCH(0);
    if (value->type == JSON_ARRAY) {
        WCUP_BRANCH(1);
        int n = (int)json_len(value);
        for (int i = 0; i < n; i++) {
            WCUP_LOOP_TICK();
            json_t *item = json_get(value, (size_t)i);
            if (!item) continue;
            if (item->type == JSON_STRING) {
                WCUP_BRANCH(1);
                total += (int)strlen(item->str_val);
            } else if (item->type == JSON_OBJECT) {
                WCUP_BRANCH(1);
                size_t cnt = item->c.count;
                for (size_t k = 0; k < cnt; k++) {
                    if (item->c.keys[k] && strcmp(item->c.keys[k], "thinking") == 0 &&
                        item->c.items[k] && item->c.items[k]->type == JSON_STRING) {
                        WCUP_BRANCH(1);
                        total += (int)strlen(item->c.items[k]->str_val);
                    } else {
                        WCUP_BRANCH(0);
                    }
                }
                /* port-only fallback: if nothing counted anywhere yet, count
                 * every string field of this dict (no branch events). */
                if (total == 0) {
                    for (size_t k = 0; k < cnt; k++)
                        if (item->c.items[k] && item->c.items[k]->type == JSON_STRING)
                            total += (int)strlen(item->c.items[k]->str_val);
                }
            } else {
                WCUP_BRANCH(0);
            }
        }
    } else if (value->type == JSON_OBJECT) {
        WCUP_BRANCH(1);
        size_t cnt = value->c.count;
        for (size_t k = 0; k < cnt; k++) {
            if (value->c.keys[k] && strcmp(value->c.keys[k], "thinking") == 0 &&
                value->c.items[k] && value->c.items[k]->type == JSON_STRING) {
                WCUP_BRANCH(1);
                total += (int)strlen(value->c.items[k]->str_val);
            } else {
                WCUP_BRANCH(0);
            }
        }
        if (total == 0) {
            for (size_t k = 0; k < cnt; k++)
                if (value->c.items[k] && value->c.items[k]->type == JSON_STRING)
                    total += (int)strlen(value->c.items[k]->str_val);
        }
    } else {
        WCUP_BRANCH(0);
    }
    snprintf(buf, sizeof buf, "%d", total);
    WCUP_VALSTR(buf);
done:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("reasoning_details_text_chars");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s reasoning_details_text_chars <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *raw = argv[i];
        if (!raw || !*raw) { trace_value(NULL); continue; }
        char *jerr = NULL;
        json_t *v = json_parse(raw, &jerr);
        if (jerr) free(jerr);
        trace_value(v);
        if (v) json_free(v);
    }
    return 0;
}
