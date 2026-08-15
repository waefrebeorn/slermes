/* hermes_memory.h — memory subsystem function declarations */
#ifndef HERMES_MEMORY_H
#define HERMES_MEMORY_H

#include <stdbool.h>
#include <stddef.h>

/* memory_storage.c / memory_tool.c */
char *memory_storage_get(const char *key);
bool memory_storage_set(const char *key, const char *value);
bool memory_storage_delete(const char *key);
char *memory_storage_list(void);

#endif /* HERMES_MEMORY_H */
