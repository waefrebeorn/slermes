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

/* PoP: atomic_yaml_write @ utils.py:atomic_yaml_write */
int util_atomic_yaml_write(const char *path, const char *yaml_text);

/* PoP: atomic_json_write @ utils.py:atomic_yaml_write */
int util_atomic_json_write(const char *path, const char *data_json);

/* PoP: fast_safe_load @ utils.py:fast_safe_load */
char *util_fast_safe_load(const char *yaml_text);

/* PoP: preserve_file_mode @ utils.py:atomic_yaml_write */
long util_preserve_file_mode(const char *path);

/* PoP: restore_file_mode @ utils.py:atomic_yaml_write */
int util_restore_file_mode(const char *path, long mode);

#ifdef __cplusplus
}
#endif

#endif /* PORT_UTILS_PORTS_H */
