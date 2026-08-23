/* gate: cron_classify_parse_scores — traced copy of
 * src/cron/classify_items.c::cron_classify_parse_scores
 * (LIVE twin: cron/scripts/classify_items.py::_parse_scores).
 *
 * argv input per cup: "<content>|<n_items>".
 *
 * Branch ladder (twin mirrors EXACTLY):
 *   B1: stripped text starts with ```
 *     B2: fenced text contains a newline (drop language line)
 *   B3: direct json parse FAILED
 *     B4: last-ditch [...] block found
 *       B5: block parse failed -> {}
 *   B6: parsed value is an array
 *   per element:
 *     B7: element is an object
 *     B8: integer index within [0, n_items)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
#include "json.h"

static char *trace_copy(const char *content, int n_items) {
    WCUP_BEGIN("cron_classify_parse_scores");
    if (!content) {
        /* C returns NULL + count 0; LIVE returns {} — same degenerate cup */
        char cup[32]; WCUP_VALSTR(""); WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup); WCUP_END("cron_classify_parse_scores");
        return NULL;
    }
    char *text = strdup(content);
    char *p = text;
    while (*p && (*p==' '||*p=='\t'||*p=='\r'||*p=='\n')) p++;
    size_t len = strlen(p);
    while (len>0 && (p[len-1]==' '||p[len-1]=='\t'||p[len-1]=='\r'||p[len-1]=='\n')) p[--len]='\0';

    if (len >= 3 && strncmp(p, "```", 3) == 0) {
        WCUP_BRANCH(1);
        char *q = p;
        while (*q == '`') q++;
        char *nl = strchr(q, '\n');
        if (nl) { WCUP_BRANCH(1); q = nl + 1; } else { WCUP_BRANCH(0); }
        size_t ql = strlen(q);
        while (ql > 0 && q[ql-1] == '`') q[--ql] = '\0';
        p = q;
    } else {
        WCUP_BRANCH(0);
    }

    char *err = NULL;
    json_t *arr = json_parse(p, &err);
    free(text);
    if (err) { free(err); err = NULL; }
    if (!arr) {
        WCUP_BRANCH(1);
        const char *s = strchr(content, '[');
        const char *e = strrchr(content, ']');
        if (s && e && e > s) {
            WCUP_BRANCH(1);
            size_t blen = (size_t)(e - s + 1);
            char *block = malloc(blen + 1);
            memcpy(block, s, blen); block[blen] = '\0';
            arr = json_parse(block, &err);
            free(block);
            if (err) { free(err); err = NULL; }
            if (!arr) { WCUP_BRANCH(1); goto fail_out; }
            WCUP_BRANCH(0);
        } else {
            WCUP_BRANCH(0);
            goto fail_out;
        }
    } else {
        WCUP_BRANCH(0);
    }

    if (!arr || arr->type != JSON_ARRAY) {
        if (arr) json_free(arr);
        WCUP_BRANCH(0);
        goto fail_out;
    }
    WCUP_BRANCH(1);

    {
        char acc[8192]; size_t ao = 0;
        for (size_t i = 0; i < arr->c.count; i++) {
            const json_t *obj = json_get(arr, i);
            if (!obj || obj->type != JSON_OBJECT) { WCUP_BRANCH(0); continue; }
            WCUP_BRANCH(1);
            double didx = json_get_num(obj, "index", -1);
            int idx = (int)didx;
            if (idx != didx || idx < 0 || (n_items > 0 && idx >= n_items)) {
                WCUP_BRANCH(0);
                continue;
            }
            WCUP_BRANCH(1);
            double dscore = json_get_num(obj, "score", 0);
            const char *reason = json_get_str(obj, "reason", "");
            ao += (size_t)snprintf(acc+ao, sizeof acc - ao, "%d=%d=%s;",
                                   idx, (int)dscore, reason ? reason : "");
        }
        json_free(arr);
        acc[ao] = '\0';   /* snprintf already NUL-terminates; belt+braces */
        WCUP_VALSTR(acc);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cron_classify_parse_scores");
        return NULL;
    }

fail_out:
    WCUP_VALSTR("");
    { char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup); }
    WCUP_END("cron_classify_parse_scores");
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_classify_parse_scores <content|n_items>...\n", argv[0]); return 2; }
    for (int a = 2; a < argc; a++) {
        char arg[16384];
        snprintf(arg, sizeof arg, "%s", argv[a]);
        char *bar = strrchr(arg, '|');
        if (!bar) { fprintf(stderr, "bad input %s\n", argv[a]); return 2; }
        *bar = '\0';
        trace_copy(arg, atoi(bar + 1));
    }
    return 0;
}
