/*
 * weixin_crypto.h - weixin AES-128-ECB / account-id helpers.
 *
 * Module-internal header (weixin.c <-> weixin_crypto.c).
 */
#ifndef WEIXIN_CRYPTO_H
#define WEIXIN_CRYPTO_H

#include <stddef.h>
#include <stdbool.h>

char * weixin_account_file(const char *hermes_home, const char *account_id);

#endif /* WEIXIN_CRYPTO_H */
