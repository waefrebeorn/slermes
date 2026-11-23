#ifndef NOC_W12_APPROVALFE_C05A66_H
#define NOC_W12_APPROVALFE_C05A66_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_approvalfe_c05a66_base @ tools/approval_fetch_exec.py:_base
 * path.rstrip("/").rsplit("/", 1)[-1]; caller frees. */
char *w12_approvalfe_c05a66_base(const char *path);

/* PoP: w12_approvalfe_c05a66_fetch_targets @ tools/approval_fetch_exec.py:DownloadThenRunMatcher.search
 * Step 1 of search(): for each curl/wget fetch segment collect target basename ->
 * end offset via _FETCH_RE/_SEGMENT_END_RE/_TARGET_RE/_base.  Caller frees. */
struct json_t *w12_approvalfe_c05a66_fetch_targets(const char *command);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_APPROVALFE_C05A66_H */
