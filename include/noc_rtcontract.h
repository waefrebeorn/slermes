#ifndef NOC_RTCONTRACT_H
#define NOC_RTCONTRACT_H
/*
 * Public C API for the port of gateway/runtime_contract.py (gate rtcontract).
 * All exported symbols are module-prefixed to avoid collisions.
 */

/*
 * Return the next gateway start action for an observation.  Returned strings
 * have static lifetime.  Returns NULL for an unknown runtime state, which is
 * where the Python original raises ValueError("unknown runtime state").
 */
const char *rtcontract_next_start_action(const char *state,
                                         int installed_service,
                                         int service_may_start,
                                         int update_paused);

#endif /* NOC_RTCONTRACT_H */
