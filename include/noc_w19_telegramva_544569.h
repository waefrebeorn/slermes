/* noc_w19_telegramva_544569.h - public C API for plugins/platforms/telegram/telegram_vault_code.py. */
#ifndef NOC_W19_TELEGRAMVA_544569_H
#define NOC_W19_TELEGRAMVA_544569_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_telegramva_544569_match_card @ plugins/platforms/telegram/telegram_vault_code.py:_match_vault_code_card */
json_t *w19_telegramva_544569_match_card(json_t *chat_id, json_t *user_id, json_t *replied_to,
                                         json_t *chat_type, double now, json_t *replies,
                                         json_t *retired);
/* PoP: w19_telegramva_544569_pop_reply @ plugins/platforms/telegram/telegram_vault_code.py:_pop_vault_code_reply */
json_t *w19_telegramva_544569_pop_reply(json_t *replies, json_t *retired, int pending,
                                        const char *chat_id, const char *message_id);
/* PoP: w19_telegramva_544569_prompt_text @ plugins/platforms/telegram/telegram_vault_code.py:send_vault_code_prompt */
json_t *w19_telegramva_544569_prompt_text(json_t *chat_id, json_t *user_id, json_t *site,
                                          json_t *hint, json_t *title_template,
                                          json_t *body_template, json_t *label_template);
/* PoP: w19_telegramva_544569_filter_allows @ plugins/platforms/telegram/telegram_vault_code.py:_DropPendingCodeUpdates.filter */
json_t *w19_telegramva_544569_filter_allows(int pending, json_t *message_text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_TELEGRAMVA_544569_H */
