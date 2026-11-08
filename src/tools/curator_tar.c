/*
 * curator_tar.c — tar.gz archive read/write helpers.
 *
 * Split from curator_backup.c (soul.md: self-contained modules). Pure
 * tar + gzip streaming; no file-scope state beyond the writer struct.
 */
#include "curator_tar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>
#include <zlib.h>

#define PATH_MAX_LEN 4096
#define TAR_BLOCK_SIZE 512

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


int create_tar_gz(const char *dest_path, const char *source_dir) {
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


int extract_tar_gz(const char *archive_path, const char *dest_dir) {
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

int is_excluded_top_level(const char *name) {
    return (strcmp(name, ".curator_backups") == 0 ||
            strcmp(name, ".hub") == 0);
}
