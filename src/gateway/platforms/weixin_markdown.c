/*
 * weixin_markdown.c - weixin markdown block split / delivery helpers.
 *
 * Split from weixin.c (soul.md: self-contained modules). Fence / heading /
 * table-rule detection, block splitting and delivery-unit packing.
 * No file-scope state.
 */
#include "weixin_markdown.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "libbase64/base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



/* PoP: _split_table_row @ gateway/platforms/weixin.py:_split_table_row */
/* PoP: weixin_split_table_row @ gateway/platforms/weixin.py:_split_table_row */
char **weixin_split_table_row(const char *line, int *count)
{
    if (count) *count = 0;
    if (!line) return NULL;
    /* Split on pipe '|' character, trim each field */
    char *work = strdup(line);
    if (!work) return NULL;
    int cap = 16, n = 0;
    char **result = (char **)malloc((size_t)cap * sizeof(char *));
    if (!result) { free(work); return NULL; }
    char *tok = strtok(work, "|");
    while (tok) {
        /* Trim */
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && end[-1] == ' ') end--;
        *end = '\0';
        if (n >= cap) {
            cap *= 2;
            char **tmp = (char **)realloc(result, (size_t)cap * sizeof(char *));
            if (!tmp) { free(work); return NULL; }
            result = tmp;
        }
        result[n++] = strdup(tok);
        tok = strtok(NULL, "|");
    }
    free(work);
    if (count) *count = n;
    return result;
}


/* PoP: _looks_like_heading_line_for_weixin @ gateway/platforms/weixin.py:_looks_like_heading_line_for_weixin */
/* PoP: weixin_looks_like_heading_line @ gateway/platforms/weixin.py:_looks_like_heading_line_for_weixin */
bool weixin_looks_like_heading_line(const char *line)
{
    if (!line || !*line) return false;
    /* Starts with # or ## or ### ... */
    const char *p = line;
    while (*p == ' ') p++;
    if (*p == '#') {
        p++;
        while (*p == '#') p++;
        return (*p == ' ' || *p == '\0');
    }
    /* Underline-style heading: line of === or --- */
    if (*p == '=' || *p == '-') {
        int count = 0;
        while (*p == '=' || *p == '-') { count++; p++; }
        while (*p == ' ') p++;
        return count >= 3 && *p == '\0';
    }
    return false;
}


/* PoP: _looks_like_chatty_line_for_weixin @ gateway/platforms/weixin.py:_looks_like_chatty_line_for_weixin */
/* PoP: weixin_looks_like_chatty_line @ gateway/platforms/weixin.py:_looks_like_chatty_line_for_weixin */
bool weixin_looks_like_chatty_line(const char *line)
{
    if (!line || !*line) return false;
    const char *p = line;
    while (*p == ' ') p++;
    /* Blockquote or list marker */
    if (*p == '>' || *p == '-' || *p == '*' || *p == '+') return true;
    /* Numbered list */
    if (isdigit((unsigned char)*p)) {
        while (isdigit((unsigned char)*p)) p++;
        while (*p == ' ') p++;
        if (*p == '.') return true;
    }
    /* Inline code backtick */
    if (*p == '`') return true;
    return false;
}


/* Local helper: join an array of lines with '\n', strip leading/trailing
 * whitespace, and return a malloc'd string (caller frees). */
static char *weixin__join_strip(char **lines, int n)
{
    size_t total = 1; /* NUL */
    for (int i = 0; i < n; i++) total += strlen(lines[i]) + 1; /* + '\n' */
    char *buf = malloc(total);
    if (!buf) return NULL;
    buf[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (i) strcat(buf, "\n");
        strcat(buf, lines[i]);
    }
    /* strip trailing whitespace */
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t' ||
                       buf[len-1] == '\n' || buf[len-1] == '\r'))
        buf[--len] = '\0';
    /* strip leading whitespace */
    const char *p = buf;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (p != buf) memmove(buf, p, strlen(p) + 1);
    return buf;
}


