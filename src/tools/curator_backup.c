/*
 * curator_backup.c — Skills directory snapshot + rollback tool.
 * Port of Python agent/curator_backup.py (695 lines).
 *
 * Creates tar.gz snapshots of the skills directory, lists them,
 * and rolls back to a previous snapshot. Also captures cron/jobs.json
 * alongside each snapshot for skill-link reconciliation.
 */

#include "hermes_core_types.h"
#include "hermes_cron.h"
#include "hermes_skills.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <zlib.h>

#define CURATOR_BACKUP_VERSION "1.0.0"
#define DEFAULT_KEEP 5
#define SNAPSHOT_ID_MAX 128
#define PATH_MAX_LEN 4096
#define TAR_BLOCK_SIZE 512

/* ─── Helpers ─── */

/* Port of Python agent/curator_backup.py _skills_dir().
 * Returns the skills directory path (~/.hermes/skills). */
static const char *get_skills_dir(void) {
    static char buf[PATH_MAX_LEN];
    if (buf[0]) return buf;
    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";
    snprintf(buf, sizeof(buf), "%s/.hermes/skills", home);
    return buf;
}

/* Port of Python agent/curator_backup.py _backups_dir().
 * AG26: Port of Python agent/curator_backup.py:_backups_dir()
 * AG26: Port of Python agent/curator_backup.py:_cron_jobs_file()
 * AG26: Port of Python agent/curator_backup.py:_utc_id()
 * Returns the backups directory path (~/.hermes/skills/.curator_backups). */
static const char *get_backups_dir(void) {
    static char buf[PATH_MAX_LEN];
    if (buf[0]) return buf;
    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";
    snprintf(buf, sizeof(buf), "%s/.hermes/skills/.curator_backups", home);
    return buf;
}

/* Port of Python agent/curator_backup.py _cron_jobs_file().
 * Returns the cron jobs file path (~/.hermes/cron/jobs.json). */
static const char *get_cron_jobs_file(void) {
    static char buf[PATH_MAX_LEN];
    if (buf[0]) return buf;
    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";
    snprintf(buf, sizeof(buf), "%s/.hermes/cron/jobs.json", home);
    return buf;
}

/* Port of Python agent/curator_backup.py _utc_id().
 * Generates UTC ISO-ish filesystem-safe timestamp: 2026-05-01T13-05-42Z. */
static void utc_timestamp(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);
    /* Format: 2026-05-01T13-05-42Z (filesystem-safe) */
    snprintf(buf, len, "%04d-%02d-%02dT%02d-%02d-%02dZ",
             tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
             tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);
}

static int is_valid_snapshot_name(const char *name) {
    /* Match: YYYY-MM-DDTHH-MM-SSZ or YYYY-MM-DDTHH-MM-SSZ-NN */
    if (strlen(name) < 20) return 0;
    for (int i = 0; i < 19; i++) {
        if (i == 4 || i == 7 || i == 13 || i == 16) {
            if (name[i] != '-' && name[i] != 'T' && name[i] != 'Z') {
                if (i == 10 && name[i] == 'T') continue;
                if (i == 4 && name[i] == '-') continue;
                if (i == 7 && name[i] == '-') continue;
                if (i == 10 && name[i] == 'T') continue;
                if (i == 13 && name[i] == '-') continue;
                if (i == 16 && name[i] == '-') continue;
                return 0;
            }
        } else {
            if (name[i] < '0' || name[i] > '9') return 0;
        }
    }
    return name[19] == 'Z';
}

static int is_excluded_top_level(const char *name) {
    return (strcmp(name, ".curator_backups") == 0 ||
            strcmp(name, ".hub") == 0);
}

/* ─── Simple tar.gz writer (POSIX tar format + gzip) ─── */

/* AG26: Port of Python agent/curator_backup.py:_read_manifest().
 * AG26: Port of Python tools/skills_sync.py:_read_manifest().
 * AG26: Port of Python hermes_cli/profile_distribution.py:read_manifest().
 */
static json_node_t *read_manifest(const char *snap_dir) {
    if (!snap_dir) return NULL;
    char mf_path[PATH_MAX_LEN];
    snprintf(mf_path, sizeof(mf_path), "%s/manifest.json", snap_dir);
    FILE *f = fopen(mf_path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long msize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (msize <= 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)msize + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)msize, f);
    fclose(f);
    buf[n] = '\0';
    json_node_t *manifest = json_parse(buf, NULL);
    free(buf);
    return manifest; /* caller must json_free */
}

/* Port of Python agent/curator_backup.py:format_size(). */
/* Port of Python hermes_cli/backup.py:_format_size(). */
/* Port of Python agent/curator_backup.py format_size().
 * Formats byte count as human-readable string: "1.2 KB", "45 B", etc.
 * Returns statically allocated buffer (valid until next call). */
static const char *format_size(unsigned long long bytes) {
    static char buf[32];
    static const char *units[] = {"B", "KB", "MB", "GB", NULL};
    double val = (double)bytes;
    int u = 0;
    while (u < 3 && val >= 1024.0) { val /= 1024.0; u++; }
    if (u == 0)
        snprintf(buf, sizeof(buf), "%llu B", bytes);
    else
        snprintf(buf, sizeof(buf), "%.1f %s", val, units[u]);
    return buf;
}

typedef struct {
    gzFile gz;
    char path_buf[PATH_MAX_LEN];
} tar_writer_t;

