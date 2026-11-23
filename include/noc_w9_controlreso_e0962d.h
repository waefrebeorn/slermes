#ifndef NOC_W9_CONTROLRESO_E0962D_H
#define NOC_W9_CONTROLRESO_E0962D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_controlreso_e0962d_plain @ gateway/control_resolve.py:_plain_spelling */
struct json_t *w9_controlreso_e0962d_plain(const struct json_t *in);

/* PoP: w9_controlreso_e0962d_requested @ gateway/control_resolve.py:_requested_home
 * Returns {"ok":bool,"home":string|null}. Caller frees. */
struct json_t *w9_controlreso_e0962d_requested(const struct json_t *in);

/* PoP: w9_controlreso_e0962d_constants @ gateway/control_resolve.py:RESOLVE_VERB/RESOLVE_PROTOCOL */
struct json_t *w9_controlreso_e0962d_constants(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_CONTROLRESO_E0962D_H */
