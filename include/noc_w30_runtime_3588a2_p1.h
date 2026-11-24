/*
 * noc_w30_runtime_3588a2_p1.h - C surface for upstream
 * tools/bot_desktop/runtime.py, gate w30_runtime_3588a2_p1.
 *
 * Partition: HOST INQUIRY + STATE PATHS + DISPLAY BAND + PUBLISHED ENV.
 * Everything here answers a question about the machine or derives a path from
 * an answer somebody else already collected.  The answers themselves are
 * seams and arrive as request fields:
 *
 *   home            get_hermes_home()
 *   platform        sys.platform
 *   present         shutil.which(b) is not None, per REQUIRED_BINARIES entry
 *   which           shutil.which(pm) is not None, per candidate package manager
 *   euid            os.geteuid() (null = no geteuid on this host)
 *   pidfile         launcher.pid, raw file text
 *   create_time     _create_time(pid)     (null = psutil has no such process)
 *   alive           _pid_alive(pid)       (False for a zombie, per the truth)
 *   raw             a lock/env file, raw file text
 *   unix_table      /proc/net/unix text   (no read = false)
 *   in_use          _display_in_use(n) for the numbers the caller probed
 *   exists          Path.exists() on rfb.sock
 *   marker          sandbox_host._read_marker() truthiness
 *
 * No symbol in this gate touches the filesystem, the process table or the
 * environment: paths are joined, texts are parsed, tables are consulted.
 */
#ifndef NOC_W30_RUNTIME_3588A2_P1_H
#define NOC_W30_RUNTIME_3588A2_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _DISPLAY_MIN, _DISPLAY_MAX - the private display band (a restart reuses it). */
#define W30_RT_DISPLAY_MIN 20
#define W30_RT_DISPLAY_MAX 89

/* len(REQUIRED_BINARIES) */
#define W30_RT_REQUIRED_COUNT 10

/* state_dir(): {"home": str} -> "<home>/bot-desktop" */
json_t *w30_rt_p1_state_dir(const json_t *req);

/* is_supported_host(): {"platform": str} -> bool (platform starts with "linux") */
json_t *w30_rt_p1_is_supported_host(const json_t *req);

/* missing_binaries(): {"present": [str]} -> [str] in REQUIRED_BINARIES order */
json_t *w30_rt_p1_missing_binaries(const json_t *req);

/* package_manager(): {"which": [str]} -> "apt"|"dnf"|"pacman"|null */
json_t *w30_rt_p1_package_manager(const json_t *req);

/* is_root(): {"euid": num|null} -> bool */
json_t *w30_rt_p1_is_root(const json_t *req);

/* installable(): {"which": [str], "euid": num|null} -> bool */
json_t *w30_rt_p1_installable(const json_t *req);

/* install_command(): {"which": [str], "euid": num|null} -> str|null */
json_t *w30_rt_p1_install_command(const json_t *req);

/* _launcher_identity(): {"pidfile","create_time","alive"} ->
 * {"pid": int, "create_time": num} | null */
json_t *w30_rt_p1_launcher_identity(const json_t *req);

/* _launcher_pid(): same request -> int|null */
json_t *w30_rt_p1_launcher_pid(const json_t *req);

/* _recorded_launcher_pid(): {"pidfile": str|null} -> int|null */
json_t *w30_rt_p1_recorded_launcher_pid(const json_t *req);

/* _x_lock_pid(): {"raw": str|null} -> int|null */
json_t *w30_rt_p1_x_lock_pid(const json_t *req);

/* _x_socket_bound(): {"unix_table": str|null, "num": int} -> bool */
json_t *w30_rt_p1_x_socket_bound(const json_t *req);

/* _display_in_use(): {"lock_pid","alive","socket_bound"} -> bool */
json_t *w30_rt_p1_display_in_use(const json_t *req);

/* _pick_display(): {"recorded": str|null, "in_use": [int]} -> int,
 * or the string "!raise: no free X display number in the Bot Desktop band" */
json_t *w30_rt_p1_pick_display(const json_t *req);

/* published_env(): {"marker","sandbox_published","launcher_pid","raw"} -> obj */
json_t *w30_rt_p1_published_env(const json_t *req);

/* rfb_socket_path(): {"marker","launcher_pid","exists","state_dir"} -> str|null */
json_t *w30_rt_p1_rfb_socket_path(const json_t *req);

/* is_running(): {"env": obj} -> bool (bool(env["DISPLAY"])) */
json_t *w30_rt_p1_is_running(const json_t *req);

/* host_published_env(): {"marker", "published"} -> obj ({} while sandboxed) */
json_t *w30_rt_p1_host_published_env(const json_t *req);

/* desktop_env(): {"base","published","marker"} -> obj with the profile's
 * DISPLAY/XAUTHORITY/DBUS_SESSION_BUS_ADDRESS merged in, or the seat's
 * GUI variables removed while the screen lives in a sandbox. */
json_t *w30_rt_p1_desktop_env(const json_t *req);

/* geometry(): {"cfg": obj} -> str (default "1440x900") */
json_t *w30_rt_p1_geometry(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W30_RUNTIME_3588A2_P1_H */
