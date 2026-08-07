/*
 * port_platforms_helpers_md.c — pure markdown-chunking core of
 * gateway/platforms/helpers.py. Cohesive port of ONE Python module section:
 * fence/table/paragraph-aware chunking primitives. No I/O, no logging,
 * no platform adapter state. Opaque, minimal includes, C11 only.
 */

#include "platforms_helpers_md.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── strlist helpers ─────────────────────────────────────────── */

void helpers_md_free_strlist(char **list)
{
    if (!list) return;
    for (char **p = list; *p; p++) free(*p);
    free(list);
}

char **helpers_md_strlist_push(char **list, const char *s)
{
    size_t n = 0;
    if (list) while (list[n]) n++;
    char **nl = (char **)realloc(list, (n + 2) * sizeof(char *));
    if (!nl) return list;
    nl[n] = strdup(s ? s : "");
    nl[n + 1] = NULL;
    return nl;
}

static bool line_is_blank(const char *s)
{
    if (!s) return true;
    while (*s) {
        if (*s != ' ' && *s != '\t' && *s != '\r')
            return false;
        s++;
    }
    return true;
}

static bool line_is_table_row(const char *s)
{
    if (!s) return false;
    const char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '|') return false;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' || p[len-1] == '\r')) len--;
    return len >= 1 && p[len-1] == '|';
}

static bool text_nonblank(const char *s)
{
    if (!s) return false;
    while (*s) {
        if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n')
            return true;
        s++;
    }
    return false;
}

/* Join current_lines (NULL-terminated) into one atom with \n separators.
 * Returns malloc'd string; the caller frees. */
static char *join_lines(char **lines)
{
    if (!lines) return strdup("");
    size_t total = 0;
    int n = 0;
    for (char **c = lines; *c; c++) { total += strlen(*c); n++; }
    if (n == 0) return strdup("");
    char *out = (char *)malloc(total + (size_t)n + 1);
    if (!out) return NULL;
    char *pos = out;
    for (int i = 0; i < n; i++) {
        if (i > 0) *pos++ = '\n';
        size_t l = strlen(lines[i]);
        memcpy(pos, lines[i], l);
        pos += l;
    }
    *pos = '\0';
    return out;
}

/* ── predicates ──────────────────────────────────────────────── */

/* PoP: text_has_unclosed_fence @ gateway/platforms/helpers.py:text_has_unclosed_fence */
bool helpers_md_text_has_unclosed_fence(const char *text)
{
    if (!text) return false;
    bool in_fence = false;
    const char *p = text;
    while (*p) {
        const char *line = p;
        const char *eol = strchr(p, '\n');
        size_t line_len = eol ? (size_t)(eol - p) : strlen(p);
        if (line_len >= 3 && strncmp(line, "```", 3) == 0)
            in_fence = !in_fence;
        if (!eol) break;
        p = eol + 1;
    }
    return in_fence;
}

/* PoP: text_ends_with_table_row @ gateway/platforms/helpers.py:text_ends_with_table_row */
bool helpers_md_text_ends_with_table_row(const char *text)
{
    if (!text || !*text) return false;
    size_t len = strlen(text);
    while (len > 0 && (text[len-1] == ' ' || text[len-1] == '\t' ||
                       text[len-1] == '\r' || text[len-1] == '\n'))
        len--;
    if (len == 0) return false;
    const char *start = text + len - 1;
    while (start > text && start[-1] != '\n') start--;
    const char *s = start;
    while (s < text + len && (*s == ' ' || *s == '\t')) s++;
    size_t sl = (size_t)(text + len - s);
    while (sl > 0 && (s[sl-1] == ' ' || s[sl-1] == '\t' || s[sl-1] == '\r')) sl--;
    if (sl < 2) return false;
    return s[0] == '|' && s[sl-1] == '|';
}

/* PoP: is_fence_atom @ gateway/platforms/helpers.py:is_fence_atom */
bool helpers_md_is_fence_atom(const char *text)
{
    if (!text) return false;
    const char *p = text;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return strncmp(p, "```", 3) == 0;
}

