#ifndef NOC_W21_DESKTOPUPD_4B3EB1_H
#define NOC_W21_DESKTOPUPD_4B3EB1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/desktop_update_verify.py (gate w21_desktopupd_4b3eb1):
 * read-only verification at the Windows Desktop hand-off receipt boundary.
 *
 * The verification itself is file bytes and regexes, so the byte readers are
 * split out where a caller can see each verdict: the ASAR Pickle header's
 * arithmetic, an entry's path rules, its byte range, and the renderer entry
 * scan. The archive's contents are the filesystem seam: `verify` takes the
 * answers a read produced (an entry error, an integrity error) rather than
 * reading them twice. */

/* PoP: w21_desktopupd_4b3eb1_root_to_verify @ hermes_cli/desktop_update_verify.py:_root_to_verify
 * The root the receipt may describe: `checkout_root()` when no root was
 * supplied, the caller's root when it actually carries a packaged Desktop app,
 * else the checkout this module was imported from (a pre-fix hand-off still
 * passes `Path.cwd()`, which is HERMES_HOME and never holds one). Caller
 * frees. */
char *w21_desktopupd_4b3eb1_root_to_verify(const char *project_root, const char *checkout_root,
                                            int has_packaged);

/* PoP: w21_desktopupd_4b3eb1_asar_header_check @ hermes_cli/desktop_update_verify.py:_verify_packaged_entry
 * The four `struct.unpack("<4I")` fields and the archive's size, as decimal
 * strings:
 *   size == 4, header_size == payload_size + 4,
 *   payload_size == 4 + ((json_size + 3) // 4) * 4,
 *   0 < json_size <= 64 MiB, 8 + header_size <= file_size.
 * Returns "" when the header parses, else "invalid ASAR header". Caller frees. */
char *w21_desktopupd_4b3eb1_asar_header_check(const char *size, const char *header_size,
                                               const char *payload_size, const char *json_size,
                                               const char *file_size);

/* PoP: w21_desktopupd_4b3eb1_asar_entry_path_check @ hermes_cli/desktop_update_verify.py:_verify_packaged_entry
 * `read_member`'s path rule: a name is refused when it is empty, absolute,
 * carries a `..` part, or contains a backslash or a colon.
 * Returns "" or "invalid ASAR entry path". Caller frees. */
char *w21_desktopupd_4b3eb1_asar_entry_path_check(const char *name);

/* PoP: w21_desktopupd_4b3eb1_asar_entry_check @ hermes_cli/desktop_update_verify.py:_verify_packaged_entry
 * The rest of `read_member` for one entry, with the bytes it read summarised:
 *   length  -- `node["size"]`, as a decimal string
 *   header_size, offset, file_size -- the archive numbers, decimal strings
 *   unpacked -- `node.get("unpacked")` truthiness (skips the range rule)
 *   read_len -- how many bytes the read returned, decimal string
 *   data_blank -- the bytes stripped to nothing
 * Returns "" or one of "empty ASAR entry: <name>", "truncated ASAR entry:
 * <name>", "incomplete ASAR entry: <name>". Caller frees. */
char *w21_desktopupd_4b3eb1_asar_entry_check(const char *name, const char *length,
                                              const char *header_size, const char *offset,
                                              int unpacked, const char *file_size,
                                              const char *read_len, int data_blank);

/* PoP: w21_desktopupd_4b3eb1_renderer_has_module_entry @ hermes_cli/desktop_update_verify.py:_verify_packaged_entry
 * `any(_MODULE_TAG.search(tag) and tag.lower().startswith("<script") and not
 * re.match(r"^[a-z]+:|^//", url, re.IGNORECASE))` over every
 * `<script|link ... src|href="..." ...>` tag: a local module entry exists. */
int w21_desktopupd_4b3eb1_renderer_has_module_entry(const char *html);

/* PoP: w21_desktopupd_4b3eb1_verify @ hermes_cli/desktop_update_verify.py:verify_windows_desktop_update
 * The raise ladder, fed the answers the byte reads produced:
 *   {"project_root": <str|null>, "checkout_root": <str>,
 *    "packaged": {<root>: <executable str|null>, ...},
 *    "integrity_error": <str>, "entry_error": <str>, "build_needed": <bool>}
 * -> {"status":"ok"} | {"status":"error","type":"RuntimeError","message":...} */
json_t *w21_desktopupd_4b3eb1_verify(const json_t *state);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_DESKTOPUPD_4B3EB1_H */
