/*
 * spill_safety.c — Port of tools/spill_safety.py (symlink-safe spill writes).
 * See include/spill_safety.h for the contract. C11, POSIX file APIs only.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "spill_safety.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* PoP: ensure_spill_dir @ tools/spill_safety.py:ensure_spill_dir
 * Python: path.mkdir(mode=0o700 if private else default, parents=True,
 * exist_ok=True); st = os.lstat(path); refuse non-dirs; chmod 0700 when
 * private and mode differs. mkdir -p semantics come from recursive parent
 * creation; exist_ok=True means EEXIST on the leaf is not an error. */
spill_status_t spill_safety_ensure_spill_dir(const char *path, int private_flag)
{
    if (!path || !*path)
        return SPILL_ERR_IO;

    /* parents=True, exist_ok=True */
    size_t len = strlen(path);
    char *tmp = malloc(len + 1);
    if (!tmp)
        return SPILL_ERR_IO;
    memcpy(tmp, path, len + 1);
    /* strip trailing slashes (Path semantics) */
    while (len > 1 && tmp[len - 1] == '/')
        tmp[--len] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, private_flag ? 0700 : 0777) != 0 && errno != EEXIST) {
                free(tmp);
                return SPILL_ERR_IO;
            }
            *p = '/';
        }
    }
    free(tmp);

    mode_t mode = private_flag ? 0700 : 0777;
    if (mkdir(path, mode) != 0 && errno != EEXIST)
        return SPILL_ERR_IO;

    struct stat st;
    if (lstat(path, &st) != 0)
        return SPILL_ERR_IO;
    if (!S_ISDIR(st.st_mode))
        return SPILL_ERR_NOT_DIR; /* OSError("spill dir is not a directory (symlink?): ...") */
    if (private_flag && (st.st_mode & 07777) != 0700 && chmod(path, 0700) != 0)
        return SPILL_ERR_IO;
    return SPILL_OK;
}

/* PoP: open_exclusive @ tools/spill_safety.py:open_exclusive */
FILE *spill_safety_open_exclusive(const char *path, int private_flag,
                                  int overwrite, spill_status_t *out_status)
{
    if (out_status)
        *out_status = SPILL_ERR_IO;
    if (!path || !*path)
        return NULL;

    if (overwrite) {
        struct stat st;
        if (lstat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (out_status)
                    *out_status = SPILL_ERR_REFUSE_DIR;
                return NULL; /* OSError("refusing to overwrite a directory: ...") */
            }
            if (unlink(path) != 0) {
                if (out_status)
                    *out_status = SPILL_ERR_IO;
                return NULL;
            }
        } else if (errno != ENOENT) {
            return NULL;
        }
    }

    /* mode = 0o600 if private else 0o666 (non-private honors umask);
     * O_NOFOLLOW is POSIX — always set here (Windows builds drop it). */
    int flags = O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW;
    mode_t mode = private_flag ? 0600 : 0666;
    int fd = open(path, flags, mode);
    if (fd < 0) {
        if (out_status)
            *out_status = (errno == EEXIST) ? SPILL_ERR_EXISTS : SPILL_ERR_IO;
        return NULL;
    }
    FILE *fh = fdopen(fd, "w");
    if (!fh) {
        close(fd);
        return NULL;
    }
    if (out_status)
        *out_status = SPILL_OK;
    return fh;
}

/* PoP: write_text_exclusive @ tools/spill_safety.py:write_text_exclusive */
spill_status_t spill_safety_write_text_exclusive(const char *path,
                                                 const char *text,
                                                 int private_flag,
                                                 int overwrite)
{
    spill_status_t st;
    FILE *fh = spill_safety_open_exclusive(path, private_flag, overwrite, &st);
    if (!fh)
        return st;
    size_t n = text ? strlen(text) : 0;
    if (n && fwrite(text, 1, n, fh) != n) {
        fclose(fh);
        return SPILL_ERR_IO;
    }
    if (fclose(fh) != 0)
        return SPILL_ERR_IO;
    return SPILL_OK;
}
