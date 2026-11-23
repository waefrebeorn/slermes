#ifndef NOC_SLTARGET_H
#define NOC_SLTARGET_H
/*
 * Public C API for the port of plugins/platforms/slack/send_targets.py
 * (gate sltarget).  All exported symbols carry the sltarget_ prefix.
 */
/*
 * PoP: sltarget_parse_send_target @ plugins/platforms/slack/send_targets.py:_parse_send_target
 * Faithful port of _parse_send_target's ordered fullmatch forms.
 * Returns 1 on a match, malloc'ing *chat and (*thread may be NULL for forms
 * without a thread id); caller frees both.  Returns 0 for Python None.
 */
int sltarget_parse_send_target(const char *ref, char **chat, char **thread);

#endif /* NOC_SLTARGET_H */
