#ifndef NOC_SECDISPLAY_H
#define NOC_SECDISPLAY_H
#include "hermes_json.h"

/* C port of hermes_cli/config_secret_display.py. All exports are
 * secdisplay_-prefixed. Returned malloc'd strings / json_t trees are owned
 * by the caller (json_free for trees). */
char *secdisplay_maskable_secret_text(const json_t *value);
json_t *secdisplay_redact_config_value(const json_t *value, int depth);
#endif
