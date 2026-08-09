/*
 * port_hermes_cli_slash_exec.h — C11 port of hermes_cli/slash_exec.py
 */
#ifndef PORT_HERMES_CLI_SLASH_EXEC_H
#define PORT_HERMES_CLI_SLASH_EXEC_H

#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#include "hermes_core_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CommandContext: surface-provided inputs for a shared command executor. */
typedef struct {
    const char *surface;     /* "cli" | "gateway" | "tui" — decoration only */
    const char *args;        /* raw argument string after command word */
    const char *options_str; /* options as "key=val\tdelta=val" */
} CommandContext;

/* CommandReply: canonical result of a shared executor. */
typedef struct {
    char *text;    /* surface-independent core text */
    const char *data; /* structured values (opaque in C) */
    const char *format; /* "plain" | "markdown" (hint) */
} CommandReply;

/* Context lifecycle */
CommandContext *slash_ctx_new(const char *surface, const char *args,
                              const char *options_str);
void slash_ctx_free(CommandContext *ctx);
const char *slash_ctx_option(const CommandContext *ctx, const char *key);

/* Reply lifecycle */
CommandReply *cmd_reply_new(const char *text, const char *fmt);
void cmd_reply_free(CommandReply *r);

/* Executors — pure formatters */
CommandReply *slash_exec_version(const CommandContext *ctx);
CommandReply *slash_exec_egress(const CommandContext *ctx);
CommandReply *slash_exec_profile(const CommandContext *ctx);
CommandReply *slash_exec_bundles(const CommandContext *ctx);
CommandReply *slash_exec_help(const CommandContext *ctx);
CommandReply *slash_exec_commands(const CommandContext *ctx);

/* Registry + resolution */
CommandReply *(*slash_exec_resolve(const char *key))(const CommandContext *);
CommandReply *slash_exec_run(const char *execute_key, const CommandContext *ctx);
CommandReply *slash_exec_execute(const char *name, const CommandContext *ctx);

/* Supporting builders (also PoP-backed; declared for reuse/testability) */
char *format_banner_version_label(void);
char *format_status_text(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HERMES_CLI_SLASH_EXEC_H */
