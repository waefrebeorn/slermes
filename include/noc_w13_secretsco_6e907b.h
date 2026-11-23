#ifndef NOC_W13_SECRETSCO_6E907B_H
#define NOC_W13_SECRETSCO_6E907B_H

#include "hermes_json.h"

/* C port of selected pure helpers in hermes_cli/_secrets_common.py
 * (gate w13_secretsco_6e907b). Every symbol is gate-prefixed.
 * char* returns are malloc'd (caller frees); json_t* returns are new refs. */

/* console_safe: strip terminal control chars, then rich-markup escape. */
char *w13_secretsco_6e907b_console_safe(const char *text);

/* yn: "[green]yes[/green]" / "[dim]no[/dim]". */
const char *w13_secretsco_6e907b_yn(bool b);

/* section_cfg: (cfg.get("secrets") or {}).get(section) or {} */
json_t *w13_secretsco_6e907b_section_cfg(const json_t *cfg, const char *section);

/* cfg_str: str(cfg.get(key, "") or "").strip() */
char *w13_secretsco_6e907b_cfg_str(const json_t *cfg, const char *key);

/* arg: (name, dict(help=help_text, **kwargs)) -> [name, dict] */
json_t *w13_secretsco_6e907b_arg(const char *name, const char *help_text, const json_t *kwargs);

/* flag: arg(name, help_text, action="store_true") */
json_t *w13_secretsco_6e907b_flag(const char *name, const char *help_text);

#endif /* NOC_W13_SECRETSCO_6E907B_H */
