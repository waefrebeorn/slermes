#ifndef NOC_W16_SLASHCOMMA_6B9C63_H
#define NOC_W16_SLASHCOMMA_6B9C63_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_slashcomma_6b9c63_paired_dm @ gateway/slash_commands_login.py:_handle_login_command */
int w16_slashcomma_6b9c63_paired_dm(const struct json_t *platform, const char *chat_type,
                  const struct json_t *chat_id);
/* PoP: w16_slashcomma_6b9c63_attempt_key @ gateway/slash_commands_login.py:_handle_login_command */
struct json_t *w16_slashcomma_6b9c63_attempt_key(const struct json_t *platform,
                               const struct json_t *chat_id,
                               const struct json_t *user_id);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_SLASHCOMMA_6B9C63_H */
