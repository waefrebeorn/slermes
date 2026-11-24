#ifndef NOC_W22_EGRESS_C95264_H
#define NOC_W22_EGRESS_C95264_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/subcommands/egress.py (gate w22_egress_c95264).
 *
 *   - spec(): the parser identity build_egress_parser registers
 *     ({"name","help","description"}); argparse itself is the CLI seam.
 *   - dispatch(): `_dispatch_egress` reduced to its branch rule.
 */

/* PoP: w22_egress_c95264_spec @ hermes_cli/subcommands/egress.py:build_egress_parser */
json_t *w22_egress_c95264_spec(void);

/* PoP: w22_egress_c95264_dispatch @ hermes_cli/subcommands/egress.py:_dispatch_egress
 * {"action":"call"|"help","help":bool}: a present egress subcommand with a
 * handler other than the dispatcher itself calls through; anything else
 * prints the egress help and returns 0. */
json_t *w22_egress_c95264_dispatch(int has_sub, int has_func, int func_is_self);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_EGRESS_C95264_H */