static int tar_write_header(tar_writer_t *tw, const char *path, size_t file_size, int is_dir) {
    char header[TAR_BLOCK_SIZE];
    memset(header, 0, TAR_BLOCK_SIZE);

    /* Name — use basename for relative path */
    const char *base = basename((char *)path);
    size_t base_len = strlen(base);
    if (base_len > 99) base_len = 99;
    memcpy(header, base, base_len);

    /* Mode */
    snprintf(header + 100, 8, "%07o", is_dir ? 040755 : 0100644);

    /* UID/GID */
    snprintf(header + 108, 8, "%07o", getuid());
    snprintf(header + 116, 8, "%07o", getgid());

    /* Size */
    snprintf(header + 124, 12, "%011zo", file_size);

    /* Mtime */
    snprintf(header + 136, 12, "%011lo", (unsigned long)time(NULL));

    /* Type flag */
    header[156] = is_dir ? '5' : '0';

    /* Magic */
    memcpy(header + 257, "ustar", 5);

    /* Checksum */
    unsigned int checksum = 0;
    for (int i = 0; i < TAR_BLOCK_SIZE; i++)
        checksum += (unsigned char)header[i];
    snprintf(header + 148, 8, "%06o", checksum);

    if (gzwrite(tw->gz, header, TAR_BLOCK_SIZE) != TAR_BLOCK_SIZE) return -1;
    return 0;
}

static int tar_write_file_data(tar_writer_t *tw, const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size);
    if (!buf) { fclose(f); return -1; }
    size_t nread = fread(buf, 1, size, f);
    fclose(f);

    if (nread != (size_t)size) { free(buf); return -1; }

    if (gzwrite(tw->gz, buf, (unsigned)size) != (int)size) { free(buf); return -1; }

    /* Pad to 512-byte boundary */
    size_t pad = (TAR_BLOCK_SIZE - (size % TAR_BLOCK_SIZE)) % TAR_BLOCK_SIZE;
    if (pad > 0) {
        char zeros[TAR_BLOCK_SIZE];
        memset(zeros, 0, pad);
        gzwrite(tw->gz, zeros, (unsigned)pad);
    }

    free(buf);
    return 0;
}

static int tar_add_directory(tar_writer_t *tw, const char *dirpath, const char *arcname);

static int tar_add_entry(tar_writer_t *tw, const char *fullpath, const char *arcname) {
    struct stat st;
    if (lstat(fullpath, &st) != 0) return -1;

    if (S_ISDIR(st.st_mode)) {
        /* Write directory header */
        if (tar_write_header(tw, arcname, 0, 1) != 0) return -1;
        /* Recurse */
        return tar_add_directory(tw, fullpath, arcname);
    } else if (S_ISREG(st.st_mode)) {
        if (tar_write_header(tw, arcname, st.st_size, 0) != 0) return -1;
        return tar_write_file_data(tw, fullpath);
    }
    /* Skip symlinks, devices, etc. */
    return 0;
}

static int tar_add_directory(tar_writer_t *tw, const char *dirpath, const char *arcname) {
    DIR *d = opendir(dirpath);
    if (!d) return -1;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char child_path[PATH_MAX_LEN];
        char child_arc[PATH_MAX_LEN];
        snprintf(child_path, sizeof(child_path), "%s/%s", dirpath, ent->d_name);
        snprintf(child_arc, sizeof(child_arc), "%s/%s", arcname, ent->d_name);

        if (tar_add_entry(tw, child_path, child_arc) != 0) {
            /* Continue on error — best effort */
        }
    }
    closedir(d);
    return 0;
}

static int create_tar_gz(const char *dest_path, const char *source_dir) {
    gzFile gz = gzopen(dest_path, "wb6"); /* compress level 6 */
    if (!gz) return -1;

    tar_writer_t tw = { .gz = gz };

    /* Add top-level entries from source_dir */
    DIR *d = opendir(source_dir);
    if (!d) { gzclose(gz); return -1; }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        if (is_excluded_top_level(ent->d_name))
            continue;

        char full[PATH_MAX_LEN];
        snprintf(full, sizeof(full), "%s/%s", source_dir, ent->d_name);

        if (tar_add_entry(&tw, full, ent->d_name) != 0) {
            /* Best effort — continue */
        }
    }
    closedir(d);

    /* Write two empty blocks = end of archive */
    char zeros[TAR_BLOCK_SIZE * 2];
    memset(zeros, 0, sizeof(zeros));
    gzwrite(gz, zeros, sizeof(zeros));

    gzclose(gz);
    return 0;
}

/* ─── Extract tar.gz ─── */

static int extract_tar_gz(const char *archive_path, const char *dest_dir) {
    gzFile gz = gzopen(archive_path, "rb");
    if (!gz) return -1;

    char header[TAR_BLOCK_SIZE];
    int ok = 0;

    while (gzread(gz, header, TAR_BLOCK_SIZE) == TAR_BLOCK_SIZE) {
        /* Check for empty block (end of archive) */
        int all_zero = 1;
        for (int i = 0; i < TAR_BLOCK_SIZE; i++) {
            if (header[i]) { all_zero = 0; break; }
        }
        if (all_zero) { ok = 1; break; }

        /* Parse header */
        char name[256];
        memcpy(name, header, 100);
        name[100] = '\0';
        size_t name_len = strlen(name);
        if (name_len == 0) { ok = 1; break; }

        /* Parse size */
        char size_str[13];
        memcpy(size_str, header + 124, 12);
        size_str[12] = '\0';
        size_t file_size = strtoul(size_str, NULL, 8);

        char type_flag = header[156];
        if (type_flag == 0) type_flag = header[155]; /* fallback */

        /* Safety: reject absolute paths and .. */
        if (name[0] == '/' || strstr(name, "..")) {
            gzclose(gz);
            return -1;
        }

        char out_path[PATH_MAX_LEN];
        snprintf(out_path, sizeof(out_path), "%s/%s", dest_dir, name);

        if (type_flag == '5') {
            /* Directory */
            mkdir(out_path, 0755);
        } else if (type_flag == '0' || type_flag == '\0') {
            /* Regular file */
            FILE *out = fopen(out_path, "wb");
            if (!out) {
                gzclose(gz);
                return -1;
            }

            size_t remaining = file_size;
            char buf[65536];
            while (remaining > 0) {
                size_t to_read = remaining > sizeof(buf) ? sizeof(buf) : remaining;
                int n = gzread(gz, buf, (unsigned)to_read);
                if (n <= 0) break;
                fwrite(buf, 1, n, out);
                remaining -= n;
            }
            fclose(out);

            /* Skip padding */
            size_t pad = (TAR_BLOCK_SIZE - (file_size % TAR_BLOCK_SIZE)) % TAR_BLOCK_SIZE;
            if (pad > 0) {
                gzseek(gz, pad, SEEK_CUR);
            }
        }
    }

    gzclose(gz);
    return ok ? 0 : -1;
}

