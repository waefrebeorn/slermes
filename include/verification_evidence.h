/*
 * verification_evidence.h — Coding verification evidence ledger (faithful C11
 * port of agent/verification_evidence.py).
 *
 * Passive ledger: records what the agent actually PROVED while working in a
 * code workspace. Never decides to run a suite, never blocks completion.
 *
 * The deterministic command-classification core is pure (no I/O) and unit
 * tested. The persistence layer stores evidence events as JSON in the
 * session DB (libdb). Callers supply the per-workspace "verify commands" and
 * "root" explicitly (the Python original pulled them from coding_context) so
 * the core stays pure and testable.
 */

#ifndef VERIFICATION_EVIDENCE_H
#define VERIFICATION_EVIDENCE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Max chars retained in an output summary. */
#define VERIFY_MAX_OUTPUT_SUMMARY_CHARS 2000
/* Retention window (days) for pruning old events. */
#define VERIFY_MAX_EVIDENCE_AGE_DAYS 30
#define VERIFY_MAX_EVENTS_PER_SESSION_ROOT 100
#define VERIFY_MAX_TOTAL_UNREFERENCED_EVENTS 10000
/* Prefixes that mark an agent-generated ad-hoc verification script. */
#define VERIFY_AD_HOC_PREFIX_COUNT 2
extern const char *VERIFY_AD_HOC_PREFIXES[VERIFY_AD_HOC_PREFIX_COUNT];

typedef enum {
    VERIFY_KIND_TEST,
    VERIFY_KIND_LINT,
    VERIFY_KIND_TYPECHECK,
    VERIFY_KIND_BUILD,
    VERIFY_KIND_FORMAT,
    VERIFY_KIND_CHECK,
    VERIFY_KIND_AD_HOC
} verify_kind_t;

typedef enum {
    VERIFY_SCOPE_FULL,
    VERIFY_SCOPE_TARGETED
} verify_scope_t;

typedef enum {
    VERIFY_STATUS_PASSED,
    VERIFY_STATUS_FAILED
} verify_status_t;

/* A classified command result worth recording. */
typedef struct {
    char          command[2048];
    char          canonical_command[1024];
    verify_kind_t kind;
    verify_scope_t scope;
    verify_status_t status;
    int           exit_code;
    char          cwd[1024];
    char          root[1024];
    char          session_id[256];
    char          output_summary[VERIFY_MAX_OUTPUT_SUMMARY_CHARS + 1];
    bool          valid;   /* false => not verification evidence */
} verification_evidence_t;

/* ── pure classification core ────────────────────────────────────── */

/* Split a command line into segments on && / || / ;, then shlex-tokenize each.
 * `out_segments` receives an array of `char**` token-arrays; `*out_count` and
 * per-segment lengths are returned. Caller frees with verify_free_segments. */
char ***verify_split_segments(const char *command, int *out_count, int **out_lens);
void   verify_free_segments(char ***segments, int count, int *lens);

/* Strip a leading "./" from a token. Caller frees the result. */
char  *verify_clean_token(const char *token);
/* Canonical tokens of a canonical command string (cleaned). Caller frees the
 * returned array (and each element). */
char **verify_canonical_tokens(const char *canonical, int *out_count);
/* First index where `needle` appears as a contiguous subsequence of `tokens`
 * (cleaned), or -1. */
int    verify_find_subsequence(char **tokens, int n, char **needle, int m);
/* Remove harmless command prefixes (env, VAR=, command/time/noglob). The
 * returned array aliases the input tokens (no copy); do not free it. */
char **verify_strip_command_prefix(char **tokens, int n, int *out_n);
/* Equivalent command spellings for a detected canonical (npm run X etc.).
 * Returns an array of token-arrays; `*out_count` set. Caller frees. */
char ***verify_equivalent_needles(char **needle, int m, int *out_count, int **out_lens);
/* Find (canonical, trailing_args) for the first matching canonical command.
 * Returns a malloc'd struct (caller frees) or NULL. */
typedef struct { char *canonical; char **trailing; int trailing_n; } verify_match_t;
verify_match_t *verify_find_canonical_match(const char *command,
                                            char **canonical_commands, int nc);
/* Derive the verification kind from a canonical command string. */
verify_kind_t verify_kind_for_command(const char *canonical);
/* Does an argument look like a test/file target? */
bool verify_looks_like_target(const char *arg);
/* Derive scope from trailing args. */
verify_scope_t verify_scope_for_args(char **args, int n);
/* Is token an absolute path under the system temp dir? */
bool verify_is_under_temp_dir(const char *token);
/* Is token under root (or equal)? */
bool verify_is_under_root(const char *token, const char *root);
/* Is token a temp ad-hoc verification script path (not under root)? */
bool verify_is_temp_script_path(const char *token, const char *root);
/* Trailing args if `tokens` invoke an ad-hoc verification script, else NULL. */
char **verify_ad_hoc_script_args(char **tokens, int n, const char *root, int *out_n);
/* Top-level ad-hoc match for a command, or NULL. Caller frees *out_n tokens. */
char **verify_find_ad_hoc_match(const char *command, const char *root, int *out_n);
/* Summarize command output (head/tail with omission note). Caller frees. */
char *verify_summarize_output(const char *output);

/* Classify a command. `verify_commands`/`nvc` and `root` come from the
 * workspace facts (coding_context). Fills `out`; sets out->valid=false when the
 * command is not verification evidence. `exit_code` drives passed/failed. */
void verification_classify(const char *command, const char *cwd,
                           const char *session_id, int exit_code,
                           const char *output, char **verify_commands, int nvc,
                           const char *root, verification_evidence_t *out);

/* ── persistence (libdb JSON ledger) ────────────────────────────── */

/* Record a terminal result when it is verification evidence. Returns true and
 * fills `out` (valid) on record; returns false when not evidence. */
bool verification_record_result(const char *db_dir, const char *command,
                                const char *cwd, const char *session_id,
                                int exit_code, const char *output,
                                char **verify_commands, int nvc, const char *root,
                                verification_evidence_t *out);
/* Mark verification evidence stale after a successful file edit. */
bool verification_mark_edited(const char *db_dir, const char *session_id,
                              const char *cwd, const char **paths, int npaths,
                              const char *root);
/* Return the best known verification state for a session/workspace as JSON
 * (caller frees): {"status","evidence"(json|null),"root","session_id",
 * "changed_paths"(json array)}. status ∈
 * not_applicable|unverified|passed|failed|stale. */
char *verification_status_json(const char *db_dir, const char *session_id,
                               const char *cwd, char **verify_commands, int nvc,
                               const char *root);

#ifdef __cplusplus
}
#endif

#endif /* VERIFICATION_EVIDENCE_H */
