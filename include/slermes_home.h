/*
 * slermes_home.h — Slermes home directory infrastructure
 *
 * Slermes has its OWN home directory (default ~/.slermes) completely
 * separate from Hermes. Own config, own sessions, own skills.
 *
 * MIT License — Slermes Fork
 */
#ifndef SLERMES_HOME_H
#define SLERMES_HOME_H

#include <stdbool.h>
#include <stddef.h>

/* ── Environment variable ─────────────────────────────────────────── */
#define SLERMES_HOME_ENV "SLERMES_HOME"
#define SLERMES_HOME_DEFAULT ".slermes"

/* ── Directory structure ──────────────────────────────────────────── */
#define SLERMES_DIR_SESSIONS  "sessions"
#define SLERMES_DIR_SKILLS    "skills"
#define SLERMES_DIR_CRON      "cron"
#define SLERMES_DIR_PROFILES  "profiles"
#define SLERMES_DIR_CACHE     "cache"
#define SLERMES_DIR_PLUGINS   "plugins"
#define SLERMES_DIR_LOGS      "logs"

/* ── File paths ───────────────────────────────────────────────────── */
#define SLERMES_FILE_CONFIG   "config.yaml"
#define SLERMES_FILE_STATE_DB "state.db"
#define SLERMES_FILE_ENV      ".env"
#define SLERMES_FILE_AUTH     "auth.json"
#define SLERMES_FILE_CRON     "cron/jobs.json"

/* ── State DB schema version ──────────────────────────────────────── */
#define SLERMES_DB_SCHEMA_VERSION 1

/* ── API ──────────────────────────────────────────────────────────── */

/* Get the Slermes home directory path. Returns the resolved path.
 * First checks SLERMES_HOME env var, then ~/.slermes.
 * The returned string is statically allocated (do not free). */
const char *slermes_home(void);

/* Get full path to a file/dir under SLERMES_HOME.
 * Writes to the provided buffer. Returns buffer. */
char *slermes_path(char *buf, size_t bufsz, const char *relpath);

/* Initialize the Slermes home directory structure.
 * Creates all standard subdirectories and a default config if missing.
 * Returns true on success. */
bool slermes_init(void);

/* Check if Slermes home is initialized. */
bool slermes_initialized(void);

/* Get default config YAML content as a string (for init). */
const char *slermes_default_config(void);

#endif /* SLERMES_HOME_H */
