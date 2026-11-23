/*
 * update_cmd_common.c — real C11 port of upstream
 * hermes_cli/update_cmd_common.py.
 *
 * PoP: updcommon_record_stop @ hermes_cli/update_cmd_common.py:_record_stop
 * PoP: updcommon_best_effort @ hermes_cli/update_cmd_common.py:_best_effort
 *
 * _record_stop never imports the receipt module: it reads sys.modules.get() and
 * calls at most one method. The C port takes the presence of that module and of
 * each method as explicit seams (the harness supplies them from the fixture),
 * which is the only observable state the Python function consults.
 *
 * _best_effort is a contextmanager that swallows Exception and calls
 * logger.debug(message, exc). The C port models the outcome of the `with` body
 * via an explicit `raises` seam; the harness echoes message/exc so the emitted
 * debug-call shape is compared against the live logger call.
 */
#include "noc_updcommon.h"

/*
 * PoP: updcommon_record_stop @ hermes_cli/update_cmd_common.py:_record_stop
 */
int updcommon_record_stop(int has_receipt, int has_record_reason,
                          int has_record_without, int has_without) {
    if (!has_without) {
        /* record = getattr(receipt, "record_stop_reason", None)
         * if record is not None: record(reason) */
        if (has_receipt && has_record_reason) return 1;
        return 0;
    }
    /* elif (record := getattr(receipt, "record_stop_without_receipt", None)) is not None */
    if (has_receipt && has_record_without) return 2;
    return 0;
}

/*
 * PoP: updcommon_best_effort @ hermes_cli/update_cmd_common.py:_best_effort
 */
int updcommon_best_effort(int raises) {
    /* A non-Exception (BaseException) is not swallowed in Python; fixtures only
     * model ordinary Exceptions, which the contextmanager swallows and logs. */
    return raises ? 1 : 0;
}
