/* t_acb_smoke.c — runtime smoke test for port_gateway/agent_cache_pressure.c */
#include "agent_cache_pressure.h"
#include "hermes_yaml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); failures++; } \
    else { printf("ok: %s\n", m); } } while (0)

static int failures = 0;

static bool always_evict(const char *key, const void *agent, void *u) {
    (void)key; (void)agent; (void)u; return true;
}

int main(void) {
    /* _positive_int / _positive_float via json_number */
    json_t *n = json_number(128.0);
    CHECK(acb_positive_int(n) == 128, "positive_int(128)");
    json_free(n);
    json_t *f = json_number(3.5);
    CHECK(acb_positive_float(f) > 3.4 && acb_positive_float(f) < 3.6, "positive_float(3.5)");
    json_free(f);
    json_t *neg = json_number(-5.0);
    CHECK(acb_positive_int(neg) == ACB_UNSET_INT, "positive_int(-5) -> unset");
    json_free(neg);
    json_t *b = json_bool(false);
    CHECK(acb_positive_int(b) == ACB_UNSET_INT, "positive_int(false) -> unset");
    json_free(b);

    /* resolve_agent_cache_bounds — absent section -> defaults */
    json_t *empty = json_parse("{}", NULL);
    agent_cache_bounds_t def = acb_resolve_agent_cache_bounds(empty);
    CHECK(def.max_evictions_per_pass == ACB_DEFAULT_MAX_EVICTIONS_PER_PASS, "absent: default evictions=16");
    CHECK(def.protect_recent == ACB_DEFAULT_PROTECT_RECENT, "absent: default protect_recent=8");
    CHECK(def.max_size == ACB_UNSET_INT, "absent: max_size unset");
    CHECK(def.idle_ttl_secs == ACB_UNSET_FLOAT, "absent: idle_ttl unset");
    CHECK(def.memory_high_mb != ACB_UNSET_INT, "absent: memory_high auto-resolved");
    json_free(empty);

    /* resolve_agent_cache_bounds — with overrides */
    json_t *cfg = json_parse("{\"agent\":{\"agent_cache\":{\"max_size\":64,\"idle_ttl_secs\":120.0,\"max_evictions_per_pass\":4,\"protect_recent\":2}}}", NULL);
    agent_cache_bounds_t c = acb_resolve_agent_cache_bounds(cfg);
    CHECK(c.max_size == 64, "override max_size=64");
    CHECK(c.idle_ttl_secs > 119.9 && c.idle_ttl_secs < 120.1, "override idle_ttl=120.0");
    CHECK(c.max_evictions_per_pass == 4, "override max_evictions=4");
    CHECK(c.protect_recent == 2, "override protect_recent=2");
    json_free(cfg);

    /* explicit protect_recent: 0 -> shed anything */
    json_t *cfg2 = json_parse("{\"agent\":{\"agent_cache\":{\"protect_recent\":0}}}", NULL);
    agent_cache_bounds_t c2 = acb_resolve_agent_cache_bounds(cfg2);
    CHECK(c2.protect_recent == 0, "protect_recent=0 explicit -> 0");
    json_free(cfg2);

    /* plan_pressure_evictions — protect recent + cap */
    typedef struct { const char *key; const void *agent; } ent_t;
    ent_t ents[8];
    for (int i = 0; i < 8; i++) { static char k[8][8]; snprintf(k[i],8,"k%d",i); ents[i].key=k[i]; ents[i].agent=NULL; }
    size_t out_count = 0;
    size_t *plan = acb_plan_pressure_evictions(ents, 8, &always_evict, NULL, 4, 2, &out_count);
    CHECK(out_count == 4, "plan: 4 evictions (capped)");
    CHECK(plan && plan[0] == 0, "plan: evicts LRU first (idx 0)");
    CHECK(plan && plan[3] == 3, "plan: evicts idx 3 last before protected");
    free(plan);

    /* plan: max_evictions=0 -> no plan */
    size_t cnt2 = 0;
    CHECK(acb_plan_pressure_evictions(ents, 8, &always_evict, NULL, 0, 2, &cnt2) == NULL, "plan: max_evictions=0 -> NULL");
    CHECK(cnt2 == 0, "plan: max_evictions=0 -> count 0");

    /* read_anon_rss_mb — should return a positive number on Linux */
    int rss = acb_read_anon_rss_mb();
    CHECK(rss >= 0, "read_anon_rss_mb returns >=0");

    printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
