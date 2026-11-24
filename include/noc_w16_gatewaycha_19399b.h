#ifndef NOC_W16_GATEWAYCHA_19399B_H
#define NOC_W16_GATEWAYCHA_19399B_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/gateway_chat_view.py (gate w16_gatewaycha_19399b). */

/* _pending_preview(text): one terminal-safe, printable, capped line. */
json_t *w16_gatewaycha_19399b_pending_preview(const json_t *text);

/* _quoted(row, fallback): a pending row's quoted/indented own line. */
json_t *w16_gatewaycha_19399b_quoted(const json_t *row, const char *fallback);

/* pending_lines(pending, interactive): human lines for attached pending admissions. */
json_t *w16_gatewaycha_19399b_pending_lines(const json_t *pending, int interactive);

/* lost_admission(snapshot, admission_id): the lost row, or an error marker object. */
json_t *w16_gatewaycha_19399b_lost_admission(const json_t *snapshot, const char *admission_id);

/* _oneshot_admission(receipt): admission id, null (voice stop), or an error marker. */
json_t *w16_gatewaycha_19399b_oneshot_admission(const json_t *receipt);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W16_GATEWAYCHA_19399B_H */
