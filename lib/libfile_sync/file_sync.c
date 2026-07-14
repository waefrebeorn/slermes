/*
 * file_sync.c — Environment file sync for remote execution backends
 *
 * Port of Python tools/environments/file_sync.py.
 * Core functions: collect files, build commands, transactional upload, and
 * a real sync_back (pull a remote .hermes/ tar, diff against pushed SHA-256
 * hashes, apply only changed, non-upload-only files).
 *
 * sync_back reuses the same shell `tar` transport the backends already use
 * (see port_tools_environments_ssh.c), and SHA-256 via libcrypto.
 */

#include "file_sync.h"
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <openssl/evp.h>

/* ── Internal helpers ────────────────────────────────────────────── */

static pthread_once_t g_home_once = PTHREAD_ONCE_INIT;
static char g_hermes_home[1024] = "";

static void default_home(void) {
    const char *home = getenv("HERMES_HOME");
    if (home && home[0])
        snprintf(g_hermes_home, sizeof(g_hermes_home), "%s", home);
    else {
        const char *user = getenv("HOME");
        if (user)
            snprintf(g_hermes_home, sizeof(g_hermes_home), "%s/.hermes", user);
        else
            snprintf(g_hermes_home, sizeof(g_hermes_home), "/root/.hermes");
    }
}

void file_sync_set_home(const char *home) {
    if (home)
        snprintf(g_hermes_home, sizeof(g_hermes_home), "%s", home);
}

static const char *get_home(void) {
    pthread_once(&g_home_once, default_home);
    return g_hermes_home;
}

/* ── File list management ────────────────────────────────────────── */

static bool list_add(file_sync_list_t *list, const char *host, const char *remote) {
    if (list->count >= list->capacity) {
        int new_cap = list->capacity ? list->capacity * 2 : 64;
        file_sync_entry_t *tmp = realloc(list->entries, new_cap * sizeof(file_sync_entry_t));
        if (!tmp) return false;
        list->entries = tmp;
        list->capacity = new_cap;
    }
    snprintf(list->entries[list->count].host_path, sizeof(list->entries[list->count].host_path), "%s", host);
    snprintf(list->entries[list->count].remote_path, sizeof(list->entries[list->count].remote_path), "%s", remote);
    list->count++;
    return true;
}

/* ── Recursive directory enumeration ─────────────────────────────── */

static bool collect_dir(file_sync_list_t *list, const char *host_dir,
                        const char *remote_base, const char *host_base) {
    DIR *d = opendir(host_dir);
    if (!d) return false;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char host_path[1024];
        snprintf(host_path, sizeof(host_path), "%s/%s", host_dir, entry->d_name);

        struct stat st;
        if (stat(host_path, &st) != 0) continue;

        if (S_ISREG(st.st_mode)) {
            char remote_path[1024];
            size_t base_len = strlen(host_base);
            if (strncmp(host_path, host_base, base_len) == 0) {
                snprintf(remote_path, sizeof(remote_path), "%s%s",
                         remote_base, host_path + base_len);
            } else {
                snprintf(remote_path, sizeof(remote_path), "%s", host_path);
            }
            list_add(list, host_path, remote_path);
        } else if (S_ISDIR(st.st_mode)) {
            collect_dir(list, host_path, remote_base, host_base);
        }
    }
    closedir(d);
    return true;
}

/* ── Collect credential files ────────────────────────────────────── */

