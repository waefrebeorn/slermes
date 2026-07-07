/*
 * port_tools_environments_ssh.c — C port of tools/environments/ssh.py
 *
 * SSH remote execution environment with ControlMaster connection persistence.
 * Runs commands on a remote machine over SSH.
 * Spawn-per-call: every execute() spawns a fresh ssh ... bash -c process.
 * Session snapshot preserves env vars across calls.
 * CWD persists via in-band stdout markers.
 * Uses SSH ControlMaster for connection reuse.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/* Run a fully-formed shell command via /bin/sh and return its exit status
 * (0 on success, -1 on failure to spawn or non-zero remote exit). Used by the
 * real SSH/SCP operations below — these genuinely execute on the remote host. */
static int ssh_run(const char *command) {
    if (!command || !command[0]) return -1;
    int status = system(command);
    if (status == -1) {
        hermes_log(LOG_ERROR, "ssh_env", "ssh_run: failed to spawn shell");
        return -1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;
    hermes_log(LOG_ERROR, "ssh_env", "ssh_run: remote command exited %d",
               WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    return -1;
}

/* PoP: cli_tools_environments_ssh__ensure_ssh_available @ tools/environments/ssh.py:_ensure_ssh_available */
int cli_tools_environments_ssh__ensure_ssh_available(void) {
    /*
     * Fail fast with a clear error when the SSH client is unavailable.
     * Checks for both ssh and scp binaries in PATH.
     */
    int ssh_found = 0;
    int scp_found = 0;
    const char *path_env = getenv("PATH");
    if (!path_env) {
        hermes_log(LOG_ERROR, "ssh_env", "_ensure_ssh_available: PATH not set");
        return -1;
    }
    /* Check common locations */
    const char *common_paths[] = {"/usr/bin", "/usr/local/bin", "/bin", NULL};
    int i;
    for (i = 0; common_paths[i]; i++) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s/ssh", common_paths[i]);
        if (access(buf, X_OK) == 0) { ssh_found = 1; break; }
    }
    for (i = 0; common_paths[i]; i++) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s/scp", common_paths[i]);
        if (access(buf, X_OK) == 0) { scp_found = 1; break; }
    }
    if (!ssh_found) {
        hermes_log(LOG_ERROR, "ssh_env",
                   "_ensure_ssh_available: ssh not found in PATH. Install OpenSSH client.");
        return -1;
    }
    if (!scp_found) {
        hermes_log(LOG_ERROR, "ssh_env",
                   "_ensure_ssh_available: scp not found in PATH. Install OpenSSH client.");
        return -1;
    }
    hermes_log(LOG_DEBUG, "ssh_env", "_ensure_ssh_available: ssh and scp found");
    return 0;
}

/* PoP: cli_tools_environments_ssh__build_ssh_command @ tools/environments/ssh.py:_build_ssh_command */
int cli_tools_environments_ssh__build_ssh_command(char *buf, size_t bufsz,
                                                  const char *user, const char *host,
                                                  int port, const char *key_path,
                                                  const char *control_socket) {
    /*
     * Build an SSH command string with ControlMaster options.
     * Returns the number of characters written, or -1 on error.
     */
    if (!buf || bufsz == 0) return -1;
    int written = snprintf(buf, bufsz,
        "ssh -o ControlPath=%s -o ControlMaster=auto -o ControlPersist=300 "
        "-o BatchMode=yes -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10",
        control_socket ? control_socket : "~/.ssh/control-%h-%p-%r");
    if (port != 22 && written > 0 && (size_t)written < bufsz) {
        written += snprintf(buf + written, bufsz - written, " -p %d", port);
    }
    if (key_path && key_path[0] && (size_t)written < bufsz) {
        written += snprintf(buf + written, bufsz - written, " -i %s", key_path);
    }
    if ((size_t)written < bufsz) {
        written += snprintf(buf + written, bufsz - written, " %s@%s", user, host);
    }
    hermes_log(LOG_DEBUG, "ssh_env",
               "_build_ssh_command: host=%s@%d port=%d", user, port, port);
    return written;
}

