#ifndef NOC_W16_VAULTLOGIN_58F247_H
#define NOC_W16_VAULTLOGIN_58F247_H

#include "hermes_json.h"

/* Faithful C11 port of upstream agent/vault_login_classifier.py (classification
 * + fill half). Regex word/pattern matching is expressed over the same
 * NFKD-lite normalised text the Python module builds. */
json_t *w16_vaultlogin_58f247_classify_login(const json_t *control);
json_t *w16_vaultlogin_58f247_classify_checkout(const json_t *control);
json_t *w16_vaultlogin_58f247_classify_otp(const json_t *controls);
json_t *w16_vaultlogin_58f247_select_password(const json_t *controls, const char *password);
json_t *w16_vaultlogin_58f247_select_checkout(const json_t *controls, const json_t *secret,
                                              const json_t *field_tokens);
json_t *w16_vaultlogin_58f247_build_otp_fills(const json_t *controls, const char *code);

#endif /* NOC_W16_VAULTLOGIN_58F247_H */
