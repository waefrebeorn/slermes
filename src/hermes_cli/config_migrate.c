/*
 * config_migrate.c -- extracted from cli/config.c monolith.
 * Real implementation of one config-lifecycle concern; public
 * hermes_config_* protos stay in include/hermes_core_types.h.
 */

#include "hermes_core_types.h"
#include "config_schema.h"
#include "hermes_yaml.h"
#include "hermes_json.h"
#include "hermes_auth.h"
#include "provider_metadata.h"
#include "curses_widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/stat.h>

static int migrate_v0_to_v1(hermes_config_t *cfg, const char *config_path) {
    (void)cfg;
    /* v0→v1: Add config_version field to YAML file.
     * Read file, find or insert config_version: 1, write back. */
    FILE *f = fopen(config_path, "r");
    if (!f) return 0; /* No file to migrate */

    /* Read entire file into memory */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return 0; }
    if (fsize > 1024 * 1024) { fclose(f); return -1; } /* Sanity cap */

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(f); return -1; }
    size_t nread = fread(buf, 1, (size_t)fsize, f);
    buf[nread] = '\0';
    fclose(f);

    /* Check if config_version already present */
    if (strstr(buf, HERMES_CONFIG_VERSION_KEY)) {
        free(buf);
        return 0; /* Already has version field */
    }

    /* Insert config_version: 1 after the first line (comment or blank) */
    char *insert_point = buf;
    /* Skip shebang or first comment line */
    while (*insert_point && *insert_point != '\n') insert_point++;
    if (*insert_point == '\n') insert_point++;

    char *new_buf;
    size_t pre_len = (size_t)(insert_point - buf);
    size_t remaining = nread - pre_len;
    /* Insert: config_version: 1\n */
    const char *version_line = "config_version: 1\n";
    size_t ver_len = strlen(version_line);
    new_buf = (char *)malloc(pre_len + ver_len + remaining + 1);
    if (!new_buf) { free(buf); return -1; }
    memcpy(new_buf, buf, pre_len);
    memcpy(new_buf + pre_len, version_line, ver_len);
    memcpy(new_buf + pre_len + ver_len, insert_point, remaining);
    new_buf[pre_len + ver_len + remaining] = '\0';
    free(buf);

    /* Write back */
    f = fopen(config_path, "w");
    if (!f) { free(new_buf); return -1; }
    size_t written = fwrite(new_buf, 1, pre_len + ver_len + remaining, f);
    fclose(f);
    free(new_buf);

    return (written == pre_len + ver_len + remaining) ? 0 : -1;
}
void hermes_file_permissions_harden(const char *hermes_home,
                                    const char *session_db_path,
                                    const char *cron_store_path,
                                    uid_t owner)
{
    if (owner == 0) return;  /* root — skip, permissions are moot */

    /* 1. Home directory — 0700 */
    if (hermes_home && *hermes_home) {
        struct stat st;
        if (stat(hermes_home, &st) == 0 && S_ISDIR(st.st_mode))
            chmod(hermes_home, 0700);
    }

    /* 2. Config file — 0600 */
    if (hermes_home && *hermes_home) {
        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/config.yaml", hermes_home);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
    }

    /* 3. .env file — 0600 */
    if (hermes_home && *hermes_home) {
        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/.env", hermes_home);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
    }

    /* 4. Session DB — 0600 */
    if (session_db_path && *session_db_path) {
        struct stat st;
        if (stat(session_db_path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(session_db_path, 0600);
    }

    /* 5. Vault file — 0600 (standard location under HERMES_HOME) */
    if (hermes_home && *hermes_home) {
        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/data/vault.dat", hermes_home);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
        /* Also check ~/.slermes/ location */
        snprintf(path, sizeof(path), "%s/.slermes/vault.dat", hermes_home);
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
        /* And ~/.slermes/ location */
        snprintf(path, sizeof(path), "%s/.slermes/vault.dat", hermes_home);
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(path, 0600);
    }

    /* 6. Cron store — 0600 (contains job configs that may embed API keys) */
    if (cron_store_path && *cron_store_path) {
        struct stat st;
        if (stat(cron_store_path, &st) == 0 && S_ISREG(st.st_mode))
            chmod(cron_store_path, 0600);
    }
}
/* PoP: hermes_config_migrate @ hermes_cli/console_engine.py:_config_migrate */
bool hermes_config_migrate(hermes_config_t *cfg, const char *config_dir) {
    if (!cfg) return false;

    char config_path[HERMES_PATH_MAX];
    if (config_dir && config_dir[0])
        snprintf(config_path, sizeof(config_path), "%s/config.yaml", config_dir);
    else {
        char home[HERMES_PATH_MAX];
        hermes_get_home(home, sizeof(home));
        snprintf(config_path, sizeof(config_path), "%s/config.yaml", home);
    }

    int version = cfg->config_version;

    /* If version not set (fresh config or legacy), check file */
    if (version <= 0) {
        /* Try reading version from file */
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse_file(config_path, &err);
        if (doc) {
            int fv = yaml_get_int(doc, HERMES_CONFIG_VERSION_KEY, 0);
            cfg->config_version = fv;
            version = fv;
            yaml_free(doc);
        }
        if (err) free(err);
    }

    if (version >= HERMES_CONFIG_VERSION)
        return false; /* Already current, no migration needed */

    fprintf(stderr, "Config migration: v%d → v%d\n", version, HERMES_CONFIG_VERSION);

    /* Run migrations sequentially */
    int current = version;
    bool changed = false;

    if (current < 1) {
        if (migrate_v0_to_v1(cfg, config_path) == 0) {
            current = 1;
            cfg->config_version = 1;
            changed = true;
        } else {
            fprintf(stderr, "Error: v0→v1 migration failed\n");
            return false;
        }
    }

    /* Future migrations:
     * if (current < 2) { migrate_v1_to_v2(cfg, config_path); current = 2; changed = true; }
     */

    if (changed) {
        fprintf(stderr, "Config migration complete: v%d → v%d\n", version, HERMES_CONFIG_VERSION);
    }

    return changed;
}
