#ifndef NOC_W21_TELEGRAM_15532D_H
#define NOC_W21_TELEGRAM_15532D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/conversation/_store/telegram.py
 * (gate w21_telegram_15532d).
 *
 * The module is one function over a live store holder: the read context, the
 * cursor, the bindings table and the `sqlite3.OperationalError` fallback all
 * belong to the open connection, so the SQL text it assembles and the bound
 * parameter tuple it hands to `conn.execute` are what this port owns.
 *
 * `_UNLINKED_SELECT_HEAD` interpolates two helper expressions at import time
 * (`_sql_preview_raw()` and `_sql_session_last_active("s")`, both from
 * `_store/sql_common.py`).  They are passed in by the caller so the template
 * here is byte-identical to the live one for any helper output.
 */

/* PoP: w21_telegram_15532d_normalize_profile @ persistence/sqlite/conversation/_store/telegram.py:_normalize_telegram_topic_profile_name
 * `str(profile_name or "").strip()` -> the name, or "default" when that is
 * empty.  Python falsiness decides first (None, "", 0, 0.0, False all read as
 * ""), then str() of the remaining scalar.  A JSON array/object is outside the
 * `Optional[str]` contract and reads as "default".  Returns a malloc'd string
 * the caller frees. */
char *w21_telegram_15532d_normalize_profile(const json_t *profile_name);

/* PoP: w21_telegram_15532d_params @ persistence/sqlite/conversation/_store/telegram.py:list_unlinked_telegram_sessions_for_user
 * The bind tuple of the profile-scoped query -- `(str(user_id),
 * profile_name, int(limit))` -- as {"status":"ok","value":[...]}, or
 * {"status":"error","type":"ValueError"|"TypeError"} when `int(limit)` raises.
 * `str()` of a scalar is Python's: None -> "None", True -> "True", integral
 * numbers without a fraction. */
json_t *w21_telegram_15532d_params(const json_t *user_id, const json_t *profile_name,
                                   const json_t *limit);

/* PoP: w21_telegram_15532d_unlinked_query @ persistence/sqlite/conversation/_store/telegram.py:_UNLINKED_SELECT_HEAD
 * `_UNLINKED_SELECT_HEAD [+ _UNLINKED_SCOPE_CLAUSES] + _UNLINKED_SELECT_TAIL`.
 * `profile_scoped` is 0 for the `sqlite3.OperationalError` fallback that runs
 * when the `telegram_dm_topic_bindings` table is absent: the profile clause and
 * its bind are dropped with it, so the fallback query takes one parameter
 * fewer.  NULL expressions read as "". */
json_t *w21_telegram_15532d_unlinked_query(int profile_scoped, const char *preview_expr,
                                           const char *last_active_expr);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_TELEGRAM_15532D_H */
