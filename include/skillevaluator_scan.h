/*
 * skillevaluator_scan.h — faithful C11 port of tools/skillevaluator_scan.py
 * (advisory NVIDIA SkillEvaluator Tier 1 scan for skill installs).
 *
 * PoP (Python -> C):
 *   SECRETS_CLASS_CHECKS        -> skillevaluator_scan_is_secrets_class
 *   Tier1Finding.is_secrets_class -> skillevaluator_scan_finding_is_secrets_class
 *   Tier1Finding.location       -> skillevaluator_scan_finding_location
 *   _parse_report               -> skillevaluator_scan_parse_report
 *   format_tier1_report         -> skillevaluator_scan_format_tier1_report
 *   scanner_available           -> skillevaluator_scanner_available
 *   tier1_advisory_enabled      -> skillevaluator_tier1_advisory_enabled
 *   run_tier1_scan              -> skillevaluator_run_tier1_scan
 *   tier1_advisory_enabled      -> skillevaluator_tier1_advisory_enabled
 *
 * Side effects (PATH lookup, subprocess execution, config load) route through
 * injectable function pointers so hosts can substitute fakes; the defaults do
 * the real thing (which(3)-style PATH search, fork/execvp of the scanner).
 */
#ifndef HERMES_SKILLEVALUATOR_SCAN_H
#define HERMES_SKILLEVALUATOR_SCAN_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SKILLEVALUATOR_SCANNER_BIN     "skillevaluator"
#define SKILLEVALUATOR_SCANNER_NAME    "skillevaluator-tier1"
#define SKILLEVALUATOR_TIER1_CHECKS    "pii,unicode,lint,license,security"
#define SKILLEVALUATOR_SCAN_TIMEOUT_SECONDS 120

/* One finding from the SkillEvaluator report (Tier1Finding). */
typedef struct {
    char *check;       /* e.g. "emails", "database_credentials" */
    char *validator;   /* e.g. "PII Scan" */
    char *severity;    /* "critical"|"high"|"medium"|"low"|"info" */
    char *message;
    char *file;
    long line;
    char *suggestion;
} skillevaluator_scan_finding_t;

/* Reduced report (Tier1Report). */
typedef struct {
    bool available;      /* scanner ran and produced a report */
    bool passed;
    skillevaluator_scan_finding_t *findings;
    size_t n_findings;
    char **incomplete_checks;
    size_t n_incomplete_checks;
    char *error;         /* why the scan is unavailable (debug only) */
} skillevaluator_scan_report_t;

/* Injectable hooks (NULL => real default behavior). */
typedef bool (*skillevaluator_avail_fn)(void);
/* Runs the scanner over skill_dir; must create skilleveloper-output-*.json
 * files under outdir. Returns 0 on launched-and-finished (regardless of the
 * scanner's own exit code, mirroring Python ignoring returncode), nonzero on
 * launch failure (OSError analog). *timed_out is set for TimeoutExpired. */
typedef int (*skillevaluator_run_fn)(const char *scanner_bin,
                                     const char *skill_dir,
                                     const char *checks,
                                     const char *outdir,
                                     int timeout,
                                     bool *timed_out,
                                     char **launch_err);

extern skillevaluator_avail_fn skillevaluator_scan_avail_hook;
extern skillevaluator_run_fn  skillevaluator_scan_run_hook;

/* SECRETS_CLASS_CHECKS membership. */
bool skillevaluator_scan_is_secrets_class(const char *check);
/* Tier1Finding.is_secrets_class. */
bool skillevaluator_scan_finding_is_secrets_class(const skillevaluator_scan_finding_t *f);
/* Tier1Finding.location() -> malloc'd string ("<file>:<line>" | file | "?"). */
char *skillevaluator_scan_finding_location(const skillevaluator_scan_finding_t *f);

/* Advisory/secrets splits. */
size_t skillevaluator_scan_advisory_findings(const skillevaluator_scan_report_t *r,
                                             skillevaluator_scan_finding_t **out);
size_t skillevaluator_scan_secrets_findings(const skillevaluator_scan_report_t *r,
                                            skillevaluator_scan_finding_t **out);

/* _parse_report(report_dict) -> malloc'd report. */
skillevaluator_scan_report_t *skillevaluator_scan_parse_report(const json_t *report);
/* format_tier1_report(report, limit) -> malloc'd plain-text summary ("" when
 * unavailable). */
char *skillevaluator_scan_format_tier1_report(const skillevaluator_scan_report_t *r,
                                              int limit);
void skillevaluator_scan_report_free(skillevaluator_scan_report_t *r);

/* scanner_available(): PATH search for SCANNER_BIN (injectable). */
bool skillevaluator_scanner_available(void);
bool skillevaluator_tier1_advisory_enabled(void);
/* run_tier1_scan(skill_dir, timeout): full pipeline — availability gate,
 * temp output dir, scanner invocation, newest-report glob, JSON parse,
 * _parse_report reduction. Returns available=false report on any failure. */
skillevaluator_scan_report_t *skillevaluator_run_tier1_scan(const char *skill_dir,
                                                            int timeout);

#ifdef __cplusplus
}
#endif
#endif /* HERMES_SKILLEVALUATOR_SCAN_H */
