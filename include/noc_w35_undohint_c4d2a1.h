/*
 * noc_w35_undohint_c4d2a1.h - C surface for upstream
 * agent/checkpoints/undo_hint.py (gate w35_undohint_c4d2a1).
 *
 * The value core of /undo's rollback hint: the timestamp normaliser every
 * decision in that module runs on, and the file list that becomes the named
 * part of the line.  Git (the snapshot walk, the diff, the throwaway index) and
 * the i18n catalogue are seams; what is ported here is the arithmetic that
 * decides WHICH stamps count and WHICH files get named, both of which are pure
 * value -> value.
 */
#ifndef NOC_W35_UNDOHINT_C4D2A1_H
#define NOC_W35_UNDOHINT_C4D2A1_H

#include "hermes_json.h"

/* _epoch(value) -> float | None
 *
 * bool is not a timestamp (True is 1 second after the epoch, and no caller means
 * that); a number must be finite and positive; a string must survive .strip()
 * and datetime.fromisoformat().  A naive stamp is local time, an aware one keeps
 * its offset -- both answer seconds since the epoch. */
json_t *w35_undohint_c4d2a1_epoch(const json_t *value);

/* files[:8] joined with ", ", plus how many were left out:
 * {"named": str, "more": int}.  [] -> {"named": "", "more": 0}; a non-list, or a
 * list holding a non-string, has no value (upstream raises TypeError). */
json_t *w35_undohint_c4d2a1_named_files(const json_t *files);

#endif
