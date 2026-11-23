#ifndef NOC_W14_LEGACYLOCA_18B83D_H
#define NOC_W14_LEGACYLOCA_18B83D_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w14_legacyloca_18b83d_parse_authenticated_subject @ session_rules/legacy_local.py:parse_authenticated_subject */
struct json_t *w14_legacyloca_18b83d_parse_authenticated_subject(const struct json_t *subject);
/* PoP: w14_legacyloca_18b83d_login_label @ session_rules/legacy_local.py:login_label */
struct json_t *w14_legacyloca_18b83d_login_label(const struct json_t *subject);
/* PoP: w14_legacyloca_18b83d_same_owner @ session_rules/legacy_local.py:same_owner */
struct json_t *w14_legacyloca_18b83d_same_owner(const struct json_t *stored, const struct json_t *subject, const struct json_t *rule);
/* PoP: w14_legacyloca_18b83d_owners_agree @ session_rules/legacy_local.py:owners_agree */
struct json_t *w14_legacyloca_18b83d_owners_agree(const struct json_t *first, const struct json_t *second, const struct json_t *rule);
/* PoP: w14_legacyloca_18b83d_binding_names @ session_rules/legacy_local.py:binding_names */
struct json_t *w14_legacyloca_18b83d_binding_names(const struct json_t *saved, const struct json_t *subject, const struct json_t *rule);
/* PoP: w14_legacyloca_18b83d_bindings_agree @ session_rules/legacy_local.py:bindings_agree */
struct json_t *w14_legacyloca_18b83d_bindings_agree(const struct json_t *first, const struct json_t *second, const struct json_t *rule);
/* PoP: w14_legacyloca_18b83d_client_label @ session_rules/legacy_local.py:client_label */
struct json_t *w14_legacyloca_18b83d_client_label(const struct json_t *source, const struct json_t *rule);
/* PoP: w14_legacyloca_18b83d_legacy_history_row @ session_rules/legacy_local.py:legacy_history_row */
struct json_t *w14_legacyloca_18b83d_legacy_history_row(const struct json_t *row, const struct json_t *rule);
/* PoP: w14_legacyloca_18b83d_legacy_creation_source @ session_rules/legacy_local.py:legacy_creation_source */
struct json_t *w14_legacyloca_18b83d_legacy_creation_source(const struct json_t *row, const struct json_t *rule);
/* PoP: w14_legacyloca_18b83d_legacy_account_label @ session_rules/legacy_local.py:legacy_account_label */
struct json_t *w14_legacyloca_18b83d_legacy_account_label(const struct json_t *row);
/* PoP: w14_legacyloca_18b83d_login_owned_by @ session_rules/legacy_local.py:login_owned_by */
struct json_t *w14_legacyloca_18b83d_login_owned_by(const struct json_t *row, const struct json_t *subject, const struct json_t *rule);
/* PoP: w14_legacyloca_18b83d_validate_legacy_row @ session_rules/legacy_local.py:validate_legacy_row */
struct json_t *w14_legacyloca_18b83d_validate_legacy_row(const struct json_t *own_profile, const struct json_t *row,
        const struct json_t *subject, const struct json_t *rule);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_LEGACYLOCA_18B83D_H */
