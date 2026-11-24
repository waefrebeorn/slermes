#ifndef NOC_W20_ERRORS_93204B_H
#define NOC_W20_ERRORS_93204B_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/conversation/_errors.py (gate w20_errors_93204b).
 *
 * ``refusals`` is one context manager whose whole content is a translation
 * table: a store-level failure comes out as the contract's class, chaining the
 * original. The table is the module; the ``with`` statement is syntax. This port
 * exposes the decision as a pure function over the raised class, the fields the
 * target constructors read, and the two live seams (``holder`` /
 * ``get_compression_tip``).
 *
 * PoP: w20_errors_93204b_translate @ persistence/sqlite/conversation/_errors.py:refusals
 * PoP: w20_errors_93204b_target_class @ persistence/sqlite/conversation/_errors.py:refusals
 */

/* PoP: w20_errors_93204b_target_class @ persistence/sqlite/conversation/_errors.py:refusals
 * The contract class ``exc_type`` is translated into, or NULL when the original
 * exception passes through unchanged. ``reason`` is the AuthorityRefusal reason
 * (ignored for every other class): a bare AuthorityRefusal is only rewritten
 * for ``unknown_execution``; every other reason re-raises as-is. This is the
 * isinstance chain's first-match order, walked by class name. */
const char *w20_errors_93204b_target_class(const char *exc_type, const char *reason);

/* PoP: w20_errors_93204b_translate @ persistence/sqlite/conversation/_errors.py:refusals
 * The context manager's outcome for one raised exception.
 *   fields = {"reason"?, "message"?, "session_id"?, "message_count"?,
 *             "limit"?, "scope"?}   the attributes the live code reads
 *   session_id                     the ``sid`` the caller passed (NULL/"" for "")
 *   holder_present                 whether a StoreHolder was supplied
 *   compression_tip                what get_compression_tip(holder, exc.session_id)
 *                                  answered -- used only when holder_present
 * Answer, either
 *   {"status":"raise","class":<contract class>,"args":[...]}   the translated raise
 *   {"status":"reraise","class":null,"exc":<original class>}   unchanged passthrough
 * str(exc) is ``fields.message`` (the store's exception text) and missing
 * attributes read as Python None, exactly like the attribute access does. */
json_t *w20_errors_93204b_translate(const char *exc_type, const json_t *fields,
                                    const char *session_id, int holder_present,
                                    const json_t *compression_tip);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_ERRORS_93204B_H */
