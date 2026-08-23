/*
 * textwrap.c — C text wrapping library (J11: Python textwrap port).
 *
 * Implements text wrapping, filling, dedent, and shorten for CLI output.
 */

#define _POSIX_C_SOURCE 200809L
#include "textwrap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* ================================================================
 *  Helper: tokenizer for word-wrapping
 * ================================================================ */

/* States for the word tokenizer */
typedef enum {
    WS_SPACE,   /* in whitespace */
    WS_WORD,    /* in a word */
    WS_NEWLINE  /* just saw \n */
} word_state_t;

/* A single word token */
typedef struct {
    char *text;       /* malloc'd word text */
    int   width;      /* display width of this word */
    bool  is_newline; /* true if this token is a forced break */
} word_t;

#define MAX_WORDS 4096

/* Tokenize text into words. Returns word count. Each word.text is malloc'd. */
static int tokenize(const char *text, word_t *words, int max_words) {
    int count = 0;
    const char *p = text;
    word_state_t state = WS_SPACE;
    const char *start = NULL;
    int word_len = 0;

    while (*p && count < max_words) {
        if (state == WS_NEWLINE) {
            state = WS_SPACE;
        }

        if (*p == '\n') {
            /* Flush current word if any */
            if (start) {
                words[count].text = strndup(start, word_len);
                words[count].width = word_len;
                words[count].is_newline = false;
                count++;
                start = NULL;
                word_len = 0;
            }
            /* Emit newline token */
            words[count].text = strdup("\n");
            words[count].width = 0;
            words[count].is_newline = true;
            count++;
            state = WS_SPACE; /* reset for next token */
            p++;
            continue;
        }

        if (isspace((unsigned char)*p)) {
            if (state == WS_WORD) {
                /* End of word */
                words[count].text = strndup(start, word_len);
                words[count].width = word_len;
                words[count].is_newline = false;
                count++;
                start = NULL;
                word_len = 0;
            }
            state = WS_SPACE;
            p++;
        } else {
            if (state == WS_SPACE) {
                start = p;
                word_len = 0;
            }
            state = WS_WORD;
            word_len++;
            p++;
        }
    }

    /* Flush last word */
    if (start && word_len > 0 && count < max_words) {
        words[count].text = strndup(start, word_len);
        words[count].width = word_len;
        words[count].is_newline = false;
        count++;
    }

    return count;
}

/* ================================================================
 *  Public API
 * ================================================================ */

textwrap_result_t textwrap_wrap(const char *text, int width) {
    textwrap_result_t result = {{0}, 0};

    if (!text || !text[0] || width <= 0) {
        if (text && text[0]) {
            result.lines[0] = strdup(text);
            result.count = 1;
        }
        return result;
    }

    word_t words[MAX_WORDS];
    int word_count = tokenize(text, words, MAX_WORDS);
    if (word_count == 0) return result;

    /* Word-wrap: build lines greedily */
    char line_buf[TEXTWRAP_MAX_LINE];
    int line_pos = 0;

    for (int i = 0; i < word_count; i++) {
        if (words[i].is_newline) {
            /* Flush current line */
            if (result.count < TEXTWRAP_MAX_LINES) {
                line_buf[line_pos] = '\0';
                result.lines[result.count] = strdup(line_buf);
                result.count++;
            }
            line_pos = 0;
            free(words[i].text);
            continue;
        }

        int w = words[i].width;

        /* Check if word fits on current line */
        if (line_pos == 0) {
            /* First word on line — always add */
            if (line_pos + w < TEXTWRAP_MAX_LINE) {
                memcpy(line_buf + line_pos, words[i].text, w);
                line_pos += w;
            }
        } else {
            /* Need space + word */
            if (line_pos + 1 + w <= width && line_pos + 1 + w < TEXTWRAP_MAX_LINE) {
                line_buf[line_pos] = ' ';
                memcpy(line_buf + line_pos + 1, words[i].text, w);
                line_pos += 1 + w;
            } else {
                /* Flush and start new line */
                if (result.count < TEXTWRAP_MAX_LINES) {
                    line_buf[line_pos] = '\0';
                    result.lines[result.count] = strdup(line_buf);
                    result.count++;
                }
                line_pos = 0;
                if (line_pos + w < TEXTWRAP_MAX_LINE) {
                    memcpy(line_buf + line_pos, words[i].text, w);
                    line_pos += w;
                }
            }
        }

        free(words[i].text);
    }

    /* Flush last line */
    if (line_pos > 0 && result.count < TEXTWRAP_MAX_LINES) {
        line_buf[line_pos] = '\0';
        result.lines[result.count] = strdup(line_buf);
        result.count++;
    }

    return result;
}

