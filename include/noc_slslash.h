#ifndef NOC_SLSLASH_H
#define NOC_SLSLASH_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: slslash_command_text @ plugins/platforms/slack/adapter_slash_payload.py:slash_command_text */
char *slslash_command_text(const struct json_t *command);
/* PoP: slslash_thread_id @ plugins/platforms/slack/adapter_slash_payload.py:slash_thread_id
 * Returns a malloc'd string, or NULL for Python None. */
char *slslash_thread_id(const struct json_t *command);
#ifdef __cplusplus
}
#endif
#endif /* NOC_SLSLASH_H */
