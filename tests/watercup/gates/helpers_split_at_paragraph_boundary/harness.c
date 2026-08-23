/* gate: helpers_split_at_paragraph_boundary — traced C copy of
 * src/gateway/platforms/platforms_helpers_md.c::helpers_md_split_at_paragraph_boundary.
 * LIVE twin: gateway/platforms/helpers.py::split_at_paragraph_boundary
 * (len_fn=None path).
 *
 * Input encoding: "<max_chars>|<text>" (text may contain newlines).
 *
 * Rung ladder (identical both sides):
 *   B1 fits within max_chars -> (text, '')
 *   B2 blank-line (\n\n) boundary found in window
 *   B3 sentence-ending punctuation + newline found in window
 *   B4 any other newline found in window
 *   V head, V tail
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static void wc_split(const char *text, size_t max_chars) {
    WCUP_BEGIN("split_at_paragraph_boundary");
    size_t total = strlen(text);
    const char *head, *tail;
    size_t head_len;

    WCUP_BRANCH(total <= max_chars);                             /* B1 */
    if (total <= max_chars) {
        head = text; head_len = total; tail = text + total;
        goto out;
    }

    {
        size_t window_len = max_chars < total ? max_chars : total;
        const char *window_end = text + window_len;

        /* 1. last blank line (\n\n), pair starting at index > 0 */
        const char *pp = window_end;
        const char *blank = NULL;
        while (pp >= text + 2) {
            if (pp[-1] == '\n' && pp[-2] == '\n' && (size_t)(pp - 2 - text) > 0) {
                blank = pp - 1;
                break;
            }
            pp--;
        }
        WCUP_BRANCH(blank != NULL);                              /* B2 */
        if (blank) {
            head_len = (size_t)(blank - text) + 1;
            head = text; tail = text + head_len;
            goto out;
        }

        /* 2. last newline following sentence-ending punctuation */
        int best_pos = -1;
        {
            const char *p = text;
            while (p < window_end) {
                const char *eol = memchr(p, '\n', (size_t)(window_end - p));
                if (!eol) break;
                int sent_end = 0;
                if (eol > text) {
                    unsigned char c = (unsigned char)eol[-1];
                    if (c == '.' || c == '!' || c == '?') sent_end = 1;
                    else if (eol >= text + 3) {
                        const unsigned char *b = (const unsigned char *)eol - 3;
                        if (!memcmp(b, "\xE3\x80\x82", 3)) sent_end = 1;
                        else if (!memcmp(b, "\xEF\xBC\x81", 3)) sent_end = 1;
                        else if (!memcmp(b, "\xEF\xBC\x9F", 3)) sent_end = 1;
                    }
                }
                if (sent_end) best_pos = (int)(eol - text) + 1;
                p = eol + 1;
            }
        }
        WCUP_BRANCH(best_pos > 0);                               /* B3 */
        if (best_pos > 0) {
            head_len = (size_t)best_pos;
            head = text; tail = text + head_len;
            goto out;
        }

        /* 3. last newline (index > 0) */
        {
            const char *q = window_end;
            const char *found = NULL;
            while (q > text) {
                q--;
                if (*q == '\n') { found = q; break; }
            }
            WCUP_BRANCH(found != NULL && found > text);          /* B4 */
            if (found && found > text) {
                head_len = (size_t)(found - text) + 1;   /* include the \n */
                head = text; tail = text + head_len;
                goto out;
            }
        }
    }

    /* 4. force split at window boundary */
    head_len = max_chars < total ? max_chars : total;
    head = text; tail = text + head_len;

out:
    {
        char hbuf[2048];
        size_t hl = head_len < sizeof hbuf - 1 ? head_len : sizeof hbuf - 1;
        memcpy(hbuf, head, hl); hbuf[hl] = '\0';
        WCUP_VALSTR(hbuf);                                       /* V head */
        WCUP_VALSTR(tail);                                       /* V tail */
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("split_at_paragraph_boundary");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s split_at_paragraph_boundary <max|text>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        char *bar = strchr(argv[i], '|');
        if (!bar) { fprintf(stderr, "bad input: %s\n", argv[i]); return 2; }
        *bar = '\0';
        wc_split(bar + 1, (size_t)atoi(argv[i]));
    }
    return 0;
}
