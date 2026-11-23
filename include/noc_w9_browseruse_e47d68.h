#ifndef NOC_W9_BROWSERUSE_E47D68_H
#define NOC_W9_BROWSERUSE_E47D68_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w9_browseruse_e47d68_harness_command @ tools/browser_use_shot.py:harness_command */
struct json_t *w9_browseruse_e47d68_harness_command(const struct json_t *cmd, const char *activate, const char *this_file);
/* PoP: w9_browseruse_e47d68_activation @ tools/browser_use_shot.py:_activation_check */
int w9_browseruse_e47d68_activation(const char *mode, int is_cloud);
/* PoP: w9_browseruse_e47d68_is_harness_helper @ tools/browser_use_shot.py:_is_harness_helper */
int w9_browseruse_e47d68_is_harness_helper(const char *module_name, int is_callable);
/* PoP: w9_browseruse_e47d68_constants @ tools/browser_use_shot.py:<module constants> */
struct json_t *w9_browseruse_e47d68_constants(void);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_BROWSERUSE_E47D68_H */
