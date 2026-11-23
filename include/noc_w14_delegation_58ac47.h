#ifndef NOC_W14_DELEGATION_58AC47_H
#define NOC_W14_DELEGATION_58AC47_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_delegation_58ac47_redact_child_text @ tools/delegation_redaction.py:redact_child_text */
struct json_t *w14_delegation_58ac47_redact_child_text(const struct json_t *text, int force);
/* PoP: w14_delegation_58ac47_redact_child_value @ tools/delegation_redaction.py:redact_child_value */
struct json_t *w14_delegation_58ac47_redact_child_value(const struct json_t *value, int force);
/* PoP: w14_delegation_58ac47_redact_child_result @ tools/delegation_redaction.py:redact_child_result */
struct json_t *w14_delegation_58ac47_redact_child_result(const struct json_t *result, const struct json_t *output_schema, int force);
/* PoP: w14_delegation_58ac47_redact_completion_event @ tools/delegation_redaction.py:redact_completion_event */
struct json_t *w14_delegation_58ac47_redact_completion_event(const struct json_t *event, int force);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_DELEGATION_58AC47_H */
