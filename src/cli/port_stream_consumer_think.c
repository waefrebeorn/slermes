/*
 * port_stream_consumer_think.c — pure think-block primitives.
 *
 * Mirrors gateway/stream_consumer_think.py StreamThinkFilterMixin:
 *   _at_block_boundary(self, buf, idx)
 *   _earliest_open_tag(self, buf, lower_buf)
 * The live methods read self._accumulated and self._OPEN_THINK_TAGS; here the
 * accumulated prefix is the parameter and the tag table is a fixed constant
 * matching agent/think_scrubber.py THINK_OPEN_TAGS.
 */

#include "port_stream_consumer_think.h"

#include <string.h>

/* THINK_OPEN_TAGS from agent/think_scrubber.py (UTF-8). First is a literal
 * leading space (" thinking") with no brackets. */
static const char *const SCT_OPEN_TAGS[] = {
    " thinking",
    "<thinking>",
    "<reasoning>",
    "<thought>",
    "<reasoning_scratchpad>",
    "<\xe6\x80\x9d\xe8\x80\x83>",   /* <思考> */
    "<\xe5\x8f\x8d\xe6\x80\x9d>",   /* <反思> */
    "<\xe6\x8e\xa8\xe7\x90\x86>",   /* <推理> */
    "<\xe6\x8e\xa8\xe6\x95\xb2>",   /* <推敲> */
};
/* Python's len(tag) is a CHARACTER count (used to advance the buffer), which for
 * the CJK tags is smaller than the UTF-8 byte length. Keep the parallel char
 * length explicitly. */
static const int SCT_OPEN_TAG_CLENS[] = {
    9, 10, 11, 9, 22, 4, 4, 4, 4,
};
#define SCT_OPEN_TAGS_N (sizeof(SCT_OPEN_TAGS) / sizeof(SCT_OPEN_TAGS[0]))

static int is_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

/* _at_block_boundary: acc_boundary = (not accumulated) or accumulated endswith
 * "\n"; idx 0 -> acc_boundary; else looking at the text after the last newline
 * before idx must be all-whitespace. */
int sct_at_block_boundary(const char *accumulated, const char *buf, int idx)
{
    int acc_boundary;
    const char *preceding;
    const char *last_nl;
    if (!buf) return 0;
    if (idx < 0) idx = 0;

    acc_boundary = (!accumulated || !accumulated[0]) ||
                   (strlen(accumulated) >= 1 &&
                    accumulated[strlen(accumulated) - 1] == '\n');
    if (idx == 0)
        return acc_boundary ? 1 : 0;

    preceding = buf;
    last_nl = NULL;
    {
        int i;
        for (i = 0; i < idx && preceding[i]; i++)
            if (preceding[i] == '\n') last_nl = preceding + i;
    }
    if (!last_nl) {
        /* no newline before idx -> preceding must be all whitespace */
        const unsigned char *p = (const unsigned char *)preceding;
        int k;
        for (k = 0; k < idx && preceding[k]; k++)
            if (!is_space(p[k])) return 0;
        return acc_boundary ? 1 : 0;
    }
    {
        const unsigned char *p = (const unsigned char *)(last_nl + 1);
        int k;
        for (k = 0; k < idx - (int)(last_nl - preceding) - 1 && p[k]; k++)
            if (!is_space(p[k])) return 0;
    }
    return 1;
}

/* PoP: _earliest_open_tag @ gateway/stream_consumer_think.py:_earliest_open_tag */
int sct_earliest_open_tag(const char *accumulated, const char *buf,
                          const char *lower_buf, int *out_idx, int *out_len)
{
    int best_idx = -1, best_len = 0;
    size_t t;
    if (!buf || !lower_buf) {
        if (out_idx) *out_idx = -1;
        if (out_len) *out_len = 0;
        return 0;
    }
    for (t = 0; t < SCT_OPEN_TAGS_N; t++) {
        const char *needle = SCT_OPEN_TAGS[t];
        size_t nlen = strlen(needle);
        int search_start = 0;
        while (1) {
            const char *hit = strstr(lower_buf + search_start, needle);
            int idx;
            if (!hit) break;
            idx = (int)(hit - lower_buf);
            if (sct_at_block_boundary(accumulated, buf, idx)) {
                if (best_idx == -1 || idx < best_idx) {
                    best_idx = idx;
                    best_len = SCT_OPEN_TAG_CLENS[t];
                }
                break; /* first boundary hit for this tag is enough */
            }
            search_start = idx + 1;
        }
    }
    if (out_idx) *out_idx = best_idx;
    if (out_len) *out_len = best_len;
    return best_idx >= 0 ? 1 : 0;
}

/* PoP: _at_block_boundary @ gateway/stream_consumer_think.py:_at_block_boundary */
/* (defined above as sct_at_block_boundary) */