char *textwrap_fill(const char *text, int width) {
    textwrap_result_t wrapped = textwrap_wrap(text, width);
    if (wrapped.count == 0) {
        textwrap_result_t r = textwrap_wrap("", width);
        if (r.count > 0) { free(r.lines[0]); }
        return strdup("");
    }

    /* Calculate total length */
    size_t total = 0;
    for (int i = 0; i < wrapped.count; i++) {
        total += strlen(wrapped.lines[i]) + 1; /* +1 for \n or \0 */
    }

    char *result = NULL;
    if (total == 0) {
        result = strdup("");
    } else {
        result = (char *)malloc(total);
        if (!result) {
            for (int i = 0; i < wrapped.count; i++) free(wrapped.lines[i]);
            return NULL;
        }
        result[0] = '\0';
        for (int i = 0; i < wrapped.count; i++) {
            if (i > 0) strcat(result, "\n");
            strcat(result, wrapped.lines[i]);
            free(wrapped.lines[i]);
        }
    }

    return result;
}

char *textwrap_dedent(const char *text) {
    if (!text || !text[0]) return strdup("");

    /* Find common leading whitespace across all non-empty lines */
    int min_indent = -1;
    const char *p = text;
    const char *line_start = p;

    while (*p) {
        if (*p == '\n') {
            /* Measure indent of the line that just ended, skip blank lines */
            const char *end = p;
            const char *cp = line_start;
            int indent = 0;
            while (cp < end && isspace((unsigned char)*cp) && *cp != '\n') {
                indent++;
                cp++;
            }
            /* Only update min_indent for non-blank lines */
            if (cp < end && *cp != '\n') {
                if (min_indent < 0 || indent < min_indent)
                    min_indent = indent;
            }
            p++;
            line_start = p;
        } else {
            p++;
        }
    }

    /* Handle last line (no trailing \n), skip blank lines */
    if (line_start < p) {
        const char *cp = line_start;
        int indent = 0;
        while (*cp && isspace((unsigned char)*cp) && *cp != '\n') {
            indent++;
            cp++;
        }
        if (*cp) { /* non-blank line */
            if (min_indent < 0 || indent < min_indent)
                min_indent = indent;
        }
    }

    if (min_indent <= 0) return strdup(text);

    /* Strip min_indent from each line */
    size_t len = strlen(text);
    char *result = (char *)malloc(len + 1);
    if (!result) return NULL;

    char *out = result;
    p = text;

    while (*p) {
        if (*p == '\n') {
            *out++ = *p++;
            continue;
        }
        /* Count leading spaces on this line */
        int leading = 0;
        const char *cp = p;
        while (*cp && *cp != '\n' && isspace((unsigned char)*cp)) {
            leading++;
            cp++;
        }
        /* Skip min_indent spaces, copy rest */
        int skip = (leading < min_indent) ? leading : min_indent;
        p += skip;
        while (*p && *p != '\n') {
            *out++ = *p++;
        }
    }
    *out = '\0';

    return result;
}

char *textwrap_shorten(const char *text, int max_len) {
    if (!text || max_len <= 0) return strdup("");
    size_t len = strlen(text);
    if ((int)len <= max_len) return strdup(text);

    /* Truncate and append "..." */
    int trunc_len = max_len - 3;
    if (trunc_len < 0) trunc_len = 0;

    char *result = (char *)malloc((size_t)trunc_len + 4);
    if (!result) return NULL;

    memcpy(result, text, trunc_len);
    result[trunc_len] = '.';
    result[trunc_len + 1] = '.';
    result[trunc_len + 2] = '.';
    result[trunc_len + 3] = '\0';

    return result;
}

/* Port of Python feishu_comment._chunk_text(). Split text into chunks at
 * newline boundaries within max_len per chunk. Returns malloc'd array with
 * *count set. Caller must free each string and the array. */
