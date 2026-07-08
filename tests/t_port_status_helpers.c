/*
 * t_port_status_helpers.c — faithful verification for port_status_helpers.c.
 * Runs sta_format_iso_timestamp on fixed inputs, dumps JSON lines
 * {in, out}; the oracle (tests/sta_oracle.py) recomputes the LIVE
 * Python and compares exactly.
 */

#include "port_status_helpers.c"
#include <stdio.h>
#include <string.h>

static void run(const char *in)
{
    char out[64];
    sta_format_iso_timestamp(in, out);
    /* JSON-escape minimally (our inputs have no quotes/backslashes) */
    printf("{\"in\":\"%s\",\"out\":\"%s\"}\n", in ? in : "", out);
}

int main(void)
{
    run("2026-07-08T12:00:00Z");
    run("2026-07-08T12:00:00+00:00");
    run("2026-07-08T14:30:00+02:00");
    run("2026-07-08T12:00:00-05:00");
    run("2026-07-08 12:00:00");         /* naive -> UTC */
    run("2026-07-08T12:00:00.500000"); /* fractional secs dropped */
    run("");                                /* empty -> (unknown) */
    run("   ");                            /* whitespace -> (unknown) */
    run("not-a-date");                    /* unparseable -> echo */
    run(NULL);                             /* NULL -> (unknown) */
    return 0;
}
