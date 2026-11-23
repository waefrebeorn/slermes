/* config_show.c — C home for upstream Hermes hermes_cli/config_show.py.
 *
 * PoP: w8_configshow_show_display_section_ex @ hermes_cli/config_show.py:_show_display_section
 *
 * Faithful port of _show_display_section's value logic.  In the oracle the
 * personality import is forced to fail (active_personality_name raises), so
 * the module's own fallback path is exercised:
 *   display = config.get('display', {})
 *   active_personality = display.get('personality') or 'none'
 *   on_off = lambda flag: 'on' if flag else 'off'
 *   ... show_reasoning (default True), bell_on_complete/prompt (default False)
 *   ump = display.get('user_message_preview', {}); ump if isinstance(ump, dict) else {}
 *   ... first_lines / last_lines (default 2 each)
 * The two presentation seams (`_section` via identity color, and `color`) are
 * deterministic in the oracle, so the six printed lines are reproduced here.
 */
#include "noc_w8_configshow_5e4345.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

/* Python truthiness for the JSON values the fixture can carry. */
static int py_truthy(const json_t *v, int dflt) {
    if (!v) return dflt;                       /* key absent -> Python .get default */
    if (v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return dflt;
    }
}

/* Best-effort Python str() for scalars used in f-strings. */
static void py_str(const json_t *v, const char *dflt, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "%s", dflt ? dflt : "None"); return; }
    switch (v->type) {
        case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, cap, "%lld", (long long)v->num_val);
            else
                snprintf(buf, cap, "%g", v->num_val);
            return;
        default: snprintf(buf, cap, "%s", dflt ? dflt : "None"); return;
    }
}

json_t *w8_configshow_show_display_section_ex(const json_t *config) {
    json_t *out = json_array();
    json_append(out, json_string(""));
    json_append(out, json_string("\xE2\x97\x86 Display")); /* ◆ Display */

    const json_t *display = (config && config->type == JSON_OBJECT)
        ? json_obj_get(config, "display") : NULL;
    if (!display || display->type != JSON_OBJECT) display = NULL;

    /* active_personality = display.get('personality') or 'none' */
    const json_t *pers = display ? json_obj_get(display, "personality") : NULL;
    char personality[1024];
    if (pers && py_truthy(pers, 0)) py_str(pers, "none", personality, sizeof(personality));
    else snprintf(personality, sizeof(personality), "none");

    /* on_off on show_reasoning (default True), bell flags (default False) */
    const json_t *sr = display ? json_obj_get(display, "show_reasoning") : NULL;
    const json_t *bc = display ? json_obj_get(display, "bell_on_complete") : NULL;
    const json_t *bp = display ? json_obj_get(display, "bell_on_prompt") : NULL;
    const char *reasoning = py_truthy(sr, 1) ? "on" : "off";
    const char *bell_complete = py_truthy(bc, 0) ? "on" : "off";
    const char *bell_prompt = py_truthy(bp, 0) ? "on" : "off";

    /* user_message_preview must be a dict, else {}; defaults 2 / 2 */
    const json_t *ump = display ? json_obj_get(display, "user_message_preview") : NULL;
    const json_t *ump_obj = (ump && ump->type == JSON_OBJECT) ? ump : NULL;
    const json_t *fl = ump_obj ? json_obj_get(ump_obj, "first_lines") : NULL;
    const json_t *ll = ump_obj ? json_obj_get(ump_obj, "last_lines") : NULL;
    char first[256], last[256];
    py_str(fl, "2", first, sizeof(first));
    py_str(ll, "2", last, sizeof(last));

    char line[4096];
    snprintf(line, sizeof(line), "  Personality:  %s", personality);
    json_append(out, json_string(line));
    snprintf(line, sizeof(line), "  Reasoning:    %s", reasoning);
    json_append(out, json_string(line));
    snprintf(line, sizeof(line), "  Bell:         complete=%s, prompt=%s", bell_complete, bell_prompt);
    json_append(out, json_string(line));
    snprintf(line, sizeof(line), "  User preview: first %s line(s), last %s line(s)", first, last);
    json_append(out, json_string(line));
    return out;
}

