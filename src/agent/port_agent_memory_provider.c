/*
 * port_agent_memory_provider.c — Port of Python agent/memory_provider.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: backup_paths */
typedef struct {
    char paths[16][1024];
    int count;
} backup_path_list_t;

backup_path_list_t memory_provider_backup_paths(void) {
    backup_path_list_t result = {0};
    /* Return external paths that store provider state outside HERMES_HOME */
    const char *home = getenv("HOME");
    if (!home) return result;
    
    /* Common external memory provider paths */
    const char *external_paths[] = {
        "/.honcho",
        "/.hindsight",
        "/.openviking",
        NULL
    };
    
    for (int i = 0; external_paths[i] && result.count < 16; i++) {
        snprintf(result.paths[result.count], 1024, "%s%s", home, external_paths[i]);
        result.count++;
    }
    return result;
}

