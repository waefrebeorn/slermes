#ifndef NOC_W16_UPDATECMD_9C2684_H
#define NOC_W16_UPDATECMD_9C2684_H

#include <stdbool.h>
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_updatecmd_9c2684_print_items @ hermes_cli/update_cmd_config.py:_print_items
 * Returns the list of lines the Python helper prints (no stdout side effects). */
struct json_t *w16_updatecmd_9c2684_print_items(const struct json_t *items,
                                                const char *label,
                                                const char *key,
                                                const char *fallback_key);

/* PoP: w16_updatecmd_9c2684_ask_configure_new_options @ hermes_cli/update_cmd_config.py:_ask_configure_new_options
 * Seam: gateway_reply / input_reply / is_interactive are injected from the fixture.
 * input_reply == NULL means EOFError; "__UDE__" means UnicodeDecodeError. */
struct json_t *w16_updatecmd_9c2684_ask_configure_new_options(
    bool assume_yes, bool gateway_mode, const char *gateway_reply,
    const char *input_reply, bool is_interactive);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_UPDATECMD_9C2684_H */
