#ifndef NOC_W14_RUNTIMSES_CD33F5_H
#define NOC_W14_RUNTIMSES_CD33F5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_runtimeses_cd33f5_refusal @ agent/runtime_session_child.py:_REFUSALS
 * The owner-refusal message for `code`, or the generic "…: <code>" fallback. */
const char *w14_runtimeses_cd33f5_refusal(const char *code);

/* PoP: w14_runtimeses_cd33f5_constructor_fields @ agent/runtime_session_child.py:ChildSessionStore.create_session
 * The constructor kwargs the child adds: model, model_config (filtered by the
 * injected WORKER_CONSTRUCTOR_CONFIG_KEYS, null when empty) and system_prompt. */
struct json_t *w14_runtimeses_cd33f5_constructor_fields(const struct json_t *fields,
        const struct json_t *allowed_keys);

/* PoP: w14_runtimeses_cd33f5_outbox_name @ agent/runtime_session_child.py:reserve_child_store
 * The outbox directory component: 'child-' + execution_id.rsplit(':', 1)[1]. */
const char *w14_runtimeses_cd33f5_outbox_name(const char *execution_id);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_RUNTIMSES_CD33F5_H */
