#ifndef NOC_W19_ADAPTEREXE_BC6B62_H
#define NOC_W19_ADAPTEREXE_BC6B62_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_adapterexe_bc6b62_init_buttons @ plugins/platforms/discord/adapter_exec_approval_view.py:build_exec_approval_view.ExecApprovalView.__init__
 * Which buttons __init__ removes, in removal order: smart_denied or !allow_session drop
 * allow_session then allow_always; else !allow_permanent drops allow_always; !allow_once
 * drops allow_once last. Returns a JSON array of names. */
json_t *w19_adapterexe_bc6b62_init_buttons(int allow_once, int allow_session,
                                          int allow_permanent, int smart_denied);

/* PoP: w19_adapterexe_bc6b62_check_auth @ plugins/platforms/discord/adapter_exec_approval_view.py:build_exec_approval_view.ExecApprovalView._check_auth
 * {"ok":bool,"warned":bool}: base admission first, then require_admin against the admin
 * id set; warns (fails closed) only when require_admin is on and no admin is configured. */
json_t *w19_adapterexe_bc6b62_check_auth(int base_ok, int require_admin,
                                        json_t *admin_ids, const char *uid);

/* PoP: w19_adapterexe_bc6b62_footer_key @ plugins/platforms/discord/adapter_exec_approval_view.py:build_exec_approval_view.ExecApprovalView.retire
 * The i18n key retire() picks for a settle reason. */
const char *w19_adapterexe_bc6b62_footer_key(const char *reason);

/* PoP: w19_adapterexe_bc6b62_resolve_outcome @ plugins/platforms/discord/adapter_exec_approval_view.py:build_exec_approval_view.ExecApprovalView._resolve
 * The branch _resolve takes: "return", "refuse", "click_failed", "no_longer_pending"
 * or "finalize"; plus the finalize colour/text and the two settle flags. */
json_t *w19_adapterexe_bc6b62_resolve_outcome(int gate_ok, int refusal, int resolve_raises,
                                             int count, int request_id_present,
                                             const char *color_name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_ADAPTEREXE_BC6B62_H */
