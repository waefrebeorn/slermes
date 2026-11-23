#ifndef NOC_W15_ANONSIGNI_6F4A1E_H
#define NOC_W15_ANONSIGNI_6F4A1E_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_anonsigni_6f4a1e_drain_sign_in_copy @ hermes_cli/anon_sign_in_cli.py:drain_sign_in_copy
 * Drain a generator of sign-in states to its last terminal state's copy. */
struct json_t *w15_anonsigni_6f4a1e_drain_sign_in_copy(const struct json_t *states, int chat);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_ANONSIGNI_6F4A1E_H */
