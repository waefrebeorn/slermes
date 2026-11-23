#ifndef NOC_W9_DATACLEANUP_07FE4B_H
#define NOC_W9_DATACLEANUP_07FE4B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_datacleanup_07fe4b_plan @ hermes_cli/data_cleanup.py:plan_data_removal
 * spec carries the virtual filesystem and the injected path-helper results;
 * returns {"remove":[...],"keep":[...]} or {"error":"..."}. */
struct json_t *w9_datacleanup_07fe4b_plan(const struct json_t *spec);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_DATACLEANUP_07FE4B_H */
