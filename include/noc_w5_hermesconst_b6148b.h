#ifndef NOC_W5_HERMESCONST_B6148B_H
#define NOC_W5_HERMESCONST_B6148B_H
#include <stddef.h>

/* Public C API for hermes_constants_scratch_stuck.py (gate w5_hermesconst_b6148b).
 * IO-bound inputs (errno, uid, fingerprint, blocker accessibility) are injected
 * by the gate, mirroring the Python seams that touch the OS. */

/* PoP: hcsstuck_is_permission_error @ hermes_constants_scratch_stuck.py:is_permission_error
 * is_oserror: whether the exception is an OSError; err: its errno value. */
int hcsstuck_is_permission_error(int is_oserror, long long err);

/* PoP: hcsstuck_accessible_ex @ hermes_constants_scratch_stuck.py:_accessible
 * lexists: os.path.lexists(path); access_ok: os.access(path, R_OK|W_OK|X_OK). */
int hcsstuck_accessible_ex(int lexists, int access_ok);

/* PoP: hcsstuck_still_stuck @ hermes_constants_scratch_stuck.py:_still_stuck
 * rec_json: the record dict; fp_json: injected _fingerprint(root/name);
 * current_uid: injected os.geteuid(); now: injected time.time();
 * access_json: JSON bool array parallel to rec["blockers"] (injected
 * _accessible results). Returns 1/0. */
int hcsstuck_still_stuck(const char *rec_json, const char *fp_json,
                         long long current_uid, double now,
                         const char *access_json);

/* PoP: hcsstuck_shlex_quote @ hermes_constants_scratch_stuck.py:removal_hint
 * Faithful port of shlex.quote(); returns a malloc'd string (caller frees). */
char *hcsstuck_shlex_quote(const char *s);

#endif /* NOC_W5_HERMESCONST_B6148B_H */
