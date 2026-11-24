/*
 * noc_w29_backend_010a82_p2.h - C surface for upstream
 * plugins/config_backends/remote/backend.py (gate w29_backend_010a82_p2).
 *
 * P2 covers what a fetched document turns into: the installed document and its
 * schema version (R6, oldest writer), the decoded lock list, the write diff
 * base, the version tuple a cache keys on, lock lookup and the advisory notes a
 * write leaves behind.  Clocks, random jitter, threads and the network are
 * fixture inputs; nothing here fetches.
 */
#ifndef NOC_W29_BACKEND_010A82_P2_H
#define NOC_W29_BACKEND_010A82_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"body": EffectiveResponse, "etag": str|null, "latest": int} -> the
 * fields _install derives: {"doc", "doc_version", "profile_version",
 * "profile_writer", "other_writers", "locks", "etag", "provenance", "notes"}. */
json_t *w29_backend_010a82_install(const json_t *req);

/* state: {"base": obj|null, "server_config": obj} -> a copy without
 * _config_version. */
json_t *w29_backend_010a82_diff_base(const json_t *state);

/* state: {"changed_ns", "etag", "gen", "private": null|{"serial","gen"}} ->
 * the _version_of tuple. */
json_t *w29_backend_010a82_version_of(const json_t *state);

/* cache_version: the version is already exact, so it is returned unchanged. */
json_t *w29_backend_010a82_cache_version(const json_t *version);

/* exists(home): the plane always resolves a document. */
json_t *w29_backend_010a82_exists(void);

/* req: {"locks": [[path, level], ...], "dotted": str} ->
 * {"found": bool, "level": any}: the first level that locks the dotted key. */
json_t *w29_backend_010a82_locked(const json_t *req);

/* req: {"path": [...], "sent": any, "found": bool, "got": any} -> the first
 * path at or under "path" whose read-back value differs, or null. */
json_t *w29_backend_010a82_first_change(const json_t *req);

/* req: {"server_config": obj, "unsets": [[...], ...]} -> the R5 advisory
 * notes, in unsets order. */
json_t *w29_backend_010a82_inherited_notes(const json_t *req);

/* req: {"server_config", "other_writers": [int], "profile_writer":
 * {"kind":"null"|"unknown"|"int","value":int}, "has_writer": bool,
 * "sets": [[path, value], ...], "unsets": [[...]], "latest": int,
 * "floor": int} -> null, or {"stored": obj, "after": int}. */
json_t *w29_backend_010a82_migration_check_input(const json_t *req);

/* req: {"url": str, "instance_id": str, "now": number,
 * "state": null|{"profile","profile_version","locks_count","fetched_at",
 * "last_error"}} -> the describe() line. */
json_t *w29_backend_010a82_describe(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
