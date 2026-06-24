#ifndef SRC_CLI_PORT_PROFILES_C
#define SRC_CLI_PORT_PROFILES_C

#include "hermes.h"
#include "hermes_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* Port of Python: profiles_to_serve */
void profiles_to_serve(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "profiles_to_serve: null context");
        return;
    }
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char profiles_dir[4096];
    snprintf(profiles_dir, sizeof(profiles_dir), "%s/profiles", home);

    hermes_log(LOG_INFO, "port", "profiles_to_serve: scanning %s", profiles_dir);

    DIR *dir = opendir(profiles_dir);
    if (!dir) {
        hermes_log(LOG_DEBUG, "port", "profiles_to_serve: no profiles directory");
        return;
    }
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        hermes_log(LOG_DEBUG, "port", "profiles_to_serve: found profile %s", entry->d_name);
        count++;
    }
    closedir(dir);
    hermes_log(LOG_INFO, "port", "profiles_to_serve: %d profiles found", count);
}

#endif /* SRC_CLI_PORT_PROFILES_C */
