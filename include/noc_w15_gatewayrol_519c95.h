#ifndef NOC_W15_GATEWAYROL_519C95_H
#define NOC_W15_GATEWAYROL_519C95_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_cli/gateway_rollback.py (gate w15_gatewayrol_519c95).
 * All results are freshly allocated json_t values freed by the caller. */

/* PoP: w15_gatewayrol_519c95_checkpoint_rows @ hermes_cli/gateway_rollback.py:_checkpoint_rows
 * Returns the list of rollback.list rows consumed by format_checkpoint_list. */
struct json_t *w15_gatewayrol_519c95_checkpoint_rows(const struct json_t *result);

/* PoP: w15_gatewayrol_519c95_diff @ hermes_cli/gateway_rollback.py:_diff
 * Returns the list of lines the live _diff prints (one element per print call).
 * The oracle stubs t() as a deterministic key formatter (identity + kwargs). */
struct json_t *w15_gatewayrol_519c95_diff(const struct json_t *result);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_GATEWAYROL_519C95_H */
