#ifndef NOC_W19_SESSIONS_9ADADC_H
#define NOC_W19_SESSIONS_9ADADC_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_sessions_9adadc_spec @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 * Whole parser tree: {"dest","parent":[arg],"subs":{name:{"args":[arg],"children":null}}}.
 * arg = {opts,dest,action,nargs,default,required,type,metavar,choices}. */
struct json_t *w19_sessions_9adadc_spec(void);

/* PoP: w19_sessions_9adadc_parent_args @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 * Arguments attached to the ``sessions`` parser itself (help + dests). */
struct json_t *w19_sessions_9adadc_parent_args(void);

/* PoP: w19_sessions_9adadc_sub_names @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 * Subcommand names in registration order. */
struct json_t *w19_sessions_9adadc_sub_names(void);

/* PoP: w19_sessions_9adadc_args @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 * Ordered argument descriptors for one subcommand, or json_null() when unknown. */
struct json_t *w19_sessions_9adadc_args(const char *sub);

/* PoP: w19_sessions_9adadc_arg @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 * The descriptor whose option strings contain `flag` (argparse lookup), else json_null(). */
struct json_t *w19_sessions_9adadc_arg(const char *sub, const char *flag);

/* PoP: w19_sessions_9adadc_arg_count @ hermes_cli/subcommands/sessions.py:build_sessions_parser */
struct json_t *w19_sessions_9adadc_arg_count(const char *sub);

/* PoP: w19_sessions_9adadc_dest @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 * dest of the action subparser ("sessions_action"). */
const char *w19_sessions_9adadc_dest(void);

/* PoP: w19_sessions_9adadc_has_sub @ hermes_cli/subcommands/sessions.py:build_sessions_parser */
int w19_sessions_9adadc_has_sub(const char *name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_SESSIONS_9ADADC_H */
