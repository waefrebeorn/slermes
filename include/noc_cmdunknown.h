#ifndef NOC_CMDUNKNOWN_H
#define NOC_CMDUNKNOWN_H
/*
 * Public C API for the port of hermes_cli/cli_unknown_command.py
 * (gate cmdunknown).  All exported symbols carry the cmdunknown_ prefix.
 */
#include "hermes_json.h"

/* str.split()[0] when there is a whitespace-delimited token, else the original. */
char *cmdunknown_command_base(const char *typed);

/* difflib.get_close_matches(base, known, n=1, cutoff=0.6)'s single winner,
 * or NULL when nothing clears the cutoff.  Caller frees a non-NULL result. */
char *cmdunknown_close_match(const char *base, const json_t *known);

/*
 * PoP: cmdunknown_unknown_command_lines @ hermes_cli/cli_unknown_command.py:unknown_command_lines
 * Returns the (lead line, pointer line) pair as two malloc'd UTF-8 strings;
 * caller frees both.  `known` is a JSON array of slash-command strings.
 * Uses the English i18n catalog (DEFAULT_LANGUAGE = "en").
 */
void cmdunknown_unknown_command_lines(const char *typed, const json_t *known,
                                      char **lead, char **pointer);

#endif /* NOC_CMDUNKNOWN_H */
