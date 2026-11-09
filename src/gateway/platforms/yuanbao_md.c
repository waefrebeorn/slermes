/*
 * yuanbao_md.c - yuanbao markdown chunking / sanitising helpers.
 *
 * Split from yuanbao.c (soul.md: self-contained modules). Fence / table-atom
 * detection, paragraph-boundary splitting and table sanitising.
 * No file-scope state.
 */
#include "yuanbao_md.h"
#include "websocket.h"
#include "protobuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include "libhash/hash.h"
#include "libhttp/http.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



/* ================================================================
 *  MarkdownProcessor — Pure string algorithms ported from
 *  gateway/platforms/yuanbao.py class MarkdownProcessor.
 *  All methods are static/class methods with no async, no network.
 * ================================================================ */

/* Check whether text has an unclosed code fence (odd number of ``` lines). */
/* PoP: yuanbao_md_has_unclosed_fence @ gateway/platforms/yuanbao.py:has_unclosed_fence */
bool yuanbao_md_has_unclosed_fence(const char *text)
{
    if (!text) return false;
    bool in_fence = false;
    const char *p = text;
    while (*p) {
        /* Find start of line */
        const char *line = p;
        const char *eol = strchr(p, '\n');
        size_t line_len = eol ? (size_t)(eol - p) : strlen(p);
        /* Check if line starts with ``` */
        if (line_len >= 3 && strncmp(line, "```", 3) == 0) {
            in_fence = !in_fence;
        }
        if (!eol) break;
        p = eol + 1;
    }
    return in_fence;
}


/* Check if the last non-empty line starts and ends with | (table row). */
/* PoP: yuanbao_md_ends_with_table_row @ gateway/platforms/yuanbao.py:ends_with_table_row */
bool yuanbao_md_ends_with_table_row(const char *text)
{
    if (!text || !*text) return false;
    /* rstrip: find last non-whitespace */
    size_t len = strlen(text);
    while (len > 0 && (text[len-1] == ' ' || text[len-1] == '\t' || text[len-1] == '\r' || text[len-1] == '\n'))
        len--;
    if (len == 0) return false;
    /* Find last line within the trimmed range */
    const char *last_line_start = text + len - 1;
    while (last_line_start > text && last_line_start[-1] != '\n') last_line_start--;
    /* strip leading whitespace of last line */
    const char *s = last_line_start;
    while (s < text + len && (*s == ' ' || *s == '\t')) s++;
    size_t s_len = (text + len) - s;
    if (s_len < 2) return false;
    /* strip trailing whitespace of last line */
    while (s_len > 0 && (s[s_len-1] == ' ' || s[s_len-1] == '\t' || s[s_len-1] == '\r')) s_len--;
    if (s_len < 2) return false;
    return s[0] == '|' && s[s_len-1] == '|';
}


/* Split text at nearest paragraph boundary within max_chars.
   Returns head (caller-free) and sets *tail_out (caller-free). */
