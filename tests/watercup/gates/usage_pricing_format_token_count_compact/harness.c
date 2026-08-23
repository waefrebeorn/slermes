/* gate: usage_pricing_format_token_count_compact — traced copy of
 * src/agent/usage_pricing.c::usage_pricing_format_token_count.
 * LIVE twin: agent/usage_pricing.py::format_token_count_compact.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: abs_value < 1000            -> plain integer string
 *   B2: abs >= 1e9 (B unit)
 *   B3: abs >= 1e6 (M unit)
 *   B4: abs >= 1e3 (K unit)
 *   V1: formatted result
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static void trace_one(long long value) {
    WCUP_BEGIN("usage_pricing_format_token_count");
    char buf[64];

    long long abs_value = value < 0 ? -value : value;
    if (abs_value < 1000) {                             /* B1 */
        WCUP_BRANCH(1);
        snprintf(buf, sizeof(buf), "%lld", value);
        goto out;
    }
    WCUP_BRANCH(0);

    {
        const char *sign = value < 0 ? "-" : "";
        static const long long units[] = {1000000000LL, 1000000LL, 1000LL};
        static const char *suf[] = {"B", "M", "K"};
        buf[0] = '\0';
        for (int i = 0; i < 3; i++) {
            WCUP_BRANCH(abs_value >= units[i]);         /* B2/B3/B4 */
            if (abs_value >= units[i]) {
                double scaled = (double)abs_value / (double)units[i];
                char text[32];
                if (scaled < 10.0)       snprintf(text, sizeof(text), "%.2f", scaled);
                else if (scaled < 100.0) snprintf(text, sizeof(text), "%.1f", scaled);
                else                     snprintf(text, sizeof(text), "%.0f", scaled);
                if (strchr(text, '.')) {
                    char *p = text + strlen(text);
                    while (p > text && p[-1] == '0') *--p = '\0';
                    if (p > text && p[-1] == '.') *--p = '\0';
                }
                snprintf(buf, sizeof(buf), "%s%s%s", sign, text, suf[i]);
                break;
            }
        }
        if (!buf[0])
            snprintf(buf, sizeof(buf), "%lld", value);
    }
out:
    WCUP_VALSTR(buf);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("usage_pricing_format_token_count");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s usage_pricing_format_token_count <int>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        trace_one(strtoll(argv[i], NULL, 10));
    return 0;
}
