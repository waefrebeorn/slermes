#ifndef NOC_W20_ADAPTERIMA_CFF594_H
#define NOC_W20_ADAPTERIMA_CFF594_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/slack/adapter_image_batch.py (gate w20_adapterima_cff594).
 *
 * `collect_image_uploads` is ported whole: the file:// path (percent-decoded,
 * basename only, missing file skipped but its alt text already claimed), the
 * SSRF refusal, the download/limit failure, the content-type -> extension table
 * with its png default, and the 1-based-by-len() upload naming.
 *
 * Seams the fixture states: the filesystem (`os.path.exists`), `is_safe_url_fn`,
 * the HTTP client's answer (content-type + body, or the failure point).
 *
 * Request:
 *   chunk: [[url, alt], ...], exists: [path,...], unsafe: [url,...],
 *   downloads: {url: {"ok":true,"content_type":"...","content":"..."} |
 *                    {"ok":false}}
 * Answer:
 *   {"status":"ok","value":{"file_uploads":[...],"initial_comment_parts":[...],
 *                           "refused":[[url, alt], ...]}}
 */
json_t *w20_adapterima_cff594_collect(const json_t *request);

/* PoP: w20_adapterima_cff594_ext_for @ plugins/platforms/slack/adapter_image_batch.py:_IMAGE_CT_EXTS
 * First content-type substring wins (jpeg/jpg/gif/webp), otherwise "png". */
const char *w20_adapterima_cff594_ext_for(const char *content_type);

/* PoP: w20_adapterima_cff594_unquote @ plugins/platforms/slack/adapter_image_batch.py:collect_image_uploads
 * `unquote_fn(image_url[7:])`: percent-decoded bytes, UTF-8 passthrough. */
char *w20_adapterima_cff594_unquote(const char *s);

/* PoP: w20_adapterima_cff594_basename @ plugins/platforms/slack/adapter_image_batch.py:collect_image_uploads
 * `os.path.basename(local_path)` as ntpath does it (both separators, drive aware). */
char *w20_adapterima_cff594_basename(const char *path);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_ADAPTERIMA_CFF594_H */
