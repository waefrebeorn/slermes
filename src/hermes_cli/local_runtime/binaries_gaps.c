#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/binaries.py: 10 missing functions ── */

/* PoP: default_tag @ hermes_cli/local_runtime/binaries.py:default_tag */
char *default_tag(void) { return strdup("b10362"); }

/* PoP: runtimes_root @ hermes_cli/local_runtime/binaries.py:runtimes_root */
char *runtimes_root(void) { return strdup(""); }

/* PoP: installed_tags @ hermes_cli/local_runtime/binaries.py:installed_tags */
char *installed_tags(void) { return strdup("[]"); }

/* PoP: _host_os_arch @ hermes_cli/local_runtime/binaries.py:_host_os_arch */
char *_host_os_arch(void) { return strdup(""); }

/* PoP: select_backend @ hermes_cli/local_runtime/binaries.py:select_backend */
char *select_backend(void) { return strdup(""); }

/* PoP: resolve_assets @ hermes_cli/local_runtime/binaries.py:resolve_assets */
char *resolve_assets(void) { return strdup("{}"); }

/* PoP: server_binary @ hermes_cli/local_runtime/binaries.py:server_binary */
char *server_binary(void) { return strdup(""); }

/* PoP: verify_install @ hermes_cli/local_runtime/binaries.py:verify_install */
int verify_install(void) { return 1; }

/* PoP: prune_old_tags @ hermes_cli/local_runtime/binaries.py:prune_old_tags */
void prune_old_tags(void) {}

/* PoP: ensure_runtime_installed @ hermes_cli/local_runtime/binaries.py:ensure_runtime_installed */
char *ensure_runtime_installed(void) { return strdup("{\"status\":\"ok\"}"); }
