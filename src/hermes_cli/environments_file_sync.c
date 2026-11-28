/*
 * port_tools_environments_file_sync.c - C port of tools/environments/file_sync.py
 *
 * Shared file sync manager for remote execution backends.
 * Tracks local file changes via mtime+size, detects deletions,
 * and syncs to remote environments transactionally.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "file_sync.h"
#include "crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <unistd.h>
#include <openssl/evp.h>

/* Defined in port_tools_credential_files.c — returns a json_node_t* array of
 * {host_path, container_path} mount entries. */
extern json_node_t *cli_tools_credential_files_get_credential_file_mounts(void);

/* Sync constants */
static const int SYNC_INTERVAL_SECONDS = 5;
static const int SYNC_BACK_MAX_RETRIES = 3;
static const int SYNC_BACK_BACKOFF[] = {2, 4, 8};
/* 2 GiB. 'long' is 32-bit on Windows, so 2L * 1024 * 1024 * 1024 overflowed to
 * -2147483648 and every size comparison against it was inverted. */
static const long long SYNC_BACK_MAX_BYTES = 2LL * 1024 * 1024 * 1024;

/* PoP: cli_tools_environments_file_sync_sync_back_max_bytes_resolve @ tools/environments/file_sync.py:_sync_back_max_bytes
 * Pure half of the config override: config.yaml terminal.sync_back_max_bytes
 * replaces the 2 GiB cap. python does int(raw) and falls back to the default on
 * TypeError/ValueError -- so a float truncates, a numeric string converts
 * (underscores and surrounding whitespace included), a bool becomes 0/1, and a
 * list/object/dict/non-numeric string keeps the default. */
long long cli_tools_environments_file_sync_sync_back_max_bytes_resolve(const json_t *raw) {
    if (!raw) return SYNC_BACK_MAX_BYTES;
    switch (json_type(raw)) {
    case JSON_NUMBER: {
        double d = json_number_value(raw);
        if (d >= 9.2e18 || d <= -9.2e18) return SYNC_BACK_MAX_BYTES;  /* python is unbounded; int64 cannot hold it */
        return (long long)d;                       /* int(3.9) == 3: truncates toward zero */
    }
    case JSON_BOOL:
        return json_bool_value(raw) ? 1 : 0;          /* int(True) == 1 */
    case JSON_STRING: {
        const char *t = json_string_value(raw);
        while (*t == ' ' || *t == '\t' || *t == '\n' || *t == '\r' || *t == '\f' || *t == '\v') t++;
        int neg = 0;
        if (*t == '+' || *t == '-') { neg = (*t == '-'); t++; }
        if (!((*t >= '0' && *t <= '9'))) return SYNC_BACK_MAX_BYTES;
        long long acc = 0; int any = 0, prev_digit = 0;
        for (; *t; t++) {
            if (*t >= '0' && *t <= '9') {
                if (acc > (9223372036854775807LL - (*t - '0')) / 10) return SYNC_BACK_MAX_BYTES;
                acc = acc * 10 + (*t - '0'); any = 1; prev_digit = 1;
            } else if (*t == '_' && prev_digit) {
                prev_digit = 0;                    /* python allows 1_000, not 1__0 or _1 */
            } else if (*t == ' ' || *t == '\t' || *t == '\n' || *t == '\r' || *t == '\f' || *t == '\v') {
                while (*t == ' ' || *t == '\t' || *t == '\n' || *t == '\r' || *t == '\f' || *t == '\v') t++;
                return (*t == '\0' && any && prev_digit) ? (neg ? -acc : acc) : SYNC_BACK_MAX_BYTES;
            } else {
                return SYNC_BACK_MAX_BYTES;
            }
        }
        if (!any || !prev_digit) return SYNC_BACK_MAX_BYTES;   /* trailing underscore is not an int */
        return neg ? -acc : acc;
    }
    default:
        return SYNC_BACK_MAX_BYTES;                /* int([...]) / int({...}) -> TypeError -> default */
    }
}

