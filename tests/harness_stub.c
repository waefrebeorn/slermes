/* harness_stub.c — minimal symbols needed to link port_*.c test harnesses
 * without pulling in the full hermes binary. */
#include <stdarg.h>
#include <stdio.h>

void hermes_log(int level, const char *component, const char *fmt, ...) {
    (void)level; (void)component; (void)fmt;
    /* silent in harness */
}
