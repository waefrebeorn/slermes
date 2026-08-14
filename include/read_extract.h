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

/* PoP: _group_ranges @ tools/read_extract.py:_group_ranges */
/* Group sorted 1-based page numbers into [start, end] runs. Fills *out_runs
 * (pairs) up to *out_count (in/out capacity); returns actual run count. */
int read_extract_group_ranges(const int *pages, int n_pages,
                              int *out_runs, int *out_count);

/* PoP: _page_ranges @ tools/read_extract.py:_page_ranges */
/* Compact 1-based range list, e.g. "2-29, 33-35, 42". Caller frees. */
char *read_extract_page_ranges(const int *pages, int n_pages);

/* PoP: _gap_map @ tools/read_extract.py:_gap_map */
/* Per-gap breakdown: each empty range labeled with the last text seen before
 * it. Returns malloc'd multi-line string. Caller frees. */
char *read_extract_gap_map(const int *counts, int n_counts,
                           const char *const *texts, const int *empty, int n_empty);

/* PoP: _human_size @ tools/read_extract.py:_human_size */
/* e.g. round(12345/1024) KB else N B. Caller frees. */
char *read_extract_human_size(long long n_bytes);

/* PoP: _base64_bytes @ tools/read_extract.py:_base64_bytes */
/* Approximate decoded size of a base64 payload (whitespace ignored). */
long long read_extract_base64_bytes(const char *payload);

/* PoP: _clean_stream_text @ tools/read_extract.py:_clean_stream_text */
/* Strip ANSI escapes and collapse \r progress-bar rewrites. Caller frees. */
char *read_extract_clean_stream_text(const char *text);

/* PoP: _notebook_output_text @ tools/read_extract.py:_notebook_output_text */
/* Render one notebook output as compact text. Takes output as a JSON object
 * string (or "" for none). Caller frees. */
char *read_extract_notebook_output_text(const char *output_json);

/* PoP: _notebook_outputs @ tools/read_extract.py:_notebook_outputs */
/* Render a cell's outputs array as joined text, tail-truncated at
 * _MAX_OUTPUT_CHARS. Takes the cell JSON object string. Caller frees. */
char *read_extract_notebook_outputs(const char *cell_json,
                                    const char *jq_pointer,
                                    const char *filename);

#endif /* SLERMES_READ_EXTRACT_H */
