/*
 * noc_w23_profilesin_594c72.h - C surface for upstream
 * hermes_cli/profiles_initialization.py (gate w23_profilesin_594c72).
 *
 * The four things a profile creation does before publication: name the staging
 * tree beside the final directory, drop a vault key that belongs to an
 * unpublished clone, strip foreign runtime state out of an imported tree, and
 * seed the files a fresh profile owns from day one.
 *
 * Seams (injected by the harness, never decided here): the pid, the filesystem
 * (an existing leftover, the extracted tree's shape), the keychain, the profiles
 * module's seeders, the default SOUL text and the config mode fixer.
 */
#ifndef NOC_W23_PROFILESIN_594C72_H
#define NOC_W23_PROFILESIN_594C72_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req keys: dir, pid, leftover ("none"|"file"|"dir").
 * -> {"path":str,"removed":bool}.  The staging path is
 *    "<dir parent>/.<dir name>.staging-<pid>"; "removed" says the leftover was
 *    discarded before the staging name was handed back. */
json_t *w23_profilesin_594c72_staging(const json_t *req);

/* req keys: ref (any|null).  -> {"deleted":bool,"ref":any|null}: own_copied_key's
 * reference is deleted as-is, and nothing is inferred when it is None. */
json_t *w23_profilesin_594c72_discard(const json_t *req);

/* req keys: dir, children[] ({name, kind}), env ("absent"|"file"|"dir"),
 * config ("absent"|"file").
 * -> {"kept":[str],"removed":[str],"config_drop":str|null,"env_chmod":number|null}. */
json_t *w23_profilesin_594c72_prepare_import(const json_t *req);

/* req keys: dir, soul (str|null), no_skills, clone_all, description (str|null),
 * default_soul_error.
 * -> {"actions":[{"kind":...}]} in execution order. */
json_t *w23_profilesin_594c72_finish_layout(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
