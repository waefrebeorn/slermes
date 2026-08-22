/*
 * spill_safety.h — Port of tools/spill_safety.py.
 *
 * Symlink-safe creation helpers for spill/cache files written into
 * predictable directories under ~/.hermes. A plain open(path,"w") follows a
 * pre-planted symlink; every helper here refuses symlinks by construction:
 *
 *  - new files are created O_CREAT|O_EXCL|O_NOFOLLOW (fails on ANY existing
 *    path, including a dangling symlink);
 *  - overwrites lstat+unlink the existing path first (deleting a link deletes
 *    the link, never its target; real directories are refused), then create
 *    exclusively;
 *  - ensure_spill_dir refuses a leaf that is not a real directory and
 *    tightens an existing leaf to 0700 when private.
 *
 * Privacy tiers mirror Python: private=True forces 0o700 dirs / 0o600 files;
 * private=False keeps umask-default permissions (0o666 file mode).
 *
 * Error surface: Python raises OSError; the C port returns a status code
 * from spill_status_t and optionally a malloc'd detail string (caller frees),
 * so call sites keep their existing best-effort handling.
 */
#ifndef SPILL_SAFETY_H
#define SPILL_SAFETY_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Status codes — the C rendering of Python's OSError outcomes. */
typedef enum {
    SPILL_OK = 0,          /* success */
    SPILL_ERR_EXISTS = 1,  /* FileExistsError: exclusive create hit existing path */
    SPILL_ERR_NOT_DIR = 2, /* OSError("spill dir is not a directory (symlink?)...) */
    SPILL_ERR_REFUSE_DIR = 3, /* OSError("refusing to overwrite a directory: ...") */
    SPILL_ERR_IO = 4       /* any other OS-level failure */
} spill_status_t;

/* PoP: ensure_spill_dir @ tools/spill_safety.py:ensure_spill_dir
 * Create path (and parents) as a directory, refusing symlinks. With
 * private!=0 the leaf is created 0700 and an already-existing leaf is
 * tightened to 0700. Returns SPILL_OK, SPILL_ERR_NOT_DIR (leaf exists and is
 * not a real directory) or SPILL_ERR_IO. */
spill_status_t spill_safety_ensure_spill_dir(const char *path, int private_flag);

/* PoP: open_exclusive @ tools/spill_safety.py:open_exclusive
 * Open path for writing via exclusive create; never follows a link. With
 * overwrite!=0, an existing path is lstat-checked (directories refused) and
 * unlinked before the exclusive create. mode is 0600 (private) or 0666
 * (umask applies). Returns a stream positioned for writing (caller fclose()s)
 * or NULL with *out_status set. */
FILE *spill_safety_open_exclusive(const char *path, int private_flag,
                                  int overwrite, spill_status_t *out_status);

/* PoP: write_text_exclusive @ tools/spill_safety.py:write_text_exclusive
 * Path.write_text equivalent that refuses to follow symlinks. */
spill_status_t spill_safety_write_text_exclusive(const char *path,
                                                 const char *text,
                                                 int private_flag,
                                                 int overwrite);

#ifdef __cplusplus
}
#endif

#endif /* SPILL_SAFETY_H */
