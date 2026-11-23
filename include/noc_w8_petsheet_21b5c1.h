#ifndef NOC_W8_PETSHEET_21B5C1_H
#define NOC_W8_PETSHEET_21B5C1_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_petsheet_21b5c1_sheet_revision @ tui_gateway/pet_sheet.py:_pet_sheet_revision
 * mtime_ns/size are 64-bit decimal strings (use strtoll).  fail=1 stands in for
 * spritesheet.stat() raising, in which case the revision is "0:0". */
struct json_t *w8_petsheet_21b5c1_sheet_revision(const char *mtime_ns,
                                                 const char *size, int fail);

/* PoP: w8_petsheet_21b5c1_clone_payload @ tui_gateway/pet_sheet.py:_clone_pet_payload
 * Shallow-clone a cached pet payload.  At the JSON value level a shallow clone
 * equals the original dict; the three typed keys are still copied explicitly.
 * Caller frees. */
struct json_t *w8_petsheet_21b5c1_clone_payload(const struct json_t *payload);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_PETSHEET_21B5C1_H */
