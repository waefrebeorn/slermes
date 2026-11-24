#ifndef NOC_W18_SLASHCOMMA_782BF8_H
#define NOC_W18_SLASHCOMMA_782BF8_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_slashcomma_782bf8_strip_resume_name @ gateway/slash_commands_session.py:_strip_resume_name
 * Join non-flag tokens, strip, then strip one wrapping <> [] "" ''. Caller frees result. */
char *w18_slashcomma_782bf8_strip_resume_name(const struct json_t *parts);

/* PoP: w18_slashcomma_782bf8_telegram_private_chat_owner @ gateway/slash_commands_session.py:_telegram_private_chat_owner
 * (chat_id, user_id) of a Telegram DM with both ids, else [] . */
struct json_t *w18_slashcomma_782bf8_telegram_private_chat_owner(const char *platform,
                                                                const char *chat_type,
                                                                const char *chat_id,
                                                                const char *user_id);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_SLASHCOMMA_782BF8_H */