/* PoP: _split_markdown_blocks @ gateway/platforms/weixin.py:_split_markdown_blocks */
/* Split content into blocks at fenced code regions and blank lines, keeping fenced blocks intact. */
char **weixin_split_markdown_blocks(const char *content, int *out_count)
{
    if (out_count) *out_count = 0;
    if (!content || !*content) return NULL;

    /* Count lines first. */
    int nlines = 1;
    for (const char *p = content; *p; p++) if (*p == '\n') nlines++;
    char **lines = malloc(sizeof(char *) * (nlines + 1));
    int nl = 0;
    const char *start = content;
    for (const char *p = content; ; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t len = p - start;
            char *line = malloc(len + 1);
            memcpy(line, start, len);
            line[len] = '\0';
            /* strip trailing CR */
            while (len > 0 && (line[len-1] == '\r')) line[--len] = '\0';
            lines[nl++] = line;
            start = p + 1;
            if (*p == '\0') break;
        }
    }
    lines[nl] = NULL;

    char **blocks = malloc(sizeof(char *) * (nl + 1));
    int nb = 0;
    char **current = malloc(sizeof(char *) * (nl + 1));
    int nc = 0;
    bool in_code = false;

    for (int i = 0; i < nl; i++) {
        const char *ls = lines[i];
        while (*ls == ' ' || *ls == '\t') ls++;
        bool is_fence = (strncmp(ls, "```", 3) == 0);

        if (is_fence) {
            if (!in_code && nc > 0) {
                blocks[nb++] = weixin__join_strip(current, nc);
                nc = 0;
            }
            current[nc++] = strdup(lines[i]);
            in_code = !in_code;
            if (!in_code) {
                blocks[nb++] = weixin__join_strip(current, nc);
                nc = 0;
            }
            continue;
        }
        if (in_code) {
            current[nc++] = strdup(lines[i]);
            continue;
        }
        if (lines[i][0] == '\0') {
            if (nc > 0) {
                blocks[nb++] = weixin__join_strip(current, nc);
                nc = 0;
            }
            continue;
        }
        current[nc++] = strdup(lines[i]);
    }
    if (nc > 0) blocks[nb++] = weixin__join_strip(current, nc);
    blocks[nb] = NULL;

    /* filter empties */
    int out_n = 0;
    for (int i = 0; i < nb; i++) {
        if (blocks[i] && blocks[i][0]) blocks[out_n++] = blocks[i];
        else free(blocks[i]);
    }
    blocks[out_n] = NULL;

    for (int i = 0; i < nl; i++) free(lines[i]);
    free(lines);
    free(current);
    if (out_count) *out_count = out_n;
    return blocks;
}


/* Return true when `line` (any leading whitespace stripped) is a fenced-code
 * opener/closer (``` or ````). Mirrors weixin._FENCE_RE. */
static bool weixin__is_fence(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    return strncmp(line, "```", 3) == 0;
}


/* Mirrors weixin._TABLE_RULE_RE: a row of dashes/colons/pipes (>=3 dashes, has '|'). */
static bool weixin__is_table_rule(const char *s) {
    bool has_dash = false, has_pipe = false;
    for (const char *p = s; *p; p++) {
        char c = *p;
        if (c == ' ' || c == '\t') continue;
        if (c == '-') has_dash = true;
        else if (c == '|') has_pipe = true;
        else if (c == ':') continue;
        else return false;
    }
    return has_dash && has_pipe;
}


/* Mirrors weixin._HEADER_RE: 1-6 '#' then space then content. */
static bool weixin__is_header_line(const char *s) {
    int n = 0;
    while (s[n] == '#') n++;
    if (n < 1 || n > 6) return false;
    return s[n] == ' ' || s[n] == '\t';
}


/* PoP: _normalize_markdown_blocks @ gateway/platforms/weixin.py:_normalize_markdown_blocks */
/* Collapse runs of >1 blank line to a single blank line; keep fenced blocks
 * intact; strip leading/trailing whitespace of the whole content. */
