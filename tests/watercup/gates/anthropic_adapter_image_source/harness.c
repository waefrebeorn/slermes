/* gate: anthropic_adapter_image_source_from_openai_url — links the REAL C
 * port src/agent/anthropic_adapter_ports.c::antr_image_source_from_openai_url
 * (post-parity-fix). LIVE twin: agent/anthropic_adapter.py::
 * _image_source_from_openai_url.
 *
 * Input argv: one URL per input.
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: url empty after strip   B2: starts with "data:"
 *   B3: data header present (comma found)   B4: mime part accepted ("image/")
 *   VSTR(JSON image source)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

char *antr_image_source_from_openai_url(const char *url);

/* Out-of-path externals (http fetch path only, never on URL classification).
 * Stubbed to avoid dragging libhttp+OpenSSL into the gate. */
void *http_new(int t) { (void)t; return NULL; }
void *http_request(void *h, int m, const char *u, const char *b, void *x) { (void)h;(void)m;(void)u;(void)b;(void)x; return NULL; }
void http_resp_free(void *r) { (void)r; }
void http_free(void *h) { (void)h; }

static void run_one(const char *url) {
    WCUP_BEGIN("_image_source_from_openai_url");
    /* mirror the port's strip for tracing */
    char clean[2048];
    snprintf(clean, sizeof clean, "%s", url ? url : "");
    char *s = clean;
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n')) *--e = '\0';
    bool empty = !*s;
    bool is_data = !empty && strncmp(s, "data:", 5) == 0;
    const char *comma = is_data ? strchr(s, ',') : NULL;
    bool has_comma = comma != NULL;

    WCUP_BRANCH(empty);                                         /* B1 */
    WCUP_BRANCH(is_data);                                       /* B2 */
    if (is_data) {
        WCUP_BRANCH(has_comma);                                  /* B3 */
        if (has_comma) {
            char mp[512];
            size_t hl = (size_t)(comma - (s + 5));
            if (hl >= sizeof mp) hl = sizeof mp - 1;
            memcpy(mp, s + 5, hl); mp[hl] = '\0';
            char *semi = strchr(mp, ';');
            if (semi) *semi = '\0';
            char *p2 = mp;
            while (*p2 == ' ') p2++;
            char *e2 = p2 + strlen(p2);
            while (e2 > p2 && e2[-1] == ' ') *--e2 = '\0';
            WCUP_BRANCH(strncmp(p2, "image/", 6) == 0 && p2[6]); /* B4 */
        }
    }

    char *r = antr_image_source_from_openai_url(url);
    WCUP_VALSTR(r ? r : "");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    free(r);
    WCUP_END("_image_source_from_openai_url");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> <url...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
