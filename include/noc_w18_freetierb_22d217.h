#ifndef NOC_W18_FREETIERB_22D217_H
#define NOC_W18_FREETIERB_22D217_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_freetierb_22d217_free_tier_route @ hermes_cli/free_tier_bootstrap.py:SetupRecord.free_tier_route */
int w18_freetierb_22d217_free_tier_route(int free_tier_account, const char *inference_provider);

/* PoP: w18_freetierb_22d217_as_payload @ hermes_cli/free_tier_bootstrap.py:SetupRecord.as_payload
 * record is a JSON object with the SetupRecord fields; the failure block is
 * flattened into the payload and free_tier_route is added. */
struct json_t *w18_freetierb_22d217_as_payload(const struct json_t *record);

/* PoP: w18_freetierb_22d217_build_record @ hermes_cli/free_tier_bootstrap.py:_build_record
 * state / failure / free_active / is_guest / inference_provider / finished_at
 * are injected seams for anon_auth and _resolve_inference/_clock. */
struct json_t *w18_freetierb_22d217_build_record(int other,
    const struct json_t *state, const struct json_t *failure,
    int free_active, int is_guest, const char *inference_provider, double finished_at);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_FREETIERB_22D217_H */
