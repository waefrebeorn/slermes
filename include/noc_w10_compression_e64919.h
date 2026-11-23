#ifndef NOC_W10_COMPRESSION_E64919_H
#define NOC_W10_COMPRESSION_E64919_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_compression_e64919_timeout_fallback_prompt @ agent/compression_facade.py:_timeout_fallback_prompt
 * Cached prompt, else a fresh build, else the raw system_message; caller frees.
 * `build_ok`/`built` are the lazy builder's seam result (built ignored when build_ok is 0). */
char *w10_compression_e64919_timeout_fallback_prompt(const char *cached, int build_ok,
                                                     const char *built, const char *system_message);

/* PoP: w10_compression_e64919_warn_commit_overrun @ agent/compression_facade.py:_warn_commit_overrun
 * The commit-ceiling warning text; caller frees. */
char *w10_compression_e64919_warn_commit_overrun(double waited, double ceiling);

/* PoP: w10_compression_e64919_compression_timeout_warning @ agent/compression_facade.py:_report_compression_timeout
 * The user-facing warning text the timeout bookkeeping emits; caller frees. */
char *w10_compression_e64919_compression_timeout_warning(double idle, double waited,
                                                         double total_ceiling, double since_progress,
                                                         int total_exhausted, int progress_observed);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_COMPRESSION_E64919_H */
