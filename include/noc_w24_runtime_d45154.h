/*
 * noc_w24_runtime_d45154.h - C surface for upstream
 * hermes_platform/host/runtime.py (gate w24_runtime_d45154).
 *
 * The cached runtime-environment predicates of WSL / container / Termux,
 * ported as the pure functions underneath them: an environment mapping or a
 * /proc text goes in, a bool comes out.  Opening /proc, reading the process
 * environment and the memo globals stay in the caller -- that is the seam.
 */
#ifndef NOC_W24_RUNTIME_D45154_H
#define NOC_W24_RUNTIME_D45154_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W24_RUNTIME_D45154_CGROUP_MARKERS 6
#define W24_RUNTIME_D45154_ROOT_MARKERS 3

/* env: {"TERMUX_VERSION": <any>, "PREFIX": <any>} -> bool.
 * bool(TERMUX_VERSION or "com.termux/files/usr" in PREFIX). */
json_t *w24_runtime_d45154_termux(const json_t *env);

/* proc: {"proc_version": <text|null>} -> bool.
 * "microsoft" in text.lower(); an unreadable file (null) is False. */
json_t *w24_runtime_d45154_wsl(const json_t *proc);

/* proc: {"content": <text|null>, "markers": [...]} -> bool.
 * any(marker in content): _proc_file_has_marker over an already-read file. */
json_t *w24_runtime_d45154_proc_marker(const json_t *proc);

/* proc: {"content": <text|null>, "markers": [...]} -> bool.
 * _root_mount_has_marker: only mountinfo lines whose 5th field is "/" count. */
json_t *w24_runtime_d45154_root_mount(const json_t *proc);

/* env: {dockerenv, containerenv, k8s_host, cgroup, mountinfo} -> bool.
 * The _detect_container() decision with its four file/env probes supplied as
 * values: the two container files' existence flags, the Kubernetes env
 * variable, the /proc/1/cgroup text and the /proc/self/mountinfo text. */
json_t *w24_runtime_d45154_container(const json_t *env);

#ifdef __cplusplus
}
#endif
#endif
