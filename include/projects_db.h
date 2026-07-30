/*
 * projects_db.h — Per-profile first-class Project store (faithful C11 port of
 * hermes_cli/projects_db.py).
 *
 * A Project is a human-named, multi-folder workspace persisted at
 * $HERMES_HOME/projects.db (per-profile). Mirrors sessions/config/cron.
 *
 * This is the pure store layer: slug/id/path helpers, CRUD (create/list/get/
 * update/add_folder/remove_folder/set_primary/archive/restore/delete), the
 * active-project pointer, discovered-repo cache, path->project resolution
 * (longest-prefix folder match), and deterministic kanban branch naming.
 *
 * Backed by sqlite3 (link lib/libdb/sqlite3.o). Callers open a connection with
 * projects_db_connect(dir) and free it with projects_db_close().
 */

#ifndef PROJECTS_DB_H
#define PROJECTS_DB_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct projects_db projects_db_t; /* opaque sqlite connection handle */

/* ── Project folder ── */
typedef struct {
    char *path;
    char *label;     /* may be NULL */
    bool  is_primary;
    long  added_at;
} project_folder_t;

/* ── Project ── */
typedef struct {
    char *id;
    char *slug;
    char *name;
    long  created_at;
    char *description;   /* may be NULL */
    char *icon;          /* may be NULL */
    char *color;         /* may be NULL */
    char *board_slug;    /* may be NULL */
    char *primary_path;  /* may be NULL */
    bool  archived;
    project_folder_t *folders;
    int   n_folders;
} project_t;

/* ── discovered repo ── */
typedef struct {
    char *root;
    char *label;
    long  last_seen;
} discovered_repo_t;

/* Connection lifecycle. db_dir is the profile home; the file is
 * <db_dir>/projects.db. Returns NULL on failure. */
projects_db_t *projects_db_connect(const char *db_dir);
void projects_db_close(projects_db_t *db);

/* Pure helpers (also unit-testable directly). */
char *projects_db_slugify(const char *name);
/* Returns a malloc'd slug, or NULL if `slug` is None/empty/invalid. Caller
 * frees. Raises-equivalent: returns NULL for invalid (Python raises ValueError). */
char *projects_db_normalize_slug(const char *slug);
/* Absolute, user-expanded, separator-normalized path (no trailing sep). */
char *projects_db_normalize_path(const char *path);

/* ── CRUD ── */
/* create_project: returns malloc'd id (caller frees) or NULL on error.
 * folders is a NULL-terminated array of paths (may be NULL). */
char *projects_db_create_project(projects_db_t *db, const char *name,
                                 const char *slug, char **folders, int n_folders,
                                 const char *primary_path, const char *description,
                                 const char *icon, const char *color,
                                 const char *board_slug);

/* list_projects: include_archived=false lists only non-archived. Caller frees
 * the returned array with projects_db_free_projects(). */
project_t *projects_db_list_projects(projects_db_t *db, bool include_archived, int *out_count);
project_t *projects_db_get_project(projects_db_t *db, const char *id_or_slug); /* NULL if missing */
bool projects_db_update_project(projects_db_t *db, const char *project_id,
                                const char *name, const char *description,
                                const char *icon, const char *color,
                                const char *board_slug);
/* add_folder: returns malloc'd normalized path (caller frees) or NULL on error. */
char *projects_db_add_folder(projects_db_t *db, const char *project_id,
                             const char *path, const char *label, bool is_primary);
bool projects_db_remove_folder(projects_db_t *db, const char *project_id, const char *path);
bool projects_db_set_primary(projects_db_t *db, const char *project_id, const char *path);
bool projects_db_archive_project(projects_db_t *db, const char *project_id);
bool projects_db_restore_project(projects_db_t *db, const char *project_id);
bool projects_db_delete_project(projects_db_t *db, const char *project_id);

/* ── active-project pointer (project_meta KV) ── */
void projects_db_set_active(projects_db_t *db, const char *project_id); /* NULL clears */
char *projects_db_get_active_id(projects_db_t *db); /* caller frees; NULL if none */

/* ── discovered repos cache ── */
/* repos: array of (root, label) pairs, n entries; label may be NULL/"" → basename.
 * replace=true clears the table first. Returns rows written. */
int projects_db_record_discovered_repos(projects_db_t *db,
                                        const char **roots, const char **labels,
                                        int n, bool replace);
discovered_repo_t *projects_db_list_discovered_repos(projects_db_t *db, int *out_count);

/* ── resolution + naming ── */
/* Longest-prefix folder match. Returns malloc'd project id owner or NULL. */
project_t *projects_db_project_for_path(projects_db_t *db, const char *path, bool include_archived);
/* Deterministic kanban branch name: <slug>/<task_id>[-<title-slug>]. Caller frees. */
char *projects_db_branch_name_for(const project_t *proj, const char *task_id, const char *title);

/* Free helpers */
void projects_db_free_project_fields(project_t *p);
void projects_db_free_project(project_t *p);
void projects_db_free_projects(project_t *arr, int n);
void projects_db_free_folders(project_folder_t *arr, int n);
void projects_db_free_repos(discovered_repo_t *arr, int n);

#ifdef __cplusplus
}
#endif

#endif /* PROJECTS_DB_H */
