/*
 * pairing.c — extracted from gateway/helpers.c monolith.
 *
 * Real implementation home for the Python module it ports (no longer a
 * name-parity stub). Public prototypes stay in include/gateway_helpers.h
 * (or hermes_gateway.h); callers are unchanged.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
#include "hermes_system_prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

/* ================================================================
 *  Pairing — secure atomic file write
 *  Port of Python gateway/pairing.py _secure_write().
 * ================================================================ */

/* Write data to file with restrictive permissions (owner read/write only).
 * Uses temp-file + atomic rename so readers see complete file only.
 * AG26: Port of Python gateway/pairing.py:_secure_write().
 */
bool secure_write(const char *path, const char *data) {
    if (!path || !data) return false;
    char tmp_path[1060];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.XXXXXX", path);
    int fd = mkstemp(tmp_path);
    if (fd < 0) return false;
    size_t len = strlen(data);
    ssize_t written = write(fd, data, len);
    if (written < 0 || (size_t)written != len) {
        close(fd); unlink(tmp_path); return false;
    }
    fsync(fd); close(fd);
    if (rename(tmp_path, path) != 0) { unlink(tmp_path); return false; }
    chmod(path, 0600);
    return true;
}