char *weixin_normalize_markdown_blocks(const char *content) {
    if (!content) return NULL;
    char **lines = NULL; int nl = 0;
    const char *start = content;
    for (const char *p = content; ; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t len = p - start;
            char *line = malloc(len + 1);
            memcpy(line, start, len); line[len] = '\0';
            while (len > 0 && (line[len-1] == '\r' || line[len-1] == ' ' || line[len-1] == '\t')) line[--len] = '\0';
            lines = realloc(lines, sizeof(char *) * (nl + 2));
            lines[nl++] = line;
            start = p + 1;
            if (*p == '\0') break;
        }
    }
    char **out = malloc(sizeof(char *) * (nl + 1));
    int no = 0, blank_run = 0; bool in_code = false;
    for (int i = 0; i < nl; i++) {
        const char *ls = lines[i];
        while (*ls == ' ' || *ls == '\t') ls++;
        if (weixin__is_fence(ls)) {
            in_code = !in_code;
            out[no++] = strdup(lines[i]);
            blank_run = 0;
            continue;
        }
        if (in_code) { out[no++] = strdup(lines[i]); continue; }
        if (lines[i][0] == '\0') {
            blank_run++;
            if (blank_run <= 1) out[no++] = strdup("");
            continue;
        }
        blank_run = 0;
        out[no++] = strdup(lines[i]);
    }
    out[no] = NULL;
    /* join with '\n', strip trailing newlines, then leading */
    size_t cap = 1;
    for (int i = 0; i < no; i++) cap += strlen(out[i]) + 1;
    char *res = malloc(cap);
    res[0] = '\0';
    for (int i = 0; i < no; i++) {
        if (i) strncat(res, "\n", cap - strlen(res) - 1);
        strncat(res, out[i], cap - strlen(res) - 1);
    }
    /* trim trailing newlines */
    size_t L = strlen(res);
    while (L > 0 && res[L-1] == '\n') res[--L] = '\0';
    for (int i = 0; i < no; i++) free(out[i]);
    free(out); free(lines);
    return res;
}


/* PoP: _looks_like_chatty_line_for_weixin @ gateway/platforms/weixin.py:_looks_like_chatty_line_for_weixin */
static bool weixin_looks_like_chatty_line_for_weixin(const char *line) {
    const char *s = line;
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) return false;
    size_t len = strlen(s);
    if (len > 48) return false;
    if (s[0] == ' ' || s[0] == '\t') return false;
    if (s[0] == '>' || s[0] == '-' || s[0] == '*' || s[0] == '【' || s[0] == '#' || s[0] == '|') return false;
    if (weixin__is_table_rule(s)) return false;
    /* ^\*\*[^*]+\*\*$  (bold-only line) */
    if (len >= 4 && s[0] == '*' && s[1] == '*' && s[len-1] == '*' && s[len-2] == '*') {
        bool inner_ok = true;
        for (size_t i = 2; i + 2 < len; i++) if (s[i] == '*') { inner_ok = false; break; }
        if (inner_ok) return false;
    }
    /* ^\d+\.  */
    size_t i = 0;
    while (isdigit((unsigned char)s[i])) i++;
    if (i > 0 && s[i] == '.') return false;
    return true;
}


/* PoP: _looks_like_heading_line_for_weixin @ gateway/platforms/weixin.py:_looks_like_heading_line_for_weixin */
static bool weixin_looks_like_heading_line_for_weixin(const char *line) {
    const char *s = line;
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) return false;
    if (weixin__is_header_line(s)) return true;
    size_t len = strlen(s);
    if (len > 24) return false;
    size_t L = len;
    return (L > 0 && (s[L-1] == ':' || s[L-1] == '：'));
}


/* PoP: _should_split_short_chat_block_for_weixin @ gateway/platforms/weixin.py:_should_split_short_chat_block_for_weixin */
static bool weixin_should_split_short_chat_block_for_weixin(const char *block) {
    char **lines = NULL; int nl = 0;
    const char *start = block;
    for (const char *p = block; ; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t len = p - start;
            char *line = malloc(len + 1);
            memcpy(line, start, len); line[len] = '\0';
            lines = realloc(lines, sizeof(char *) * (nl + 2));
            lines[nl++] = line;
            start = p + 1;
            if (*p == '\0') break;
        }
    }
    int nonblank = 0;
    for (int i = 0; i < nl; i++) if (lines[i][0]) nonblank++;
    bool result = false;
    if (nonblank >= 2 && nonblank <= 6) {
        if (weixin_looks_like_heading_line_for_weixin(lines[0])) {
            result = false;
        } else {
            result = true;
            for (int i = 0; i < nl; i++) {
                if (!lines[i][0]) continue;
                if (!weixin_looks_like_chatty_line_for_weixin(lines[i])) { result = false; break; }
            }
        }
    }
    for (int i = 0; i < nl; i++) free(lines[i]);
    free(lines);
    return result;
}


