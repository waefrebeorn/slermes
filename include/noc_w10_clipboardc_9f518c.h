#ifndef NOC_W10_CLIPBOARDC_9F518C_H
#define NOC_W10_CLIPBOARDC_9F518C_H

/* PoP: w10_clipboardc_9f518c_mime @ hermes_cli/clipboard_cmd.py:_mime
 * PoP: w10_clipboardc_9f518c_exit_codes @ hermes_cli/clipboard_cmd.py:EXIT_OK */
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Image MIME from magic bytes. `head` is a JSON array of byte values.
 * Mirrors _mime(head): RIFF/WebP special case, then _SIGNATURES in order,
 * defaulting to "application/octet-stream". */
struct json_t *w10_clipboardc_9f518c_mime(const struct json_t *head);

/* The four module exit-code constants as {"EXIT_OK":0,"EXIT_NO_IMAGE":1,
 * "EXIT_NO_TOOL":3,"EXIT_WRITE_FAILED":4}. */
struct json_t *w10_clipboardc_9f518c_exit_codes(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_CLIPBOARDC_9F518C_H */
