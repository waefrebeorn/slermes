#ifndef NOC_W17_WARNINGNOT_AF9536_H
#define NOC_W17_WARNINGNOT_AF9536_H

#include "hermes_json.h"

/* Faithful C11 port of gateway/warning_notifications.py (gate w17_warningnot_af9536).
 * Producer-owned DiagnosticText markers and the display resolver are injected
 * as explicit arguments/seams so the classification logic stays testable. */
int w17_warningnot_af9536_is_warning_status(const char *event_type, int message_is_diagnostic);
int w17_warningnot_af9536_is_diagnostic_notice(const char *level, int level_is_null, const char *key);
json_t *w17_warningnot_af9536_diagnostic_metadata(int internal, const json_t *metadata);
int w17_warningnot_af9536_warning_notifications_enabled(const char *platform, const json_t *user_config);
int w17_warningnot_af9536_diagnostic_turn_muted(const json_t *display_metadata,
                                                const char *platform, const json_t *user_config);

#endif /* NOC_W17_WARNINGNOT_AF9536_H */