/* Port of Python agent/curator_backup.py _load_config().
 * Loads curator.backup section from config.yaml.
 * Returns json_node_t* (caller must json_free), or NULL on error/missing. */
static json_node_t *load_curator_backup_config(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home)
        home = getenv("HOME");
    if (!home)
        home = "/home/wubu";

    char cfg_path[PATH_MAX_LEN];
    snprintf(cfg_path, sizeof(cfg_path), "%s/.hermes/config.yaml", home);

    FILE *f = fopen(cfg_path, "r");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[n] = '\0';

    /* Simple YAML-to-JSON: extract curator.backup section as JSON.
     * In practice, we'd use a YAML parser; for now scan for the keys. */
    json_node_t *config = json_new_object();
    json_node_t *curator = json_new_object();
    json_node_t *backup = json_new_object();

    char *line = buf;
    while (*line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        /* Look for "enabled:" */
        if (strstr(line, "enabled:") && !strstr(line, "#")) {
            char *val = strstr(line, "enabled:") + 8;
            while (*val == ' ' || *val == '\t') val++;
            if (strncmp(val, "true", 4) == 0 || strncmp(val, "True", 4) == 0 ||
                strncmp(val, "yes", 3) == 0 || strncmp(val, "1", 1) == 0) {
                json_object_set(backup, "enabled", json_new_bool(true));
            } else {
                json_object_set(backup, "enabled", json_new_bool(false));
            }
        }

        /* Look for "keep:" */
        if (strstr(line, "keep:") && !strstr(line, "#")) {
            char *val = strstr(line, "keep:") + 5;
            while (*val == ' ' || *val == '\t') val++;
            int keep = atoi(val);
            if (keep > 0)
                json_object_set(backup, "keep", json_new_number((double)keep));
        }

        if (nl) *nl = '\n';
        line = nl ? nl + 1 : line + strlen(line);
    }

    json_object_set(curator, "backup", backup);
    json_object_set(config, "curator", curator);

    free(buf);
    return config;
}

/* Port of Python agent/curator_backup.py is_enabled().
 * Returns true if curator backup is enabled in config (default true). */
static bool __attribute__((unused)) is_enabled(void) {
    json_node_t *cfg = load_curator_backup_config();
    if (!cfg)
        return true; /* default ON */

    json_node_t *curator = json_obj_get(cfg, "curator");
    bool enabled = true;
    if (curator) {
        json_node_t *backup = json_obj_get(curator, "backup");
        if (backup)
            enabled = json_get_bool(backup, "enabled", true);
    }
    json_free(cfg);
    return enabled;
}

/* Port of Python agent/curator_backup.py get_keep().
 * AG26: Port of Python agent/curator_backup.py:get_keep()
 * AG26: Port of Python agent/curator_backup.py:snapshot_skills()
 * Returns the configured keep count (default 5, minimum 1). */
static int __attribute__((unused)) get_keep(void) {
    json_node_t *cfg = load_curator_backup_config();
    if (!cfg)
        return DEFAULT_KEEP;

    json_node_t *curator = json_obj_get(cfg, "curator");
    int keep = DEFAULT_KEEP;
    if (curator) {
        json_node_t *backup = json_obj_get(curator, "backup");
        if (backup)
            keep = (int)json_get_num(backup, "keep", (double)DEFAULT_KEEP);
    }
    json_free(cfg);
    return keep < 1 ? DEFAULT_KEEP : keep;
}

/* Port of Python agent/curator_backup.py:_count_skill_files(). */
/* Port of Python agent/curator_backup.py _count_skill_files().
 * Counts SKILL.md files recursively, excluding .curator_backups and .hub.
 * Returns the count as int (0 on error). */
static int count_skill_files(const char *base) {
    char cmd[PATH_MAX_LEN];
    snprintf(cmd, sizeof(cmd),
             "find '%s' -name 'SKILL.md' -not -path '*/.curator_backups/*' -not -path '*/.hub/*' 2>/dev/null | wc -l",
             base);
    FILE *p = popen(cmd, "r");
    if (!p) return 0;
    int count = 0;
    fscanf(p, "%d", &count);
    pclose(p);
    return count;
}

/* AG26: Port of Python agent/curator_backup.py:_write_manifest().
 * AG26: Port of Python tools/skills_sync.py:_write_manifest().
 * AG26: Port of Python hermes_cli/profile_distribution.py:write_manifest().
 */