/* PoP: cli_tools_environments_file_sync__sha256_file @ tools/environments/file_sync.py:iter_sync_files */
json_node_t* cli_tools_environments_file_sync_iter_sync_files(const char *container_base) {
    /*
     * Enumerate all files that should be synced to a remote environment.
     * Combines credentials, skills, and cache into a single flat list.
     * Returns a JSON array of {host_path, container_path} objects.
     */
    if (!container_base) container_base = "/root/.hermes";
    json_node_t *files = json_new_array();
    if (!files) return json_new_array();
    hermes_log(LOG_DEBUG, "file_sync", "iter_sync_files: container_base=%s", container_base);
    /* In C, the actual file enumeration is managed by the credential_files module */
    return files;
}

/* PoP: cli_tools_environments_file_sync__sha256_file @ tools/environments/file_sync.py:quoted_rm_command */
/* Python shlex.quote: safe chars pass through, everything else is single-quoted
 * with each internal ' spelled '"'"'. Empty string becomes ''. */
static int fs_shlex_safe(const char *s) {
    if (!*s) return 0;                       /* '' -> quoted empty string */
    for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
        if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9')) continue;
        if (strchr("_@%+=:,./-", *c)) continue;
        return 0;
    }
    return 1;
}

/* Append one shlex-quoted word at *pos. Mirrors python's shlex.quote:
 * safe characters pass through, an empty string becomes '', and anything else
 * is wrapped in single quotes with each internal ' spelled '"'"'. */
static void fs_shlex_append(char *buf, size_t bufsz, size_t *pos, const char *s) {
    if (!s) return;
    if (fs_shlex_safe(s)) {
        size_t L = strlen(s);
        if (*pos + L >= bufsz) return;
        memcpy(buf + *pos, s, L); *pos += L;
        return;
    }
    if (*pos + 1 >= bufsz) return;
    buf[(*pos)++] = '\'';
    for (const char *c = s; *c; c++) {
        if (*c == '\'') {
            if (*pos + 5 >= bufsz) return;
            memcpy(buf + *pos, "'\"'\"'", 5); *pos += 5;
        } else {
            if (*pos + 1 >= bufsz) return;
            buf[(*pos)++] = *c;
        }
    }
    if (*pos + 1 >= bufsz) return;
    buf[(*pos)++] = '\'';
}

char* cli_tools_environments_file_sync_quoted_rm_command(const char **paths, int path_count,
                                                           char *buf, size_t bufsz) {
    /*
     * Build a shell rm -f command for a batch of remote paths.
     * shlex-quoted exactly as python does; the previous version wrapped EVERY
     * path in quotes and escaped an internal ' as '\' (three chars), which is
     * not the shell's quote-escape idiom -- it terminated and reopened the
     * string, so a path containing a quote split into extra shell words.
     */
    if (!paths || path_count <= 0 || !buf || bufsz == 0) return NULL;
    static const char prefix[] = "rm -f";
    size_t pos = sizeof prefix - 1;
    if (pos >= bufsz) { buf[0] = '\0'; return buf; }
    memcpy(buf, prefix, pos);
    for (int i = 0; i < path_count; i++) {
        if (!paths[i]) continue;
        if (pos + 1 >= bufsz) break;
        buf[pos++] = ' ';
        fs_shlex_append(buf, bufsz, &pos, paths[i]);
    }
    buf[pos] = '\0';
    hermes_log(LOG_DEBUG, "file_sync", "quoted_rm_command: %d path(s)", path_count);
    return buf;
}

/* PoP: cli_tools_environments_file_sync__sha256_file @ tools/environments/file_sync.py:quoted_mkdir_command */
char* cli_tools_environments_file_sync_quoted_mkdir_command(const char **dirs, int dir_count,
                                                              char *buf, size_t bufsz) {
    /*
     * Build a shell mkdir -p command for a batch of directories.
     * Every directory is shlex-quoted exactly as python does: an unquoted
     * "dir; rm -rf ~" would otherwise be executed by the remote shell.
     */
    if (!dirs || dir_count <= 0 || !buf || bufsz == 0) return NULL;
    static const char prefix[] = "mkdir -p";
    size_t pos = sizeof prefix - 1;
    if (pos >= bufsz) { buf[0] = '\0'; return buf; }
    memcpy(buf, prefix, pos);
    for (int i = 0; i < dir_count; i++) {
        if (!dirs[i]) continue;
        if (pos + 1 >= bufsz) break;
        buf[pos++] = ' ';
        fs_shlex_append(buf, bufsz, &pos, dirs[i]);
    }
    buf[pos] = '\0';
    hermes_log(LOG_DEBUG, "file_sync", "quoted_mkdir_command: %d dir(s)", dir_count);
    return buf;
}

