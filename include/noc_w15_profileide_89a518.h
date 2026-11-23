#ifndef NOC_W15_PROFILEIDE_89A518_H
#define NOC_W15_PROFILEIDE_89A518_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_profileide_89a518_control_answer_failure @ hermes_cli/profile_identity.py:_control_answer_failure */
struct json_t *w15_profileide_89a518_control_answer_failure(const struct json_t *answer);
/* PoP: w15_profileide_89a518_store_error_text @ hermes_cli/profile_identity.py:_store_error_text */
struct json_t *w15_profileide_89a518_store_error_text(const char *class_name, const char *message,
                                                      int collision);
/* PoP: w15_profileide_89a518_gateway_accepts_verb @ hermes_cli/profile_identity.py:_gateway_accepts_profile_identity_verb */
int w15_profileide_89a518_gateway_accepts_verb(int identified, int raises);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_PROFILEIDE_89A518_H */