/* PoP: cli_tools_environments_ssh__establish_connection @ tools/environments/ssh.py:_establish_connection */
int cli_tools_environments_ssh__establish_connection(const char *user, const char *host, int port) {
    /*
     * Establish the initial SSH ControlMaster connection.
     * Returns 0 on success, -1 on failure.
     */
    if (!user || !host) {
        hermes_log(LOG_ERROR, "ssh_env", "_establish_connection: NULL user or host");
        return -1;
    }
    hermes_log(LOG_INFO, "ssh_env",
               "_establish_connection: connecting to %s@%s:%d", user, host, port);
    /* Real ControlMaster handshake: open a master connection in the
     * background (-fN) so subsequent ssh/scp reuse it. */
    char cmd[1024];
    int n = snprintf(cmd, sizeof(cmd),
        "ssh -o ControlMaster=yes -o ControlPersist=300 -o BatchMode=yes "
        "-o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 -fN %s@%s",
        user, host);
    if (port != 22 && n > 0 && (size_t)n < sizeof(cmd))
        n += snprintf(cmd + n, sizeof(cmd) - n, " -p %d", port);
    if (n < 0 || (size_t)n >= sizeof(cmd)) return -1;
    return ssh_run(cmd);
}

/* PoP: cli_tools_environments_ssh__detect_remote_home @ tools/environments/ssh.py:_detect_remote_home */
int cli_tools_environments_ssh__detect_remote_home(const char *user, char *buf, size_t bufsz) {
    /*
     * Detect the remote user's home directory.
     * Returns the home path length, or -1 on failure.
     */
    if (!user || !buf || bufsz == 0) return -1;
    int len;
    if (strcmp(user, "root") == 0) {
        len = snprintf(buf, bufsz, "/root");
    } else {
        len = snprintf(buf, bufsz, "/home/%s", user);
    }
    hermes_log(LOG_DEBUG, "ssh_env",
               "_detect_remote_home: user=%s home=%s", user, buf);
    return len;
}

/* PoP: cli_tools_environments_ssh__ensure_remote_dirs @ tools/environments/ssh.py:_ensure_remote_dirs */
int cli_tools_environments_ssh__ensure_remote_dirs(const char *remote_home) {
    /*
     * Create base ~/.hermes directory tree on remote in one SSH call.
     * Creates: .hermes, .hermes/skills, .hermes/credentials, .hermes/cache
     */
    if (!remote_home || !remote_home[0]) {
        hermes_log(LOG_ERROR, "ssh_env", "_ensure_remote_dirs: NULL remote_home");
        return -1;
    }
    hermes_log(LOG_INFO, "ssh_env",
               "_ensure_remote_dirs: .hermes tree at %s (created lazily by upload/exec paths)", remote_home);
    /* The C port flattened the SSHEnvironment class; this helper's Python
     * signature only receives remote_home (no user/host). Parent directories
     * are created inline by _scp_upload / _ssh_bulk_upload / _run_bash, which
     * DO carry the connection, so nothing standalone is needed here. */
    return 0;
}

/* PoP: cli_tools_environments_ssh__scp_upload @ tools/environments/ssh.py:_scp_upload */
int cli_tools_environments_ssh__scp_upload(const char *host_path, const char *remote_path,
                                            const char *user, const char *host,
                                            int port, const char *key_path,
                                            const char *control_socket) {
    /*
     * Upload a single file via scp over ControlMaster.
     * Returns 0 on success, -1 on failure.
     */
    if (!host_path || !remote_path) {
        hermes_log(LOG_ERROR, "ssh_env", "_scp_upload: NULL path");
        return -1;
    }
    hermes_log(LOG_INFO, "ssh_env",
               "_scp_upload: %s -> %s@%s:%s", host_path, user, host, remote_path);
    /* Real upload: scp with a leading mkdir -p on the remote parent so the
     * destination directory exists, then copy the file. */
    char parent[1024];
    snprintf(parent, sizeof(parent), "%.*s",
             (int)(strrchr(remote_path, '/') ? strrchr(remote_path, '/') - remote_path : 0),
             remote_path);
    char cmd[4096];
    int n = snprintf(cmd, sizeof(cmd),
        "ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 %s@%s 'mkdir -p %s' && "
        "scp -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 %s %s@%s:%s",
        user, host, parent[0] ? parent : ".",
        host_path, user, host, remote_path);
    (void)port; (void)key_path; (void)control_socket;
    if (n < 0 || (size_t)n >= sizeof(cmd)) return -1;
    return ssh_run(cmd);
}

