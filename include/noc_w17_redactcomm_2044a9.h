#ifndef NOC_W17_REDACTCOMM_2044A9_H
#define NOC_W17_REDACTCOMM_2044A9_H

#include "hermes_json.h"

/* Replay of agent/redact_command_segments.py::_command_segments.
 *
 * Splits a shell command on unquoted ``| ; &`` only, so quoted awk/grep
 * programs stay whole. Backslash is NOT an escape (Windows paths). Each
 * segment is str.strip()'d and empty segments are dropped.
 *
 * Returns a new JSON array of strings; caller owns it. */
json_t *w17_redactcomm_2044a9_command_segments(const char *command);

#endif
