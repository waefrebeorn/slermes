#ifndef HERMES_FALLBACK_CMD_H
#define HERMES_FALLBACK_CMD_H

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

void fallback_format_entry(const json_t *entry, char *out, size_t out_cap);
json_t *fallback_extract_from_model_cfg(const json_t *model_cfg);
void fallback_describe_primary(const json_t *config, char *out, size_t out_cap);

/* Subcommand handlers (hermes_cli/fallback_cmd.py) */
int cmd_fallback_list(void);
int cmd_fallback_add(void);
int cmd_fallback_remove(void);
int cmd_fallback_clear(void);

/* PoP: _numbered_pick @ hermes_cli/fallback_cmd.py:_numbered_pick */
int fallback_numbered_pick(const char *question, const char **choices, int n);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_FALLBACK_CMD_H */
