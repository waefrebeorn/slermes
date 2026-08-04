/*
 * port_startup_fast.h — C11 port of pure helpers from
 * hermes_cli/_startup_fast.py.
 *
 * Ports deterministic, I/O-free helpers from the fast startup path.
 */

#ifndef PORT_STARTUP_FAST_H
#define PORT_STARTUP_FAST_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: is_termux_env @ hermes_cli/_startup_fast.py:is_termux_env */
/* True when the process appears to be running inside Termux. */
bool sf_is_termux_env(void);

/* PoP: is_termux_fast_version_argv @ hermes_cli/_startup_fast.py:is_termux_fast_version_argv */
/* True when argv matches Termux's fast version list: --version/-V/version. */
bool sf_is_termux_fast_version_argv(int argc, char **argv);

/* PoP: is_global_fast_version_argv @ hermes_cli/_startup_fast.py:is_global_fast_version_argv */
/* True when argv matches the global fast version list: --version/-V. */
bool sf_is_global_fast_version_argv(int argc, char **argv);

/* PoP: is_container_startup_environment @ hermes_cli/_startup_fast.py:is_container_startup_environment */
/* True when INSIDE a container (fast path then safe). */
bool sf_is_container_startup_environment(void);

/* PoP: active_profile_may_override_home @ hermes_cli/_startup_fast.py:active_profile_may_override_home */
/* Does an active non-default profile redirect HERMES_HOME? */
bool sf_active_profile_may_override_home(const char *hermes_root);

/* PoP: container_mode_may_be_active @ hermes_cli/_startup_fast.py:container_mode_may_be_active */
/* Conservative probe for NixOS container-mode routing. */
bool sf_container_mode_may_be_active(void);

/* PoP: read_openai_version @ hermes_cli/_startup_fast.py:read_openai_version */
/* Read OpenAI SDK version from sys.path without importlib.metadata. */
char *sf_read_openai_version(void);

/* PoP: read_install_method @ hermes_cli/_startup_fast.py:read_install_method */
/* Read the .install_method stamp file, if present. */
char *sf_read_install_method(void);

/* PoP: _resolved_home @ hermes_cli/_startup_fast.py:_resolved_home */
/* Resolve HERMES_HOME: $HERMES_HOME or ~/.hermes. */
char *sf_resolved_home(void);

/* PoP: project_root_str @ hermes_cli/_startup_fast.py:project_root_str */
/* The project root (repo dir) as a string. */
char *sf_project_root_str(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_STARTUP_FAST_H */
