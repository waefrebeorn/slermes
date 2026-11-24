#ifndef NOC_W20_CONTEXTCOM_4EBD6D_H
#define NOC_W20_CONTEXTCOM_4EBD6D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w20_contextcom_4ebd6d_accepts_keyword_argument @ agent/context_compressor_summary.py:accepts_keyword_argument
 * Whether an inspectable callable accepts `name` as a keyword. `signature_spec`
 * describes the callable's inspect.signature(): `{"params":[{"name":..,
 * "kind":"pos_only|pos_or_kw|var_pos|kw_only|var_kw"}], "var_keyword":bool}`;
 * null/absent means a callable inspect.signature() refuses (TypeError). */
struct json_t *w20_contextcom_4ebd6d_accepts_keyword_argument(const struct json_t *signature_spec,
                                                             const char *name);

/* PoP: w20_contextcom_4ebd6d_apply_summary_route @ agent/context_compressor_summary.py:SummaryDispatchMixin._apply_summary_route
 * The call_kwargs dict after the summary route is pinned onto it: a truthy
 * `pinned` route replaces the whole route, else self.summary_model wins, else a
 * fallen-back summary names the main runtime explicitly. `self_spec` carries
 * summary_model / _summary_model_fallen_back / provider / model / base_url /
 * api_key / api_mode. Returns the resulting dict (the live method mutates). */
struct json_t *w20_contextcom_4ebd6d_apply_summary_route(const struct json_t *call_kwargs,
                                                         const struct json_t *pinned,
                                                         const struct json_t *self_spec);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W20_CONTEXTCOM_4EBD6D_H */
