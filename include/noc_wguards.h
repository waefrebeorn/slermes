#ifndef NOC_WGUARDS_H
#define NOC_WGUARDS_H
#include <stddef.h>

/* check_outcome is the injected outcome of _check_transcript_write_guards:
 *   0 -> returns normally
 *   1 -> raises SessionCompressionInProgressError
 *   2 -> raises SessionTurnLeaseLostError
 * _write_guards_reject returns True only for 1 and 2. */
int wguards_write_guards_reject(int check_outcome);

/* Mirrors {sid for sid in ids if _write_guards_reject(...)}. outcomes[i] is
 * the check outcome for ids[i]. Unique guarded ids are written to out[] in
 * sorted order (Python set -> sorted output); returns the count written. */
size_t wguards_guarded_ids(const char *ids[], const int *outcomes, size_t n,
                           const char *out[], size_t out_cap);
#endif