/* PoP: _wrap_copy_friendly_lines_for_weixin @ gateway/platforms/weixin.py:_wrap_copy_friendly_lines_for_weixin */
/* Greedy word-wrap long display lines (break_long_words=false): a word longer
 * than the width is kept intact on its own line. Fenced blocks, short lines,
 * table rows, and empty lines pass through unchanged. */
char *weixin_wrap_copy_friendly_lines_for_weixin(const char *content) {
    if (!content || !*content) return strdup(content ? content : "");
    char **lines = NULL; int nl = 0;
    const char *start = content;
    for (const char *p = content; ; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t len = p - start;
            char *line = malloc(len + 1);
            memcpy(line, start, len); line[len] = '\0';
            lines = realloc(lines, sizeof(char *) * (nl + 2));
            lines[nl++] = line;
            start = p + 1;
            if (*p == '\0') break;
        }
    }
    char **out = malloc(sizeof(char *) * (nl * 4 + 1));
    int no = 0; bool in_code = false;
    for (int i = 0; i < nl; i++) {
        const char *raw = lines[i];
        const char *ls = raw; while (*ls == ' ' || *ls == '\t') ls++;
        if (weixin__is_fence(ls)) { in_code = !in_code; out[no++] = strdup(raw); continue; }
        if (in_code || strlen(raw) <= WEIXIN_COPY_LINE_WIDTH || !ls[0] ||
            ls[0] == '|' || weixin__is_table_rule(ls)) {
            out[no++] = strdup(raw);
            continue;
        }
        /* greedy wrap on spaces */
        const char *cur = raw;
        char *buf = malloc(strlen(raw) + 1); buf[0] = '\0';
        bool has = false;
        const char *w = cur;
        while (*w) {
            while (*w == ' ') w++;
            if (!*w) break;
            const char *we = w;
            while (*we && *we != ' ') we++;
            size_t wlen = we - w;
            if (has && strlen(buf) + 1 + wlen > WEIXIN_COPY_LINE_WIDTH) {
                out[no++] = strdup(buf);
                buf[0] = '\0'; has = false;
            }
            if (has) { strncat(buf, " ", 1); }
            strncat(buf, w, wlen);
            has = true;
            w = we;
        }
        if (has) out[no++] = strdup(buf);
        free(buf);
    }
    out[no] = NULL;
    size_t cap = 1;
    for (int i = 0; i < no; i++) cap += strlen(out[i]) + 1;
    char *res = malloc(cap); res[0] = '\0';
    for (int i = 0; i < no; i++) {
        if (i) strncat(res, "\n", cap - strlen(res) - 1);
        strncat(res, out[i], cap - strlen(res) - 1);
    }
    size_t L = strlen(res);
    while (L > 0 && res[L-1] == '\n') res[--L] = '\0';
    for (int i = 0; i < no; i++) free(out[i]);
    free(out);
    for (int i = 0; i < nl; i++) free(lines[i]);
    free(lines);
    return res;
}


/* PoP: _split_delivery_units_for_weixin @ gateway/platforms/weixin.py:_split_delivery_units_for_weixin */
/* Split formatted content into chat-friendly delivery units: one unit per
 * fenced block, otherwise one unit per top-level (non-indented) line group,
 * attaching indented continuation lines to the preceding unit. */
