#ifndef NOC_W17_REDACTAPPR_569956_H
#define NOC_W17_REDACTAPPR_569956_H

/* Replay of agent/redact_approval_display.py::sanitize_command_for_display.
 *
 * Makes invisible/blank-rendering code points visible, collapses blank-line
 * runs and long whitespace padding. ``prose`` selects the lighter pass that
 * leaves ZWJ/ZWNJ/variation selectors alone.
 *
 * Returns a malloc'd UTF-8 string; caller frees. NULL/empty input -> "". */
char *w17_redactappr_569956_sanitize(const char *text, int prose);

#endif
