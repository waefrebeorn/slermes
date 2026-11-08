/*
 * terminal_cmd.c — terminal command helpers.
 *
 * Split from terminal.c (soul.md: self-contained modules). SSH sync and
 * command-classification helpers; no file-scope state.
 */
#include "terminal_cmd.h"
#include "hermes_json.h"
#include "file_sync.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ssh_sync_upload(const char *ssh_host, const char *ssh_user,
                            int ssh_port, const char *ssh_key,
                            const char *remote_home) {
    if (!ssh_host || !ssh_host[0]) return -1;

    /* Collect files to sync */
    file_sync_list_t *files = file_sync_collect(remote_home);
    if (!files || files->count == 0) {
        if (files) file_sync_list_free(files);
        return 0; /* nothing to sync */
    }

    /* Build scp command prefix */
    char scp_prefix[4096];
    int pos = 0;
    pos += snprintf(scp_prefix + pos, sizeof(scp_prefix) - pos,
                    "scp -o ControlMaster=auto -o ControlPersist=300"
                    " -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10"
                    " -o BatchMode=yes");
    if (ssh_port != 22)
        pos += snprintf(scp_prefix + pos, sizeof(scp_prefix) - pos, " -P %d", ssh_port);
    if (ssh_key && ssh_key[0])
        pos += snprintf(scp_prefix + pos, sizeof(scp_prefix) - pos, " -i \"%s\"", ssh_key);

    /* Create remote dirs first */
    char *mkdir_cmd = file_sync_mkdir_cmd(files);
    if (mkdir_cmd && mkdir_cmd[0]) {
        char remote_mkdir[8192];
        snprintf(remote_mkdir, sizeof(remote_mkdir),
                 "ssh %s %s@%s \"%s\" 2>/dev/null",
                 scp_prefix, ssh_user ? ssh_user : "", ssh_host, mkdir_cmd);
        run_command(remote_mkdir, 30);
    }
    free(mkdir_cmd);

    /* Upload each file via scp */
    for (int i = 0; i < files->count; i++) {
        char scp_cmd[16384];
        snprintf(scp_cmd, sizeof(scp_cmd),
                 "%s \"%s\" %s@%s:\"%s\" 2>/dev/null",
                 scp_prefix,
                 files->entries[i].host_path,
                 ssh_user ? ssh_user : "", ssh_host,
                 files->entries[i].remote_path);
        char *result = run_command(scp_cmd, 60);
        if (result) free(result);
    }

    file_sync_list_free(files);
    return 0;
}

int ssh_sync_download(const char *ssh_host, const char *ssh_user,
                              int ssh_port, const char *ssh_key,
                              const char *remote_home) {
    if (!ssh_host || !ssh_host[0]) return -1;

    const char *local_home = getenv("HOME");
    if (!local_home) local_home = "/root";

    /* Build ssh command prefix (same as upload) */
    char ssh_prefix[4096];
    int pos = 0;
    pos += snprintf(ssh_prefix + pos, sizeof(ssh_prefix) - pos,
                    "ssh -o ControlMaster=auto -o ControlPersist=300"
                    " -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10"
                    " -o BatchMode=yes");
    if (ssh_port != 22)
        pos += snprintf(ssh_prefix + pos, sizeof(ssh_prefix) - pos, " -p %d", ssh_port);
    if (ssh_key && ssh_key[0])
        pos += snprintf(ssh_prefix + pos, sizeof(ssh_prefix) - pos, " -i \"%s\"", ssh_key);

    /* Build the download command:
     * ssh ... "tar czf - -C <remote_home> .hermes 2>/dev/null" | tar xzf - -C <local_home>
     * Uses tar over ssh pipe for efficient transfer. */
    char download_cmd[16384];
    snprintf(download_cmd, sizeof(download_cmd),
             "%s %s@%s \"tar czf - -C '%s' .hermes 2>/dev/null\" 2>/dev/null"
             " | tar xzf - -C '%s' 2>/dev/null",
             ssh_prefix,
             ssh_user ? ssh_user : "", ssh_host,
             remote_home ? remote_home : "~",
             local_home);

    char *result = run_command(download_cmd, 60);
    if (result) free(result);
    return 0;
}

void _inject_sudo_failure(json_t *rj, const char *command) {
    (void)command;
    if (!rj) return;
    const char *output = json_get_str(rj, "output", NULL);
    if (!output) return;

    static const char *sudo_failures[] = {
        "sudo: a password is required",
        "sudo: no tty present",
        "sudo: a terminal is required",
        NULL
    };
    for (int i = 0; sudo_failures[i]; i++) {
        if (strstr(output, sudo_failures[i])) {
            json_set(rj, "sudo_tip",
                json_string("Tip: To enable sudo over messaging, add SUDO_PASSWORD to your .env file"));
            return;
        }
    }
}

bool looks_like_env_assignment(const char *token) {
    if (!token || !token[0]) return false;
    const char *eq = strchr(token, '=');
    if (!eq || eq == token) return false;
    /* Check that chars before '=' are valid env var name */
    for (const char *p = token; p < eq; p++) {
        if (p == token) {
            if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_'))
                return false;
        } else {
            if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                  (*p >= '0' && *p <= '9') || *p == '_'))
                return false;
        }
    }
    return true;
}