char **weixin_split_delivery_units_for_weixin(const char *content, int *out_count) {
    if (out_count) *out_count = 0;
    if (!content || !*content) return NULL;
    char **blocks = weixin_split_markdown_blocks(content, NULL);
    if (!blocks) return NULL;
    int nb = 0; while (blocks[nb]) nb++;
    char **units = malloc(sizeof(char *) * (nb * 8 + 1));
    int nu = 0;
    for (int b = 0; b < nb; b++) {
        const char *blk = blocks[b];
        const char *first = blk; while (*first == ' ' || *first == '\t') first++;
        if (weixin__is_fence(first)) { units[nu++] = strdup(blk); continue; }
        char **lines = NULL; int nl = 0;
        const char *start = blk;
        for (const char *p = blk; ; p++) {
            if (*p == '\n' || *p == '\0') {
                size_t len = p - start;
                char *line = malloc(len + 1);
                memcpy(line, start, len); line[len] = '\0';
                lines = realloc(lines, sizeof(char *) * (nl + 2));
                lines[nl++] = line;
                start = p + 1;
                if (*p == '\0') break;
            }
        }
        char **cur = malloc(sizeof(char *) * (nl + 1));
        int nc = 0;
        for (int i = 0; i < nl; i++) {
            const char *rl = lines[i];
            if (rl[0] == '\0') {
                if (nc > 0) { cur[nc] = NULL; units[nu++] = weixin__join_strip(cur, nc); nc = 0; }
                continue;
            }
            bool cont = (nc > 0) && (rl[0] == ' ' || rl[0] == '\t');
            if (cont) { cur[nc++] = strdup(rl); continue; }
            if (nc > 0) { cur[nc] = NULL; units[nu++] = weixin__join_strip(cur, nc); }
            cur[nc++] = strdup(rl);
        }
        if (nc > 0) { cur[nc] = NULL; units[nu++] = weixin__join_strip(cur, nc); }
        for (int i = 0; i < nl; i++) free(lines[i]);
        free(lines); free(cur);
    }
    units[nu] = NULL;
    int out_n = 0;
    for (int i = 0; i < nu; i++) if (units[i] && units[i][0]) units[out_n++] = units[i]; else free(units[i]);
    units[out_n] = NULL;
    for (int i = 0; i < nb; i++) free(blocks[i]);
    free(blocks);
    if (out_count) *out_count = out_n;
    return units;
}


/* PoP: truncate_message @ gateway/platforms/base.py:truncate_message */
/* Faithful port: split long content into <=max_length chunks, preserving
 * fenced-code-block boundaries (close orphan fences, reopen next chunk with the
 * same language tag), and append (i/total) indicators when multiple chunks. */