/* PoP: is_table_atom @ gateway/platforms/helpers.py:is_table_atom */
bool helpers_md_is_table_atom(const char *text)
{
    if (!text) return false;
    const char *eol = strchr(text, '\n');
    size_t fl = eol ? (size_t)(eol - text) : strlen(text);
    const char *s = text;
    size_t sl = fl;
    while (sl > 0 && (*s == ' ' || *s == '\t')) { s++; sl--; }
    while (sl > 0 && (s[sl-1] == ' ' || s[sl-1] == '\t' || s[sl-1] == '\r')) sl--;
    if (sl < 2) return false;
    return s[0] == '|' && s[sl-1] == '|';
}

/* PoP: split_at_paragraph_boundary @ gateway/platforms/helpers.py:split_at_paragraph_boundary */
char *helpers_md_split_at_paragraph_boundary(const char *text, size_t max_chars,
                                             char **tail_out)
{
    if (!text || !tail_out) return NULL;
    *tail_out = NULL;
    size_t total = strlen(text);
    if (total <= max_chars) {
        char *head = strdup(text);
        if (!head) return NULL;
        *tail_out = strdup("");
        return head;
    }
    size_t window_len = max_chars < total ? max_chars : total;
    const char *window_end = text + window_len;

    /* 1. Prefer the last blank line (\n\n) as paragraph boundary */
    const char *pp = window_end;
    const char *blank = NULL;
    while (pp >= text + 2) {
        if (pp[-1] == '\n' && pp[-2] == '\n') {
            blank = pp - 1; /* points at the 2nd \n of the pair */
            break;
        }
        pp--;
    }
    /* blank != NULL means blank is the position of the 2nd \n; head = text..blank+1 */
    if (blank) {
        size_t pos = (size_t)(blank - text) + 1; /* include the 2nd \n */
        char *head = (char *)malloc(pos + 1);
        if (!head) return NULL;
        memcpy(head, text, pos);
        head[pos] = '\0';
        *tail_out = strdup(text + pos);
        return head;
    }

    /* 2. Then the last newline following sentence-ending punctuation
     *    ([。！？.!?]\n) — find the LAST match end in the window.
     *    CJK chars are 3-byte UTF-8; match the full sequences. */
    int best_pos = -1;
    {
        const char *p = text;
        while (p < window_end) {
            const char *eol = memchr(p, '\n', (size_t)(window_end - p));
            if (!eol) break;
            bool sent_end = false;
            if (eol > text) {
                unsigned char c = (unsigned char)eol[-1];
                if (c == '.' || c == '!' || c == '?') {
                    sent_end = true;
                } else if (eol >= text + 3) {
                    /* 。= E3 80 82, ！= EF BC 81, ？= EF BC 9F */
                    const unsigned char *b = (const unsigned char *)eol - 3;
                    if (b[0] == 0xE3 && b[1] == 0x80 && b[2] == 0x82) sent_end = true;
                    else if (b[0] == 0xEF && b[1] == 0xBC && b[2] == 0x81) sent_end = true;
                    else if (b[0] == 0xEF && b[1] == 0xBC && b[2] == 0x9F) sent_end = true;
                }
            }
            if (sent_end)
                best_pos = (int)(eol - text) + 1; /* match end (after \n) */
            p = eol + 1;
        }
    }
    if (best_pos > 0) {
        char *head = (char *)malloc((size_t)best_pos + 1);
        if (!head) return NULL;
        memcpy(head, text, (size_t)best_pos);
        head[best_pos] = '\0';
        *tail_out = strdup(text + best_pos);
        return head;
    }

    /* 3. Fallback: last newline */
    {
        const char *p = window_end;
        while (p > text && p[-1] != '\n') p--;
        if (p > text) {
            /* p[-1] is '\n'; pos = index of '\n' */
            size_t pos = (size_t)(p - text - 1); /* index of the \n */
            char *head = (char *)malloc(pos + 1);
            if (!head) return NULL;
            memcpy(head, text, pos);
            head[pos] = '\0';
            *tail_out = strdup(text + pos + 1);
            return head;
        }
    }

    /* 4. No valid split point: force split at the window boundary */
    char *head = (char *)malloc(window_len + 1);
    if (!head) return NULL;
    memcpy(head, text, window_len);
    head[window_len] = '\0';
    *tail_out = strdup(text + window_len);
    return head;
}

/* ── atom splitting ──────────────────────────────────────────── */

