/*
 * noc_w30_runtime_3588a2_p2.h - C surface for upstream
 * tools/bot_desktop/runtime.py, gate w30_runtime_3588a2_p2.
 *
 * Partition: LIFECYCLE DECISIONS + IDLE MATH + STATUS/PLACEMENT SHAPING.
 * The bodies here are the ones that decide *whether* something happens; the
 * thing itself (spawning a launcher, killing a group, starting a sandbox,
 * reading the lease file) is a seam whose answer arrives as a request field:
 *
 *   cfg               load_config_readonly()["bot_desktop"]
 *   limit             idle_stop_seconds()
 *   running           is_running()
 *   current           _launcher_identity() now     (null = no live launcher)
 *   idle              idle_seconds()
 *   holder            lease.get().holder           ("human" | "agent" | null)
 *   supported         is_supported_host()
 *   missing           missing_binaries()
 *   auto_start        cfg["auto_start"]
 *   where/reason      placement.resolve().where / .reason
 *   sandbox_running   sandbox_screen_running()
 *   published         published_env()
 *   still_due         the caller's still_due() answer (null = no callable)
 *   sandbox_stopped   sandbox_host.stop(env, profile)   (env is not None)
 *   reaped            _reap_orphaned_server(sd)
 */
#ifndef NOC_W30_RUNTIME_3588A2_P2_H
#define NOC_W30_RUNTIME_3588A2_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DEFAULT_IDLE_STOP_MINUTES */
#define W30_RT_DEFAULT_IDLE_STOP_MINUTES 30.0
/* HUMAN_LEASE_ABANDONED_WINDOWS, HUMAN_LEASE_ABANDONED_MIN_S */
#define W30_RT_HUMAN_LEASE_ABANDONED_WINDOWS 4.0
#define W30_RT_HUMAN_LEASE_ABANDONED_MIN_S 600.0
/* The default screen geometry and the gateway placement tag. */
#define W30_RT_DEFAULT_GEOMETRY "1440x900"
#define W30_RT_PLACEMENT_GATEWAY "gateway"
#define W30_RT_PLACEMENT_TERMINAL "terminal"
#define W30_RT_PLACEMENT_REFUSED "refused"

/* idle_stop_seconds(): {"cfg": obj} -> num (0.0 means "never") */
json_t *w30_rt_p2_idle_stop_seconds(const json_t *req);

/* idle_seconds(): {"now","activity_mtime","env_mtime"} -> num|null */
json_t *w30_rt_p2_idle_seconds(const json_t *req);

/* _idle_due(): {"limit","running","launcher","current","idle","holder"} ->
 * idle seconds when the screen is due now, else null */
json_t *w30_rt_p2_idle_due(const json_t *req);

/* _should_auto_start(): {"supported","env","missing","auto_start"} -> bool */
json_t *w30_rt_p2_should_auto_start(const json_t *req);

/* tool_placement(): {"where","reason"} -> "gateway"|"terminal",
 * or "!raise: <reason>" for a refused placement. */
json_t *w30_rt_p2_tool_placement(const json_t *req);

/* ensure_started_for_tool(): the same request as _should_auto_start plus
 * "where"/"reason" -> {"start_calls": 0|1, "touch_calls": 0|1} - what the
 * decision actually asks the seams to do. */
json_t *w30_rt_p2_ensure_started_for_tool(const json_t *req);

/* start(): {"where","reason","supported","missing","pm","installable",
 * "install_command"} -> "proceed"|"sandbox"|"!raise: <message>" */
json_t *w30_rt_p2_start(const json_t *req);

/* status(): the gateway-host DesktopStatus, as a dict (as_dict()). */
json_t *w30_rt_p2_status(const json_t *req);

/* _sandbox_status(): the sandbox-placed DesktopStatus, as a dict. */
json_t *w30_rt_p2_sandbox_status(const json_t *req);

/* stop(): {"marker","where","still_due","supported","launcher_pid",
 * "sandbox_stopped","reaped"} -> bool */
json_t *w30_rt_p2_stop(const json_t *req);

/* in_sandbox(): {"where": str} -> bool */
json_t *w30_rt_p2_in_sandbox(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W30_RUNTIME_3588A2_P2_H */