char **textwrap_chunk(const char *text, int max_len, size_t *count) {
    if (!count) return NULL;
    *count = 0;
    if (!text || max_len <= 0) return NULL;

    size_t capacity = 16;
    size_t n = 0;
    char **chunks = (char **)calloc(capacity, sizeof(char *));
    if (!chunks) return NULL;

    size_t text_len = strlen(text);
    size_t pos = 0;

    while (pos < text_len) {
        size_t remaining = text_len - pos;
        size_t this_chunk = remaining < (size_t)max_len ? remaining : (size_t)max_len;

        /* Try to find last newline within the chunk for a clean break */
        if (this_chunk == (size_t)max_len) {
            size_t newline_pos = pos + this_chunk;  /* scan backwards from end */
            while (newline_pos > pos) {
                newline_pos--;
                if (text[newline_pos] == '\n') {
                    this_chunk = newline_pos - pos;  /* exclude newline */
                    break;
                }
            }
        }

        /* Allocate and copy chunk */
        char *chunk = (char *)malloc(this_chunk + 1);
        if (!chunk) {
            for (size_t i = 0; i < n; i++) free(chunks[i]);
            free(chunks);
            *count = 0;
            return NULL;
        }
        memcpy(chunk, text + pos, this_chunk);
        chunk[this_chunk] = '\0';
        pos += this_chunk;

        /* Skip leading newlines on next iteration */
        while (pos < text_len && text[pos] == '\n') pos++;

        /* Grow array if needed */
        if (n >= capacity) {
            capacity *= 2;
            char **new_chunks = (char **)realloc(chunks, capacity * sizeof(char *));
            if (!new_chunks) {
                free(chunk);
                for (size_t i = 0; i < n; i++) free(chunks[i]);
                free(chunks);
                *count = 0;
                return NULL;
            }
            chunks = new_chunks;
        }
        chunks[n++] = chunk;
    }

    *count = n;
    return chunks;
}

/* ================================================================
 *  PoP: textwrap.shorten / textwrap.indent (2026-08-22 lib audit)
 * ================================================================ */

char *textwrap_indent(const char *text, const char *prefix) {
    if (!prefix) prefix = "";
    if (!text) return strdup("");
    size_t plen = strlen(prefix);
    size_t tlen = strlen(text);
    /* count non-whitespace-only lines */
    size_t cap = tlen + 1;
    const char *p = text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t ll = nl ? (size_t)(nl - p) : strlen(p);
        int only_ws = 1;
        for (size_t i = 0; i < ll; i++)
            if (!isspace((unsigned char)p[i])) { only_ws = 0; break; }
        if (!only_ws && ll > 0) cap += plen;
        if (!nl) break;
        p = nl + 1;
    }
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    p = text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t ll = nl ? (size_t)(nl - p) : strlen(p);
        int only_ws = 1;
        for (size_t i = 0; i < ll; i++)
            if (!isspace((unsigned char)p[i])) { only_ws = 0; break; }
        if (!only_ws && ll > 0) {
            memcpy(out + o, prefix, plen);
            o += plen;
        }
        memcpy(out + o, p, ll);
        o += ll;
        if (!nl) break;
        out[o++] = '\n';
        p = nl + 1;
    }
    out[o] = '\0';
    return out;
}

char *textwrap_shorten_py(const char *text, int max_len) {
    if (!text || max_len <= 0) return strdup("");
    static const char PLACEHOLDER[] = " [...]";
    const size_t phlen = sizeof(PLACEHOLDER) - 1;
    size_t len = strlen(text);
    if ((int)len <= max_len) return strdup(text);

    /* tokenize into whitespace-separated words (Python re-joins with a
     * single space, matching textwrap's collapse-whitespace default) */
    char **words = (char **)calloc(MAX_WORDS, sizeof(char *));
    if (!words) return NULL;
    int nwords = 0;
    const char *p = text;
    while (*p && nwords < MAX_WORDS) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        words[nwords] = (char *)malloc((size_t)(p - start) + 1);
        if (!words[nwords]) break;
        memcpy(words[nwords], start, (size_t)(p - start));
        words[nwords][p - start] = '\0';
        nwords++;
    }

    /* drop words from the end until body + placeholder fits */
    char *result = NULL;
    for (int end = nwords; end >= 0; end--) {
        /* measure: words joined by single spaces, then placeholder
         * (preceded by a space) when any words are kept; placeholder
         * alone when none fit */
        size_t need = 0;
        for (int i = 0; i < end; i++) need += strlen(words[i]);
        if (end > 1) need += (size_t)(end - 1);   /* joining spaces */
        if (end > 0) need += phlen;      /* space + "[...]" body */
        else if (phlen <= (size_t)max_len) need += phlen;
        if (need == 0) need = 1;         /* empty fallback */
        if ((long long)need <= (long long)max_len || end == 0) {
            result = (char *)malloc(need + 1);
            if (result) {
                size_t o = 0;
                for (int i = 0; i < end; i++) {
                    if (i > 0) result[o++] = ' ';
                    size_t wl = strlen(words[i]);
                    memcpy(result + o, words[i], wl);
                    o += wl;
                }
                if (end > 0 || phlen <= (size_t)max_len) {
                    if (end > 0) result[o++] = ' ';
                    memcpy(result + o, PLACEHOLDER + 1, phlen - 1);
                    o += phlen - 1;
                }
                result[o] = '\0';
            }
            break;
        }
    }
    for (int i = 0; i < nwords; i++) free(words[i]);
    free(words);
    return result ? result : strdup("");
}
