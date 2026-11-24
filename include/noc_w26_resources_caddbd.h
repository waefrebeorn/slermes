/*
 * noc_w26_resources_caddbd.h - C surface for upstream
 * tools/bot_desktop/resources.py (gate w26_resources_caddbd).
 *
 * The memory gate a headed desktop is admitted through: how the host's free
 * memory is read (cgroup v2/v1 accounting, /proc/meminfo kB lines), how the two
 * candidate numbers are folded into one "available", and the advisory the pane
 * shows instead of the Start button.  Every file read is a seam: each entry
 * point takes the text that a reader would have returned.
 */
#ifndef NOC_W26_RESOURCES_CADDBD_H
#define NOC_W26_RESOURCES_CADDBD_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 1 MiB in bytes (the module's _MIB). */
#define W26_RESOURCES_CADDBD_MIB 1048576.0
/* DEFAULT_MIN_FREE_MB */
#define W26_RESOURCES_CADDBD_DEFAULT_MIN_FREE_MB 1536

/* {"text": "<file text>"} -> int | null   (resources._read_int) */
json_t *w26_resources_caddbd_read_int(const json_t *in);

/* {"text": "<meminfo text>"} -> {"MemTotal": <bytes>, ...}  (resources._meminfo) */
json_t *w26_resources_caddbd_meminfo(const json_t *in);

/* {"text": "<memory.stat text>", "key": "<name>"} -> int | null  (resources._stat_value) */
json_t *w26_resources_caddbd_stat_value(const json_t *in);

/* {"cgroup": {"version": 1|2, "limit": n|null, "usage": n|null, "cache": n|null}}
 * -> [limit|null, usage|null]   (resources._cgroup_limit_and_usage) */
json_t *w26_resources_caddbd_cgroup(const json_t *in);

/* {"meminfo": {...}, "cgroup": {...}} -> {"available_mb": n|null, "limit_mb": n|null}
 * (resources.memory_info) */
json_t *w26_resources_caddbd_memory_info(const json_t *in);

/* {"cfg": {"min_free_memory_mb": <any>}} -> int   (resources.min_free_mb) */
json_t *w26_resources_caddbd_min_free_mb(const json_t *in);

/* {"floor": n|null, "cfg": {...}} -> int   (resources.tight_headroom_mb) */
json_t *w26_resources_caddbd_tight_headroom_mb(const json_t *in);

/* {"info": {"available_mb": n|null, "limit_mb": n|null} | null, "need": n|null,
 *  "cfg": {...}, "meminfo": {...}, "cgroup": {...}} -> str | null
 * (resources.memory_blocker; info null falls back to memory_info()) */
json_t *w26_resources_caddbd_memory_blocker(const json_t *in);

/* The module's literal budgets/constants. */
json_t *w26_resources_caddbd_constants(void);

#ifdef __cplusplus
}
#endif
#endif
