/*
 * platforms_helpers_md.h — public API for the pure markdown-chunking core of
 * gateway/platforms/helpers.py (the shared fence-aware chunker extracted from
 * the yuanbao MarkdownProcessor). Opaque, minimal includes.
 */

#ifndef PLATFORMS_HELPERS_MD_H
#define PLATFORMS_HELPERS_MD_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: text_has_unclosed_fence @ gateway/platforms/helpers.py:text_has_unclosed_fence */
bool helpers_md_text_has_unclosed_fence(const char *text);

/* PoP: text_ends_with_table_row @ gateway/platforms/helpers.py:text_ends_with_table_row */
bool helpers_md_text_ends_with_table_row(const char *text);

/* PoP: is_fence_atom @ gateway/platforms/helpers.py:is_fence_atom */
bool helpers_md_is_fence_atom(const char *text);

/* PoP: is_table_atom @ gateway/platforms/helpers.py:is_table_atom */
bool helpers_md_is_table_atom(const char *text);

/* PoP: split_at_paragraph_boundary @ gateway/platforms/helpers.py:split_at_paragraph_boundary */
/* Returns malloc'd head; sets *tail_out to malloc'd tail. head+tail == text. */
char *helpers_md_split_at_paragraph_boundary(const char *text, size_t max_chars,
                                             char **tail_out);

/* PoP: split_markdown_atoms @ gateway/platforms/helpers.py:split_markdown_atoms */
/* Returns NULL-terminated array of malloc'd atoms; caller frees via
 * helpers_md_free_strlist. */
char **helpers_md_split_markdown_atoms(const char *text);

/* PoP: infer_block_separator @ gateway/platforms/helpers.py:infer_block_separator */
/* Returns "\n" or "\n\n" (static strings; do not free). */
const char *helpers_md_infer_block_separator(const char *prev_chunk,
                                             const char *next_chunk);

/* PoP: merge_streaming_fences @ gateway/platforms/helpers.py:merge_streaming_fences */
/* Merges chunks truncated mid-fence. Returns NULL-terminated malloc'd list;
 * caller frees via helpers_md_free_strlist. */
char **helpers_md_merge_streaming_fences(char **chunks);

/* PoP: balance_fences_across_chunks @ gateway/platforms/helpers.py:balance_fences_across_chunks */
/* Closes orphaned ``` at chunk boundaries and reopens on the next. Returns
 * NULL-terminated malloc'd list. */
char **helpers_md_balance_fences_across_chunks(char **chunks);

/* PoP: greedy_pack_blocks @ gateway/platforms/helpers.py:greedy_pack_blocks */
/* Greedily packs blocks joined by sep into chunks <= max_length. Returns
 * NULL-terminated malloc'd list. */
char **helpers_md_greedy_pack_blocks(char **blocks, size_t max_length,
                                     const char *sep);

/* PoP: split_text_fence_aware @ gateway/platforms/helpers.py:split_text_fence_aware */
/* Split markdown text into chunks of at most limit, fence-aware. */
char **helpers_md_split_text_fence_aware(const char *text, size_t limit,
                                         int prefer_paragraphs,
                                         int balance_fences);

/* PoP: _chunk_markdown_paragraphs @ gateway/platforms/helpers.py:_chunk_markdown_paragraphs */
/* Paragraph/atom chunking pipeline (yuanbao-derived). */
char **helpers_md_chunk_markdown_paragraphs(const char *text, size_t max_chars);

/* PoP: _chunk_newline_preferred @ gateway/platforms/helpers.py:_chunk_newline_preferred */
/* len_fn: NULL means strlen (codepoint count). */
char **helpers_md_chunk_newline_preferred(const char *text, size_t limit,
                                          size_t (*len_fn)(const char *));

/* Free a NULL-terminated strlist (each string + the array). */
void helpers_md_free_strlist(char **list);

/* Internal: push a strdup'd copy onto a NULL-terminated strlist. */
char **helpers_md_strlist_push(char **list, const char *s);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORMS_HELPERS_MD_H */
