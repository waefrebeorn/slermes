/*
 * cli.h — Public entry point for the Hermes command-line interface.
 *
 * Faithful extraction from the monolithic hermes.h god header (the
 * god-header-elimination pass). Defined in src/cli/cli.c. The CLI
 * entry point is consumed by the thin src/cli/main.c / src/main.c shims.
 */

#ifndef CLI_H
#define CLI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Main CLI entry point. Parses argv and dispatches to command handlers. */
int cli_main(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* CLI_H */
