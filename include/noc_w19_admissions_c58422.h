/* noc_w19_admissions_c58422.h - public C API for persistence/authority/admissions.py port. */
#ifndef NOC_W19_ADMISSIONS_C58422_H
#define NOC_W19_ADMISSIONS_C58422_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_admissions_c58422_raise_gate @ persistence/authority/admissions.py:raise_gate
 * gate_kind: 0 open (None), 1 a decided GateRefusal, 2 a callable answering None,
 * 3 a callable answering a GateRefusal. Answers the refusal the verb raises. */
json_t *w19_admissions_c58422_raise_gate(int gate_kind, const char *reason,
                                        const char *field, const char *detail);

/* PoP: w19_admissions_c58422_refusal @ persistence/authority/admissions.py:GateRefusal.refusal
 * The refusal object a decided gate raises: reason / field / detail. */
json_t *w19_admissions_c58422_refusal(const char *reason, const char *field, const char *detail);

/* PoP: w19_admissions_c58422_steer_replayed @ persistence/authority/admissions.py:steer_replayed
 * A committed transfer (terminal, steering or steered) replays an exact retry. */
int w19_admissions_c58422_steer_replayed(const char *status, const char *outcome);

/* PoP: w19_admissions_c58422_coerce_record @ persistence/authority/admissions.py:AdmissionRecord.__post_init__
 * The stored intent/status CHECK sets: a value outside them is refused with the enum's ValueError;
 * a stored value becomes its member (equal to the stored string). */
json_t *w19_admissions_c58422_coerce_record(const char *intent, const char *status);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_ADMISSIONS_C58422_H */
