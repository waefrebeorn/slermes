#ifndef NOC_W15_SESSIONWOR_066F13_H
#define NOC_W15_SESSIONWOR_066F13_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_sessionwor_066f13_frozen_cwd @ hermes_cli/session_worktree.py:_frozen_cwd */
struct json_t *w15_sessionwor_066f13_frozen_cwd(const struct json_t *policy);
/* PoP: w15_sessionwor_066f13_restore @ hermes_cli/session_worktree.py:restore */
struct json_t *w15_sessionwor_066f13_restore(const char *frozen_cwd, const struct json_t *record_cwd,
                                             int is_dir, const struct json_t *recreated);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_SESSIONWOR_066F13_H */