/* PoP: yuanbao_md_split_at_paragraph_boundary @ gateway/platforms/yuanbao.py:split_at_paragraph_boundary */
char *yuanbao_md_split_at_paragraph_boundary(const char *text, size_t max_chars, char **tail_out)
{
    if (!text || !tail_out) return NULL;
    *tail_out = NULL;
    size_t total = strlen(text);
    if ((int)total <= max_chars) {
        char *head = strdup(text);
        return head;
    }
    int window_len = max_chars < (int)total ? max_chars : (int)total;
    const char *window_end = text + window_len;

    /* 1. Prefer last blank line (\n\n) in window */
    const char *pos = NULL;
    for (const char *q = text; q + 1 < window_end; q++) {
        if (q[0] == '\n' && q[1] == '\n') pos = q;
    }
    if (pos && pos > text) {
        size_t head_len = (size_t)(pos - text) + 2;
        char *head = malloc(head_len + 1);
        memcpy(head, text, head_len);
        head[head_len] = '\0';
        *tail_out = strdup(pos + 2);
        return head;
    }

    /* 2. Last sentence-ending punctuation followed by newline: 。！？.!? */
    const char *best_pos = NULL;
    for (const char *q = text; q < window_end; q++) {
        char c = *q;
        if ((c == '.' || c == '!' || c == '?' || c == '\xe3') && q + 1 < window_end && q[1] == '\n') {
            /* Check for Chinese punctuation: 0xE3 0x80 0x82/0x90/0x9A (。？！) */
            if (c == '\xe3' && q + 2 < window_end) {
                unsigned char c2 = (unsigned char)q[1];
                unsigned char c3 = (unsigned char)q[2];
                if (c2 == '\x80' && (c3 == '\x82' || c3 == '\x90' || c3 == '\x9A')) {
                    best_pos = q + 3; /* after the 3-byte CJK char */
                }
            } else {
                best_pos = q + 1; /* after the ASCII punct */
            }
        }
    }
    if (best_pos && best_pos > text) {
        size_t head_len = (size_t)(best_pos - text);
        char *head = malloc(head_len + 1);
        memcpy(head, text, head_len);
        head[head_len] = '\0';
        *tail_out = strdup(best_pos);
        return head;
    }

    /* 3. Fallback: last newline in window */
    pos = NULL;
    for (const char *q = text; q < window_end; q++) {
        if (*q == '\n') pos = q;
    }
    if (pos && pos > text) {
        size_t head_len = (size_t)(pos - text) + 1;
        char *head = malloc(head_len + 1);
        memcpy(head, text, head_len);
        head[head_len] = '\0';
        *tail_out = strdup(pos + 1);
        return head;
    }

    /* 4. Force split at window boundary */
    char *head = malloc((size_t)window_len + 1);
    memcpy(head, text, window_len);
    head[window_len] = '\0';
    *tail_out = strdup(text + window_len);
    return head;
}


/* Check if text (atom) is a code fence block (starts with ```). */
/* PoP: yuanbao_md_is_fence_atom @ gateway/platforms/yuanbao.py:is_fence_atom */
bool yuanbao_md_is_fence_atom(const char *text)
{
    if (!text) return false;
    while (*text == ' ' || *text == '\t') text++;
    return strncmp(text, "```", 3) == 0;
}


/* Check if text (atom) is a table (first line starts and ends with |). */
/* PoP: yuanbao_md_is_table_atom @ gateway/platforms/yuanbao.py:is_table_atom */
bool yuanbao_md_is_table_atom(const char *text)
{
    if (!text) return false;
    const char *eol = strchr(text, '\n');
    size_t first_len = eol ? (size_t)(eol - text) : strlen(text);
    /* strip leading whitespace */
    const char *s = text;
    while (s < text + first_len && (*s == ' ' || *s == '\t')) s++;
    size_t s_len = (text + first_len) - s;
    /* strip trailing whitespace */
    while (s_len > 0 && (s[s_len-1] == ' ' || s[s_len-1] == '\t' || s[s_len-1] == '\r')) s_len--;
    if (s_len < 2) return false;
    return s[0] == '|' && s[s_len-1] == '|';
}


/* Split text into atoms (code fences, tables, paragraphs separated by blank lines).
   Returns a dynamically-allocated NULL-terminated array of strings. Caller frees each + array. */
