#ifndef NOC_W16_SESSIONLOC_EAA05A_H
#define NOC_W16_SESSIONLOC_EAA05A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_sessionloc_eaa05a_validate_editor @ gateway/session_local_editor.py:validate_editor */
struct json_t *w16_sessionloc_eaa05a_validate_editor(const struct json_t *source,
                                                     const struct json_t *editor);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_SESSIONLOC_EAA05A_H */
