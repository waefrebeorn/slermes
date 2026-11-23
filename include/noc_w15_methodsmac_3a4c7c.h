#ifndef NOC_W15_METHODSMAC_3A4C7C_H
#define NOC_W15_METHODSMAC_3A4C7C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_methodsmac_3a4c7c_machine_facts @ tui_gateway/methods_machine.py:_
 * The machine.facts handler publishes the host summary minus `full_name`
 * (the account name reaches the desktop only on the onboarding name card). */
struct json_t *w15_methodsmac_3a4c7c_machine_facts(const struct json_t *summary);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_METHODSMAC_3A4C7C_H */
