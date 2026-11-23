#ifndef NOC_W7_VENVSYNC_D6306E_H
#define NOC_W7_VENVSYNC_D6306E_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_venvsync_d6306e_is_read_only_gateway_status @ hermes_cli/venv_sync.py:_is_read_only_gateway_status */
int w7_venvsync_d6306e_is_read_only_gateway_status(const struct json_t *command);

/* PoP: w7_venvsync_d6306e_supervised_child @ hermes_cli/venv_sync.py:_supervised_child
 * The two launcher-marker env values are injected (no process env mutation). */
int w7_venvsync_d6306e_supervised_child(const char *supervised, const char *s6_supervised);

/* PoP: w7_venvsync_d6306e_completion_retry_constants @ hermes_cli/venv_sync.py:COMPLETION_RETRY_* */
struct json_t *w7_venvsync_d6306e_completion_retry_constants(void);

/* PoP: w7_venvsync_d6306e_string_set @ hermes_cli/venv_sync.py:_METADATA_FLAGS /
 * _REPORT_ONLY_UPDATE_FLAGS / _TAIL_SCRIPTS / _SYNC_INPUTS */
struct json_t *w7_venvsync_d6306e_string_set(const char *which);

/* PoP: w7_venvsync_d6306e_relaunch_command @ hermes_cli/venv_sync.py:relaunch_command
 * python/root are injected as their str() values; argv/original are string arrays. */
struct json_t *w7_venvsync_d6306e_relaunch_command(const char *python, const char *root,
                                                  const struct json_t *argv,
                                                  const struct json_t *original,
                                                  const char *module);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_VENVSYNC_D6306E_H */
