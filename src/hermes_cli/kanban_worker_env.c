/*
 * kanban_worker_env.c — C home for upstream Hermes hermes_cli/kanban_worker_env.py.
 *
 * PoP: w16_kanbanwork_02049e_worker_client_env @ hermes_cli/kanban_worker_env.py:worker_client_env
 *
 * The module's whole logic is an allowlist: host plumbing + Hermes location names
 * (and the selected config backend's deployment names), plus the exact LC_ prefix.
 * The two origin-resident dependencies (strip_launch_profile_env, deployment_env_names)
 * are injected seams in the oracle; stripping is identity, deployment names are a
 * fixture-supplied string array.
 */
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w16_kanbanwork_02049e.h"

static const char *const HOST_NAMES[] = {
    "PATH", "HOME", "USER", "LOGNAME", "SHELL", "TERM", "TZ", "LANG", "LANGUAGE",
    "TMPDIR", "TMP", "TEMP", "PYTHONPATH", "PYTHONUTF8", "PYTHONIOENCODING",
    "VIRTUAL_ENV", "CONDA_PREFIX",
    "DBUS_SESSION_BUS_ADDRESS", "SUDO_USER",
    "SSH_AUTH_SOCK", "DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY",
    "SSL_CERT_FILE", "SSL_CERT_DIR", "REQUESTS_CA_BUNDLE", "CURL_CA_BUNDLE",
    "NODE_EXTRA_CA_CERTS",
    "HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "NO_PROXY",
    "SYSTEMROOT", "SYSTEMDRIVE", "WINDIR", "COMSPEC", "PATHEXT", "OS",
    "PROCESSOR_ARCHITECTURE", "NUMBER_OF_PROCESSORS", "PUBLIC", "ALLUSERSPROFILE",
    "PROGRAMDATA", "PROGRAMFILES", "PROGRAMFILES(X86)", "PROGRAMW6432", "APPDATA",
    "LOCALAPPDATA", "USERPROFILE", "USERDOMAIN", "USERNAME", "HOMEDRIVE", "HOMEPATH",
    "COMPUTERNAME", "PROCESSOR_ARCHITEW6432",
    "HERMES_HOME", "HERMES_REAL_HOME", "HERMES_SCRATCH_DIR", "HERMES_RUNTIME_DIR",
    "HERMES_INSTALL_ROOT", "HERMES_PYTHON_SRC_ROOT", "HERMES_DATA_DIR_SUFFIX",
    "HERMES_HOME_MODE", "HERMES_MANAGED", "HERMES_CONTAINER", "HERMES_SKIP_CHMOD",
    "HERMES_GATEWAY_LOCK_DIR", "HERMES_DISABLE_LAZY_INSTALLS", "HERMES_KANBAN_HOME",
    "HERMES_KANBAN_BUSY_TIMEOUT_MS", "HERMES_MANAGED_DIR",
    "XDG_RUNTIME_DIR", "XDG_STATE_HOME", "XDG_CONFIG_HOME", "XDG_CONFIG_DIRS",
    "XDG_DATA_HOME", "XDG_DATA_DIRS", "XDG_CACHE_HOME",
    "PREFIX", "TERMUX_VERSION",
};
#define HOST_NAMES_COUNT (sizeof(HOST_NAMES) / sizeof(HOST_NAMES[0]))

static char *dup_upper(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; ++i) out[i] = (char)toupper((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

json_t *w16_kanbanwork_02049e_worker_client_env(const json_t *env,
                                                const json_t *deployment_names) {
    json_t *out = json_object();
    if (!env || env->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < json_object_size(env); ++i) {
        const char *key = json_object_get_key_at(env, i);
        json_t *val = json_object_get_at(env, i);
        if (!key) continue;
        char *up = dup_upper(key);
        if (!up) continue;
        int keep = 0;
        for (size_t j = 0; j < HOST_NAMES_COUNT; ++j)
            if (!strcmp(up, HOST_NAMES[j])) { keep = 1; break; }
        if (!keep && deployment_names && deployment_names->type == JSON_ARRAY) {
            for (size_t j = 0; j < json_len(deployment_names); ++j) {
                json_t *d = json_get(deployment_names, j);
                if (!d || d->type != JSON_STRING || !d->str_val) continue;
                char *dup = dup_upper(d->str_val);
                if (dup && !strcmp(up, dup)) { free(dup); keep = 1; break; }
                free(dup);
            }
        }
        if (!keep && !strncmp(up, "LC_", 3)) keep = 1;
        free(up);
        if (keep) json_set(out, key, json_copy(val));
    }
    return out;
}