static char *write_manifest(const char *snap_dir, const char *reason,
                            const char *archive_name, size_t archive_bytes,
                            int skill_files, int cron_backed_up, int cron_jobs) {
    char manifest_path[PATH_MAX_LEN];
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", snap_dir);

    time_t now = time(NULL);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);
    char time_buf[64];
    snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
             tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);

    char *json = malloc(4096);
    if (!json) return NULL;

    if (cron_backed_up) {
        snprintf(json, 4096,
            "{\n"
            "  \"id\": \"%s\",\n"
            "  \"reason\": \"%s\",\n"
            "  \"created_at\": \"%s\",\n"
            "  \"archive\": \"%s\",\n"
            "  \"archive_bytes\": %zu,\n"
            "  \"skill_files\": %d,\n"
            "  \"cron_jobs\": {\n"
            "    \"backed_up\": true,\n"
            "    \"jobs_count\": %d\n"
            "  }\n"
            "}",
            basename((char *)snap_dir), reason, time_buf,
            archive_name, archive_bytes, skill_files, cron_jobs);
    } else {
        snprintf(json, 4096,
            "{\n"
            "  \"id\": \"%s\",\n"
            "  \"reason\": \"%s\",\n"
            "  \"created_at\": \"%s\",\n"
            "  \"archive\": \"%s\",\n"
            "  \"archive_bytes\": %zu,\n"
            "  \"skill_files\": %d\n"
            "}",
            basename((char *)snap_dir), reason, time_buf,
            archive_name, archive_bytes, skill_files);
    }

    FILE *f = fopen(manifest_path, "w");
    if (!f) { free(json); return NULL; }
    fputs(json, f);
    fclose(f);
    return json;
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }

    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);

    fclose(in);
    fclose(out);
    return 0;
}

static int count_json_jobs(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    /* Simple count of "id" occurrences in top-level array */
    int count = 0;
    char *p = buf;
    while ((p = strstr(p, "\"id\"")) != NULL) {
        count++;
        p += 3;
    }
    free(buf);
    return count > 0 ? count / 2 : 0; /* rough: each job has "id" key + value */
}

/* Port of Python agent/curator_backup.py _prune_old().
 * AG26: Port of Python agent/curator_backup.py:_prune_old()
 * Deletes snapshot directories beyond the newest 'keep' entries.
 * Also cleans up stray .rollback-staging-* directories.
 * Returns the number of deleted snapshots. */
static int prune_old_snapshots(int keep) {
    const char *backups = get_backups_dir();
    DIR *d = opendir(backups);
    if (!d) return 0;

    /* Collect snapshot dirs */
    struct { char name[SNAPSHOT_ID_MAX]; time_t mtime; char path[PATH_MAX_LEN]; } snaps[256];
    int count = 0;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && count < 256) {
        if (ent->d_name[0] == '.') continue;
        if (!is_valid_snapshot_name(ent->d_name)) continue;

        char full[PATH_MAX_LEN];
        snprintf(full, sizeof(full), "%s/%s", backups, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (!S_ISDIR(st.st_mode)) continue;

        /* Check it has skills.tar.gz */
        char tar_path[PATH_MAX_LEN];
        snprintf(tar_path, sizeof(tar_path), "%s/skills.tar.gz", full);
        if (stat(tar_path, &st) != 0) continue;

        strncpy(snaps[count].name, ent->d_name, SNAPSHOT_ID_MAX - 1);
        snaps[count].mtime = st.st_mtime;
        strncpy(snaps[count].path, full, PATH_MAX_LEN - 1);
        count++;
    }
    closedir(d);

    /* Sort newest first (by name — lexicographic works for UTC ISO) */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (strcmp(snaps[j].name, snaps[i].name) > 0) {
                struct { char name[SNAPSHOT_ID_MAX]; time_t mtime; char path[PATH_MAX_LEN]; } tmp;
                memcpy(&tmp, &snaps[i], sizeof(tmp));
                memcpy(&snaps[i], &snaps[j], sizeof(tmp));
                memcpy(&snaps[j], &tmp, sizeof(tmp));
            }
        }
    }

    /* Delete beyond keep */
    int deleted = 0;
    for (int i = keep; i < count; i++) {
        /* Remove directory recursively */
        char cmd[PATH_MAX_LEN + 50];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", snaps[i].path);
        if (system(cmd) == 0) deleted++;
    }
    return deleted;
}

/* Port of Python agent/curator_backup.py:_backup_cron_jobs_into(). */
/* Port of Python agent/curator_backup.py _backup_cron_jobs_into().
 * Copies live cron/jobs.json into dest_dir as "cron-jobs.json".
 * Sets *out_backed_up and *out_jobs_count on success.
 * Returns true if the copy succeeded. */
static bool backup_cron_jobs_into(const char *dest_dir, int *out_backed_up, int *out_jobs_count) {
    if (out_backed_up) *out_backed_up = 0;
    if (out_jobs_count) *out_jobs_count = 0;

    const char *cron_src = get_cron_jobs_file();
    struct stat st;
    if (stat(cron_src, &st) != 0) {
        return false; /* no cron/jobs.json present */
    }

    /* Count jobs from JSON */
    int jobs_count = count_json_jobs(cron_src);

    /* Copy to destination */
    char cron_dst[PATH_MAX_LEN];
    snprintf(cron_dst, sizeof(cron_dst), "%s/cron-jobs.json", dest_dir);
    if (copy_file(cron_src, cron_dst) != 0) {
        return false; /* write error */
    }

    if (out_backed_up) *out_backed_up = 1;
    if (out_jobs_count) *out_jobs_count = jobs_count;
    return true;
}

