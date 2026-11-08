/*
 * vault_login_tokens.h - login-form tokenisation helpers.
 *
 * Module-internal header (port_vault_login_classifier.c <-> vault_login_tokens.c).
 */
#ifndef VAULT_LOGIN_TOKENS_H
#define VAULT_LOGIN_TOKENS_H

#include <stddef.h>
#include <stdbool.h>
#include "port_vault_login_classifier.h"

int any_tok(char **t, int n, const char *const *set, size_t ns);
char * dup_str(const char *s);
void vl_free_tokens(char **t, int n);
int has_pair(char **t, int n, const char *a, const char *b);
int has_tok(char **t, int n, const char *w);
int has_triple(char **t, int n, const char *a, const char *b, const char *c);
int pair_at(char **t, int n, int i, const char *a, const char *b);
int tokenize(const char *normalized, char **tokens, int max);
char * vl_normalize_text(const char *value);

#endif /* VAULT_LOGIN_TOKENS_H */
