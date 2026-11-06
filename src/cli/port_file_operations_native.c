/*
 * port_file_operations_native.c — Port of tools/file_operations_native.py::sha256_of
 * and _open_regular (the fd open/regular-file gate). Returns the hex SHA-256 of
 * a regular file's contents, or NULL when it cannot be read natively (missing,
 * not a regular file, or an OS error) — mirroring Python's except->None contract.
 */

#include "port_file_operations_native.h"

#include <errno.h>
#include <fcntl.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Python: _open_regular(path) -> fd of a regular file, or (-1 semantics) when
 * missing; a non-regular file raises -> caller returns NULL. We open O_RDONLY
 * with O_NONBLOCK (as Python does when available) and judge by fstat. */
static int open_regular(const char *path, struct stat *st)
{
#ifdef O_NONBLOCK
    int flags = O_RDONLY | O_NONBLOCK;
#else
    int flags = O_RDONLY;
#endif
    int fd = open(path, flags);
    if (fd < 0)
        return -1; /* missing / denied / OSError */
    if (fstat(fd, st) != 0 || !S_ISREG(st->st_mode)) {
        close(fd);
        return -1; /* not a regular file -> Python raises _NotRegular -> NULL */
    }
    return fd;
}

/* PoP: fop_sha256_of @ tools/file_operations_native.py:sha256_of */
char *fop_sha256_of(const char *path)
{
    if (!path || !*path)
        return NULL;
    struct stat st;
    int fd = open_regular(path, &st);
    if (fd < 0)
        return NULL;

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    char *hex = NULL;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        close(fd);
        return NULL;
    }
    unsigned char buf[1 << 20]; /* Python reads 1MiB chunks */

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1)
        goto done;
    ssize_t n;
    while (1) {
        n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            goto done; /* read error -> Python OSError -> NULL */
        }
        if (n == 0)
            break;
        if (EVP_DigestUpdate(ctx, buf, (size_t)n) != 1)
            goto done;
    }
    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1)
        goto done;

    size_t hi = (size_t)digest_len * 2;
    hex = malloc(hi + 1);
    if (hex) {
        static const char hexc[] = "0123456789abcdef";
        for (unsigned int i = 0; i < digest_len; i++) {
            hex[i * 2] = hexc[digest[i] >> 4];
            hex[i * 2 + 1] = hexc[digest[i] & 0x0f];
        }
        hex[hi] = '\0';
    }
done:
    EVP_MD_CTX_free(ctx);
    close(fd);
    return hex;
}
