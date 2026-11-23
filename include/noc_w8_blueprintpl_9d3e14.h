#ifndef NOC_W8_BLUEPRINTPL_9D3E14_H
#define NOC_W8_BLUEPRINTPL_9D3E14_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: blueprintpl_text @ cron/blueprint_plugins.py:_text */
struct json_t *blueprintpl_text(const char *name, struct json_t *value);

/* PoP: blueprintpl_strings @ cron/blueprint_plugins.py:_strings */
struct json_t *blueprintpl_strings(const char *name, struct json_t *value);

/* PoP: blueprintpl_placeholders @ cron/blueprint_plugins.py:_placeholders
 *
 * Mirrors string.Formatter().parse() field-name extraction and the
 * ^[A-Za-z_]\w{0,63}$ slot-name check from the Python source. */
struct json_t *blueprintpl_placeholders(const char *template_str, const char *what);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_BLUEPRINTPL_9D3E14_H */