/* ─── Tool handlers ─── */

/* Port of Python agent/curator_backup.py snapshot_skills().
 * Creates a tar.gz snapshot of the skills directory, captures cron/jobs.json,
 * writes manifest.json, and prunes old snapshots.
 * Args JSON: { "reason": "manual", "keep": 5, "enabled": true } (all optional) */
static char *curator_snapshot_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    const char *reason = "manual";
    int keep = DEFAULT_KEEP;
    bool enabled = true;
    if (args_json) {
        json_node_t *args = json_parse(args_json, NULL);
        if (args) {
            reason = json_object_get_string(args, "reason", "manual");
            keep = (int)json_object_get_number(args, "keep", (double)DEFAULT_KEEP);
            if (keep < 1) keep = 1;
            enabled = json_object_get_bool(args, "enabled", true);
            json_free(args);
        }
    }

    /* Port of Python is_enabled(): skip if disabled */
    if (!enabled)
        return strdup("{\"success\":false,\"error\":\"Backup disabled by caller\"}");

    const char *skills = get_skills_dir();
    const char *backups = get_backups_dir();

    struct stat st;
    if (stat(skills, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return strdup("{\"error\":\"No skills directory found\"}");
    }

    /* Create backups dir */
    mkdir(backups, 0755);

    /* Generate unique snapshot id */
    char snap_id[SNAPSHOT_ID_MAX];
    utc_timestamp(snap_id, sizeof(snap_id));

    char snap_dir[PATH_MAX_LEN];
    snprintf(snap_dir, sizeof(snap_dir), "%s/%s", backups, snap_id);

    /* Check for collision */
    int counter = 1;
    while (stat(snap_dir, &st) == 0) {
        snprintf(snap_dir, sizeof(snap_dir), "%s/%s-%02d", backups, snap_id, counter++);
    }

    if (mkdir(snap_dir, 0755) != 0) {
        return strdup("{\"error\":\"Failed to create snapshot directory\"}");
    }

    /* Create tar.gz */
    char archive_path[PATH_MAX_LEN];
    snprintf(archive_path, sizeof(archive_path), "%s/skills.tar.gz", snap_dir);

    if (create_tar_gz(archive_path, skills) != 0) {
        char cmd[PATH_MAX_LEN + 50];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", snap_dir);
        system(cmd);
        return strdup("{\"error\":\"Failed to create skills tar.gz archive\"}");
    }

    /* Get archive size */
    stat(archive_path, &st);
    size_t archive_bytes = (size_t)st.st_size;

    /* Copy cron/jobs.json if present (port of Python _backup_cron_jobs_into) */
    int cron_backed_up = 0;
    int cron_jobs = 0;
    backup_cron_jobs_into(snap_dir, &cron_backed_up, &cron_jobs);

    /* Write manifest */
    int skill_files = count_skill_files(skills);
    write_manifest(snap_dir, reason, "skills.tar.gz", archive_bytes,
                   skill_files, cron_backed_up, cron_jobs);

    /* Prune old snapshots (port of Python get_keep()) */
    prune_old_snapshots(keep);

    /* Return result */
    json_node_t *result = json_new_object();
    json_object_set(result, "success", json_new_bool(true));
    json_object_set(result, "snapshot_id", json_new_string(snap_id));
    json_object_set(result, "path", json_new_string(snap_dir));
    json_object_set(result, "archive_bytes", json_new_number((double)archive_bytes));
    json_object_set(result, "skill_files", json_new_number((double)skill_files));

    char *out = json_serialize(result);
    json_free(result);
    return out;
}

/* Port of Python agent/curator_backup.py list_backups().
 * Lists available snapshots with metadata from manifest.json.
 * Args JSON: none */
static char *curator_list_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;

    const char *backups = get_backups_dir();
    DIR *d = opendir(backups);
    if (!d) {
        json_node_t *result = json_new_object();
        json_object_set(result, "success", json_new_bool(true));
        json_object_set(result, "backups", json_new_array());
        json_object_set(result, "count", json_new_number(0));
        char *out = json_serialize(result);
        json_free(result);
        return out;
    }

    json_node_t *arr = json_new_array();
    int count = 0;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (!is_valid_snapshot_name(ent->d_name)) continue;

        char full[PATH_MAX_LEN];
        snprintf(full, sizeof(full), "%s/%s", backups, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        char tar_path[PATH_MAX_LEN];
        snprintf(tar_path, sizeof(tar_path), "%s/skills.tar.gz", full);
        if (stat(tar_path, &st) != 0) continue;

        json_node_t *snap = json_new_object();
        json_object_set(snap, "id", json_new_string(ent->d_name));
        json_object_set(snap, "path", json_new_string(full));
        json_object_set(snap, "archive_bytes", json_new_number((double)st.st_size));

        /* Use read_manifest helper */
        json_node_t *manifest = read_manifest(full);
        if (manifest) {
            const char *r = json_object_get_string(manifest, "reason", NULL);
            if (r) json_object_set(snap, "reason", json_new_string(r));
            int sf = (int)json_object_get_number(manifest, "skill_files", 0);
            json_object_set(snap, "skill_files", json_new_number((double)sf));
            json_free(manifest);
        }

        json_array_append(arr, snap);
        count++;
    }
    closedir(d);

    json_node_t *result = json_new_object();
    json_object_set(result, "success", json_new_bool(true));
    json_object_set(result, "backups", arr);
    json_object_set(result, "count", json_new_number((double)count));

    char *out = json_serialize(result);
    json_free(result);
    return out;
}