char **weixin_truncate_message(const char *content, int max_length, int *out_count) {
    if (out_count) *out_count = 0;
    if (!content) return NULL;
    int total = (int)strlen(content);
    if (total <= max_length) {
        char **r = malloc(sizeof(char *) * 2);
        r[0] = strdup(content); r[1] = NULL;
        if (out_count) *out_count = 1;
        return r;
    }
    const int INDICATOR_RESERVE = 10;
    const char *FENCE_CLOSE = "\n```";
    char **chunks = malloc(sizeof(char *) * 64);
    int nch = 0;
    char *remaining = strdup(content);
    char *carry_lang = NULL;
    while (remaining && *remaining) {
        char *prefix = NULL;
        if (carry_lang) {
            size_t pl = strlen(carry_lang) + 5;
            prefix = malloc(pl + 1);
            snprintf(prefix, pl + 1, "```%s\n", carry_lang);
        }
        int pref_len = prefix ? (int)strlen(prefix) : 0;
        int headroom = max_length - INDICATOR_RESERVE - pref_len - (int)strlen(FENCE_CLOSE);
        if (headroom < 1) headroom = max_length / 2;
        int rem_len = (int)strlen(remaining);
        if (pref_len + rem_len <= max_length - INDICATOR_RESERVE) {
            char *full = malloc(pref_len + rem_len + 1);
            full[0] = '\0';
            if (prefix) strcat(full, prefix);
            strcat(full, remaining);
            chunks[nch++] = full;
            break;
        }
        int cp_limit = headroom;
        if (cp_limit > rem_len) cp_limit = rem_len;
        int region_len = cp_limit;
        int split_at = -1;
        for (int i = region_len - 1; i >= 0; i--) {
            if (remaining[i] == '\n') { split_at = i; break; }
        }
        if (split_at < region_len / 2) {
            for (int i = region_len - 1; i >= 0; i--) {
                if (remaining[i] == ' ') { split_at = i; break; }
            }
        }
        if (split_at < 1) split_at = region_len;
        /* avoid splitting inside an inline code span */
        char *candidate = malloc(split_at + 1);
        memcpy(candidate, remaining, split_at); candidate[split_at] = '\0';
        int bt = 0; for (int i = 0; candidate[i]; i++) if (candidate[i] == '`') bt++;
        if (bt % 2 == 1) {
            int last_bt = -1;
            for (int i = (int)strlen(candidate) - 1; i > 0; i--) if (candidate[i] == '`') { last_bt = i; break; }
            if (last_bt > 0) {
                int safe = -1;
                for (int i = last_bt - 1; i >= 0; i--) if (candidate[i] == ' ' || candidate[i] == '\n') { safe = i; break; }
                if (safe > region_len / 4) split_at = safe;
            }
        }
        free(candidate);
        int chunk_body_len = split_at;
        char *chunk_body = malloc(chunk_body_len + 1);
        memcpy(chunk_body, remaining, chunk_body_len); chunk_body[chunk_body_len] = '\0';
        char *rest = strdup(remaining + split_at);
        /* lstrip rest */
        char *p = rest; while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        memmove(rest, p, strlen(p) + 1);
        /* determine if we end inside an open code block */
        bool in_code = (carry_lang != NULL);
        char *lang = carry_lang ? strdup(carry_lang) : strdup("");
        char *cb = chunk_body;
        char *line = NULL; size_t cap = 0;
        /* iterate lines of chunk_body */
        {
            const char *s = chunk_body;
            while (*s) {
                const char *e = s; while (*e && *e != '\n') e++;
                size_t ll = e - s;
                free(line); line = malloc(ll + 1); memcpy(line, s, ll); line[ll] = '\0';
                char *st = line; while (*st == ' ' || *st == '\t') st++;
                if (strncmp(st, "```", 3) == 0) {
                    if (in_code) { in_code = false; free(lang); lang = strdup(""); }
                    else {
                        in_code = true;
                        char *tag = st + 3; while (*tag == ' ') tag++;
                        char *sp = tag; while (*sp && *sp != ' ' && *sp != '\n' && *sp != '\t') sp++;
                        *sp = '\0';
                        free(lang); lang = strdup(tag);
                    }
                }
                s = (*e) ? e + 1 : e;
            }
        }
        free(line);
        char *full_chunk = malloc(pref_len + chunk_body_len + (in_code ? strlen(FENCE_CLOSE) : 0) + 1);
        full_chunk[0] = '\0';
        if (prefix) strcat(full_chunk, prefix);
        strcat(full_chunk, chunk_body);
        if (in_code) { strcat(full_chunk, FENCE_CLOSE); free(carry_lang); carry_lang = strdup(lang); }
        else { free(carry_lang); carry_lang = NULL; }
        chunks[nch++] = full_chunk;
        free(chunk_body); free(remaining); remaining = rest;
        free(prefix); free(lang);
    }
    /* append indicators when multiple chunks */
    if (nch > 1) {
        int total_n = nch;
        for (int i = 0; i < nch; i++) {
            char *with = malloc(strlen(chunks[i]) + 16);
            snprintf(with, strlen(chunks[i]) + 16, "%s (%d/%d)", chunks[i], i + 1, total_n);
            free(chunks[i]); chunks[i] = with;
        }
    }
    chunks[nch] = NULL;
    free(remaining); free(carry_lang);
    if (out_count) *out_count = nch;
    return chunks;
}


