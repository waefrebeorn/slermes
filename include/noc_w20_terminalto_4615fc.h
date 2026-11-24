/*
 * noc_w20_terminalto_4615fc.h -- C11 port of the macOS ``open`` frontmost
 * raise-ladder of upstream tools/terminal_tool_macos_open.py.
 */
#ifndef NOC_W20_TERMINALTO_4615FC_H
#define NOC_W20_TERMINALTO_4615FC_H

#include "hermes_json.h"

/* shlex.quote(s): POSIX shell single-quote wrapping. Caller frees. */
char *w20_terminalto_4615fc_quote(const char *s);

/* _osascript_app(script): "osascript -e '<script>' 2>/dev/null". Caller frees. */
char *w20_terminalto_4615fc_osascript_app(const char *script);

/* _build_macos_open_raise_ladder(app or NULL, file). Caller frees. */
char *w20_terminalto_4615fc_ladder(const char *app, const char *file);

/*
 * _transform_macos_open_command(command, system): the rewritten command, or the
 * input unchanged (NULL in, NULL out). Caller frees.
 */
char *w20_terminalto_4615fc_transform(const char *command, const char *system);

/* _MACOS_OPEN_NO_TRANSFORM_FLAGS, sorted. */
json_t *w20_terminalto_4615fc_no_transform_flags(void);

/* shlex.split(command) as data: {"ok":true,"tokens":[...]} or {"ok":false,"error":...} */
json_t *w20_terminalto_4615fc_split(const char *command);

#endif /* NOC_W20_TERMINALTO_4615FC_H */