/* PoP: split_markdown_atoms @ gateway/platforms/helpers.py:split_markdown_atoms */
char **helpers_md_split_markdown_atoms(const char *text)
{
    char **atoms = NULL;
    if (!text) return NULL;

    char *dup = strdup(text);
    if (!dup) return NULL;

    char **current = NULL; /* NULL-terminated current_lines */
    bool in_fence = false;

    /* Iterate lines */
    char *line = dup;
    while (line) {
        char *eol = strchr(line, '\n');
        char *line_copy;
        if (eol) {
            *eol = '\0';
            line_copy = strdup(line);
            line = eol + 1;
        } else {
            line_copy = strdup(line);
            line = NULL;
        }
        if (!line_copy) {
            free(dup);
            helpers_md_free_strlist(atoms);
            helpers_md_free_strlist(current);
            return NULL;
        }

        if (in_fence) {
            current = helpers_md_strlist_push(current, line_copy);
            /* close when a ``` line arrives and it's not the opening line */
            if (strncmp(line_copy, "```", 3) == 0 && current && current[1]) {
                in_fence = false;
                char *atom = join_lines(current);
                if (atom && text_nonblank(atom)) atoms = helpers_md_strlist_push(atoms, atom);
                free(atom);
                helpers_md_free_strlist(current);
                current = NULL;
            }
        } else if (strncmp(line_copy, "```", 3) == 0) {
            /* flush current, then start fence */
            if (current) {
                char *atom = join_lines(current);
                if (atom && text_nonblank(atom)) atoms = helpers_md_strlist_push(atoms, atom);
                free(atom);
                helpers_md_free_strlist(current);
                current = NULL;
            }
            in_fence = true;
            current = helpers_md_strlist_push(current, line_copy);
        } else if (line_is_table_row(line_copy)) {
            bool prev_is_table = false;
            if (current) {
                size_t n = 0;
                while (current[n]) n++;
                if (n > 0) prev_is_table = line_is_table_row(current[n-1]);
            }
            if (current && !prev_is_table) {
                char *atom = join_lines(current);
                if (atom && text_nonblank(atom)) atoms = helpers_md_strlist_push(atoms, atom);
                free(atom);
                helpers_md_free_strlist(current);
                current = NULL;
            }
            current = helpers_md_strlist_push(current, line_copy);
        } else if (line_is_blank(line_copy)) {
            if (current) {
                char *atom = join_lines(current);
                if (atom && text_nonblank(atom)) atoms = helpers_md_strlist_push(atoms, atom);
                free(atom);
                helpers_md_free_strlist(current);
                current = NULL;
            }
        } else {
            /* plain line: flush if current is a table */
            bool prev_is_table = false;
            if (current) {
                size_t n = 0;
                while (current[n]) n++;
                if (n > 0) prev_is_table = line_is_table_row(current[n-1]);
            }
            if (current && prev_is_table) {
                char *atom = join_lines(current);
                if (atom && text_nonblank(atom)) atoms = helpers_md_strlist_push(atoms, atom);
                free(atom);
                helpers_md_free_strlist(current);
                current = NULL;
            }
            current = helpers_md_strlist_push(current, line_copy);
        }
        free(line_copy);
    }

    /* final flush */
    if (current) {
        char *atom = join_lines(current);
        if (atom && text_nonblank(atom)) atoms = helpers_md_strlist_push(atoms, atom);
        free(atom);
        helpers_md_free_strlist(current);
    }

    free(dup);
    if (!atoms) return NULL; /* empty text → Python returns [] */
    return atoms;
}

/* ── separator inference ─────────────────────────────────────── */