/* PoP: yuanbao_md_split_into_atoms @ gateway/platforms/yuanbao.py:split_into_atoms */
char **yuanbao_md_split_into_atoms(const char *text)
{
    if (!text) {
        char **arr = calloc(1, sizeof(char*));
        return arr;
    }
    size_t cap = 16, count = 0;
    char **atoms = calloc(cap, sizeof(char*));

    /* current_lines buffer */
    char *buf = NULL;
    size_t buf_len = 0, buf_cap = 0;
    bool in_fence = false;

    #define YB_FLUSH() do { \
        if (buf_len > 0) { \
            /* Check non-whitespace */ \
            bool has_content = false; \
            for (size_t _i = 0; _i < buf_len; _i++) if (!isspace((unsigned char)buf[_i])) { has_content = true; break; } \
            if (has_content) { \
                if (count >= cap - 1) { cap *= 2; atoms = realloc(atoms, cap * sizeof(char*)); } \
                /* Python joins lines with '\n' and the atom itself has NO trailing \
                 * newline, so strip the trailing '\n' that YB_APPEND_LINE added. */ \
                size_t out_len = buf_len; \
                if (out_len > 0 && buf[out_len - 1] == '\n') out_len--; \
                atoms[count++] = strndup(buf, out_len); \
            } \
            buf_len = 0; \
        } \
    } while(0)

    #define YB_APPEND_LINE(start, len) do { \
        if (buf_len + (len) + 1 > buf_cap) { \
            buf_cap = (buf_len + (len) + 1) * 2; if (buf_cap < 256) buf_cap = 256; \
            buf = realloc(buf, buf_cap); \
        } \
        if (len > 0) memcpy(buf + buf_len, start, len); \
        buf_len += len; \
        buf[buf_len++] = '\n'; \
    } while(0)

    const char *p = text;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t ll = eol ? (size_t)(eol - p) : strlen(p);

        /* helper: is table line */
        bool is_tbl = false;
        {
            const char *s = p; size_t sl = ll;
            while (sl > 0 && (*s == ' ' || *s == '\t')) { s++; sl--; }
            while (sl > 0 && (s[sl-1] == ' ' || s[sl-1] == '\t' || s[sl-1] == '\r')) sl--;
            is_tbl = (sl >= 2 && s[0] == '|' && s[sl-1] == '|');
        }

        if (in_fence) {
            YB_APPEND_LINE(p, ll);
            if (ll >= 3 && strncmp(p, "```", 3) == 0 && buf_len > ll + 1) {
                in_fence = false;
                YB_FLUSH();
            }
        } else if (ll >= 3 && strncmp(p, "```", 3) == 0) {
            YB_FLUSH();
            in_fence = true;
            YB_APPEND_LINE(p, ll);
        } else if (is_tbl) {
            /* If previous line in buf was not table, flush */
            if (buf_len > 0) {
                /* Find last line in buf */
                size_t last_start = buf_len - 1;
                while (last_start > 0 && buf[last_start - 1] != '\n') last_start--;
                size_t last_real_len = buf_len - 1 - last_start;
                const char *ls = buf + last_start;
                while (last_real_len > 0 && (*ls == ' ' || *ls == '\t')) { ls++; last_real_len--; }
                while (last_real_len > 0 && (ls[last_real_len-1] == ' ' || ls[last_real_len-1] == '\t' || ls[last_real_len-1] == '\r')) last_real_len--;
                bool last_was_tbl = (last_real_len >= 2 && ls[0] == '|' && ls[last_real_len-1] == '|');
                if (!last_was_tbl) YB_FLUSH();
            }
            YB_APPEND_LINE(p, ll);
        } else {
            /* blank? */
            bool blank = true;
            for (size_t i = 0; i < ll; i++) if (!isspace((unsigned char)p[i])) { blank = false; break; }
            if (blank) {
                YB_FLUSH();
            } else {
                if (buf_len > 0) {
                    /* If previous was table, flush */
                    size_t last_start = buf_len - 1;
                    while (last_start > 0 && buf[last_start - 1] != '\n') last_start--;
                    size_t last_real_len = buf_len - 1 - last_start;
                    const char *ls = buf + last_start;
                    while (last_real_len > 0 && (*ls == ' ' || *ls == '\t')) { ls++; last_real_len--; }
                    while (last_real_len > 0 && (ls[last_real_len-1] == ' ' || ls[last_real_len-1] == '\t' || ls[last_real_len-1] == '\r')) last_real_len--;
                    bool last_was_tbl = (last_real_len >= 2 && ls[0] == '|' && ls[last_real_len-1] == '|');
                    if (last_was_tbl) YB_FLUSH();
                }
                YB_APPEND_LINE(p, ll);
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    YB_FLUSH();
    free(buf);

    atoms[count] = NULL;
    return atoms;

    #undef YB_FLUSH
    #undef YB_APPEND_LINE
}


/* Infer separator between two chunks: "\n" if fence/table continuation, else "\n\n". */
/* PoP: yuanbao_md_infer_block_separator @ gateway/platforms/yuanbao.py:infer_block_separator */
char *yuanbao_md_infer_block_separator(const char *prev_chunk, const char *next_chunk)
{
    if (!prev_chunk || !next_chunk) return strdup("\n\n");
    /* rstrip prev */
    size_t plen = strlen(prev_chunk);
    while (plen > 0 && isspace((unsigned char)prev_chunk[plen-1])) plen--;
    /* lstrip next */
    const char *ns = next_chunk;
    while (*ns == ' ' || *ns == '\t' || *ns == '\n') ns++;
    /* prev ends with ``` or next starts with ``` */
    if (plen >= 3 && strncmp(prev_chunk + plen - 3, "```", 3) == 0) return strdup("\n");
    if (strncmp(ns, "```", 3) == 0) return strdup("\n");
    /* table continuation */
    if (yuanbao_md_ends_with_table_row(prev_chunk)) {
        const char *eol = strchr(ns, '\n');
        size_t fl_len = eol ? (size_t)(eol - ns) : strlen(ns);
        const char *fs = ns;
        while (fs < ns + fl_len && (*fs == ' ' || *fs == '\t')) fs++;
        size_t fs_len = (ns + fl_len) - fs;
        while (fs_len > 0 && (fs[fs_len-1] == ' ' || fs[fs_len-1] == '\t' || fs[fs_len-1] == '\r')) fs_len--;
        if (fs_len >= 2 && fs[0] == '|' && fs[fs_len-1] == '|') return strdup("\n");
    }
    return strdup("\n\n");
}


/* Strip outer markdown fence when entire text is ```markdown\n...\n``` */
/* PoP: yuanbao_md_strip_outer_markdown_fence @ gateway/platforms/yuanbao.py:strip_outer_markdown_fence */
char *yuanbao_md_strip_outer_markdown_fence(const char *text)
{
    if (!text || !*text) return text ? strdup(text) : NULL;
    /* Count lines */
    int nlines = 1;
    for (const char *p = text; *p; p++) if (*p == '\n') nlines++;
    if (nlines < 3) return strdup(text);

    /* Find first and last line */
    const char *first = text;
    const char *first_end = strchr(first, '\n');
    if (!first_end) return strdup(text);
    size_t first_len = first_end - first;
    /* rstrip first line */
    while (first_len > 0 && (first[first_len-1] == ' ' || first[first_len-1] == '\t' || first[first_len-1] == '\r')) first_len--;

    /* Check first line matches ^```(?:markdown|md)?\s*$ (case-insensitive) */
    if (first_len < 3 || strncmp(first, "```", 3) != 0) return strdup(text);
    /* Remaining optional tag */
    const char *tag = first + 3;
    size_t tag_len = first_len - 3;
    /* skip trailing whitespace already stripped */
    bool match_md = false;
    if (tag_len == 0) {
        match_md = true; /* plain ``` */
    } else {
        /* case-insensitive match "markdown" or "md" */
        if (tag_len == 2 && (tag[0] == 'm' || tag[0] == 'M') && (tag[1] == 'd' || tag[1] == 'D'))
            match_md = true;
        else if (tag_len == 8 && strncasecmp(tag, "markdown", 8) == 0)
            match_md = true;
    }
    if (!match_md) return strdup(text);

    /* Find last line */
    const char *p = text + strlen(text);
    while (p > text && (p[-1] == '\n' || p[-1] == '\r' || p[-1] == ' ' || p[-1] == '\t')) p--;
    /* p points to end of last non-empty content */
    const char *last_start = p;
    while (last_start > text && last_start[-1] != '\n') last_start--;
    size_t last_len = p - last_start;
    /* strip whitespace of last line */
    const char *ls = last_start;
    while (ls < last_start + last_len && (*ls == ' ' || *ls == '\t')) ls++;
    size_t ls_len = (last_start + last_len) - ls;
    while (ls_len > 0 && (ls[ls_len-1] == ' ' || ls[ls_len-1] == '\t' || ls[ls_len-1] == '\r')) ls_len--;
    if (ls_len != 3 || strncmp(ls, "```", 3) != 0) return strdup(text);

    /* Strip first and last lines, return inner content */
    const char *inner_start = first_end + 1;
    size_t inner_len = last_start > inner_start ? (size_t)(last_start - inner_start) : 0;
    /* Remove trailing newline before last_start if present */
    if (inner_len > 0 && inner_start[inner_len - 1] == '\n') inner_len--;
    char *result = malloc(inner_len + 1);
    memcpy(result, inner_start, inner_len);
    result[inner_len] = '\0';
    return result;
}


/* Sanitize markdown table: normalize separator rows, remove empty table rows. */
/* PoP: yuanbao_md_sanitize_markdown_table @ gateway/platforms/yuanbao.py:sanitize_markdown_table */
char *yuanbao_md_sanitize_markdown_table(const char *text)
{
    if (!text) return NULL;
    if (!strchr(text, '|')) return strdup(text);

    /* Build result line by line */
    size_t cap = strlen(text) + 256;
    char *result = malloc(cap);
    size_t rlen = 0;

    const char *p = text;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t ll = eol ? (size_t)(eol - p) : strlen(p);
        /* strip the line */
        const char *ls = p;
        while (ls < p + ll && (*ls == ' ' || *ls == '\t')) ls++;
        size_t ls_len = (p + ll) - ls;
        while (ls_len > 0 && (ls[ls_len-1] == ' ' || ls[ls_len-1] == '\t' || ls[ls_len-1] == '\r')) ls_len--;

        bool is_table_row = (ls_len >= 2 && ls[0] == '|' && ls[ls_len-1] == '|');

        if (is_table_row) {
            /* Check if separator row: ^|[\s\-:]+(\|[\s\-:]+)+\|$ */
            bool is_sep = true;
            if (ls_len < 2) is_sep = false;
            /* Must contain only -, :, space, | */
            for (size_t i = 0; i < ls_len && is_sep; i++) {
                char c = ls[i];
                if (c != '-' && c != ':' && c != ' ' && c != '\t' && c != '|') is_sep = false;
            }
            if (is_sep) {
                /* Normalize: split by |, strip cells, rejoin, preserving the
                 * leading/trailing pipe (Python's stripped.split('|') keeps the
                 * empty edge cells, so the result is |cell|cell|...|). */
                char *scopy = strndup(ls, ls_len);
                char *norm = malloc(ls_len + 1);
                size_t nlen = 0;
                norm[nlen++] = '|';  /* leading pipe */
                char *save = NULL;
                char *tok = strtok_r(scopy, "|", &save);
                bool first = true;
                while (tok) {
                    char *ts = tok; while (*ts == ' ' || *ts == '\t') ts++;
                    char *te = ts + strlen(ts);
                    while (te > ts && (te[-1] == ' ' || te[-1] == '\t')) te--;
                    if (!first) norm[nlen++] = '|';
                    memcpy(norm + nlen, ts, te - ts); nlen += te - ts;
                    first = false;
                    tok = strtok_r(NULL, "|", &save);
                }
                norm[nlen++] = '|';  /* trailing pipe */
                norm[nlen] = '\0';
                free(scopy);
                /* Append normalized line */
                if (rlen + nlen + 1 > cap) { cap = (rlen + nlen + 1) * 2; result = realloc(result, cap); }
                memcpy(result + rlen, norm, nlen); rlen += nlen;
                free(norm);
            } else if (ls_len <= 2 || ls_len == 2) {
                /* Empty table row || → skip */
                /* Check if all non-| chars are whitespace */
                bool only_pipe_ws = true;
                for (size_t i = 0; i < ls_len; i++) {
                    if (ls[i] != '|' && !isspace((unsigned char)ls[i])) { only_pipe_ws = false; break; }
                }
                if (only_pipe_ws) {
                    /* skip empty table row */
                } else {
                    if (rlen + ls_len + 1 > cap) { cap = (rlen + ls_len + 1) * 2; result = realloc(result, cap); }
                    memcpy(result + rlen, ls, ls_len); rlen += ls_len;
                }
            } else {
                /* Non-empty, non-separator: append stripped */
                if (rlen + ls_len + 1 > cap) { cap = (rlen + ls_len + 1) * 2; result = realloc(result, cap); }
                memcpy(result + rlen, ls, ls_len); rlen += ls_len;
            }
        } else {
            /* Non-table line: append as-is (original, not stripped) */
            if (rlen + ll + 1 > cap) { cap = (rlen + ll + 1) * 2; result = realloc(result, cap); }
            memcpy(result + rlen, p, ll); rlen += ll;
        }
        if (eol) {
            if (rlen + 1 > cap) { cap = (rlen + 1) * 2; result = realloc(result, cap); }
            result[rlen++] = '\n';
            p = eol + 1;
        } else {
            break;
        }
    }
    result[rlen] = '\0';
    return result;
}


