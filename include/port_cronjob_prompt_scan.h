#ifndef SLERMES_PORT_CRONJOB_PROMPT_SCAN_H
#define SLERMES_PORT_CRONJOB_PROMPT_SCAN_H

/* _first_pattern_error(text, pattern_sets...): scan `text` against each
 * (regex, pid) in the newline-tab-encoded `patterns_json` (PATTERN<TAB>PID per
 * line); return the fixed block message naming the first pid whose
 * case-insensitive search matches, else "" (see the .c for the literal). */
const char *cps_first_pattern_error(const char *text, const char *patterns_json);

#endif /* SLERMES_PORT_CRONJOB_PROMPT_SCAN_H */