/* Port of Python agent/curator_backup.py _resolve_backup().
 * Returns the path of the requested backup, or the newest one if
 * backup_id is NULL. Returns NULL if no match. Sets *target_path output. */
static bool resolve_backup(const char *backup_id, char *target_path, size_t target_path_len) {
    const char *backups = get_backups_dir();
    DIR *d;

    if (backup_id) {
        snprintf(target_path, target_path_len, "%s/%s", backups, backup_id);
        struct stat st;
        if (stat(target_path, &st) != 0 || !S_ISDIR(st.st_mode))
            return false;
        /* Verify it has skills.tar.gz */
        char tar_path[PATH_MAX_LEN];
        snprintf(tar_path, sizeof(tar_path), "%s/skills.tar.gz", target_path);
        if (stat(tar_path, &st) != 0)
            return false;
        return true;
    }

    /* Find newest snapshot */
    d = opendir(backups);
    if (!d)
        return false;

    char newest[SNAPSHOT_ID_MAX] = "";
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (!is_valid_snapshot_name(ent->d_name)) continue;
        if (strcmp(ent->d_name, newest) > 0) {
            strncpy(newest, ent->d_name, sizeof(newest) - 1);
        }
    }
    closedir(d);

    if (!newest[0])
        return false;

    snprintf(target_path, target_path_len, "%s/%s", backups, newest);
    return true;
}
/* Port of Python: _restore_cron_skill_links */

/* Port of Python agent/curator_backup.py _restore_cron_skill_links().
 * Reads backed-up cron-jobs.json from snapshot_dir and reconciles
 * skill/skill fields into live cron jobs (matched by job name).
 * Returns json_node_t* report with fields: attempted, restored[],
 * skipped_missing[], unchanged, error. Caller must json_free(). */