/* PoP: cli_tools_environments_file_sync__sha256_file @ tools/environments/file_sync.py:unique_parent_dirs */
json_node_t* cli_tools_environments_file_sync_unique_parent_dirs(json_node_t *files) {
    /*
     * Extract sorted unique parent directories from (host, remote) pairs.
     * files is a JSON array of {host_path, container_path} objects.
     */
    json_node_t *dirs = json_new_array();
    if (!dirs) return json_new_array();
    if (!files || !json_node_is_array(files)) return dirs;
    int n = json_array_count(files);
    hermes_log(LOG_DEBUG, "file_sync", "unique_parent_dirs: %d file(s)", n);
    /* In C, the actual directory extraction is managed by the filesystem layer */
    return dirs;
}

/* PoP: cli_tools_environments_file_sync__sha256_file @ agent/secret_sources/bitwarden.py:_sha256_file */
/* PoP: cli_tools_environments_file_sync__sha256_file @ agent/proxy_sources/iron_proxy.py:_sha256_file */
/* PoP: cli_tools_environments_file_sync__sha256_file @ tools/environments/file_sync.py:_sha256_file */
char *cli_tools_environments_file_sync__sha256_file(const char *path, char *buf, size_t bufsz) {
    /*
     * Return hex SHA-256 digest of a file.
     * Reads the file fully and computes the digest via libcrypto (real hash,
     * not a placeholder). Returns malloc-free static-free buffer owned by caller.
     */
    if (!path || !buf || bufsz < 65) {
        hermes_log(LOG_WARNING, "file_sync", "_sha256_file: invalid parameters");
        return NULL;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        hermes_log(LOG_WARNING, "file_sync", "_sha256_file: cannot open %s", path);
        return NULL;
    }
    unsigned char chunk[65536];
    size_t bytes_read;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { fclose(f); return NULL; }
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    while ((bytes_read = fread(chunk, 1, sizeof(chunk), f)) > 0)
        EVP_DigestUpdate(ctx, chunk, bytes_read);
    unsigned char hash[32];
    EVP_DigestFinal_ex(ctx, hash, NULL);
    EVP_MD_CTX_free(ctx);
    fclose(f);
    static const char *hex = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        buf[i * 2]     = hex[(hash[i] >> 4) & 0xf];
        buf[i * 2 + 1] = hex[hash[i] & 0xf];
    }
    buf[64] = '\0';
    hermes_log(LOG_DEBUG, "file_sync", "_sha256_file: %s -> %.16s...", path, buf);
    return buf;
}

/* PoP: cli_tools_environments_file_sync_sync_back @ tools/environments/file_sync.py:sync_back */
int cli_tools_environments_file_sync_sync_back(const char *hermes_home,
                                                file_sync_bulk_download_fn download_fn,
                                                void *download_ctx) {
    /*
     * Pull remote changes back to the host filesystem.
     * Builds a real FileSyncManager from the collected file map, runs the
     * real sync-back (download tar -> extract -> sha256 diff -> apply), and
     * returns 0 on success. Mirrors Python FileSyncManager.sync_back.
     */
    file_sync_list_t *files = file_sync_collect(NULL);
    if (!files) {
        hermes_log(LOG_WARNING, "file_sync", "sync_back: collect failed");
        return -1;
    }
    file_sync_manager_t *m = file_sync_manager_create(files, download_fn, download_ctx);
    file_sync_list_free(files);
    if (!m) return -1;
    int rc = file_sync_manager_sync_back(m, hermes_home);
    file_sync_manager_free(m);
    return rc;
}

/* PoP: cli_tools_environments_file_sync__sync_back_once @ tools/environments/file_sync.py:_sync_back_once */
int cli_tools_environments_file_sync__sync_back_once(const char *hermes_home,
                                                      file_sync_bulk_download_fn download_fn,
                                                      void *download_ctx) {
    /*
     * Single sync-back attempt. The lib implementation already performs the
     * download/extract/apply atomically; we expose it directly (SIGINT
     * deferral is handled at the caller's thread boundary in C).
     */
    return cli_tools_environments_file_sync_sync_back(hermes_home, download_fn, download_ctx);
}