/* PoP: infer_block_separator @ gateway/platforms/helpers.py:infer_block_separator */
const char *helpers_md_infer_block_separator(const char *prev_chunk,
                                             const char *next_chunk)
{
    const char *prev_trimmed = prev_chunk;
    if (prev_trimmed) while (*prev_trimmed == ' ' || *prev_trimmed == '\t' ||
                            *prev_trimmed == '\r' || *prev_trimmed == '\n') prev_trimmed++;
    size_t plen = prev_trimmed ? strlen(prev_trimmed) : 0;
    while (plen > 0 && (prev_trimmed[plen-1] == ' ' || prev_trimmed[plen-1] == '\t' ||
                        prev_trimmed[plen-1] == '\r' || prev_trimmed[plen-1] == '\n')) plen--;
    if (plen >= 3 && strncmp(prev_trimmed + plen - 3, "```", 3) == 0)
        return "\n";

    if (next_chunk && next_chunk[0]) {
        const char *nt = next_chunk;
        while (*nt == ' ' || *nt == '\t' || *nt == '\r' || *nt == '\n') nt++;
        if (nt[0] == '`' && nt[1] == '`' && nt[2] == '`')
            return "\n";
    }

    if (helpers_md_text_ends_with_table_row(prev_chunk)) {
        const char *nt = next_chunk;
        if (nt) while (*nt == ' ' || *nt == '\t' || *nt == '\r' || *nt == '\n') nt++;
        if (nt && *nt) {
            /* first line of next_chunk */
            const char *nl = strchr(nt, '\n');
            size_t fl = nl ? (size_t)(nl - nt) : strlen(nt);
            char first_line[1024];
            if (fl < sizeof(first_line)) {
                memcpy(first_line, nt, fl);
                first_line[fl] = '\0';
                const char *s = first_line;
                while (*s == ' ' || *s == '\t') s++;
                size_t sl = strlen(s);
                while (sl > 0 && (s[sl-1] == ' ' || s[sl-1] == '\t')) sl--;
                if (sl >= 2 && s[0] == '|' && s[sl-1] == '|')
                    return "\n";
            }
        }
    }

    return "\n\n";
}

/* ── merge_streaming_fences ───────────────────────────────────── */

/* PoP: merge_streaming_fences @ gateway/platforms/helpers.py:merge_streaming_fences */
char **helpers_md_merge_streaming_fences(char **chunks)
{
    if (!chunks) return NULL;
    char **result = NULL;
    size_t i = 0;
    while (chunks[i]) {
        char *current = strdup(chunks[i]);
        while (helpers_md_text_has_unclosed_fence(current) && chunks[i + 1]) {
            i++;
            const char *sep = helpers_md_infer_block_separator(current, chunks[i]);
            char *merged = (char *)malloc(strlen(current) + strlen(sep) + strlen(chunks[i]) + 1);
            if (!merged) { free(current); helpers_md_free_strlist(result); return NULL; }
            sprintf(merged, "%s%s%s", current, sep, chunks[i]);
            free(current);
            current = merged;
        }
        result = helpers_md_strlist_push(result, current);
        free(current);
        i++;
    }
    return result;
}

/* ── balance_fences_across_chunks ─────────────────────────────── */