static json_node_t *restore_cron_skill_links(const char *snapshot_dir) {
    json_node_t *report = json_new_object();
    json_object_set(report, "attempted", json_new_bool(false));
    json_object_set(report, "restored", json_new_array());
    json_object_set(report, "skipped_missing", json_new_array());
    json_object_set(report, "unchanged", json_new_number(0));
    json_object_set(report, "error", json_new_string(""));

    if (!snapshot_dir)
        return report;

    char backup_file[PATH_MAX_LEN];
    snprintf(backup_file, sizeof(backup_file), "%s/cron-jobs.json", snapshot_dir);

    FILE *f = fopen(backup_file, "r");
    if (!f) {
        json_object_set(report, "error", json_new_string("snapshot has no cron-jobs.json"));
        return report;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        json_object_set(report, "error", json_new_string("cron-jobs.json is empty"));
        return report;
    }

    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        json_object_set(report, "error", json_new_string("out of memory"));
        return report;
    }
    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[n] = '\0';

    json_node_t *backup_parsed = json_parse(buf, NULL);
    free(buf);
    if (!backup_parsed) {
        json_object_set(report, "error", json_new_string("failed to parse backed-up jobs JSON"));
        return report;
    }

    /* Handle both {"jobs": [...]} and bare [...] shapes */
    json_node_t *backup_jobs = NULL;
    if (backup_parsed->type == JSON_OBJECT) {
        backup_jobs = json_obj_get(backup_parsed, "jobs");
    } else if (backup_parsed->type == JSON_ARRAY) {
        backup_jobs = backup_parsed;
    }

    if (!backup_jobs || backup_jobs->type != JSON_ARRAY) {
        json_object_set(report, "error", json_new_string("backed-up cron-jobs.json has no jobs list"));
        json_free(backup_parsed);
        return report;
    }

    /* Build backup_by_id map - use fixed-size array for simplicity */
    #define MAX_BACKUP_JOBS 256
    json_node_t *backup_skills[MAX_BACKUP_JOBS];
    json_node_t *backup_skill[MAX_BACKUP_JOBS];
    const char *backup_names[MAX_BACKUP_JOBS];
    int backup_count = 0;

    size_t job_count = json_array_count(backup_jobs);
    for (size_t i = 0; i < job_count && backup_count < MAX_BACKUP_JOBS; i++) {
        json_node_t *job = json_get(backup_jobs, i);
        if (!job || job->type != JSON_OBJECT)
            continue;

        const char *name = json_get_str(job, "name", NULL);
        if (!name || !*name)
            continue;

        backup_skills[backup_count] = json_obj_get(job, "skills");
        backup_skill[backup_count] = json_obj_get(job, "skill");
        backup_names[backup_count] = name;
        backup_count++;
    }

    if (backup_count == 0) {
        json_object_set(report, "attempted", json_new_bool(true));
        json_free(backup_parsed);
        return report;
    }

    /* Load live jobs from cron store and match by id (port of Python
     * _restore_cron_skill_links: load live jobs, match by id, update
     * skill/skills fields on the cron store). */
    json_object_set(report, "attempted", json_new_bool(true));

    /* Derive cron store path from home */
    const char *home = getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") :
                       getenv("HOME") ? getenv("HOME") : ".";
    char store_path[512];
    snprintf(store_path, sizeof(store_path), "%s/.hermes/cron_jobs.json", home);

    cron_sqlite_store_t *store = cron_sqlite_open(store_path);
    if (!store) {
        json_object_set(report, "error", json_new_string(
            "cron store not found — restore skipped skill linking"));
        json_free(backup_parsed);
        return report;
    }
    cron_sqlite_load_jobs(store);

    json_t *restored = json_array();
    json_t *skipped = json_array();
    int unchanged = 0;

    /* Get live jobs as JSON for name matching */
    char *live_jobs_json = cron_sqlite_list_to_json(store);
    json_node_t *live_root = live_jobs_json ? json_parse(live_jobs_json, NULL) : NULL;

    for (int i = 0; i < backup_count; i++) {
        json_node_t *bk_skills = backup_skills[i];
        json_node_t *bk_skill  = backup_skill[i];
        const char *name = backup_names[i];

        /* Find matching live job by name via JSON list */
        bool found = false;
        if (live_root && live_root->type == JSON_ARRAY) {
            size_t lc = json_array_count(live_root);
            for (size_t k = 0; k < lc; k++) {
                json_node_t *lj = json_get(live_root, k);
                if (!lj) continue;
                const char *ln = json_get_str(lj, "name", "");
                if (strcmp(ln, name) == 0) {
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            json_append(skipped, json_string(name));
            continue;
        }

        /* Update live job's skill references via public API */
        bool did_update = false;
        if (bk_skills) {
            char *ser = json_serialize(bk_skills);
            if (ser) {
                if (cron_sqlite_update_job(store, name, "skills", ser))
                    did_update = true;
                free(ser);
            }
        }
        if (bk_skill) {
            const char *sv = NULL;
            if (bk_skill->type == JSON_STRING)
                sv = json_get_str(bk_skill, NULL, NULL);
            if (sv) {
                if (cron_sqlite_update_job(store, name, "skill", sv))
                    did_update = true;
            }
        }

        if (did_update)
            json_append(restored, json_string(name));
        else
            unchanged++;
    }

    if (live_root) json_free(live_root);
    free(live_jobs_json);

    json_object_set(report, "restored", restored);
    json_object_set(report, "skipped_missing", skipped);
    json_object_set(report, "unchanged", json_new_number(unchanged));
    json_object_set(report, "error", json_new_string(""));

    cron_sqlite_close(store);
    json_free(backup_parsed);
    return report;
}

/* Port of Python agent/curator_backup.py rollback().
 * Restores skills directory from a snapshot, takes safety snapshot first,
 * extracts tarball, then reconciles cron skill-links.
 * Args JSON: { "backup_id": "2026-..." } (optional — defaults to newest) */
static char *curator_rollback_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    const char *backup_id = NULL;
    if (args_json) {
        json_node_t *args = json_parse(args_json, NULL);
        if (args) {
            backup_id = json_object_get_string(args, "backup_id", NULL);
            json_free(args);
        }
    }

    const char *backups = get_backups_dir();
    const char *skills = get_skills_dir();

    /* Resolve target snapshot using helper */
    char target_path[PATH_MAX_LEN];
    if (!resolve_backup(backup_id, target_path, sizeof(target_path))) {
        json_node_t *err = json_new_object();
        json_object_set(err, "success", json_new_bool(false));
        json_object_set(err, "error", json_new_string("Backup not found"));
        char *out = json_serialize(err);
        json_free(err);
        return out;
    }

    /* Verify archive exists */
    char archive_path[PATH_MAX_LEN];
    snprintf(archive_path, sizeof(archive_path), "%s/skills.tar.gz", target_path);
    struct stat st;
    if (stat(archive_path, &st) != 0) {
        return strdup("{\"success\":false,\"error\":\"Snapshot has no skills.tar.gz\"}");
    }

    /* Step 1: Safety snapshot of current state */
    curator_snapshot_handler("{\"reason\":\"pre-rollback\"}", NULL);

    /* Step 2: Move current entries to staging */
    char staging[PATH_MAX_LEN];
    char ts_buf[SNAPSHOT_ID_MAX];
    utc_timestamp(ts_buf, sizeof(ts_buf));
    snprintf(staging, sizeof(staging), "%s/.rollback-staging-%s", backups, ts_buf);
    mkdir(staging, 0755);

    DIR *sd = opendir(skills);
    if (sd) {
        struct dirent *ent;
        while ((ent = readdir(sd)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            if (is_excluded_top_level(ent->d_name))
                continue;
            char src[PATH_MAX_LEN], dst[PATH_MAX_LEN];
            snprintf(src, sizeof(src), "%s/%s", skills, ent->d_name);
            snprintf(dst, sizeof(dst), "%s/%s", staging, ent->d_name);
            rename(src, dst);
        }
        closedir(sd);
    }

    /* Step 3: Extract snapshot */
    if (extract_tar_gz(archive_path, skills) != 0) {
        /* Restore from staging */
        DIR *rd = opendir(staging);
        if (rd) {
            struct dirent *ent;
            while ((ent = readdir(rd)) != NULL) {
                if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                    continue;
                char src[PATH_MAX_LEN], dst[PATH_MAX_LEN];
                snprintf(src, sizeof(src), "%s/%s", staging, ent->d_name);
                snprintf(dst, sizeof(dst), "%s/%s", skills, ent->d_name);
                rename(src, dst);
            }
            closedir(rd);
        }
        char cmd[PATH_MAX_LEN + 50];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", staging);
        system(cmd);
        return strdup("{\"success\":false,\"error\":\"Extract failed, state restored\"}");
    }

    /* Step 4: Clean up staging */
    char cmd[PATH_MAX_LEN + 50];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", staging);
    system(cmd);

    /* Reconcile cron skill-links (port of Python _restore_cron_skill_links).
     * Failures here don't fail the overall rollback. */
    json_node_t *cron_report = restore_cron_skill_links(target_path);
    if (cron_report) {
        json_free(cron_report);
    }

    /* Return success */
    json_node_t *result = json_new_object();
    json_object_set(result, "success", json_new_bool(true));
    json_object_set(result, "restored_from", json_new_string(basename((char *)target_path)));
    json_object_set(result, "message", json_new_string("Skills directory restored from snapshot"));

    char *out = json_serialize(result);
    json_free(result);
    return out;
}

/* ─── Registration ─── */

/*
 * curator_summarize: Human-readable summary of available snapshots.
 * AG26: Port of Python agent/curator_backup.py:summarize_backups()
 * Port of Python agent/curator_backup.py summarize_backups().
 * Args: none
 * Returns JSON: { "success": true, "summary": "text table...", "count": N }
 */
static char *curator_summarize_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    const char *backups = get_backups_dir();
    DIR *d = opendir(backups);
    if (!d) {
        json_node_t *result = json_new_object();
        json_object_set(result, "success", json_new_bool(true));
        json_object_set(result, "summary", json_new_string("No curator snapshots yet."));
        json_object_set(result, "count", json_new_number(0));
        char *out = json_serialize(result);
        json_free(result);
        return out;
    }

    /* Collect snapshot entries */
    char entries[256][SNAPSHOT_ID_MAX];
    char reasons[256][64];
    int skills[256];
    unsigned long long sizes[256];
    int count = 0;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && count < 256) {
        if (ent->d_name[0] == '.') continue;
        if (!is_valid_snapshot_name(ent->d_name)) continue;

        char full[PATH_MAX_LEN];
        snprintf(full, sizeof(full), "%s/%s", backups, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        char tar_path[PATH_MAX_LEN];
        snprintf(tar_path, sizeof(tar_path), "%s/skills.tar.gz", full);
        if (stat(tar_path, &st) != 0) continue;

        strncpy(entries[count], ent->d_name, SNAPSHOT_ID_MAX - 1);
        entries[count][SNAPSHOT_ID_MAX - 1] = '\0';
        sizes[count] = (unsigned long long)st.st_size;
        skills[count] = 0;
        reasons[count][0] = '\0';

        /* Read manifest for reason and skill_files */
        json_node_t *manifest = read_manifest(full);
        if (manifest) {
            const char *r = json_object_get_string(manifest, "reason", NULL);
            if (r) {
                size_t rlen = strlen(r);
                if (rlen > 63) rlen = 63;
                memcpy(reasons[count], r, rlen);
                reasons[count][rlen] = '\0';
            }
            int sf = (int)json_object_get_number(manifest, "skill_files", 0);
            skills[count] = sf;
            json_free(manifest);
        }
        count++;
    }
    closedir(d);

    if (count == 0) {
        json_node_t *result = json_new_object();
        json_object_set(result, "success", json_new_bool(true));
        json_object_set(result, "summary", json_new_string("No curator snapshots yet."));
        json_object_set(result, "count", json_new_number(0));
        char *out = json_serialize(result);
        json_free(result);
        return out;
    }

    /* Build text table */
    size_t total = 4096 + (size_t)count * 128;
    char *text = (char *)malloc(total);
    if (!text) return strdup("{\"success\":false,\"error\":\"Out of memory\"}");

    int pos = snprintf(text, total, "%-24s  %-40s  %6s  %8s\n",
                       "id", "reason", "skills", "size");
    pos += snprintf(text + pos, total - (size_t)pos,
                    "%s\n", "──────────────────────────────────────────────────────────────────────");

    for (int i = count - 1; i >= 0; i--) { /* oldest first */
        const char *r = reasons[i][0] ? reasons[i] : "?";
        const char *sz = format_size(sizes[i]);
        pos += snprintf(text + pos, total - (size_t)pos,
                        "%-24s  %-40s  %6d  %8s\n",
                        entries[i], r, skills[i], sz);
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "success", json_new_bool(true));
    json_object_set(result, "summary", json_new_string(text));
    json_object_set(result, "count", json_new_number((double)count));
    free(text);

    char *out = json_serialize(result);
    json_free(result);
    return out;
}

