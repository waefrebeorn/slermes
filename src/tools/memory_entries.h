/*
 * memory_entries.h - memory entry-array helpers.
 *
 * Module-internal header (port_memory_tool.c <-> memory_entries.c).
 */
#ifndef MEMORY_ENTRIES_H
#define MEMORY_ENTRIES_H

#include <stddef.h>
#include <stdbool.h>
#include "memory_store.h"


#define ENTRY_DELIMITER "\n§\n"

int char_count_arr(const char **arr, int n);
char ** copy_entries(const char **src, int n);
char ** dedup_entries(char **arr, int n, int *out_n);
void free_entries(char **arr, int n);

#endif /* MEMORY_ENTRIES_H */
