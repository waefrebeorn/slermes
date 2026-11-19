/*
 * port_utils_ports.h — C11 port of pure helpers from utils.py
 */
#ifndef PORT_UTILS_PORTS_H
#define PORT_UTILS_PORTS_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: atomic_yaml_write @ utils/files.py:atomic_yaml_write */
int util_atomic_yaml_write(const char *path, const char *yaml_text);

/* PoP: atomic_json_write @ utils/files.py:atomic_yaml_write */
int util_atomic_json_write(const char *path, const char *data_json);

/* util_rename_replace @ os.replace -- atomic replace that WORKS on Windows.
 *
 * POSIX rename() overwrites an existing destination; Windows/MinGW rename()
 * fails with EEXIST instead. Every atomic-write path here (temp file + rename)
 * therefore silently kept the STALE file after its first write, so a config
 * edit, a cache refresh or a state update looked like it succeeded and then
 * read back the old contents. os.replace overwrites on all platforms. */
int util_rename_replace(const char *src, const char *dst);

/* PoP: fast_safe_load @ utils/files.py:fast_safe_load */
char *util_fast_safe_load(const char *yaml_text);

/* PoP: preserve_file_mode @ utils/files.py:atomic_yaml_write */
long util_preserve_file_mode(const char *path);

/* PoP: restore_file_mode @ utils/files.py:atomic_yaml_write */
int util_restore_file_mode(const char *path, long mode);

#ifdef __cplusplus
}
#endif

#endif /* PORT_UTILS_PORTS_H */
