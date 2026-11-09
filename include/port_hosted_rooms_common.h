/*
 * port_hosted_rooms_common.h -- pure helpers ported from
 * gateway/hosted_rooms_common.py.
 *
 * The sqlite / clock half stays in Python.
 */
#ifndef PORT_HOSTED_ROOMS_COMMON_H
#define PORT_HOSTED_ROOMS_COMMON_H

#include <stddef.h>

#include "../lib/libjson/json.h"

/* hosted_rooms_common.py:identifier -- strip and validate a bounded string.
 * `kind` carries the Python type: 0 = not a str, 2 = str. `use_pattern` 0
 * selects pattern=None (skip the shape check). Returns 0 on success, -1 on the
 * equivalent of the Python raise; *out receives the stripped value and *err
 * the exact message (both malloc'd, caller frees). */
int hrc_identifier(int kind, const char *value, const char *label,
                   int max_chars, int use_pattern,
                   const char *invalid_msg, char **out, char **err);

/* hosted_rooms_common.py:exact_fields -- require exactly the required keys
 * plus any optional ones. Returns 0 or -1; *err receives the exact message. */
int hrc_exact_fields(const char *const *keys, int nkeys,
                     const char *const *required, int nrequired,
                     const char *const *optional, int noptional,
                     const char *label, const char *not_object,
                     char **err);

/* hosted_rooms_common.py:text -- non-blank string bounded by max_bytes of
 * UTF-8. The blank check runs on the UNSTRIPPED value, the size check on the
 * stripped one. `strip` 0 keeps and measures the raw text. */
int hrc_text(int kind, const char *value, const char *label,
             int max_bytes, int strip, char **out, char **err);

/* hosted_rooms_common.py:compact_json -- sorted-key, separator-free JSON
 * (the digest/storage canonical form). Caller frees. */
char *hrc_compact_json(const json_t *value);

/* hosted_rooms_common.py:utf8_len -- len("".join(parts).encode("utf-8")). */
size_t hrc_utf8_len(const char *const *parts, int nparts);

#endif /* PORT_HOSTED_ROOMS_COMMON_H */
