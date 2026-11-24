/*
 * noc_w20_telegramov_43149b.h -- public C API for the port of
 *   plugins/platforms/telegram/telegram_overflow_edit.py (wave 20 batch 9).
 *
 * An oversized final/preview edit split across the existing Telegram message
 * plus continuations. The chunker, the Telegram API and the sent-text store
 * arrive as scripted values; the decision record returned here carries the
 * SendResult the adapter would hand its caller plus the ordered adapter calls.
 */
#ifndef NOC_W20_TELEGRAMOV_43149B_H
#define NOC_W20_TELEGRAMOV_43149B_H

#include "hermes_json.h"

typedef struct {
    const json_t *adapter;       /* {"name": str, "thread_id": str|null} */
    const json_t *split;         /* {"raw_chunks": [...], "chunks": [...]} */
    const json_t *first_edit;    /* {"outcome": "ok"|"raise", "error": str} */
    const json_t *continuations; /* per-continuation outcomes */
    const json_t *metadata;      /* metadata argument (may be NULL) */
    int finalize;
} w20_telegramov_43149b_env;

/* edit_overflow_split(adapter, chat_id, message_id, content, finalize=...,
 *                     metadata=...). Returns {"result": <SendResult>,
 * "calls": [...]} -- see the gate oracle for the call vocabulary. */
json_t *w20_telegramov_43149b_edit_overflow_split(const char *chat_id,
                                                  const char *message_id,
                                                  const char *content,
                                                  const w20_telegramov_43149b_env *e);

#endif /* NOC_W20_TELEGRAMOV_43149B_H */
