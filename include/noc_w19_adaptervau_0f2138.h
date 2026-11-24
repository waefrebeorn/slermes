/* noc_w19_adaptervau_0f2138.h - public C API for plugins/platforms/slack/adapter_vault_code.py. */
#ifndef NOC_W19_ADAPTERVAU_0F2138_H
#define NOC_W19_ADAPTERVAU_0F2138_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_adaptervau_0f2138_mrkdwn_escape @ plugins/platforms/slack/adapter_vault_code.py:_mrkdwn_escape */
json_t *w19_adaptervau_0f2138_mrkdwn_escape(json_t *text);
/* PoP: w19_adaptervau_0f2138_author_matches @ plugins/platforms/slack/adapter_vault_code.py:_vault_code_author_matches */
json_t *w19_adaptervau_0f2138_author_matches(json_t *card_user, json_t *card_team,
                                             json_t *user_id, json_t *team_id);
/* PoP: w19_adaptervau_0f2138_card_text @ plugins/platforms/slack/adapter_vault_code.py:send_vault_code_prompt */
json_t *w19_adaptervau_0f2138_card_text(json_t *site, json_t *hint, json_t *title_template,
                                        json_t *body_template, json_t *button_template);
/* PoP: w19_adaptervau_0f2138_live_card @ plugins/platforms/slack/adapter_vault_code.py:_live_vault_code_card */
json_t *w19_adaptervau_0f2138_live_card(json_t *expires_at, double now);
/* PoP: w19_adaptervau_0f2138_filter_allows @ plugins/platforms/slack/adapter_vault_code.py:_DropVaultCodeFrames.filter */
json_t *w19_adaptervau_0f2138_filter_allows(json_t *message_text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_ADAPTERVAU_0F2138_H */
