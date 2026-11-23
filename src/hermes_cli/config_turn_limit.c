/*
 * config_turn_limit.c — C port of upstream Hermes hermes_cli/config_turn_limit.py.
 * PoP: cfgturn_limit_label @ hermes_cli/config_turn_limit.py:turn_limit_label
 */
#include "noc_cfgturn.h"
#include <stdio.h>

const char *cfgturn_limit_label(int64_t limit) {
    if (limit == CFGTURN_UNLIMITED) return "unlimited";
    static char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)limit);
    return buf;
}
