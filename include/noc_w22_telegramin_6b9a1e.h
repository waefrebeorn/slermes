#ifndef NOC_W22_TELEGRAMIN_6B9A1E_H
#define NOC_W22_TELEGRAMIN_6B9A1E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/telegram/telegram_intake.py (gate
 * w22_telegramin_6b9a1e).
 *
 * `effective_update_message(update)` picks the message-like payload core treats
 * as a DM to the bot: `update.effective_message or update.message`, dropped when
 * that very object is also `update.business_message` or
 * `update.edited_business_message` (the owner's own chats relayed by Telegram
 * Business are never a DM to the bot).
 *
 * The `is` test is object identity. A decoded JSON tree cannot express two
 * references to one object, so every fixture message may carry a "label" string
 * and two messages with the same non-empty label are the same object. Python
 * singletons are honoured too: two equal booleans, and two equal integers in
 * [-5, 256], are the same object. Strings and other numbers are distinct.
 */

/* PoP: w22_telegramin_6b9a1e_effective_update_message @ plugins/platforms/telegram/telegram_intake.py:effective_update_message
 * The chosen message (a copy of the update attribute's value) or JSON null for
 * Python None. `update` may be absent (NULL) or any JSON value; a non-object
 * update has no attributes and yields null. */
json_t *w22_telegramin_6b9a1e_effective_update_message(const json_t *update);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_TELEGRAMIN_6B9A1E_H */
