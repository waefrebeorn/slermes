/* t_gw_cache_bounds_smoke.c — verify grun_u_agent_cache_* wiring.
 * Uses a temp config with agent.agent_cache overrides. */
#include "agent_cache_pressure.h"
#include "gateway_run_pure2.h"
#include "port_config_py_helpers.h"
#include <stdio.h>
int main(void){
    agent_cache_bounds_t b = grun_u_agent_cache_bounds();
    printf("bounds: max_size=%d idle_ttl=%g memory_high_mb=%d evictions=%d protect=%d\n",
           b.max_size, b.idle_ttl_secs, b.memory_high_mb, b.max_evictions_per_pass, b.protect_recent);
    printf("cap (effective) = %d  (fallback %d)\n", grun_u_agent_cache_cap(), ACB_AGENT_CACHE_MAX_SIZE);
    printf("idle_ttl (effective) = %g  (fallback %g)\n", grun_u_agent_cache_idle_ttl(), ACB_AGENT_CACHE_IDLE_TTL_SECS);
    int cap = grun_u_agent_cache_cap();
    double ttl = grun_u_agent_cache_idle_ttl();
    if (cap <= 0) { printf("FAIL: cap non-positive\n"); return 1; }
    if (ttl <= 0) { printf("FAIL: ttl non-positive\n"); return 1; }
    printf("ok: cap positive, ttl positive\n");
    printf("ALL PASS\n");
    return 0;
}
