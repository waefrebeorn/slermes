/*
 * noc_w21_telegramst_ddf41d.h -- public C API for the port of
 *   plugins/platforms/telegram/telegram_status.py (wave 21 batch 5).
 *
 * `send_or_update_status` is one serialized editable-status step: take the
 * per-(chat, status_key) lock, edit the cached message when the key has one
 * (writing the new id back only when nobody evicted it during the await), fall
 * back to a fresh send, and FIFO-trim the id map at the adapter bound.
 *
 * The adapter is a seam: the caller supplies the id map in insertion order, the
 * scripted answers of edit_message()/send(), and what the map does while the
 * edit is awaited. The returned record is the SendResult plus the resulting id
 * map and the ordered adapter calls.
 */
#ifndef NOC_W21_TELEGRAMST_DDF41D_H
#define NOC_W21_TELEGRAMST_DDF41D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* TelegramAdapter._STATUS_MESSAGE_IDS_MAX (adapter.py). */
#define W21_TELEGRAMST_DDF41D_STATUS_IDS_MAX 2000

typedef struct {
    const json_t *message_ids; /* [["<chat>","<status_key>","<message id>"], ...] in dict order */
    long long max_ids;         /* adapter._STATUS_MESSAGE_IDS_MAX (0 when unset -> the default) */
    int lock_exists;           /* adapter._status_locks already holds (chat, status_key) */
    const json_t *edit;        /* scripted edit_message answer {"success":bool,"message_id":...} */
    const json_t *send;        /* scripted send answer                                  */
    const char *during_edit;   /* "replace" | "pop" | NULL: what the map does during the await */
} w21_telegramst_ddf41d_env;

/* PoP: w21_telegramst_ddf41d_send_or_update @ plugins/platforms/telegram/telegram_status.py:send_or_update_status
 * Returns {"result": {"success":..., "message_id":...}, "ids": [[chat,key,id],...],
 * "lock_created": bool, "ops": ["edit_message"|"send", ...]}. */
json_t *w21_telegramst_ddf41d_send_or_update(const json_t *chat_id, const json_t *status_key,
                                             const char *content,
                                             const w21_telegramst_ddf41d_env *e);

/* `str(value)` for a decoded JSON scalar (NULL reads as None): the key
 * components the live code builds as `(str(chat_id), str(status_key))`.
 * Caller frees. */
char *w21_telegramst_ddf41d_py_str(const json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_TELEGRAMST_DDF41D_H */