/* PoP: balance_fences_across_chunks @ gateway/platforms/helpers.py:balance_fences_across_chunks */
char **helpers_md_balance_fences_across_chunks(char **chunks)
{
    if (!chunks) return NULL;
    size_t n = 0;
    while (chunks[n]) n++;
    if (n <= 1) {
        char **copy = NULL;
        for (size_t i = 0; i < n; i++) copy = helpers_md_strlist_push(copy, chunks[i]);
        return copy;
    }

    char **out = NULL;
    char *carry_lang = NULL;
    for (size_t ci = 0; ci < n; ci++) {
        char *chunk = chunks[ci];
        char *prefix = NULL;
        bool in_code = false;
        char lang[256];
        lang[0] = '\0';

        if (carry_lang) {
            prefix = (char *)malloc(4 + strlen(carry_lang) + 2);
            if (!prefix) { helpers_md_free_strlist(out); return NULL; }
            sprintf(prefix, "```%s\n", carry_lang);
            in_code = true;
            strncpy(lang, carry_lang, sizeof(lang) - 1);
            lang[sizeof(lang) - 1] = '\0';
        }

        /* Scan lines for fence toggling */
        char buf[8192];
        strncpy(buf, chunk, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        char *line = buf;
        while (line) {
            char *eol = strchr(line, '\n');
            if (eol) *eol = '\0';
            char *stripped = line;
            while (*stripped == ' ' || *stripped == '\t' || *stripped == '\r') stripped++;
            size_t sl = strlen(stripped);
            while (sl > 0 && (stripped[sl-1] == ' ' || stripped[sl-1] == '\t' || stripped[sl-1] == '\r')) stripped[--sl] = '\0';

            if (sl >= 3 && strncmp(stripped, "```", 3) == 0) {
                if (in_code) {
                    in_code = false;
                    lang[0] = '\0';
                } else {
                    in_code = true;
                    const char *tag = stripped + 3;
                    while (*tag == ' ' || *tag == '\t') tag++;
                    /* tag is the language (first word) */
                    char tmp[256];
                    const char *sp = tag;
                    while (*sp && *sp != ' ' && *sp != '\t' && *sp != '\r') sp++;
                    size_t tlen = (size_t)(sp - tag);
                    if (tlen >= sizeof(tmp)) tlen = sizeof(tmp) - 1;
                    memcpy(tmp, tag, tlen);
                    tmp[tlen] = '\0';
                    strncpy(lang, tmp, sizeof(lang) - 1);
                    lang[sizeof(lang) - 1] = '\0';
                }
            }
            if (!eol) break;
            line = eol + 1;
        }

        char *body = NULL;
        if (prefix) {
            body = (char *)malloc(strlen(prefix) + strlen(chunk) + 2);
            if (!body) { free(prefix); helpers_md_free_strlist(out); return NULL; }
            sprintf(body, "%s%s", prefix, chunk);
            free(prefix);
        } else {
            body = strdup(chunk);
        }

        if (in_code) {
            char *nb = (char *)malloc(strlen(body) + 5);
            if (!nb) { free(body); helpers_md_free_strlist(out); return NULL; }
            sprintf(nb, "%s\n```", body);
            free(body);
            body = nb;
            /* carry_lang = lang (empty string is NOT None — Python: carry_lang = lang) */
            if (carry_lang) free(carry_lang);
            carry_lang = strdup(lang);
        } else {
            if (carry_lang) { free(carry_lang); carry_lang = NULL; }
        }

        out = helpers_md_strlist_push(out, body);
        free(body);
    }

    if (carry_lang) free(carry_lang);
    return out;
}

/* ── greedy_pack_blocks ───────────────────────────────────────── */

/* PoP: greedy_pack_blocks @ gateway/platforms/helpers.py:greedy_pack_blocks */
char **helpers_md_greedy_pack_blocks(char **blocks, size_t max_length,
                                     const char *sep)
{
    if (!blocks) return NULL;
    const char *sepstr = sep ? sep : "\n\n";
    size_t sep_len = strlen(sepstr);

    char **packed = NULL;
    char *current = NULL;
    size_t current_len = 0;

    for (size_t i = 0; blocks[i]; i++) {
        const char *block = blocks[i];
        size_t block_len = strlen(block);
        /* candidate = current + sep + block (or just block if no current) */
        size_t candidate_len = current ? (current_len + sep_len + block_len) : block_len;
        if (candidate_len <= max_length) {
            char *nb = (char *)malloc(candidate_len + 1);
            if (!nb) { free(current); helpers_md_free_strlist(packed); return NULL; }
            if (current) sprintf(nb, "%s%s%s", current, sepstr, block);
            else strcpy(nb, block);
            free(current);
            current = nb;
            current_len = candidate_len;
            continue;
        }
        /* doesn't fit; flush current, start fresh with block */
        if (current) {
            packed = helpers_md_strlist_push(packed, current);
            free(current);
            current = NULL;
            current_len = 0;
        }
        /* block alone exceeds limit → emit as-is */
        if (block_len <= max_length) {
            current = strdup(block);
            current_len = block_len;
        } else {
            /* overflow would apply here, but Python emits as-is when no overflow fn */
            packed = helpers_md_strlist_push(packed, block);
        }
    }
    if (current) {
        packed = helpers_md_strlist_push(packed, current);
        free(current);
    }
    if (!packed) packed = helpers_md_strlist_push(NULL, "");
    return packed;
}

/* ── _chunk_markdown_paragraphs ──────────────────────────────── */

/* PoP: _chunk_markdown_paragraphs @ gateway/platforms/helpers.py:_chunk_markdown_paragraphs */
char **helpers_md_chunk_markdown_paragraphs(const char *text, size_t max_chars)
{
    if (!text) return NULL;
    size_t total = strlen(text);
    if (total <= max_chars) {
        char **r = helpers_md_strlist_push(NULL, text);
        return r;
    }

    char **chunks = NULL;
    size_t *indiv = NULL; /* indices into chunks that are indivisible */
    size_t n_indiv = 0;

    /* Phase 1: Extract atomic blocks */
    char **atoms = helpers_md_split_markdown_atoms(text);

    /* Phase 2: Greedy merge */
    char *current = NULL;
    size_t current_len = 0;
    char **parts = NULL; /* for _flush_parts */
    int parts_count = 0;

    for (size_t ai = 0; atoms && atoms[ai]; ai++) {
        const char *atom = atoms[ai];
        size_t atom_len = strlen(atom);
        size_t sep_len = parts_count > 0 ? 2 : 0;
        size_t projected = current_len + sep_len + atom_len;

        if (projected > max_chars && parts_count > 0) {
            /* flush parts */
            if (current) {
                chunks = helpers_md_strlist_push(chunks, current);
                free(current); current = NULL; current_len = 0;
            }
            /* reset parts */
            for (int i = 0; i < parts_count; i++) {
                /* parts are now in chunks */
            }
            parts_count = 0;
            sep_len = 0;
        }

        if (parts_count == 0 && atom_len > max_chars &&
            (helpers_md_is_fence_atom(atom) || helpers_md_is_table_atom(atom))) {
            /* indivisible oversized atom */
            chunks = helpers_md_strlist_push(chunks, atom);
            /* mark last as indivisible */
            size_t n = 0; while (chunks[n]) n++;
            /* record index n-1 as indivisible */
            indiv = (size_t *)realloc(indiv, (n_indiv + 1) * sizeof(size_t));
            if (!indiv) { free(current); helpers_md_free_strlist(chunks); helpers_md_free_strlist(atoms); return NULL; }
            indiv[n_indiv++] = n - 1;
            continue;
        }

        /* add to current */
        size_t new_len = (parts_count > 0) ? (current_len + 2 + atom_len) : atom_len;
        char *nc = (char *)malloc(new_len + 1);
        if (!nc) { free(current); helpers_md_free_strlist(chunks); helpers_md_free_strlist(atoms); free(indiv); return NULL; }
        if (parts_count > 0) sprintf(nc, "%s\n\n%s", current, atom);
        else strcpy(nc, atom);
        free(current);
        current = nc;
        current_len = new_len;
        parts_count++;
    }
    /* final flush */
    if (current) {
        chunks = helpers_md_strlist_push(chunks, current);
        free(current);
    }

    /* Phase 3: Split still-oversized chunks at paragraph boundaries */
    char **result = NULL;
    for (size_t i = 0; chunks && chunks[i]; i++) {
        char *chunk = chunks[i];
        size_t cl = strlen(chunk);
        bool oversized = cl > max_chars;
        bool is_indiv = false;
        for (size_t j = 0; j < n_indiv; j++) if (indiv[j] == i) { is_indiv = true; break; }

        if (!oversized || is_indiv || helpers_md_text_has_unclosed_fence(chunk)) {
            result = helpers_md_strlist_push(result, chunk);
            continue;
        }

        char *remaining = strdup(chunk);
        while (remaining && strlen(remaining) > max_chars) {
            char *tail = NULL;
            char *head = helpers_md_split_at_paragraph_boundary(remaining, max_chars, &tail);
            if (!head) break;
            if (head && strlen(head) == 0) {
                free(head);
                head = strndup(remaining, max_chars);
                tail = strdup(remaining + max_chars);
            }
            if (head && strlen(head) > 0) result = helpers_md_strlist_push(result, head);
            free(head);
            free(remaining);
            remaining = tail;
        }
        if (remaining && strlen(remaining) > 0) result = helpers_md_strlist_push(result, remaining);
        free(remaining);
    }

    /* Phase 4: Merge small trailing/leading chunks */
    char **merged = NULL;
    for (size_t i = 0; result && result[i]; i++) {
        char *chunk = result[i];
        if (merged) {
            /* find last element of merged */
            size_t last = 0;
            while (merged[last]) last++;
            if (last > 0) {
                char *prev = merged[last - 1];
                size_t combined_len = strlen(prev) + 2 + strlen(chunk);
                if (combined_len <= max_chars) {
                    char *nb = (char *)malloc(combined_len + 1);
                    if (!nb) { merged = helpers_md_strlist_push(merged, chunk); continue; }
                    sprintf(nb, "%s\n\n%s", prev, chunk);
                    free(prev);
                    /* pop last element (it was prev, now freed) */
                    merged[last - 1] = NULL;
                    /* push the combined */
                    merged = helpers_md_strlist_push(merged, nb);
                    free(nb);
                    continue;
                }
            }
        }
        merged = helpers_md_strlist_push(merged, chunk);
    }

    helpers_md_free_strlist(atoms);
    helpers_md_free_strlist(chunks);
    free(indiv);
    return merged ? merged : helpers_md_strlist_push(NULL, "");
}

/* ── _chunk_newline_preferred ─────────────────────────────────── */

/* PoP: _chunk_newline_preferred @ gateway/platforms/helpers.py:_chunk_newline_preferred */
/* len_fn: NULL means strlen (codepoint count). Caller frees the returned list. */
char **helpers_md_chunk_newline_preferred(const char *text, size_t limit,
                                          size_t (*len_fn)(const char *))
{
    if (!text) return NULL;
    if (!len_fn) len_fn = (size_t (*)(const char *))strlen;

    size_t total = len_fn(text);
    if (total <= limit) {
        return helpers_md_strlist_push(NULL, text);
    }

    /* Reserve headroom for fence close/reopen markers when text has ```. */
    size_t split_limit = limit;
    if (strstr(text, "```")) {
        /* max(limit - 16, limit // 2, 1) */
        size_t candidate = limit > 16 ? limit - 16 : 0;
        size_t half = limit / 2;
        if (candidate < half) candidate = half;
        if (candidate < 1) candidate = 1;
        split_limit = candidate;
    }

    /* _cp_budget = _custom_unit_to_cp(remaining, split_limit, len_fn)
     * which finds the largest codepoint offset n s.t. len_fn(remaining[:n]) <= split_limit.
     * With len_fn = strlen, this is just min(strlen, split_limit). */
    char **chunks = NULL;
    char *remaining = strdup(text);
    if (!remaining) return NULL;

    while (len_fn(remaining) > split_limit) {
        /* _cp_budget: largest n s.t. len_fn(remaining[:n]) <= split_limit */
        size_t lo = 0, hi = strlen(remaining);
        while (lo < hi) {
            size_t mid = (lo + hi + 1) / 2;
            char saved = remaining[mid];
            remaining[mid] = '\0';
            size_t ln = len_fn(remaining);
            remaining[mid] = saved;
            if (ln <= split_limit) lo = mid;
            else hi = mid - 1;
        }
        size_t cp_budget = lo;
        /* split_at = remaining.rfind("\n", 0, _cp_budget) */
        size_t split_at = (size_t)-1;
        for (size_t j = cp_budget; j > 0; j--) {
            if (remaining[j-1] == '\n') { split_at = j - 1; break; }
        }
        /* if split_at < _cp_budget // 2: split_at = _cp_budget */
        if (split_at == (size_t)-1 || split_at < cp_budget / 2)
            split_at = cp_budget;

        char *chunk = strndup(remaining, split_at);
        if (!chunk) { free(remaining); helpers_md_free_strlist(chunks); return NULL; }
        chunks = helpers_md_strlist_push(chunks, chunk);
        free(chunk);

        /* remaining = remaining[split_at:].lstrip("\n") */
        char *new_remaining = strdup(remaining + split_at);
        free(remaining);
        remaining = new_remaining;
        while (remaining && (*remaining == '\n' || *remaining == '\r')) remaining++;
        /* but we strduplicated, so we need to shift */
        if (new_remaining != remaining) {
            size_t shift = remaining - new_remaining;
            char *shifted = strdup(remaining);
            free(new_remaining);
            remaining = shifted;
        }
    }
    if (remaining && len_fn(remaining) > 0) {
        chunks = helpers_md_strlist_push(chunks, remaining);
    }
    free(remaining);
    if (!chunks) chunks = helpers_md_strlist_push(NULL, "");
    return chunks;
}

/* ── split_text_fence_aware ───────────────────────────────────── */

/* PoP: split_text_fence_aware @ gateway/platforms/helpers.py:split_text_fence_aware */
char **helpers_md_split_text_fence_aware(const char *text, size_t limit,
                                         int prefer_paragraphs,
                                         int balance_fences)
{
    if (!text || !*text) return NULL;
    size_t total = strlen(text);
    if (total <= limit) {
        return helpers_md_strlist_push(NULL, text);
    }

    char **chunks = NULL;
    if (prefer_paragraphs) {
        chunks = helpers_md_chunk_markdown_paragraphs(text, limit);
    } else {
        chunks = helpers_md_chunk_newline_preferred(text, limit, NULL);
    }

    if (balance_fences && chunks) {
        char **balanced = helpers_md_balance_fences_across_chunks(chunks);
        helpers_md_free_strlist(chunks);
        chunks = balanced;
    }

    if (!chunks) chunks = helpers_md_strlist_push(NULL, "");
    return chunks;
}