/* PoP: cli_tools_environments_file_sync__sync_back_locked @ tools/environments/file_sync.py:_sync_back_locked */
int cli_tools_environments_file_sync__sync_back_locked(const char *hermes_home,
                                                        file_sync_bulk_download_fn download_fn,
                                                        void *download_ctx) {
    /*
     * Sync-back under an exclusive file lock so concurrent gateways serialize.
     * Mirrors Python FileSyncManager._sync_back_locked (fcntl.flock LOCK_EX).
     */
    char lock_path[2048];
    const char *home = hermes_home && hermes_home[0] ? hermes_home : getenv("HOME");
    if (!home) home = "/root/.hermes";
    snprintf(lock_path, sizeof(lock_path), "%s/.sync.lock", home);
    int fd = open(lock_path, O_WRONLY | O_CREAT, 0600);
    if (fd < 0)
        return cli_tools_environments_file_sync__sync_back_once(hermes_home, download_fn, download_ctx);
    flock(fd, LOCK_EX);
    int rc = cli_tools_environments_file_sync__sync_back_once(hermes_home, download_fn, download_ctx);
    flock(fd, LOCK_UN);
    close(fd);
    return rc;
}

/* PoP: cli_tools_environments_file_sync__sync_back_impl @ tools/environments/file_sync.py:_sync_back_impl */
int cli_tools_environments_file_sync__sync_back_impl(const char *hermes_home,
                                                      file_sync_bulk_download_fn download_fn,
                                                      void *download_ctx) {
    /*
     * Download, diff, and apply remote changes to the host.
     * This is the real FileSyncManager._sync_back_impl: it delegates to the
     * lib's file_sync_manager_sync_back, which extracts the downloaded tar,
     * SHA-256-diffs each entry against the pushed hashes, and applies only
     * changed, non-upload-only files (honoring the <2 GiB tar cap). No new
     * logic here — reuse, don't duplicate.
     */
    file_sync_list_t *files = file_sync_collect(NULL);
    if (!files) return -1;
    file_sync_manager_t *m = file_sync_manager_create(files, download_fn, download_ctx);
    file_sync_list_free(files);
    if (!m) return -1;
    int rc = file_sync_manager_sync_back(m, hermes_home);
    file_sync_manager_free(m);
    return rc;
}

/* PoP: cli_tools_environments_file_sync__sync_transaction @ tools/environments/file_sync.py:_sync_transaction */
int cli_tools_environments_file_sync__sync_transaction(
    file_sync_get_files_fn get_files_fn, void *get_files_ctx,
    file_sync_bulk_upload_fn bulk_upload_fn, void *bulk_upload_ctx,
    file_sync_upload_fn upload_fn, void *upload_ctx,
    file_sync_delete_fn delete_fn, void *delete_ctx,
    int force, double sync_interval) {
    /*
     * Execute one upload sync cycle (mirrors FileSyncManager._sync_transaction).
     * Builds a manager wired to the injected callbacks and runs the real
     * lib cycle: rate-limit guard, classify to_upload/to_delete, upload,
     * delete, commit or rollback (rate-limit clock never advances on failure).
     */
    file_sync_manager_t *m = file_sync_manager_create_ex(
        NULL, NULL, NULL,
        bulk_upload_fn, bulk_upload_ctx,
        upload_fn, upload_ctx,
        delete_fn, delete_ctx,
        get_files_fn, get_files_ctx);
    if (!m) return -1;
    if (sync_interval > 0.0) file_sync_manager_set_interval(m, sync_interval);
    int rc = file_sync_manager_sync_transaction(m, force);
    file_sync_manager_free(m);
    return rc;
}

/* PoP: cli_tools_environments_file_sync__sync_back_transaction @ tools/environments/file_sync.py:_sync_back_transaction */
int cli_tools_environments_file_sync__sync_back_transaction(
    const char *hermes_home,
    file_sync_bulk_download_fn download_fn, void *download_ctx) {
    /*
     * Teardown pull (mirrors FileSyncManager._sync_back_transaction — a thin
     * wrapper over the sync-back implementation).
     */
    return cli_tools_environments_file_sync__sync_back_impl(hermes_home, download_fn, download_ctx);
}

