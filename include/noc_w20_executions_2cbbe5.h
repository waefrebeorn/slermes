#ifndef NOC_W20_EXECUTIONS_2CBBE5_H
#define NOC_W20_EXECUTIONS_2CBBE5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w20_executions_2cbbe5_register @ persistence/sqlite/workers/_executions.py:register_worker_execution
 * Worker registration against a scripted authority store: parameter/_text
 * validation, the adoption digest, epoch and generation fences, the
 * admission-conflict replay, the idle/unknown admission probes, and the insert
 * receipt. Returns `{"status":"ok","value":<public row>}`,
 * `{"status":"refused","reason":<AuthorityRefusal reason>}`, or
 * `{"status":"raised","type":..,"message":..}`. */
struct json_t *w20_executions_2cbbe5_register(const struct json_t *request);

/* PoP: w20_executions_2cbbe5_adopt @ persistence/sqlite/workers/_executions.py:adopt_worker_execution
 * Worker adoption: the producer-claim digest check, the terminal/stale fences,
 * the admission-worker same-epoch rule, the linked admission transition and the
 * running receipt. Same result shapes as register. */
struct json_t *w20_executions_2cbbe5_adopt(const struct json_t *request);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W20_EXECUTIONS_2CBBE5_H */
