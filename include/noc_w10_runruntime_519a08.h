#ifndef NOC_W10_RUNRUNTIME_519A08_H
#define NOC_W10_RUNRUNTIME_519A08_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_runruntime_519a08_persistence @ gateway/run_runtime_init.py:GatewayRuntimeInitMixin.persistence
 * Input keys: "pinned", "has_store", "store_persistence", "scope_result".
 * Returns the same handle the property returns (JSON null / string / value). Caller frees. */
struct json_t *w10_runruntime_519a08_persistence(const struct json_t *in);

/* PoP: w10_runruntime_519a08_profile_name @ gateway/run_runtime_init.py:GatewayRuntimeInitMixin._init_runtime_caches
 * Input keys: "launch" (injected _active_profile_name()), "multiplexed".
 * Returns {"primary": <name>, "kanban": <launch name>}. Caller frees. */
struct json_t *w10_runruntime_519a08_profile_name(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_RUNRUNTIME_519A08_H */
