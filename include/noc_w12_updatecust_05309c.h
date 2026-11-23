#ifndef NOC_W12_UPDATECUST_05309C_H
#define NOC_W12_UPDATECUST_05309C_H

#include "hermes_json.h"

/* PoP port of hermes_cli/update_custody.py (gate w12_updatecust_05309c). */

/* git_subcommand(args[]) -> string|null. */
json_t *w12_updatecust_05309c_git_subcommand(const json_t *args);
/* is_local_mutator(args[]) -> bool. */
json_t *w12_updatecust_05309c_is_local_mutator(const json_t *args);
/* git_argv(git_cmd[], args[], devnull) -> string[]. */
json_t *w12_updatecust_05309c_git_argv(const json_t *git_cmd, const json_t *args, const char *devnull);

#endif /* NOC_W12_UPDATECUST_05309C_H */
