/* gate: cc_should_compress_guards — traced copy of
 * src/agent/context_compressor_pure.c::cc_should_compress_info +
 * cc_compression_block_reason (LIVE twin:
 * agent/context_compressor.py::should_compress_info +
 * _compression_block_reason).
 *
 * Probe encoding: "<prompt_tokens>|<threshold_tokens>|<cooldown_remaining>|<ineff>|<streak>"
 * (cooldown_remaining is seconds; both sides convert to an absolute
 * monotonic deadline before evaluating).
 *
 * Rung order (must match twin.py EXACTLY):
 *   B1: prompt_tokens < threshold_tokens   -> no compression (V "no-compress")
 *   B2/B3: cooldown remaining > 0          -> blocked (V "cooldown")
 *   B4: ineffective>=2 OR streak>=2        -> blocked (V "ineffective")
 *   else                                   -> fire (V "fire")
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "watercup.h"

static double mono_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fn <probe>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        long tokens = 0, threshold = 0, ineff = 0, streak = 0;
        double cooldown_remaining = 0;
        {
            char probe[512];
            snprintf(probe, sizeof probe, "%s", argv[i]);
            char *save = probe;
            char *f;
            f = strsep(&save, "|"); tokens = f ? atol(f) : 0;
            f = strsep(&save, "|"); threshold = f ? atol(f) : 0;
            f = strsep(&save, "|"); cooldown_remaining = f ? atof(f) : 0;
            f = strsep(&save, "|"); ineff = f ? atol(f) : 0;
            f = strsep(&save, "|"); streak = f ? atol(f) : 0;
        }

        WCUP_BEGIN("cc_should_compress_guards");

        /* cc_should_compress_info outer gate */
        if (tokens < threshold) {                       /* B1 */
            WCUP_BRANCH(1);
            WCUP_VALSTR("no-compress");
        } else {
            WCUP_BRANCH(0);
            /* cc_compression_block_reason ladder */
            double until = mono_seconds() + cooldown_remaining;
            double rem = until - mono_seconds();
            if (rem > 0.0) {                            /* B2/B3 */
                WCUP_BRANCH(1);
                WCUP_VALSTR("cooldown");
            } else if (ineff >= 2 || streak >= 2) {     /* B4 */
                WCUP_BRANCH(1);
                WCUP_VALSTR("ineffective");
            } else {
                WCUP_BRANCH(0);
                WCUP_VALSTR("fire");
            }
        }

        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cc_should_compress_guards");
    }
    return 0;
}
