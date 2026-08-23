/* gate: msgraph_parse_allowed_source_cidrs — traced copy of
 * src/gateway/platforms/msgraph_webhook/msgraph_webhook_security.c::
 * msgraph_parse_allowed_source_cidrs (+ split_cidr_list / parse_one_cidr).
 * LIVE twin: MSGraphWebhookAdapter._parse_allowed_source_cidrs.
 * Output cup traces the COUNT ladder (twin emits the count as V1).
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: empty/absent raw -> 0
 *   per comma-chunk: LOOP tick; B2 empty chunk -> skip; B3 valid CIDR -> count++
 *   V1: count
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>
#include "watercup.h"

static bool parse_one_cidr(const char *chunk) {
    char buf[256];
    size_t n = 0;
    while (*chunk && n < sizeof(buf) - 1 && *chunk != '/' && !isspace((unsigned char)*chunk))
        buf[n++] = *chunk++;
    buf[n] = '\0';
    int bits = -1;
    if (*chunk == '/') bits = atoi(chunk + 1);
    if (bits > 32 && !strchr(buf, ':')) return false;   /* guard: out-of-range v4 prefix */
    if (strchr(buf, ':')) {
        if (inet_pton(AF_INET6, buf, (void *)buf) != 1) return false;
        if (bits < 0) bits = 128;
        return bits >= 0 && bits <= 128;
    }
    unsigned char v4[4];
    if (inet_pton(AF_INET, buf, v4) != 1) return false;
    if (bits < 0) bits = 32;
    return bits >= 0 && bits <= 32;
}

static void wc_parse_cidrs(const char *raw) {
    WCUP_BEGIN("parse_allowed_source_cidrs");
    int count = 0;
    if (!raw || !*raw) {                        /* B1 */
        WCUP_BRANCH(1);
        goto emit;
    }
    WCUP_BRANCH(0);
    {
        /* py: raw.split(",") -> exactly (commas+1) chunks */
        size_t len = strlen(raw);
        int nchunks = 1;
        for (const char *q = raw; *q; q++) if (*q == ',') nchunks++;
        const char *p = raw;
        for (int ci = 0; ci < nchunks; ci++) {
            WCUP_LOOP_TICK();
            char chunk[256];
            int k = 0;
            while (*p && *p != ',')
                if (k < 255) chunk[k++] = *p++;
            chunk[k] = '\0';
            /* strip surrounding whitespace (py: chunk.strip()) */
            char *s = chunk;
            while (k > 0 && (unsigned char)chunk[k-1] <= ' ') chunk[--k] = '\0';
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++, k--;
            if (k <= 0) {                       /* B2 empty chunk */
                WCUP_BRANCH(1);
            } else {
                WCUP_BRANCH(0);
                memmove(chunk, s, strlen(s) + 1);
                if (parse_one_cidr(chunk)) {    /* B3 */
                    WCUP_BRANCH(1);
                    count++;
                } else {
                    WCUP_BRANCH(0);
                }
            }
            if (*p) p++;   /* consume the separating comma */
        }
        (void)len;
    }
emit:
    {
        char vbuf[16];
        snprintf(vbuf, sizeof vbuf, "%d", count);
        WCUP_VALSTR(vbuf);                      /* V1 */
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("parse_allowed_source_cidrs");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_allowed_source_cidrs <raw>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_parse_cidrs(argv[i]);
    return 0;
}
