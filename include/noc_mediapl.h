#ifndef NOC_MEDIAPL_H
#define NOC_MEDIAPL_H
struct json_t;
/* flags: JSON array (or NULL) of bool/null; media_urls: JSON array.
 * Returns JSON array of bool/null (caller frees). */
struct json_t *mediapl_pad_inline_flags(struct json_t *flags, struct json_t *media_urls);
#endif