static void collect_credentials(file_sync_list_t *list, const char *hermes_home,
                                const char *container_base) {
    const char *cred_dirs[] = {
        "credentials",
        "data/skills",
        NULL
    };

    char cred_path[1024];
    for (int i = 0; cred_dirs[i]; i++) {
        (void)snprintf(cred_path, sizeof(cred_path), "%s/%s", hermes_home, cred_dirs[i]);
        struct stat st;
        if (stat(cred_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            collect_dir(list, cred_path, container_base, hermes_home);
        }
    }

    const char *top_files[] = {
        "config.yaml",
        ".env",
        NULL
    };
    for (int i = 0; top_files[i]; i++) {
        char host_path[1024], remote_path[1024];
        snprintf(host_path, sizeof(host_path), "%s/%s", hermes_home, top_files[i]);
        struct stat st;
        if (stat(host_path, &st) == 0 && S_ISREG(st.st_mode)) {
            snprintf(remote_path, sizeof(remote_path), "%s/%s",
                     container_base, top_files[i]);
            list_add(list, host_path, remote_path);
        }
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

file_sync_list_t *file_sync_collect(const char *container_base) {
    file_sync_list_t *list = calloc(1, sizeof(file_sync_list_t));
    if (!list) return NULL;

    const char *home = get_home();
    const char *cb = container_base && container_base[0] ? container_base : "/root/.hermes";

    collect_credentials(list, home, cb);

    char skills_path[1024];
    snprintf(skills_path, sizeof(skills_path), "%s/skills", home);
    struct stat st;
    if (stat(skills_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        collect_dir(list, skills_path, cb, home);
    }

    return list;
}

void file_sync_list_free(file_sync_list_t *list) {
    if (!list) return;
    free(list->entries);
    free(list);
}

char *file_sync_mkdir_cmd(file_sync_list_t *files) {
    if (!files || files->count == 0) return strdup("");

    char **dirs = NULL;
    int dir_count = 0, dir_cap = 0;

    for (int i = 0; i < files->count; i++) {
        const char *slash = strrchr(files->entries[i].remote_path, '/');
        if (!slash) continue;

        char parent[1024];
        size_t len = slash - files->entries[i].remote_path;
        if (len >= sizeof(parent)) len = sizeof(parent) - 1;
        memcpy(parent, files->entries[i].remote_path, len);
        parent[len] = '\0';

        int found = 0;
        for (int j = 0; j < dir_count; j++) {
            if (strcmp(dirs[j], parent) == 0) { found = 1; break; }
        }
        if (found) continue;

        if (dir_count >= dir_cap) {
            int new_cap = dir_cap ? dir_cap * 2 : 16;
            char **tmp = realloc(dirs, new_cap * sizeof(char *));
            if (!tmp) { break; }
            dirs = tmp;
            dir_cap = new_cap;
        }
        dirs[dir_count] = strdup(parent);
        if (dirs[dir_count]) dir_count++;
    }

    size_t total = 10; /* "mkdir -p " */
    for (int i = 0; i < dir_count; i++)
        total += strlen(dirs[i]) + 2;

    char *cmd = malloc(total + 10);
    if (!cmd) {
        for (int i = 0; i < dir_count; i++) free(dirs[i]);
        free(dirs);
        return NULL;
    }

    size_t pos = 0;
    pos += snprintf(cmd + pos, total - pos, "mkdir -p");
    for (int i = 0; i < dir_count; i++) {
        pos += snprintf(cmd + pos, total - pos, " '%s'", dirs[i]);
        free(dirs[i]);
    }
    free(dirs);
    return cmd;
}

bool file_sync_upload_all(file_sync_list_t *files,
                           file_sync_upload_fn upload, void *upload_ctx) {
    if (!files || !upload) return false;

    for (int i = 0; i < files->count; i++) {
        if (!upload(files->entries[i].host_path,
                     files->entries[i].remote_path,
                     upload_ctx))
            return false;
    }
    return true;
}

/* ── SHA-256 helpers ─────────────────────────────────────────────── */

/* Streaming SHA-256 of a file. Returns true and writes 32 hex chars +
 * NUL into out_hex (>= 65 bytes). out_hex may be NULL to skip. */
static bool file_sha256_hex(const char *path, char *out_hex) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { fclose(f); return false; }
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    unsigned char chunk[65536];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0)
        EVP_DigestUpdate(ctx, chunk, n);
    unsigned char raw[32];
    EVP_DigestFinal_ex(ctx, raw, NULL);
    EVP_MD_CTX_free(ctx);
    fclose(f);
    if (out_hex) {
        static const char *hex = "0123456789abcdef";
        for (int i = 0; i < 32; i++) {
            out_hex[i * 2]     = hex[(raw[i] >> 4) & 0xf];
            out_hex[i * 2 + 1] = hex[raw[i] & 0xf];
        }
        out_hex[64] = '\0';
    }
    return true;
}

/* mkdir -p (no shell) for a single path. Declared before apply_staged uses it. */
static void mkdir_p(const char *path);

/* Resolve a host path like Python Path(...).expanduser().resolve(): expand a
 * leading "~" and canonicalize via realpath(). Returns malloc'd string or
 * NULL. */
static char *resolve_host_path(const char *host_path) {
    if (!host_path) return NULL;
    char expanded[FILE_SYNC_PATH_MAX];
    if (host_path[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = "";
        if (host_path[1] == '/' || host_path[1] == '\0')
            snprintf(expanded, sizeof(expanded), "%s%s", home, host_path + 1);
        else
            snprintf(expanded, sizeof(expanded), "%s", host_path);
    } else {
        snprintf(expanded, sizeof(expanded), "%s", host_path);
    }
    char resolved[FILE_SYNC_PATH_MAX];
    if (realpath(expanded, resolved) == NULL)
        return strdup(expanded);
    return strdup(resolved);
}

/* ── Manager (mirrors Python FileSyncManager) ────────────────────── */

typedef struct {
    char remote_path[FILE_SYNC_PATH_MAX];
    char host_path[FILE_SYNC_PATH_MAX];
    char pushed_hash[65];   /* SHA-256 hex, or "" if not yet pushed */
} file_sync_map_entry_t;

typedef struct {
    char *resolved;         /* malloc'd resolved host path */
} file_sync_upload_only_t;

struct file_sync_manager {
    file_sync_map_entry_t *map;
    int map_count;
    int map_cap;
    file_sync_upload_only_t *upload_only;
    int uo_count;
    int uo_cap;
    file_sync_bulk_download_fn download_fn;
    void *download_ctx;
};

static bool manager_add_map(file_sync_manager_t *m, const char *remote,
                            const char *host) {
    if (m->map_count >= m->map_cap) {
        int nc = m->map_cap ? m->map_cap * 2 : 64;
        file_sync_map_entry_t *t = realloc(m->map, nc * sizeof(*t));
        if (!t) return false;
        m->map = t;
        m->map_cap = nc;
    }
    file_sync_map_entry_t *e = &m->map[m->map_count++];
    snprintf(e->remote_path, sizeof(e->remote_path), "%s", remote);
    snprintf(e->host_path, sizeof(e->host_path), "%s", host);
    e->pushed_hash[0] = '\0';
    return true;
}

static file_sync_map_entry_t *manager_find_map(file_sync_manager_t *m,
                                                const char *remote) {
    for (int i = 0; i < m->map_count; i++)
        if (strcmp(m->map[i].remote_path, remote) == 0)
            return &m->map[i];
    return NULL;
}

file_sync_manager_t *file_sync_manager_create(
    const file_sync_list_t *files,
    file_sync_bulk_download_fn download_fn, void *download_ctx) {
    file_sync_manager_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->download_fn = download_fn;
    m->download_ctx = download_ctx;
    if (files) {
        for (int i = 0; i < files->count; i++)
            manager_add_map(m, files->entries[i].remote_path,
                            files->entries[i].host_path);
    }
    return m;
}

void file_sync_manager_free(file_sync_manager_t *m) {
    if (!m) return;
    free(m->map);
    for (int i = 0; i < m->uo_count; i++) free(m->upload_only[i].resolved);
    free(m->upload_only);
    free(m);
}

void file_sync_manager_mark_pushed(file_sync_manager_t *m,
                                    const char *remote_path,
                                    const char *host_path) {
    if (!m || !remote_path || !host_path) return;
    file_sync_map_entry_t *e = manager_find_map(m, remote_path);
    if (!e) {
        if (!manager_add_map(m, remote_path, host_path)) return;
        e = manager_find_map(m, remote_path);
        if (!e) return;
    }
    file_sha256_hex(host_path, e->pushed_hash);
}

void file_sync_manager_add_upload_only(file_sync_manager_t *m,
                                        const char *host_path) {
    if (!m || !host_path) return;
    char *resolved = resolve_host_path(host_path);
    if (!resolved) return;
    if (m->uo_count >= m->uo_cap) {
        int nc = m->uo_cap ? m->uo_cap * 2 : 16;
        file_sync_upload_only_t *t = realloc(m->upload_only, nc * sizeof(*t));
        if (!t) { free(resolved); return; }
        m->upload_only = t;
        m->uo_cap = nc;
    }
    m->upload_only[m->uo_count].resolved = resolved;
    m->uo_count++;
}

static bool is_upload_only(file_sync_manager_t *m, const char *host_path) {
    char *resolved = resolve_host_path(host_path);
    if (!resolved) return false;
    bool found = false;
    for (int i = 0; i < m->uo_count; i++) {
        if (strcmp(m->upload_only[i].resolved, resolved) == 0) { found = true; break; }
    }
    free(resolved);
    return found;
}

/* Recursive walk over staging; applies changed files. Returns count applied. */
static int apply_staged(file_sync_manager_t *m, const char *staging) {
    int applied = 0;
    /* Use nftw-free recursion via opendir. */
    DIR *d = opendir(staging);
    if (!d) return 0;
    struct dirent *e;
    /* Two-pass: files first, then descend. Simple approach: snapshot. */
    char subdirs[512][FILE_SYNC_PATH_MAX];
    int nsub = 0;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        char full[FILE_SYNC_PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", staging, e->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (nsub < 512) snprintf(subdirs[nsub++], sizeof(subdirs[0]), "%s", full);
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;

        char rel[FILE_SYNC_PATH_MAX];
        snprintf(rel, sizeof(rel), "%s", full + strlen(staging) + 1);
        char remote_path[FILE_SYNC_PATH_MAX];
        snprintf(remote_path, sizeof(remote_path), "/%s", rel);

        file_sync_map_entry_t *me = manager_find_map(m, remote_path);
        char remote_hash[65];
        bool have_remote = file_sha256_hex(full, remote_hash);

        if (me && me->pushed_hash[0]) {
            if (have_remote && strcmp(remote_hash, me->pushed_hash) == 0)
                continue; /* unchanged since push */
        }

        const char *host_path = me ? me->host_path : NULL;
        if (!host_path) {
            /* Infer host path by prefix substitution from an existing map
             * entry sharing the remote parent directory. */
            char remote_dir[FILE_SYNC_PATH_MAX];
            char *slash = strrchr(remote_path, '/');
            size_t dlen = slash ? (size_t)(slash - remote_path) : 0;
            memcpy(remote_dir, remote_path, dlen);
            remote_dir[dlen] = '\0';
            for (int i = 0; i < m->map_count; i++) {
                if (is_upload_only(m, m->map[i].host_path)) continue;
                const char *rh = m->map[i].remote_path;
                size_t rh_dir_len = strrchr(rh, '/') ? (size_t)(strrchr(rh, '/') - rh) : 0;
                if (rh_dir_len && (int)strlen(rh) >= (int)rh_dir_len &&
                    strncmp(rh, remote_dir, dlen) == 0) {
                    char host_dir[FILE_SYNC_PATH_MAX];
                    snprintf(host_dir, sizeof(host_dir), "%.*s",
                             (int)(strrchr(m->map[i].host_path, '/') - m->map[i].host_path),
                             m->map[i].host_path);
                    int need = snprintf(rel, sizeof(rel), "%s%s", host_dir,
                                        remote_path + dlen);
                    (void)need;
                    host_path = rel;
                    break;
                }
            }
            if (!host_path) continue; /* no mapping -> skip */
        }

        if (is_upload_only(m, host_path)) continue; /* skip credential files */

        if (me && me->pushed_hash[0] && have_remote &&
            strcmp(remote_hash, me->pushed_hash) != 0) {
            /* conflict: host modified since push, remote also changed ->
             * last-write-wins (apply remote). Python logs a warning; we apply. */
        }

        char dirbuf[FILE_SYNC_PATH_MAX];
        snprintf(dirbuf, sizeof(dirbuf), "%s", host_path);
        char *ls = strrchr(dirbuf, '/');
        if (ls) { *ls = '\0'; mkdir_p(dirbuf); }
        if (rename(full, host_path) == 0) {
            applied++;
        } else if (errno == EXDEV) {
            /* cross-device: copy instead. */
            FILE *src = fopen(full, "rb");
            FILE *dst = fopen(host_path, "wb");
            if (src && dst) {
                unsigned char buf[65536];
                size_t rn;
                while ((rn = fread(buf, 1, sizeof(buf), src)) > 0)
                    fwrite(buf, 1, rn, dst);
                applied++;
            }
            if (src) fclose(src);
            if (dst) fclose(dst);
        }
    }
    closedir(d);
    for (int i = 0; i < nsub; i++)
        applied += apply_staged(m, subdirs[i]);
    return applied;
}

/* mkdir -p (no shell) for a single path. */
static void mkdir_p(const char *path) {
    char tmp[FILE_SYNC_PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len && tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

int file_sync_manager_sync_back(file_sync_manager_t *m,
                                 const char *hermes_home) {
    if (!m || !m->download_fn) return -1;
    if (m->map_count == 0) return 0; /* nothing pushed yet; skip */

    const char *home = hermes_home && hermes_home[0] ? hermes_home : get_home();

    /* Download tar into a temp file. */
    char tmpl[FILE_SYNC_PATH_MAX];
    snprintf(tmpl, sizeof(tmpl), "%s/.sync.XXXXXX", home);
    int fd = mkstemp(tmpl);
    if (fd < 0) return -1;
    close(fd);

    if (!m->download_fn(tmpl, m->download_ctx)) {
        unlink(tmpl);
        return -1;
    }

    /* Defensive size cap. */
    struct stat st;
    if (stat(tmpl, &st) == 0 && st.st_size > FILE_SYNC_MAX_BYTES) {
        unlink(tmpl);
        return -1;
    }

    /* Extract into a temp staging dir. */
    char stagedir[FILE_SYNC_PATH_MAX];
    snprintf(stagedir, sizeof(stagedir), "%s/.sync-back.XXXXXX", home);
    if (!mkdtemp(stagedir)) { unlink(tmpl); return -1; }

    char cmd[FILE_SYNC_PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "tar xf '%s' -C '%s'", tmpl, stagedir);
    int rc = system(cmd);
    unlink(tmpl);
    if (rc != 0) {
        /* cleanup staging */
        char rm[FILE_SYNC_PATH_MAX + 32];
        snprintf(rm, sizeof(rm), "rm -rf '%s'", stagedir);
        system(rm);
        return -1;
    }

    int applied = apply_staged(m, stagedir);

    char rm[FILE_SYNC_PATH_MAX + 32];
    snprintf(rm, sizeof(rm), "rm -rf '%s'", stagedir);
    system(rm);
    (void)applied;
    return 0;
}
