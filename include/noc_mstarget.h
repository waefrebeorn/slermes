#ifndef NOC_MSTARGET_H
#define NOC_MSTARGET_H
/*
 * Public C API for the port of plugins/platforms/matrix/send_targets.py
 * (gate mstarget).  All exported symbols carry the mstarget_ prefix.
 */
/*
 * PoP: mstarget_parse_send_target @ plugins/platforms/matrix/send_targets.py:_parse_send_target
 * Mirrors _parse_send_target: strip() the ref, rfind(":$"), and when the
 * separator starts at index > 0 return the (room, "$event") pair.
 * Returns 1 on a match (malloc'ing *room and *event_id; caller frees both),
 * 0 when there is no match (Python None).
 */
int mstarget_parse_send_target(const char *ref, char **room, char **event_id);

#endif /* NOC_MSTARGET_H */
