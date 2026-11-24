/*
 * noc_w24_basekanban_865998.h - C surface for upstream
 * gateway/platforms/base_kanban_media.py (gate w24_basekanban_865998).
 *
 * The Kanban attachment/board path algebra behind the media-delivery allow and
 * deny lists: which board directory names the allow side accepts, which durable
 * attachment roots exist (the shared root, the env override, one per named board
 * holding a kanban.db), and where a named board's SQLite store and its
 * WAL/SHM/journal sidecars sit.
 *
 * Seams: ``Path.iterdir``/``is_dir``/``is_symlink``/``is_file`` and
 * ``os.environ`` are not read here -- the caller passes the board entries and
 * the override as values.  ``expanduser`` is a no-op seam (paths arrive
 * already absolute).  Paths are rendered POSIX-style so the same string works
 * on either host; the join/normalise rules are pathlib's.
 */
#ifndef NOC_W24_BASEKANBAN_865998_H
#define NOC_W24_BASEKANBAN_865998_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The named-board directory pattern, and the sidecars a board store owns. */
#define W24_BASEKANBAN_865998_SQLITE_FILES 4

/* name: <any> -> bool.  re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,63}", name):
 * 1..64 chars, lowercase alnum first, then lowercase alnum / "_" / "-".
 * A non-string name is the re TypeError seam -> null. */
json_t *w24_basekanban_865998_board_name_ok(const json_t *name);

/* {root, override, boards: [{name, is_symlink, has_db}, ...]} -> [path, ...].
 * The override (stripped) wins alone; else the shared attachments root plus one
 * <root>/kanban/boards/<name>/attachments per board that is not a symlink, has
 * an acceptable name and holds a kanban.db. */
json_t *w24_basekanban_865998_attachment_roots(const json_t *req);

/* {root, boards: [{name}, ...]} -> [path, ...].  Every board dir (no name
 * filter: the deny side must catch a board whatever its name) times the four
 * kanban.db files. */
json_t *w24_basekanban_865998_board_db_paths(const json_t *req);

/* The path literals and the name pattern, as an object. */
json_t *w24_basekanban_865998_constants(void);

#ifdef __cplusplus
}
#endif
#endif
