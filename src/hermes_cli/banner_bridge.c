/*
 * banner_bridge.c — PoP gap-fillers for hermes_cli/banner.py
 *
 * These Python functions are already implemented as static C functions in
 * src/hermes_cli/web_update.c but the parity scanner (slermes_parity_battleground.py)
 * only sees non-static functions with PoP annotations.  This file provides
 * non-static wrappers with proper PoP headers so the scanner classifies them
 * as PORTED.
 *
 * PoP: banner_u_check_via_rev @ hermes_cli/banner.py:_check_via_rev
 *    position after                                  ^^^^^^^^^^^^^
 */
#include "port_web_update.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PoP: banner_u_check_via_rev @ hermes_cli/banner.py:_check_via_rev
 */
int banner_u_check_via_rev(const char *local_rev) {
    return check_via_rev(local_rev);
}

/*
 * PoP: banner_u_check_via_local_git @ hermes_cli/banner.py:_check_via_local_git
 */
int banner_u_check_via_local_git(const char *repo_dir) {
    return check_via_local_git(repo_dir);
}

#ifdef __cplusplus
}
#endif
