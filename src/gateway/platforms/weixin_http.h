/*
 * weixin_http.h - weixin sync-buf + token path helpers.
 *
 * Module-internal header (weixin.c <-> weixin_http.c).
 */
#ifndef WEIXIN_HTTP_H
#define WEIXIN_HTTP_H

#include <stddef.h>
#include <stdbool.h>

#define CONTEXT_TOKEN_DIR "weixin"

void ctx_token_save(const char *user_id, const char *token);
void ensure_weixin_dir(void);

/* defined in weixin.c: reads the file-scope g_wx state */
void ctx_token_path(char *buf, size_t sz);

/* defined in weixin_crypto.c */
char *weixin_account_file(const char *hermes_home, const char *account_id);

#endif /* WEIXIN_HTTP_H */
