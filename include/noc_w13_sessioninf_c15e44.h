#ifndef NOC_W13_SESSIONINF_C15E44_H
#define NOC_W13_SESSIONINF_C15E44_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w13_sessioninf_c15e44_owner_lines @ gateway/session_info_reads.py:owner_lines
 * Joined lines ("\n") with only leading/trailing newlines stripped, as an
 * {"type":"exec","output":...} reply. `lines` is a JSON array of strings. */
struct json_t *w13_sessioninf_c15e44_owner_lines(const struct json_t *lines);
/* PoP: w13_sessioninf_c15e44_run_info_read @ gateway/session_info_reads.py:run_info_read
 * Renders the "toolsets" or "platforms" info read for a local session.
 * `policy` (bool) is a fixture seam: no frozen route means no toolsets. */
struct json_t *w13_sessioninf_c15e44_run_info_read(const struct json_t *name,
                                                    const struct json_t *toolsets,
                                                    const struct json_t *policy);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_SESSIONINF_C15E44_H */
