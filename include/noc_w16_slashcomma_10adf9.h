#ifndef NOC_W16_SLASHCOMMA_10ADF9_H
#define NOC_W16_SLASHCOMMA_10ADF9_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_slashcomma_10adf9_mgr_call @ gateway/slash_commands_goals.py:_mgr_call
 * Scripted seam: exc_class/exc_msg stand in for fn(*args) raising; only
 * RuntimeError and ValueError are caught, as in the live `errors=` default. */
struct json_t *w16_slashcomma_10adf9_mgr_call(const char *prefix, const struct json_t *result,
                            const char *exc_class, const char *exc_msg);
/* PoP: w16_slashcomma_10adf9_quiet_bool @ gateway/slash_commands_goals.py:_quiet_bool */
int w16_slashcomma_10adf9_quiet_bool(const struct json_t *value, int raises);
/* PoP: w16_slashcomma_10adf9_loop_route @ gateway/slash_commands_goals.py:_handle_loop_command */
struct json_t *w16_slashcomma_10adf9_loop_route(const struct json_t *src);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_SLASHCOMMA_10ADF9_H */
