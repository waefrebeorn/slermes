#ifndef NOC_W5_HERMESCONST_E6F362_H
#define NOC_W5_HERMESCONST_E6F362_H
#include <stddef.h>

/* Public C API for hermes_constants_scratch_detach.py (gate w5_hermesconst_e6f362).
 * The lineage string is the wire format between the launcher and the detached
 * child; these are the two pure transforms on that format. */

/* PoP: hcsdetach_encode_lineage @ hermes_constants_scratch_detach.py:_encode_lineage
 * chain_json: JSON null, or an array of [pid, started] pairs (started is a
 * float). Returns 1 and a malloc'd string ("-" for null), else 0. */
int hcsdetach_encode_lineage(const char *chain_json, char **out);

/* PoP: hcsdetach_decode_lineage @ hermes_constants_scratch_detach.py:run
 * Inverse transform used by the child: "-" -> null, otherwise a JSON array of
 * [pid, started] pairs. Returns 1 and a malloc'd JSON string, 0 on malformed
 * input (Python raises ValueError). */
int hcsdetach_decode_lineage(const char *lineage, char **out_json);

/* PoP: hcsdetach_child_script @ hermes_constants_scratch_detach.py:_CHILD
 * The -c program passed to the detached interpreter; malloc'd string. */
char *hcsdetach_child_script(void);

#endif /* NOC_W5_HERMESCONST_E6F362_H */
