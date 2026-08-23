/* gate: platforms_helpers_greedy_pack_blocks — traced copy of
 * src/gateway/platforms/platforms_helpers_md.c::helpers_md_greedy_pack_blocks
 * (PoP: gateway/platforms/helpers.py:greedy_pack_blocks).
 *
 * Input encoding: "maxlen|SEP|b1|b2|..." — SEP is the literal escape "\n"
 * (backslash n) for newline, decoded identically on both sides.
 * Rungs: per-block loop -> fits? -> flush-current? -> block-alone-fits?.
 * Value event: final packing joined with US (\x1e).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static char **push(char **list, char *s) {
    size_t n = 0;
    if (list) while (list[n]) n++;
    list = (char **)realloc(list, (n + 2) * sizeof(char *));
    list[n] = s;
    list[n + 1] = NULL;
    return list;
}

static void greedy_pack_blocks(size_t max_length, const char *sep, char **blocks) {
    WCUP_BEGIN("greedy_pack_blocks");
    size_t sep_len = strlen(sep);
    char **packed = NULL;
    char *current = NULL;
    size_t current_len = 0;

    for (size_t i = 0; blocks[i]; i++) {
        WCUP_LOOP_TICK();
        const char *block = blocks[i];
        size_t block_len = strlen(block);
        size_t candidate_len = current ? (current_len + sep_len + block_len) : block_len;
        if (candidate_len <= max_length) {
            WCUP_BRANCH(1);
            char *nb = (char *)malloc(candidate_len + 1);
            if (current) sprintf(nb, "%s%s%s", current, sep, block);
            else strcpy(nb, block);
            free(current);
            current = nb;
            current_len = candidate_len;
            continue;
        }
        WCUP_BRANCH(0);
        if (current) {
            WCUP_BRANCH(1);
            packed = push(packed, current);
            current = NULL;
            current_len = 0;
        } else WCUP_BRANCH(0);
        if (block_len <= max_length) {
            WCUP_BRANCH(1);
            current = strdup(block);
            current_len = block_len;
        } else {
            WCUP_BRANCH(0);
            packed = push(packed, strdup(block));
        }
    }
    if (current) { packed = push(packed, current); current = NULL; }

    /* value event: joined packed chunks */
    char joined[4096];
    joined[0] = '\0';
    if (packed) for (size_t i = 0; packed[i]; i++) {
        if (i) strncat(joined, "\x1e", sizeof joined - strlen(joined) - 1);
        strncat(joined, packed[i], sizeof joined - strlen(joined) - 1);
    }
    WCUP_VALSTR(joined);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("greedy_pack_blocks");
}

static char *decode_sep(const char *esc) {
    char *out = malloc(strlen(esc) + 1);
    size_t j = 0;
    for (size_t i = 0; esc[i]; i++) {
        if (esc[i] == '\\' && esc[i+1] == 'n') { out[j++] = '\n'; i++; }
        else out[j++] = esc[i];
    }
    out[j] = '\0';
    return out;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s greedy_pack_blocks '<maxlen>|<sep>|<b1>|<b2>...'...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *arg = strdup(argv[i]);
        char *p1 = strchr(arg, '|');
        char *p2 = p1 ? strchr(p1 + 1, '|') : NULL;
        if (!p1 || !p2) { fprintf(stderr, "bad input: %s\n", argv[i]); return 2; }
        *p1 = '\0'; *p2 = '\0';
        size_t maxlen = (size_t)strtoul(arg, NULL, 10);
        char *sep = decode_sep(p1 + 1);
        char *blocks[64];
        int nb = 0;
        char *q = p2 + 1;
        while (*q && nb < 63) {
            blocks[nb++] = q;
            char *bar = strchr(q, '|');
            if (!bar) break;
            *bar = '\0';
            q = bar + 1;
        }
        blocks[nb] = NULL;
        greedy_pack_blocks(maxlen, sep, blocks);
        free(sep); free(arg);
    }
    return 0;
}