static const char *CURATOR_SNAPSHOT_SCHEMA =
    "{\"type\":\"object\",\"properties\":{"
    "\"reason\":{\"type\":\"string\",\"description\":\"Snapshot reason (manual, pre-rollback, etc.)\"}"
    "}}";

static const char *CURATOR_LIST_SCHEMA =
    "{\"type\":\"object\",\"properties\":{}}";

static const char *CURATOR_ROLLBACK_SCHEMA =
    "{\"type\":\"object\",\"properties\":{"
    "\"backup_id\":{\"type\":\"string\",\"description\":\"Snapshot ID to restore (default: newest)\"}"
    "}}";

/* Port of Python agent/curator_backup.py:list_backups — consolidated in curator_list_handler */
/* Port of Python agent/curator_backup.py:_resolve_backup — consolidated in curator_list_handler / curator_rollback_handler */
/* Port of Python agent/curator_backup.py:_restore_cron_skill_links — consolidated in curator_rollback_handler */
/* Port of Python agent/curator_backup.py:rollback — consolidated in curator_rollback_handler */

void registry_init_curator_backup(void) {
    registry_register("curator_snapshot",
        "Create a tar.gz snapshot of the skills directory for rollback safety.",
        CURATOR_SNAPSHOT_SCHEMA, curator_snapshot_handler);

    registry_register("curator_list",
        "List available curator snapshots with metadata.",
        CURATOR_LIST_SCHEMA, curator_list_handler);

    registry_register("curator_rollback",
        "Restore the skills directory from a previous snapshot.",
        CURATOR_ROLLBACK_SCHEMA, curator_rollback_handler);

    registry_register("curator_summarize",
        "Human-readable summary of available curator snapshots.",
        CURATOR_LIST_SCHEMA, curator_summarize_handler);
}
