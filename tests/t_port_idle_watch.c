/* t_port_idle_watch.c — oracle harness for next_check_in.
 * Emits {"tag":"nc","case":N,"limit":d,"idle_null":0|1,"idle":d,"out":int}. */
#include "port_idle_watch.h"

#include <stdio.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    struct { double limit; int inull; double idle; } cs[] = {
        {0.0, 0, 0.0},      /* <=0 -> 1800 */
        {-5.0, 1, 0.0},     /* <=0 -> 1800, idle null */
        {300.0, 0, 100.0},  /* limit 300, idle 100 -> ceil(max(30, 200)) = 200 */
        {300.0, 0, 400.0},  /* idle 400 >= limit -> ceil(max(30,300)) = 300 */
        {10.0, 0, 3.0},     /* limit 10 < MIN_DELAY -> floor 10; tail 7 -> 7 */
        {10.0, 0, 9.0},     /* tail 1 -> 10 */
        {10.0, 1, 0.0},     /* idle null -> 0; tail 10 -> 10 */
        {30.0, 0, 0.0},     /* floor 30, tail 30 -> 30 */
        {1.5, 0, 1.0},      /* floor 1.5, tail 0.5 -> ceil(1.5) = 2 */
        {2.0, 0, 0.7},      /* tail 1.3, floor 2.0 -> ceil(2.0)=2 */
    };
    size_t n = sizeof(cs) / sizeof(cs[0]);
    for (size_t i = 0; i < n; i++) {
        int out = idw_next_check_in(cs[i].limit, cs[i].inull, cs[i].idle);
        printf("{\"tag\":\"nc\",\"case\":%zu,\"limit\":%.1f,\"idle_null\":%d,\"idle\":%.1f,\"out\":%d}\n",
               i, cs[i].limit, cs[i].inull, cs[i].idle, out);
    }
    return 0;
}
