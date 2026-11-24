#ifndef NOC_W21_VERIFY_824376_H
#define NOC_W21_VERIFY_824376_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/subcommands/verify.py (gate w21_verify_824376).
 *
 * `build_verify_parser` is a declaration, not a compute: the whole artifact is
 * the argparse surface it attaches. Both halves are ported:
 *   the SPEC  -- name/help/description, every argument in order with its
 *                nargs/default/type/action/choices/help, and the handler
 *                default the parser binds;
 *   the PARSE -- what that surface accepts, so the spec is proven executable
 *                rather than merely transcribed (argparse's own answers are
 *                the oracle).
 */

/* PoP: w21_verify_824376_spec @ hermes_cli/subcommands/verify.py:build_verify_parser
 * {"name","help","description","arguments":[{"name","flags","nargs","default",
 *  "type","action","choices","help"}],"defaults":{"func":"cmd_verify"}} */
json_t *w21_verify_824376_spec(void);

/* PoP: w21_verify_824376_parse @ hermes_cli/subcommands/verify.py:build_verify_parser
 * argparse's result for `hermes verify <argv>`:
 *   {"status":"ok","value":{path,detect_only,save,skip_start,phase,port,
 *                            timeout,ready_timeout,json}}
 * or {"status":"error","code":2} for an unrecognised option, a bad choice,
 * a bad int/float, or an extra positional (argparse exits 2). */
json_t *w21_verify_824376_parse(const json_t *argv);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_VERIFY_824376_H */