/* Return the markdown hint system prompt string (static). */
/* PoP: yuanbao_md_markdown_hint_system_prompt @ gateway/platforms/yuanbao.py:markdown_hint_system_prompt */
const char *yuanbao_md_markdown_hint_system_prompt(void)
{
    return "The current platform supports Markdown rendering. You can use the following formats:\n"
           "- Code blocks: ```language\\ncode\\n```\n"
           "- Tables: | col1 | col2 |\\n|---|---|\\n| val1 | val2 |\n"
           "- Bold: **text** / Italic: *text*\n"
           "Please use Markdown formatting when appropriate to improve readability.";
}


/* Merge adjacent chunks when current has unclosed fence and next starts with ```.
   Returns merged list (NULL-terminated array, caller frees each + array). */
/* PoP: yuanbao_md_merge_block_streaming_fences @ gateway/platforms/yuanbao.py:merge_block_streaming_fences */
char **yuanbao_md_merge_block_streaming_fences(char **chunks)
{
    if (!chunks || !chunks[0]) {
        char **arr = calloc(1, sizeof(char*));
        return arr;
    }
    size_t cap = 16, count = 0;
    char **result = calloc(cap, sizeof(char*));
    size_t i = 0;
    while (chunks[i]) {
        char *current = strdup(chunks[i]);
        while (yuanbao_md_has_unclosed_fence(current) && chunks[i + 1]) {
            char *sep = yuanbao_md_infer_block_separator(current, chunks[i + 1]);
            size_t clen = strlen(current);
            size_t slen = strlen(sep);
            size_t nlen = strlen(chunks[i + 1]);
            char *merged = malloc(clen + slen + nlen + 1);
            memcpy(merged, current, clen);
            memcpy(merged + clen, sep, slen);
            memcpy(merged + clen + slen, chunks[i + 1], nlen);
            merged[clen + slen + nlen] = '\0';
            free(current);
            free(sep);
            current = merged;
            i++;
        }
        if (count >= cap - 1) { cap *= 2; result = realloc(result, cap * sizeof(char*)); }
        result[count++] = current;
        i++;
    }
    result[count] = NULL;
    return result;
}


