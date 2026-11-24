#ifndef NOC_W19_SSH_5F7591_H
#define NOC_W19_SSH_5F7591_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_ssh_5f7591_sync_error @ tools/environments/ssh.py:_sync_error
 * {"reason":..,"retry_hint":..} of the EnvironmentConnectionError the helper builds. */
json_t *w19_ssh_5f7591_sync_error(const char *reason, const char *subject, const char *what);

/* PoP: w19_ssh_5f7591_private_runtime_dir @ tools/environments/ssh.py:_is_private_runtime_dir
 * 1 when the path is a real directory we own with no group/other access (the gateway
 * socket's own test); 0 on lstat failure or any failed leg. */
int w19_ssh_5f7591_private_runtime_dir(int lstat_fails, long long mode, long long uid, long long my_uid);

/* PoP: w19_ssh_5f7591_control_socket_for @ tools/environments/ssh.py:SSHEnvironment._control_socket_for
 * One ControlMaster socket per SendEnv name-set beside the plain target socket.
 * Returns a malloc'd path the caller frees (NULL on allocation failure). */
char *w19_ssh_5f7591_control_socket_for(const char *control_socket, json_t *send_env);

/* PoP: w19_ssh_5f7591_target_flags @ tools/environments/ssh.py:SSHEnvironment._target_flags
 * Port/key flags shared by ssh (-p) and scp (-P). */
json_t *w19_ssh_5f7591_target_flags(const char *port_flag, long long port, const char *key_path);

/* PoP: w19_ssh_5f7591_build_ssh_command @ tools/environments/ssh.py:SSHEnvironment._build_ssh_command
 * The full ssh argv for one call: multiplex options when enabled, batch/strict-host
 * options, one SendEnv per name (sorted), the target flags, extra args, user@host. */
json_t *w19_ssh_5f7591_build_ssh_command(const char *control_socket, const char *user,
                                        const char *host, long long port, const char *key_path,
                                        json_t *send_env, json_t *extra_args, int multiplex);

/* PoP: w19_ssh_5f7591_ensure_ssh_available @ tools/environments/ssh.py:_ensure_ssh_available
 * NULL when both clients resolve, else the RuntimeError text (ssh, then scp). */
const char *w19_ssh_5f7591_ensure_ssh_available(int ssh_ok, int scp_ok);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_SSH_5F7591_H */
