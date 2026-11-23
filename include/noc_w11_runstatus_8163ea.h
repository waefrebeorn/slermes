#ifndef NOC_W11_RUNSTATUS_8163EA_H
#define NOC_W11_RUNSTATUS_8163EA_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_runstatus_8163ea_hidden_outside_dm @ gateway/run_status_filter.py:hidden_outside_dm
 * True when `message` is the DM-only compression-blocked warning and the
 * chat type is not a DM. `chat_type` may be a JSON string/null/number/bool or
 * an object {"value": ...}; its .value is used, then str(x or "").strip().lower().
 * The fixed emit-site template is matched with fullmatch semantics. */
int w11_runstatus_8163ea_hidden_outside_dm(const struct json_t *chat_type,
                                            const char *message);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W11_RUNSTATUS_8163EA_H */
