/* t_port_scheduler_tick.c — oracle harness for is_recurring and
 * _advance_or_drop_recurring's decision. */
#include "port_scheduler_tick.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    static const char *const jobs[] = {
        "{\"id\":\"j1\",\"schedule\":{\"kind\":\"cron\",\"expr\":\"* * * * *\"}}",
        "{\"id\":\"j2\",\"schedule\":{\"kind\":\"interval\"}}",
        "{\"id\":\"j3\",\"schedule\":{}}",
        "{\"id\":\"j4\"}",
        "{\"id\":\"j5\",\"schedule\":{\"kind\":\"once\"}}",
        "{\"id\":\"j6\",\"schedule\":null}",
    };
    for (int i = 0; i < 6; i++) {
        int r = js_is_recurring(jobs[i]);
        printf("{\"family\":\"recur\",\"case\":%d,\"job\":%s,\"out\":%d}\n",
               i, jobs[i], r);
    }

    const char *due = "[{\"id\":\"a\",\"schedule\":{\"kind\":\"cron\"}},"
                      "{\"id\":\"b\"},"
                      "{\"id\":\"c\",\"schedule\":{\"kind\":\"interval\"}},"
                      "{\"id\":\"d\",\"schedule\":{\"kind\":\"once\"}}]";
    char *o1 = js_advance_or_drop(due, 1);
    char *o0 = js_advance_or_drop(due, 0);
    printf("{\"family\":\"aod\",\"case\":0,\"due\":%s,\"advance_ok\":1,\"out\":%s}\n", due, o1);
    printf("{\"family\":\"aod\",\"case\":1,\"due\":%s,\"advance_ok\":0,\"out\":%s}\n", due, o0);
    free(o1);
    free(o0);
    return 0;
}
