/*
 * command_token_source.h — PoP of agent/command_token_source.py.
 *
 * Python -> C symbol map (1:1):
 *   _MINT_TIMEOUT_SECONDS          -> CMD_TOKEN_MINT_TIMEOUT_SECONDS
 *   _TOKEN_REFRESH_LEEWAY_SECONDS  -> CMD_TOKEN_REFRESH_LEEWAY_SECONDS
 *   _NO_TTL_REFRESH_SECONDS        -> CMD_TOKEN_NO_TTL_REFRESH_SECONDS
 *   CommandTokenError              -> error string via out-param / NULL return
 *   _mint                          -> command_token_source__mint
 *   CommandTokenSource             -> struct command_token_source
 *   CommandTokenSource.__call__    -> command_token_source_call
 *   build_command_token_provider   -> build_command_token_provider
 */
#ifndef HERMES_AGENT_COMMAND_TOKEN_SOURCE_H
#define HERMES_AGENT_COMMAND_TOKEN_SOURCE_H

#include <stddef.h>
#include <pthread.h>

extern const int CMD_TOKEN_MINT_TIMEOUT_SECONDS;
extern const double CMD_TOKEN_REFRESH_LEEWAY_SECONDS;
extern const double CMD_TOKEN_NO_TTL_REFRESH_SECONDS;

typedef struct command_token_source {
    char *command;
    char *label;
    pthread_mutex_t lock;
    char *token;
    double expires_at; /* CLOCK_MONOTONIC seconds */
} command_token_source_t;

/* _mint: run `command` through the shell, return a malloc'd token or NULL.
 * On failure copies the exact CommandTokenError message into err.
 * out_ttl receives the advertised lifetime in seconds, or -1.0 for None. */
char *command_token_source__mint(const char *command, const char *label,
                                 char *err, size_t err_sz, double *out_ttl);

int  command_token_source_init(command_token_source_t *s, const char *command,
                               const char *label);
void command_token_source_free(command_token_source_t *s);
char *command_token_source_call(command_token_source_t *s);

command_token_source_t *build_command_token_provider(const char *key_cmd,
                                                     const char *provider_label);

#endif /* HERMES_AGENT_COMMAND_TOKEN_SOURCE_H */