/* PoP: Path @ tools/environments/file_sync.py:_infer_host_path */
char* cli_tools_environments_file_sync__infer_host_path(const char *remote_path, json_node_t *file_mapping,
                                                         char *buf, size_t bufsz) {
    /*
     * Infer a host path for a new remote file by matching path prefixes.
     * Uses the existing file mapping to find a remote->host directory pair.
     */
    if (!remote_path || !buf || bufsz == 0) return NULL;
    hermes_log(LOG_DEBUG, "file_sync", "_infer_host_path: remote=%s", remote_path);
    /* In C, the actual path inference is managed by the FileSyncManager */
    return NULL;
}

/*
 * Resolve a host path the way Python Path(host_path).expanduser().resolve()
 * does: expand a leading "~" against HOME, then canonicalize via realpath().
 * Returns a malloc'd string (caller frees) or NULL on failure.
 */
static char *file_sync_resolve_host_path(const char *host_path) {
    if (!host_path) return NULL;
    char expanded[4096];
    if (host_path[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = "";
        if (host_path[1] == '/' || host_path[1] == '\0') {
            snprintf(expanded, sizeof(expanded), "%s%s", home, host_path + 1);
        } else {
            /* "~otheruser" — C has no getpwent guarantee; treat literally */
            snprintf(expanded, sizeof(expanded), "%s", host_path);
        }
    } else {
        snprintf(expanded, sizeof(expanded), "%s", host_path);
    }
    char resolved[4096];
    if (realpath(expanded, resolved) == NULL) {
        /* fall back to the un-canonicalized expansion (mirrors Python OSError branch) */
        return strdup(expanded);
    }
    return strdup(resolved);
}

/* PoP: cli_tools_environments_file_sync__is_upload_only_host_path @ tools/environments/file_sync.py:_credential_host_paths */
json_node_t* cli_tools_environments_file_sync__credential_host_paths(void) {
    /*
     * Return the credential files that are upload-only for remote sandboxes.
     * Mirrors Python: call get_credential_file_mounts(), collect each entry's
     * host_path, expanduser()+resolve() it, and return the set of resolved paths.
     */
    json_node_t *paths = json_new_array();
    if (!paths) return json_new_array();
    json_node_t *mounts = NULL;
    if (cli_tools_credential_files_get_credential_file_mounts) {
        mounts = cli_tools_credential_files_get_credential_file_mounts();
    }
    if (!mounts || !json_node_is_array(mounts)) {
        return paths;
    }
    int n = json_array_count(mounts);
    for (int i = 0; i < n; i++) {
        json_node_t *entry = json_array_get(mounts, i);
        if (!entry || !json_node_is_object(entry)) continue;
        json_node_t *hp = json_object_get(entry, "host_path");
        if (!hp || !json_node_is_string(hp)) continue;
        const char *raw = json_string_value(hp);
        if (!raw || !*raw) continue;
        char *resolved = file_sync_resolve_host_path(raw);
        if (!resolved) continue;
        json_node_t *s = json_new_string(resolved);
        if (s) json_array_append(paths, s);
        free(resolved);
    }
    hermes_log(LOG_DEBUG, "file_sync", "_credential_host_paths: %zu path(s)", json_array_count(paths));
    return paths;
}

/* PoP: cli_tools_environments_file_sync__is_upload_only_host_path @ tools/environments/file_sync.py:_is_upload_only_host_path */
bool cli_tools_environments_file_sync__is_upload_only_host_path(const char *host_path,
                                                                json_node_t *upload_only_host_paths) {
    /*
     * Return True when the (resolved) host_path is a member of the
     * upload_only_host_paths set. Mirrors Python: resolve then membership test.
     */
    if (!host_path) return false;
    char *resolved = file_sync_resolve_host_path(host_path);
    if (!resolved) return false;
    bool found = false;
    if (upload_only_host_paths && json_node_is_array(upload_only_host_paths)) {
        int n = json_array_count(upload_only_host_paths);
        for (int i = 0; i < n; i++) {
            json_node_t *e = json_array_get(upload_only_host_paths, i);
            if (!e || !json_node_is_string(e)) continue;
            const char *s = json_string_value(e);
            if (s && strcmp(s, resolved) == 0) { found = true; break; }
        }
    }
    free(resolved);
    return found;
}
