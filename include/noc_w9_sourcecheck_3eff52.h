#ifndef NOC_W9_SOURCECHECK_3EFF52_H
#define NOC_W9_SOURCECHECK_3EFF52_H

#ifdef __cplusplus
extern "C" {
#endif

struct json_t;

/* PoP: w9_sourcecheck_3eff52_is_full_sha @ hermes_cli/source_check.py:_is_full_sha */
int w9_sourcecheck_3eff52_is_full_sha(const struct json_t *value);
/* PoP: w9_sourcecheck_3eff52_configured_branch @ hermes_cli/source_check.py:_configured_branch */
struct json_t *w9_sourcecheck_3eff52_configured_branch(const struct json_t *desktop_config);
/* PoP: w9_sourcecheck_3eff52_checked_out_branch @ hermes_cli/source_check.py:_checked_out_branch */
struct json_t *w9_sourcecheck_3eff52_checked_out_branch(const struct json_t *current_branch,
                                                       const struct json_t *fallback);
/* PoP: w9_sourcecheck_3eff52_branch_remote @ hermes_cli/source_check.py:_branch_remote */
struct json_t *w9_sourcecheck_3eff52_branch_remote(int embedded, const char *repository,
                                                   const char *origin, const char *selected_branch);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_SOURCECHECK_3EFF52_H */
