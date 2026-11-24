/*
 * noc_w20_worktree_959719.h -- C11 port of the ``hermes worktree`` subcommand
 * parser spec from upstream hermes_cli/subcommands/worktree.py.
 */
#ifndef NOC_W20_WORKTREE_959719_H
#define NOC_W20_WORKTREE_959719_H

#include "hermes_json.h"

/*
 * The attached ``worktree`` parser as argparse sees it:
 *   {"root_help", "root_description", "dest", "subcommands":[
 *      {"name","aliases":[...],"help","options":[
 *         {"flags":[...],"dest","action","nargs","default","type","metavar","help"}]}]}
 */
json_t *w20_worktree_959719_spec(void);

/* argparse aliases set ``dest`` to the literal typed string ("ls"/"audit"). */
const char *w20_worktree_959719_canonical_action(const char *action);

#endif /* NOC_W20_WORKTREE_959719_H */
