#ifndef NOC_W12_CLICRONLI_42411F_H
#define NOC_W12_CLICRONLI_42411F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_clicronli_42411f_normalize_skills @ hermes_cli/cli_cron_lines.py:_normalize_skills */
struct json_t *w12_clicronli_42411f_normalize_skills(const struct json_t *values);

/* PoP: w12_clicronli_42411f_parse_cron_flags @ hermes_cli/cli_cron_lines.py:_parse_cron_flags */
struct json_t *w12_clicronli_42411f_parse_cron_flags(const struct json_t *tokens);

/* PoP: w12_clicronli_42411f_add_lines @ hermes_cli/cli_cron_lines.py:_add_lines
 * _t is stubbed to identity and the _cron create result is injected. */
struct json_t *w12_clicronli_42411f_add_lines(const char *subcommand, const struct json_t *opts,
                                              const struct json_t *result);

/* PoP: w12_clicronli_42411f_job_action_lines @ hermes_cli/cli_cron_lines.py:_job_action_lines
 * _t is stubbed to identity and the _cron action result is injected. */
struct json_t *w12_clicronli_42411f_job_action_lines(const char *subcommand, const struct json_t *opts,
                                                     const struct json_t *result);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_CLICRONLI_42411F_H */
