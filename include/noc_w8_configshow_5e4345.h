#ifndef NOC_W8_CONFIGSHOW_5E4345_H
#define NOC_W8_CONFIGSHOW_5E4345_H
#include "hermes_json.h"

/* C home for upstream Hermes hermes_cli/config_show.py (gate w8_configshow_5e4345).
 *
 * PoP: w8_configshow_show_display_section_ex @ hermes_cli/config_show.py:_show_display_section
 *
 * Faithful port of the Display section's value logic.  Under a stubbed
 * `active_personality_name` (raises -> fallback) and an identity `color()`,
 * the section prints six lines:
 *   "" / "◆ Display" / "  Personality:  <name|none>" /
 *   "  Reasoning:    on|off" /
 *   "  Bell:         complete=on|off, prompt=on|off" /
 *   "  User preview: first N line(s), last N line(s)"
 * Returns a JSON array of those lines.
 */
json_t *w8_configshow_show_display_section_ex(const json_t *config);

#endif /* NOC_W8_CONFIGSHOW_5E4345_H */