/* PoP: _pack_markdown_blocks_for_weixin @ gateway/platforms/weixin.py:_pack_markdown_blocks_for_weixin */
char **weixin_pack_markdown_blocks_for_weixin(const char *content, int max_length, int *out_count) {
    if (out_count) *out_count = 0;
    if (!content) return NULL;
    if ((int)strlen(content) <= max_length) {
        char **r = malloc(sizeof(char *) * 2);
        r[0] = strdup(content); r[1] = NULL;
        if (out_count) *out_count = 1;
        return r;
    }
    char **blocks = weixin_split_markdown_blocks(content, NULL);
    if (!blocks) return NULL;
    int nb = 0; while (blocks[nb]) nb++;
    char **packed = malloc(sizeof(char *) * (nb * 4 + 1));
    int np = 0;
    char *current = strdup("");
    for (int b = 0; b < nb; b++) {
        char *candidate;
        if (!*current) candidate = strdup(blocks[b]);
        else {
            size_t cl = strlen(current), bl = strlen(blocks[b]);
            candidate = malloc(cl + 2 + bl + 1);
            snprintf(candidate, cl + 2 + bl + 1, "%s\n\n%s", current, blocks[b]);
        }
        if ((int)strlen(candidate) <= max_length) {
            free(current); current = candidate;
            continue;
        }
        if (*current) packed[np++] = current; else free(current);
        current = strdup("");
        if ((int)strlen(blocks[b]) <= max_length) {
            current = strdup(blocks[b]);
            continue;
        }
        int tn = 0;
        char **tr = weixin_truncate_message(blocks[b], max_length, &tn);
        for (int i = 0; i < tn; i++) packed[np++] = tr[i];
        free(tr);
        free(candidate);
    }
    if (*current) packed[np++] = current; else free(current);
    packed[np] = NULL;
    int out_n = 0;
    for (int i = 0; i < np; i++) if (packed[i] && packed[i][0]) packed[out_n++] = packed[i]; else free(packed[i]);
    packed[out_n] = NULL;
    for (int i = 0; i < nb; i++) free(blocks[i]);
    free(blocks);
    if (out_count) *out_count = out_n;
    return packed;
}


/* PoP: _split_text_for_weixin_delivery @ gateway/platforms/weixin.py:_split_text_for_weixin_delivery */
char **weixin_split_text_for_weixin_delivery(const char *content, int max_length, bool split_per_line, int *out_count) {
    if (out_count) *out_count = 0;
    if (!content || !*content) return NULL;
    char **result = NULL;
    if (split_per_line) {
        if ((int)strlen(content) <= max_length && strchr(content, '\n') == NULL) {
            result = malloc(sizeof(char *) * 2);
            result[0] = strdup(content); result[1] = NULL;
            if (out_count) *out_count = 1;
            return result;
        }
        char **units = weixin_split_delivery_units_for_weixin(content, NULL);
        int nu = 0; while (units && units[nu]) nu++;
        char **chunks = malloc(sizeof(char *) * (nu * 4 + 1));
        int nc = 0;
        for (int i = 0; i < nu; i++) {
            if ((int)strlen(units[i]) <= max_length) {
                chunks[nc++] = strdup(units[i]);
            } else {
                int pn = 0;
                char **pk = weixin_pack_markdown_blocks_for_weixin(units[i], max_length, &pn);
                for (int j = 0; j < pn; j++) chunks[nc++] = pk[j];
                free(pk);
            }
        }
        chunks[nc] = NULL;
        int out_n = 0;
        for (int i = 0; i < nc; i++) if (chunks[i] && chunks[i][0]) chunks[out_n++] = chunks[i]; else free(chunks[i]);
        chunks[out_n] = NULL;
        for (int i = 0; i < nu; i++) free(units[i]);
        free(units);
        if (out_n == 0) { free(chunks); result = malloc(sizeof(char *) * 2); result[0] = strdup(content); result[1] = NULL; if (out_count) *out_count = 1; return result; }
        if (out_count) *out_count = out_n;
        return chunks;
    }
    /* compact (default): single message when under the limit — unless the
     * content looks like a short chatty exchange, in which case split. */
    if ((int)strlen(content) <= max_length) {
        if (weixin_should_split_short_chat_block_for_weixin(content)) {
            char **units = weixin_split_delivery_units_for_weixin(content, NULL);
            int nu = 0; while (units && units[nu]) nu++;
            char **out = malloc(sizeof(char *) * (nu + 1));
            int on = 0;
            for (int i = 0; i < nu; i++) if (units[i] && units[i][0]) out[on++] = strdup(units[i]);
            out[on] = NULL;
            for (int i = 0; i < nu; i++) free(units[i]);
            free(units);
            if (out_count) *out_count = on;
            return out;
        }
        result = malloc(sizeof(char *) * 2);
        result[0] = strdup(content); result[1] = NULL;
        if (out_count) *out_count = 1;
        return result;
    }
    result = weixin_pack_markdown_blocks_for_weixin(content, max_length, out_count);
    if (!result || !*result) { result = malloc(sizeof(char *) * 2); result[0] = strdup(content); result[1] = NULL; if (out_count) *out_count = 1; }
    return result;
}
