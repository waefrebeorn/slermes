/*
 * read_extract.h — minimal declaration surface for the deterministic
 * document-type helpers ported from tools/read_extract.py in
 * src/tools/port_tools_read_extract.c. Opaque / minimal: no god-header.
 */

#ifndef SLERMES_READ_EXTRACT_H
#define SLERMES_READ_EXTRACT_H

#include <stddef.h>
#include <stdbool.h>

/* Port of tools/read_extract.py:_extension. Returns the lowercased suffix if
 * it is in EXTRACTABLE_EXTENSIONS, else "". (Returned pointer is into a static
 * table — do not free.) */
const char *read_extract_extension(const char *path);

/* Port of tools/read_extract.py:is_extractable_document. */
bool read_extract_is_extractable_document(const char *path);

#endif /* SLERMES_READ_EXTRACT_H */
