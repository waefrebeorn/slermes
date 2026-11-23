#ifndef NOC_UPDCOMMON_H
#define NOC_UPDCOMMON_H

/* C port of hermes_cli/update_cmd_common.py. All exports updcommon_-prefixed.
 *
 * The Python helpers are seam-modeled: the sys.modules receipt lookup is an
 * explicit (has_receipt/has_record_*) input, and the context-manager outcome is
 * an explicit "raises" input. The C functions model the exact observable result
 * of the live Python code for those inputs.
 */

/* Model _record_stop(reason, without_receipt=None):
 *   0 = no record call
 *   1 = receipt.record_stop_reason(reason)
 *   2 = receipt.record_stop_without_receipt(reason, without_receipt)
 * has_without is 1 when without_receipt is a non-None value. */
int updcommon_record_stop(int has_receipt, int has_record_reason,
                          int has_record_without, int has_without);

/* Model _best_effort(message) context manager: returns 1 when the wrapped body
 * raised and logger.debug(message, exc) fires, else 0. */
int updcommon_best_effort(int raises);

#endif
