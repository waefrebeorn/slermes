/*
 * noc_w23_readpool_4f9a79.h - C surface for upstream
 * persistence/sqlite/_state/readpool.py (gate w23_readpool_4f9a79).
 *
 * Read-connection budgeting for the store holder's WAL read path: the
 * descriptor-headroom gate, the process-wide permit ceiling and the per-file
 * permit ceiling, plus the two diagnostic counters (duplicate writer handles on
 * one path, rate-limited exhaustion warnings).
 *
 * Seams (injected by the harness, never decided here): the soft RLIMIT_NOFILE,
 * the open-descriptor count, the monotonic clock, the process table of live
 * holders on a path, and the logger.
 */
#ifndef NOC_W23_READPOOL_4F9A79_H
#define NOC_W23_READPOOL_4F9A79_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _READ_POOL_MAX: read connections alive at once against one database file. */
#define W23_READPOOL_4F9A79_POOL_MAX 8
/* _READ_POOL_PROCESS_MAX: read connections alive at once in this process. */
#define W23_READPOOL_4F9A79_PROCESS_MAX 24
/* _HANDLES_PER_PATH_WARN: writer handles on one file before the warning. */
#define W23_READPOOL_4F9A79_HANDLES_WARN 4
/* _FD_HEADROOM_RESERVE: descriptors kept back for everything else. */
#define W23_READPOOL_4F9A79_FD_RESERVE 64
/* _FD_USAGE_CACHE_SECONDS: staleness allowed on the descriptor count. */
#define W23_READPOOL_4F9A79_FD_CACHE_SECONDS 0.25
/* _POOL_EXHAUSTION_WARN_INTERVAL_S: one exhaustion warning per db per window. */
#define W23_READPOOL_4F9A79_POOL_WARN_SECONDS 60.0

/* -> {pool_max, process_max, handles_warn, fd_reserve, fd_cache_seconds,
 *     pool_warn_seconds}. */
json_t *w23_readpool_4f9a79_consts(void);

/* req keys: soft (number|null), stamp, now, cached (number|null), probe
 * (number|null).  -> {"ok":bool,"cached":number|null,"probed":bool}. */
json_t *w23_readpool_4f9a79_fd_headroom(const json_t *req);

/* req keys: module (bool), soft (number|null), infinity (bool), raise
 * ("attr"|"os"|"value"|null).  -> {"soft":number|null}. */
json_t *w23_readpool_4f9a79_fd_soft_limit(const json_t *req);

/* req keys: dirs[] ({entries:[...]} | {error:"emfile"|"enfile"|"enoent"}).
 * -> {"count":number|null}. */
json_t *w23_readpool_4f9a79_fd_count(const json_t *req);

/* req keys: path.  -> {"key":str}: Path(path).resolve(), str(path) on OSError. */
json_t *w23_readpool_4f9a79_budget_key(const json_t *req);

/* req keys: fd_ok (bool), process_used, path_used, requester (member index or
 * -1), members[] ({evictable:bool}), ops[] ("acquire"|"release").
 * -> {"results":[bool],"process_used":n,"path_used":n,"denied_fd":n,
 *     "evicted":n}. */
json_t *w23_readpool_4f9a79_acquire(const json_t *req);

/* req keys: db_path, members[] ({read_only:bool, has_conn:bool, site:str}).
 * -> {"warned":bool,"msg":str|null}. */
json_t *w23_readpool_4f9a79_register(const json_t *req);

/* req keys: ops[] ({db:str, t:number}).  -> {"warned":[bool],"msgs":[str|null]}. */
json_t *w23_readpool_4f9a79_note_exhausted(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