/* Split markdown text into chunks each <= max_chars.
   Guarantees: fences not split, tables not split, split at paragraph boundaries,
   small chunks merged. Returns NULL-terminated array, caller frees. */
/* PoP: yuanbao_md_chunk_markdown_text @ gateway/platforms/yuanbao.py:chunk_markdown_text */
char **yuanbao_md_chunk_markdown_text(const char *text, size_t max_chars)
{
    if (!text || !*text) {
        char **arr = calloc(1, sizeof(char*));
        return arr;
    }
    if ((int)strlen(text) <= max_chars) {
        char **arr = calloc(2, sizeof(char*));
        arr[0] = strdup(text);
        return arr;
    }

    /* Phase 1: Extract atomic blocks */
    char **atoms = yuanbao_md_split_into_atoms(text);
    size_t atom_count = 0;
    while (atoms[atom_count]) atom_count++;

    /* Phase 2: Greedy merge */
    char **chunks = calloc(atom_count + 1, sizeof(char*));
    size_t n_chunks = 0;
    bool *indivisible = calloc(atom_count + 1, sizeof(bool));

    char *cur_parts = NULL;
    size_t cur_parts_len = 0, cur_parts_cap = 0;
    int cur_len = 0;

    #define YB_FLUSH_PARTS() do { \
        if (cur_parts_len > 0) { \
            chunks[n_chunks++] = strndup(cur_parts, cur_parts_len); \
            cur_parts_len = 0; \
            cur_len = 0; \
        } \
    } while(0)

    for (size_t a = 0; a < atom_count; a++) {
        const char *atom = atoms[a];
        size_t atom_len_sz = strlen(atom);
        int atom_len = (int)atom_len_sz;
        int sep_len = cur_parts_len > 0 ? 2 : 0;
        int projected = cur_len + sep_len + atom_len;

        if (projected > max_chars && cur_parts_len > 0) {
            YB_FLUSH_PARTS();
            sep_len = 0;
        }

        if (cur_parts_len == 0 && atom_len > max_chars &&
            (yuanbao_md_is_fence_atom(atom) || yuanbao_md_is_table_atom(atom))) {
            indivisible[n_chunks] = true;
            chunks[n_chunks++] = strdup(atom);
            continue;
        }

        if (cur_parts_len + sep_len + atom_len_sz + 1 > cur_parts_cap) {
            cur_parts_cap = (cur_parts_len + sep_len + atom_len_sz + 1) * 2;
            if (cur_parts_cap < 256) cur_parts_cap = 256;
            cur_parts = realloc(cur_parts, cur_parts_cap);
        }
        if (sep_len > 0) {
            cur_parts[cur_parts_len++] = '\n';
            cur_parts[cur_parts_len++] = '\n';
        }
        memcpy(cur_parts + cur_parts_len, atom, atom_len_sz);
        cur_parts_len += atom_len_sz;
        cur_len += sep_len + atom_len;
    }
    YB_FLUSH_PARTS();
    free(cur_parts);

    #undef YB_FLUSH_PARTS

    /* Phase 3: Post-processing — split still-oversized chunks at paragraph boundaries */
    char **result = calloc(n_chunks + 1, sizeof(char*));
    size_t n_result = 0;
    for (size_t idx = 0; idx < n_chunks; idx++) {
        char *chunk = chunks[idx];
        if ((int)strlen(chunk) <= max_chars) {
            result[n_result++] = chunk;
            continue;
        }
        if (indivisible[idx]) {
            result[n_result++] = chunk;
            continue;
        }
        if (yuanbao_md_has_unclosed_fence(chunk)) {
            result[n_result++] = chunk;
            continue;
        }
        char *remaining = chunk;
        while ((int)strlen(remaining) > max_chars) {
            char *tail = NULL;
            char *head = yuanbao_md_split_at_paragraph_boundary(remaining, max_chars, &tail);
            if (head && head[0]) {
                result[n_result++] = head;
            } else {
                free(head);
                /* Force split at max_chars */
                head = strndup(remaining, max_chars);
                result[n_result++] = head;
                if (tail) free(tail);
                tail = strdup(remaining + max_chars);
            }
            if (remaining != chunk) free(remaining);
            remaining = tail;
        }
        if (remaining && remaining[0]) {
            result[n_result++] = remaining;
        } else {
            if (remaining && remaining != chunk) free(remaining);
        }
    }
    result[n_result] = NULL;
    free(chunks);
    free(indivisible);
    /* Free atoms */
    for (size_t a = 0; a < atom_count; a++) free(atoms[a]);
    free(atoms);

    /* Phase 4: Merge small trailing/leading chunks with neighbours */
    if (n_result > 1) {
        char **merged = calloc(n_result + 1, sizeof(char*));
        size_t n_merged = 1;
        merged[0] = result[0];
        for (size_t i = 1; i < n_result; i++) {
            char *prev = merged[n_merged - 1];
            size_t plen = strlen(prev);
            size_t clen = strlen(result[i]);
            size_t combined_len = plen + 2 + clen;
            if ((int)combined_len <= max_chars) {
                char *combined = malloc(combined_len + 1);
                memcpy(combined, prev, plen);
                combined[plen] = '\n';
                combined[plen + 1] = '\n';
                memcpy(combined + plen + 2, result[i], clen);
                combined[combined_len] = '\0';
                free(prev);
                merged[n_merged - 1] = combined;
                free(result[i]);
            } else {
                merged[n_merged++] = result[i];
            }
        }
        merged[n_merged] = NULL;
        free(result);
        result = merged;
    }

    /* Filter NULLs and return */
    return result;
}