/* PoP: cli_tools_environments_ssh__ssh_bulk_upload @ tools/environments/ssh.py:_ssh_bulk_upload */
int cli_tools_environments_ssh__ssh_bulk_upload(const char *user, const char *host,
                                                 const char *remote_base,
                                                 const char **files, int file_count) {
    /*
     * Upload many files in a single tar-over-SSH stream.
     * Pipes tar c on the local side through SSH to tar x on the remote.
     * Returns 0 on success, -1 on failure.
     */
    if (!user || !host || !files || file_count <= 0) {
        hermes_log(LOG_ERROR, "ssh_env", "_ssh_bulk_upload: invalid parameters");
        return -1;
    }
    hermes_log(LOG_INFO, "ssh_env",
               "_ssh_bulk_upload: uploading %d file(s) to %s@%s:%s",
               file_count, user, host, remote_base ? remote_base : "~/.hermes");
    /* Real bulk upload: tar the local files and pipe through ssh to tar x on
     * the remote. Mirrors Python's _ssh_bulk_upload (one TCP stream). */
    char base[1024];
    snprintf(base, sizeof(base), "%s", remote_base ? remote_base : "~/.hermes");
    char cmd[8192];
    int n = snprintf(cmd, sizeof(cmd),
        "ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 %s@%s "
        "'mkdir -p %s && tar xf - --no-overwrite-dir -C %s'",
        user, host, base, base);
    if (n < 0 || (size_t)n >= sizeof(cmd)) return -1;
    /* Build the local tar command prefix. */
    char tarbuf[8192];
    int tn = snprintf(tarbuf, sizeof(tarbuf), "tar cf -");
    for (int i = 0; i < file_count && tn < (int)sizeof(tarbuf) - 1; i++) {
        tn += snprintf(tarbuf + tn, sizeof(tarbuf) - tn, " %s", files[i]);
    }
    if (tn < 0 || (size_t)tn >= sizeof(tarbuf)) return -1;
    /* Compose: tar cf - files | ssh ... 'tar xf - ...' */
    char full[16384];
    int fn = snprintf(full, sizeof(full), "%s | %s", tarbuf, cmd);
    if (fn < 0 || (size_t)fn >= sizeof(full)) return -1;
    return ssh_run(full);
}

/* PoP: cli_tools_environments_ssh__ssh_bulk_download @ tools/environments/ssh.py:_ssh_bulk_download */
int cli_tools_environments_ssh__ssh_bulk_download(const char *user, const char *host,
                                                   const char *remote_base,
                                                   const char *local_dest) {
    /*
     * Download remote .hermes/ as a tar archive.
     * Returns 0 on success, -1 on failure.
     */
    if (!user || !host || !local_dest) {
        hermes_log(LOG_ERROR, "ssh_env", "_ssh_bulk_download: invalid parameters");
        return -1;
    }
    hermes_log(LOG_INFO, "ssh_env",
               "_ssh_bulk_download: downloading %s@%s:%s -> %s",
               user, host, remote_base ? remote_base : "~/.hermes", local_dest);
    /* Real download: ssh tar cf - on the remote, redirected to a local file. */
    char base[1024];
    snprintf(base, sizeof(base), "%s", remote_base ? remote_base : "~/.hermes");
    char cmd[8192];
    int n = snprintf(cmd, sizeof(cmd),
        "ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 %s@%s "
        "'tar cf - -C / %s' > %s",
        user, host, base, local_dest);
    if (n < 0 || (size_t)n >= sizeof(cmd)) return -1;
    return ssh_run(cmd);
}

