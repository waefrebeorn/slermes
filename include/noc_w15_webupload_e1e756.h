#ifndef NOC_W15_WEBUPLOAD_E1E756_H
#define NOC_W15_WEBUPLOAD_E1E756_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_webupload_e1e756_check @ hermes_cli/web_upload_limit.py:ChatFileUploadLimitMiddleware.__call__
 * Returns {"status":200|413}, the final HTTP status the middleware lets through.
 * file_max/image_max are the injected route body caps; chunk_total is the total
 * bytes delivered by receive() before the request completes. */
struct json_t *w15_webupload_e1e756_check(const char *path, const char *root_path,
                                          const char *scope_type, const char *method,
                                          int browser_files, const char *content_length,
                                          long long chunk_total, long long file_max,
                                          long long image_max);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_WEBUPLOAD_E1E756_H */
