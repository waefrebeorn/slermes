/*
 * memory_entries.c - memory entry-array helpers.
 *
 * Split from port_memory_tool.c (soul.md: self-contained modules).
 * Copy / free / count / dedup helpers over a plain entry array. No
 * file-scope state and no memory_store_t access.
 */
#include "memory_entries.h"
#include "memory_store.h"
#include "hermes_json.h"
#include "registry.h"  /* live "memory" tool registration */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>


/* ---- small helpers ---- */ void free_entries(char **arr, int n) {
    for (int i = 0; i < n; i++) free(arr[i]);
    free(arr);
}
 char **copy_entries(const char **src, int n) {
    char **out = malloc(sizeof(char*) * (n > 0 ? n : 1));
    for (int i = 0; i < n; i++) out[i] = strdup(src[i]);
    return out;
}
 int char_count_arr(const char **arr, int n) {
    if (n == 0) return 0;
    /* delimiter between n entries = (n-1) * strlen(ENTRY_DELIMITER) */
    int total = (n - 1) * (int)strlen(ENTRY_DELIMITER);
    for (int i = 0; i < n; i++) total += (int)strlen(arr[i]);
    return total;
}


/* dedup preserving order (keep first occurrence) */ char **dedup_entries(char **arr, int n, int *out_n) {
    char **out = malloc(sizeof(char*) * (n > 0 ? n : 1));
    int m = 0;
    for (int i = 0; i < n; i++) {
        int seen = 0;
        for (int j = 0; j < m; j++) if (strcmp(out[j], arr[i]) == 0) { seen = 1; break; }
        if (!seen) out[m++] = arr[i]; else free(arr[i]);
    }
    *out_n = m;
    free(arr);
    return out;
}