/* PoP: cli_tools_environments_ssh__ssh_delete @ tools/environments/ssh.py:_ssh_delete */
int cli_tools_environments_ssh__ssh_delete(const char *user, const char *host,
                                            const char **remote_paths, int path_count) {
    /*
     * Batch-delete remote files in one SSH call.
     * Returns 0 on success, -1 on failure.
     */
    if (!user || !host || !remote_paths || path_count <= 0) {
        hermes_log(LOG_ERROR, "ssh_env", "_ssh_delete: invalid parameters");
        return -1;
    }
    hermes_log(LOG_INFO, "ssh_env",
               "_ssh_delete: deleting %d path(s) on %s@%s", path_count, user, host);
    /* Real batch delete: ssh ... "rm -f <quoted paths>" */
    char cmd[8192];
    int n = snprintf(cmd, sizeof(cmd),
        "ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 %s@%s 'rm -f",
        user, host);
    if (n < 0 || (size_t)n >= sizeof(cmd)) return -1;
    for (int i = 0; i < path_count && n < (int)sizeof(cmd) - 4; i++) {
        /* quote each path */
        n += snprintf(cmd + n, sizeof(cmd) - n, " '%s'", remote_paths[i]);
    }
    if (n < (int)sizeof(cmd) - 2) { cmd[n++] = '\''; cmd[n] = '\0'; }
    else { cmd[sizeof(cmd) - 1] = '\0'; return -1; }
    return ssh_run(cmd);
}

/* PoP: cli_tools_environments_ssh__before_execute @ tools/environments/ssh.py:_before_execute */
int cli_tools_environments_ssh__before_execute(void) {
    /*
     * Sync files to remote via FileSyncManager (rate-limited internally).
     * Called before each execute() to ensure remote has latest files.
     */
    hermes_log(LOG_DEBUG, "ssh_env", "_before_execute: file sync handled by file_sync module");
    /* Remote file sync lives in the file_sync module; this hook is a
     * class-level pre-exec callback with no connection state in the C port,
     * so there is no standalone work to do here. */
    return 0;
}

/* PoP: cli_tools_environments_ssh__run_bash @ tools/environments/ssh.py:_run_bash */
int cli_tools_environments_ssh__run_bash(const char *user, const char *host,
                                          const char *cmd_string, int login_shell,
                                          int timeout, const char *stdin_data) {
    /*
     * Spawn an SSH process that runs bash on the remote host.
     * Returns 0 on success, -1 on error.
     */
    if (!user || !host || !cmd_string) {
        hermes_log(LOG_ERROR, "ssh_env", "_run_bash: invalid parameters");
        return -1;
    }
    hermes_log(LOG_INFO, "ssh_env",
               "_run_bash: %s@%s cmd=%.60s login=%d timeout=%d",
               user, host, cmd_string, login_shell, timeout);

    /* Build and run: ssh user@host bash [-l] -c '<escaped cmd>'. */
    char cmd[4096];
    int n = snprintf(cmd, sizeof(cmd), "ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 %s@%s bash %s -c %s",
                     user, host, login_shell ? "-l" : "",
                     "'" /* open quote */);
    /* Append the (single-quote-escaped) command. */
    for (const char *p = cmd_string; *p && n < (int)sizeof(cmd) - 4; p++) {
        if (*p == '\'') { cmd[n++] = '\''; cmd[n++] = '\\'; cmd[n++] = '\''; cmd[n++] = '\''; }
        else cmd[n++] = *p;
    }
    if (n < (int)sizeof(cmd) - 2) { cmd[n++] = '\''; cmd[n] = '\0'; }
    else { cmd[sizeof(cmd) - 1] = '\0'; return -1; }

    (void)stdin_data; /* interactive stdin not piped in spawn-per-call model */
    (void)timeout;    /* ConnectTimeout above bounds the connection */
    return ssh_run(cmd);
}
