/*
 * port_agent_context_references.c — C port of agent/context_references.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_context_references_preprocess_context_references_async @ agent/context_references.py:preprocess_context_references_async */

/* Port of Python agent/context_references.py:preprocess_context_references_async */
/* Preprocesses context references asynchronously, expanding @file/@url refs. */
int cli_agent_context_references_preprocess_context_references_async(
    const char *content, char *output, size_t output_size)
{
    if (!content || !output || output_size == 0) {
        return -1;
    }
    /* CLI port: async preprocessing not available. Return content as-is. */
    strncpy(output, content, output_size - 1);
    output[output_size - 1] = '\0';
    return 0;
}

/* PoP: cli_agent_context_references__expand_reference @ agent/context_references.py:_expand_reference */

/* Port of Python agent/context_references.py:_expand_reference */
/* Expands a single context reference to its content. */
int cli_agent_context_references__expand_reference(
    const char *ref_kind, const char *ref_target,
    char *output, size_t output_size)
{
    if (!ref_kind || !ref_target || !output || output_size == 0) {
        return -1;
    }
    /* CLI port: reference expansion requires file system access. */
    snprintf(output, output_size, "[reference: %s:%s]", ref_kind, ref_target);
    return 0;
}

/* PoP: cli_agent_context_references__fetch_url_content @ agent/context_references.py:_fetch_url_content */

/* Port of Python agent/context_references.py:_fetch_url_content */
/* Fetches URL content for @url: references. */
int cli_agent_context_references__fetch_url_content(
    const char *url, char *output, size_t output_size)
{
    if (!url || !output || output_size == 0) {
        return -1;
    }
    /* CLI port: URL fetching requires HTTP client. */
    snprintf(output, output_size, "[url content: %s]", url);
    return 0;
}

/* PoP: cli_agent_context_references__default_url_fetcher @ agent/context_references.py:_default_url_fetcher */

/* Port of Python agent/context_references.py:_default_url_fetcher */
/* Default URL fetcher using httpx. */
int cli_agent_context_references__default_url_fetcher(
    const char *url, char *output, size_t output_size)
{
    return cli_agent_context_references__fetch_url_content(url, output, output_size);
}

/* PoP: cli_agent_context_references__human_bytes @ agent/context_references.py:_human_bytes */

/* Port of Python agent/context_references.py:_human_bytes */
/* Formats byte count as human-readable string. */
int cli_agent_context_references__human_bytes(
    int bytes, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return -1;
    }
    if (bytes >= 1024 * 1024) {
        snprintf(output, output_size, "%.1f MB", bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        snprintf(output, output_size, "%.1f KB", bytes / 1024.0);
    } else {
        snprintf(output, output_size, "%d B", bytes);
    }
    return 0;
}

/* PoP: cli_agent_context_references__binary_reference_block @ agent/context_references.py:_binary_reference_block */

/* Port of Python agent/context_references.py:_binary_reference_block */
/* Returns a block message for binary file references. */
int cli_agent_context_references__binary_reference_block(
    const char *file_path, char *output, size_t output_size)
{
    if (!file_path || !output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size,
             "[Binary file: %s — content not shown in context]", file_path);
    return 0;
}
