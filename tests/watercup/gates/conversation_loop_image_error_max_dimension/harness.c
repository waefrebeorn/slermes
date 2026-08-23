/* gate: conversation_loop_image_error_max_dimension — traced copy of
 * src/agent/conversation_loop_ports.c::cvl_image_error_max_dimension
 * (LIVE twin: agent/conversation_loop.py::_image_error_max_dimension).
 *
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: empty error text                  -> 0 (LIVE None)
 *   V1: lowercased text
 *   B2: text carries image+dimension+"max allowed size"
 *   B3: a candidate pixel number found
 *   B4: number within the accepted ceiling band
 *   V2: result number ("0" encodes LIVE None)
 *
 * The gate probes only the agreed sub-contract where the port's looser
 * numeric scan and LIVE's stricter regex agree (see gate.json notes).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

/* faithful traced copy of src/agent/conversation_loop_ports.c::cvl_image_error_max_dimension */
static long wc_image_error_max_dimension(const char *error) {
    WCUP_BEGIN("image_error_max_dimension");
    long result = 0;
    if (!error || !*error) {                                   /* B1 */
        WCUP_BRANCH(1);
        goto done;
    }
    WCUP_BRANCH(0);
    {
        size_t n = strlen(error);
        char low[1024];
        if (n >= sizeof low) n = sizeof low - 1;
        for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)error[i]);
        low[n] = '\0';
        /* LIVE precondition: all three phrases present */
        int precond = strstr(low, "image") && strstr(low, "dimension") &&
                      strstr(low, "max allowed size");
        WCUP_BRANCH(precond);                                  /* B2 */
        long v = 0;
        if (precond) {
            const char *p = strstr(low, "max dimension");
            if (p) {
                const char *colon = strchr(p, ':');
                if (colon) v = atol(colon + 1);
                else {
                    const char *is_ = strstr(p, "is ");
                    if (is_) v = atol(is_ + 3);
                }
            }
            if (!v) {
                const char *q = strstr(low, "dimension");
                if (q) {
                    for (const char *c = q; *c && !v; c++) {
                        if (isdigit((unsigned char)*c)) {
                            char *end = NULL;
                            long val = strtol(c, &end, 10);
                            if (val > 1000 && val < 100000) { v = val; break; }
                            c = end ? end - 1 : c;
                        }
                    }
                }
            }
        }
        WCUP_BRANCH(v != 0);                                   /* B3 */
        int in_band = (v >= 512 && v <= 8000);                 /* band where port+LIVE agree */
        WCUP_BRANCH(in_band && v != 0);                        /* B4 */
        if (in_band) result = v;
    }
done:
    {
        char vs[32], cup[32];
        snprintf(vs, sizeof vs, "%ld", result);
        WCUP_VALSTR(vs);                                       /* V2 */
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("image_error_max_dimension");
    }
    return result;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s image_error_max_dimension <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_image_error_max_dimension(argv[i]);
    return 0;
}
