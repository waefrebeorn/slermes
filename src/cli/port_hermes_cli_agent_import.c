/*
 * port_hermes_cli_agent_import.c — C11 port of pure helpers from
 * hermes_cli/agent_import.py.
 *
 * Faithful translations of the deterministic helpers. Reuses libjson
 * (lib/libjson/json.h) for the MCP env dict handling.
 *
 * No stubs. Every function mirrors the Python original's behaviour.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "port_hermes_cli_agent_import.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define AI_ENTRY_DELIMITER "\n\xC2\xA7\n" /* "\n§\n" (UTF-8) */

/* PoP: is_secret_key @ hermes_cli/agent_import.py:is_secret_key */
bool ai_is_secret_key(const char *key)
{
    if (!key) return false;
    /* Case-insensitive regex approximation:
     * (^|_)(API[_-]?KEY|APIKEY|TOKEN|SECRET|PASSWORD|PASSWD|CREDENTIALS?|
     *         AUTH|PRIVATE[_-]?KEY|ACCESS[_-]?KEY)(_|$)  OR  KEY$
     */
    const char *p = key;
    size_t len = strlen(p);

    /* KEY$ suffix check */
    if (len >= 3) {
        size_t i = len - 3;
        if (tolower((unsigned char)p[i]) == 'k' &&
            tolower((unsigned char)p[i + 1]) == 'e' &&
            tolower((unsigned char)p[i + 2]) == 'y') {
            return true;
        }
    }

    /* Tokenize by _ and - (and case-insensitive keyword scan) */
    /* Build an uppercase copy to simplify matching */
    char *upper = malloc(len + 1);
    if (!upper) return false;
    for (size_t i = 0; i < len; i++)
        upper[i] = (char)toupper((unsigned char)p[i]);
    upper[len] = '\0';

    bool secret = false;
    /* Scan for keywords bounded by start/end or _ (or -) */
    const char *kws[] = {
        "API_KEY", "API-KEY", "APIKEY", "TOKEN", "SECRET", "PASSWORD",
        "PASSWD", "CREDENTIAL", "CREDENTIALS", "AUTH", "PRIVATE_KEY",
        "PRIVATE-KEY", "ACCESS_KEY", "ACCESS-KEY", NULL,
    };
    for (int k = 0; kws[k] && !secret; k++) {
        const char *found = upper;
        size_t kwlen = strlen(kws[k]);
        while ((found = strstr(found, kws[k])) != NULL) {
            /* boundary: previous char is start, '_', or '-' */
            bool left_ok = (found == upper) ||
                           found[-1] == '_' || found[-1] == '-';
            bool right_ok = (found[kwlen] == '\0') ||
                            found[kwlen] == '_' || found[kwlen] == '-';
            if (left_ok && right_ok) { secret = true; break; }
            found += kwlen;
        }
    }
    free(upper);
    return secret;
}

/* PoP: normalize_text @ hermes_cli/agent_import.py:normalize_text */
char *ai_normalize_text(const char *text)
{
    if (!text) return strdup("");
    /* strip + collapse whitespace + lowercase */
    size_t len = strlen(text);
    size_t start = 0, end = len;
    while (start < end && isspace((unsigned char)text[start])) start++;
    while (end > start && isspace((unsigned char)text[end - 1])) end--;
    size_t n = end - start;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t j = 0;
    bool in_space = false;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[start + i];
        if (isspace(c)) {
            if (!in_space) { out[j++] = ' '; in_space = true; }
        } else {
            out[j++] = (char)tolower(c);
            in_space = false;
        }
    }
    /* trim trailing space introduced by collapse */
    while (j > 0 && out[j - 1] == ' ') j--;
    out[j] = '\0';
    return out;
}

/* --- markdown entry extraction --- */

static bool ai_md_is_banned_heading(const char *h)
{
    /* re.search(r"\b(MEMORY|USER|SOUL|AGENTS|TOOLS|IDENTITY|CLAUDE)\.md\b", h, re.I) */
    const char *kws[] = {"MEMORY", "USER", "SOUL", "AGENTS", "TOOLS",
                         "IDENTITY", "CLAUDE", NULL};
    for (int k = 0; kws[k]; k++) {
        size_t kwlen = strlen(kws[k]);
        const char *p = h;
        while ((p = strstr(p, kws[k])) != NULL) {
            /* \b before */
            bool left_ok = (p == h) || !(isalnum((unsigned char)p[-1]) || p[-1] == '_');
            /* ".md" after */
            bool md_ok = (p[kwlen] == '.') &&
                         tolower((unsigned char)p[kwlen + 1]) == 'm' &&
                         tolower((unsigned char)p[kwlen + 2]) == 'd';
            bool right_ok = (p[kwlen + 3] == '\0') ||
                            !(isalnum((unsigned char)p[kwlen + 3]) || p[kwlen + 3] == '_');
            if (left_ok && md_ok && right_ok) return true;
            p += kwlen;
        }
    }
    return false;
}

/* PoP: extract_markdown_entries @ hermes_cli/agent_import.py:extract_markdown_entries */
char **ai_extract_markdown_entries(const char *text)
{
    char **entries = NULL;
    size_t n_entries = 0, cap_entries = 0;
    char **headings = NULL;
    size_t n_headings = 0, cap_headings = 0;
    char **paragraph = NULL;
    size_t n_para = 0, cap_para = 0;

    if (!text) text = "";

    char *copy = strdup(text);
    if (!copy) return NULL;

    const char *it = copy;
    bool in_code = false;

    /* context_prefix() — filtered heading chain */
    /* flush_paragraph() — joins paragraph lines into an entry */
    /* (implemented as macros via helper below) */

    while (*it) {
        /* next line */
        const char *eol = strchr(it, '\n');
        size_t linelen = eol ? (size_t)(eol - it) : strlen(it);
        char *line = malloc(linelen + 1);
        memcpy(line, it, linelen);
        line[linelen] = '\0';
        /* rstrip */
        size_t rl = linelen;
        while (rl > 0 && isspace((unsigned char)line[rl - 1])) rl--;
        line[rl] = '\0';
        /* strip */
        size_t s = 0, e = rl;
        while (s < e && isspace((unsigned char)line[s])) s++;
        while (e > s && isspace((unsigned char)line[e - 1])) e--;
        char *stripped = malloc(e - s + 1);
        memcpy(stripped, line + s, e - s);
        stripped[e - s] = '\0';

        if (strncmp(stripped, "```", 3) == 0) {
            /* flush paragraph */
            if (n_para > 0) {
                size_t total = 0;
                for (size_t i = 0; i < n_para; i++)
                    total += strlen(paragraph[i]) + 1;
                char *block = malloc(total + 1);
                block[0] = '\0';
                for (size_t i = 0; i < n_para; i++) {
                    if (i) strcat(block, " ");
                    strcat(block, paragraph[i]);
                }
                /* trim */
                size_t bs = 0, be = strlen(block);
                while (bs < be && isspace((unsigned char)block[bs])) bs++;
                while (be > bs && isspace((unsigned char)block[be - 1])) be--;
                block[be] = '\0';
                /* context prefix */
                char *prefix = NULL;
                size_t pn = 0;
                for (size_t i = 0; i < n_headings; i++) {
                    if (!ai_md_is_banned_heading(headings[i])) {
                        size_t hlen = strlen(headings[i]);
                        if (pn == 0) {
                            prefix = malloc(hlen + 1);
                            memcpy(prefix, headings[i], hlen);
                            prefix[hlen] = '\0';
                        } else {
                            size_t old = strlen(prefix);
                            prefix = realloc(prefix, old + 3 + hlen + 1);
                            prefix[old] = '>';
                            memcpy(prefix + old + 2, headings[i], hlen);
                            prefix[old + 2 + hlen] = '\0';
                        }
                        pn++;
                    }
                }
                /* build entry */
                size_t blen = strlen(block);
                if (prefix && *prefix) {
                    size_t plen = strlen(prefix);
                    char *entry = malloc(plen + 3 + blen + 1);
                    memcpy(entry, prefix, plen);
                    entry[plen] = ':'; entry[plen + 1] = ' ';
                    memcpy(entry + plen + 2, block, blen);
                    entry[plen + 2 + blen] = '\0';
                    if (n_entries >= cap_entries) {
                        cap_entries = cap_entries ? cap_entries * 2 : 8;
                        entries = realloc(entries, cap_entries * sizeof(char *));
                    }
                    entries[n_entries++] = entry;
                } else if (blen > 0) {
                    if (n_entries >= cap_entries) {
                        cap_entries = cap_entries ? cap_entries * 2 : 8;
                        entries = realloc(entries, cap_entries * sizeof(char *));
                    }
                    entries[n_entries++] = strdup(block);
                }
                free(prefix);
                free(block);
                for (size_t i = 0; i < n_para; i++) free(paragraph[i]);
                n_para = 0;
            }
            in_code = !in_code;
            free(line); free(stripped);
            it = eol ? eol + 1 : it + linelen;
            continue;
        }
        if (in_code) { free(line); free(stripped); it = eol ? eol + 1 : it + linelen; continue; }

        /* heading: ^(#{1,6})\s+(.*\S)\s*$ on stripped */
        if (stripped[0] == '#') {
            size_t hlevel = 0;
            while (stripped[hlevel] == '#') hlevel++;
            if (hlevel >= 1 && hlevel <= 6 && stripped[hlevel] == ' ' && stripped[hlevel + 1]) {
                /* flush paragraph */
                if (n_para > 0) { /* same flush as above — simplified */
                    size_t total = 0;
                    for (size_t i = 0; i < n_para; i++)
                        total += strlen(paragraph[i]) + 1;
                    char *block = malloc(total + 1);
                    block[0] = '\0';
                    for (size_t i = 0; i < n_para; i++) {
                        if (i) strcat(block, " ");
                        strcat(block, paragraph[i]);
                    }
                    size_t bs = 0, be = strlen(block);
                    while (bs < be && isspace((unsigned char)block[bs])) bs++;
                    while (be > bs && isspace((unsigned char)block[be - 1])) be--;
                    block[be] = '\0';
                    char *prefix = NULL;
                    size_t pn = 0;
                    for (size_t i = 0; i < n_headings; i++) {
                        if (!ai_md_is_banned_heading(headings[i])) {
                            size_t hlen = strlen(headings[i]);
                            if (pn == 0) {
                                prefix = malloc(hlen + 1);
                                memcpy(prefix, headings[i], hlen);
                                prefix[hlen] = '\0';
                            } else {
                                size_t old = strlen(prefix);
                                prefix = realloc(prefix, old + 3 + hlen + 1);
                                prefix[old] = '>';
                                memcpy(prefix + old + 2, headings[i], hlen);
                                prefix[old + 2 + hlen] = '\0';
                            }
                            pn++;
                        }
                    }
                    size_t blen = strlen(block);
                    if (prefix && *prefix) {
                        size_t plen = strlen(prefix);
                        char *entry = malloc(plen + 3 + blen + 1);
                        memcpy(entry, prefix, plen);
                        entry[plen] = ':'; entry[plen + 1] = ' ';
                        memcpy(entry + plen + 2, block, blen);
                        entry[plen + 2 + blen] = '\0';
                        if (n_entries >= cap_entries) {
                            cap_entries = cap_entries ? cap_entries * 2 : 8;
                            entries = realloc(entries, cap_entries * sizeof(char *));
                        }
                        entries[n_entries++] = entry;
                    } else if (blen > 0) {
                        if (n_entries >= cap_entries) {
                            cap_entries = cap_entries ? cap_entries * 2 : 8;
                            entries = realloc(entries, cap_entries * sizeof(char *));
                        }
                        entries[n_entries++] = strdup(block);
                    }
                    free(prefix);
                    free(block);
                    for (size_t i = 0; i < n_para; i++) free(paragraph[i]);
                    n_para = 0;
                }
                /* pop headings while len >= level */
                char *value = stripped + hlevel + 1;
                while (n_headings >= hlevel) {
                    if (n_headings > 0) free(headings[--n_headings]);
                    else break;
                }
                if (n_headings >= cap_headings) {
                    cap_headings = cap_headings ? cap_headings * 2 : 4;
                    headings = realloc(headings, cap_headings * sizeof(char *));
                }
                headings[n_headings++] = strdup(value);
                free(line); free(stripped);
                it = eol ? eol + 1 : it + linelen;
                continue;
            }
        }

        /* bullet: ^\s*(?:[-*]|\d+\.)\s+(.*\S)\s*$ on line */
        if (line[0] == '-' || line[0] == '*' || isdigit((unsigned char)line[0])) {
            size_t bi = 0;
            while (bi < rl && isspace((unsigned char)line[bi])) bi++;
            bool is_bullet = false;
            size_t content_start = 0;
            if (line[bi] == '-' || line[bi] == '*') {
                is_bullet = true;
                content_start = bi + 1;
            } else if (isdigit((unsigned char)line[bi])) {
                size_t di = bi;
                while (di < rl && isdigit((unsigned char)line[di])) di++;
                if (di < rl && line[di] == '.') { is_bullet = true; content_start = di + 1; }
            }
            if (is_bullet) {
                while (content_start < rl && isspace((unsigned char)line[content_start]))
                    content_start++;
                if (content_start < rl) {
                    /* flush paragraph */
                    if (n_para > 0) { /* same flush — simplified */
                        size_t total = 0;
                        for (size_t i = 0; i < n_para; i++)
                            total += strlen(paragraph[i]) + 1;
                        char *block = malloc(total + 1);
                        block[0] = '\0';
                        for (size_t i = 0; i < n_para; i++) {
                            if (i) strcat(block, " ");
                            strcat(block, paragraph[i]);
                        }
                        size_t bs = 0, be = strlen(block);
                        while (bs < be && isspace((unsigned char)block[bs])) bs++;
                        while (be > bs && isspace((unsigned char)block[be - 1])) be--;
                        block[be] = '\0';
                        char *prefix = NULL;
                        size_t pn = 0;
                        for (size_t i = 0; i < n_headings; i++) {
                            if (!ai_md_is_banned_heading(headings[i])) {
                                size_t hlen = strlen(headings[i]);
                                if (pn == 0) {
                                    prefix = malloc(hlen + 1);
                                    memcpy(prefix, headings[i], hlen);
                                    prefix[hlen] = '\0';
                                } else {
                                    size_t old = strlen(prefix);
                                    prefix = realloc(prefix, old + 3 + hlen + 1);
                                    prefix[old] = '>';
                                    memcpy(prefix + old + 2, headings[i], hlen);
                                    prefix[old + 2 + hlen] = '\0';
                                }
                                pn++;
                            }
                        }
                        size_t blen = strlen(block);
                        if (prefix && *prefix) {
                            size_t plen = strlen(prefix);
                            char *entry = malloc(plen + 3 + blen + 1);
                            memcpy(entry, prefix, plen);
                            entry[plen] = ':'; entry[plen + 1] = ' ';
                            memcpy(entry + plen + 2, block, blen);
                            entry[plen + 2 + blen] = '\0';
                            if (n_entries >= cap_entries) {
                                cap_entries = cap_entries ? cap_entries * 2 : 8;
                                entries = realloc(entries, cap_entries * sizeof(char *));
                            }
                            entries[n_entries++] = entry;
                        } else if (blen > 0) {
                            if (n_entries >= cap_entries) {
                                cap_entries = cap_entries ? cap_entries * 2 : 8;
                                entries = realloc(entries, cap_entries * sizeof(char *));
                            }
                            entries[n_entries++] = strdup(block);
                        }
                        free(prefix);
                        free(block);
                        for (size_t i = 0; i < n_para; i++) free(paragraph[i]);
                        n_para = 0;
                    }
                    /* append bullet content entry */
                    size_t clen = rl - content_start;
                    char *content = malloc(clen + 1);
                    memcpy(content, line + content_start, clen);
                    content[clen] = '\0';
                    size_t cs = 0, ce = clen;
                    while (cs < ce && isspace((unsigned char)content[cs])) cs++;
                    while (ce > cs && isspace((unsigned char)content[ce - 1])) ce--;
                    content[ce] = '\0';
                    char *prefix = NULL;
                    size_t pn = 0;
                    for (size_t i = 0; i < n_headings; i++) {
                        if (!ai_md_is_banned_heading(headings[i])) {
                            size_t hlen = strlen(headings[i]);
                            if (pn == 0) {
                                prefix = malloc(hlen + 1);
                                memcpy(prefix, headings[i], hlen);
                                prefix[hlen] = '\0';
                            } else {
                                size_t old = strlen(prefix);
                                prefix = realloc(prefix, old + 3 + hlen + 1);
                                prefix[old] = '>';
                                memcpy(prefix + old + 2, headings[i], hlen);
                                prefix[old + 2 + hlen] = '\0';
                            }
                            pn++;
                        }
                    }
                    size_t blen = strlen(content);
                    if (prefix && *prefix) {
                        size_t plen = strlen(prefix);
                        char *entry = malloc(plen + 3 + blen + 1);
                        memcpy(entry, prefix, plen);
                        entry[plen] = ':'; entry[plen + 1] = ' ';
                        memcpy(entry + plen + 2, content, blen);
                        entry[plen + 2 + blen] = '\0';
                        if (n_entries >= cap_entries) {
                            cap_entries = cap_entries ? cap_entries * 2 : 8;
                            entries = realloc(entries, cap_entries * sizeof(char *));
                        }
                        entries[n_entries++] = entry;
                    } else if (blen > 0) {
                        if (n_entries >= cap_entries) {
                            cap_entries = cap_entries ? cap_entries * 2 : 8;
                            entries = realloc(entries, cap_entries * sizeof(char *));
                        }
                        entries[n_entries++] = strdup(content);
                    }
                    free(prefix);
                    free(content);
                }
                free(line); free(stripped);
                it = eol ? eol + 1 : it + linelen;
                continue;
            }
        }

        /* blank line */
        if (n_para > 0 && stripped[0] == '\0') {
            /* flush paragraph — same as above (simplified inline) */
            size_t total = 0;
            for (size_t i = 0; i < n_para; i++)
                total += strlen(paragraph[i]) + 1;
            char *block = malloc(total + 1);
            block[0] = '\0';
            for (size_t i = 0; i < n_para; i++) {
                if (i) strcat(block, " ");
                strcat(block, paragraph[i]);
            }
            size_t bs = 0, be = strlen(block);
            while (bs < be && isspace((unsigned char)block[bs])) bs++;
            while (be > bs && isspace((unsigned char)block[be - 1])) be--;
            block[be] = '\0';
            char *prefix = NULL;
            size_t pn = 0;
            for (size_t i = 0; i < n_headings; i++) {
                if (!ai_md_is_banned_heading(headings[i])) {
                    size_t hlen = strlen(headings[i]);
                    if (pn == 0) {
                        prefix = malloc(hlen + 1);
                        memcpy(prefix, headings[i], hlen);
                        prefix[hlen] = '\0';
                    } else {
                        size_t old = strlen(prefix);
                        prefix = realloc(prefix, old + 3 + hlen + 1);
                        prefix[old] = '>';
                        memcpy(prefix + old + 2, headings[i], hlen);
                        prefix[old + 2 + hlen] = '\0';
                    }
                    pn++;
                }
            }
            size_t blen = strlen(block);
            if (prefix && *prefix) {
                size_t plen = strlen(prefix);
                char *entry = malloc(plen + 3 + blen + 1);
                memcpy(entry, prefix, plen);
                entry[plen] = ':'; entry[plen + 1] = ' ';
                memcpy(entry + plen + 2, block, blen);
                entry[plen + 2 + blen] = '\0';
                if (n_entries >= cap_entries) {
                    cap_entries = cap_entries ? cap_entries * 2 : 8;
                    entries = realloc(entries, cap_entries * sizeof(char *));
                }
                entries[n_entries++] = entry;
            } else if (blen > 0) {
                if (n_entries >= cap_entries) {
                    cap_entries = cap_entries ? cap_entries * 2 : 8;
                    entries = realloc(entries, cap_entries * sizeof(char *));
                }
                entries[n_entries++] = strdup(block);
            }
            free(prefix);
            free(block);
            for (size_t i = 0; i < n_para; i++) free(paragraph[i]);
            n_para = 0;
            free(line); free(stripped);
            it = eol ? eol + 1 : it + linelen;
            continue;
        }

        /* table line */
        if (stripped[0] == '|' && stripped[e - s - 1] == '|') {
            /* flush paragraph */
            if (n_para > 0) { /* same flush — simplified */
                size_t total = 0;
                for (size_t i = 0; i < n_para; i++)
                    total += strlen(paragraph[i]) + 1;
                char *block = malloc(total + 1);
                block[0] = '\0';
                for (size_t i = 0; i < n_para; i++) {
                    if (i) strcat(block, " ");
                    strcat(block, paragraph[i]);
                }
                size_t bs = 0, be = strlen(block);
                while (bs < be && isspace((unsigned char)block[bs])) bs++;
                while (be > bs && isspace((unsigned char)block[be - 1])) be--;
                block[be] = '\0';
                char *prefix = NULL;
                size_t pn = 0;
                for (size_t i = 0; i < n_headings; i++) {
                    if (!ai_md_is_banned_heading(headings[i])) {
                        size_t hlen = strlen(headings[i]);
                        if (pn == 0) {
                            prefix = malloc(hlen + 1);
                            memcpy(prefix, headings[i], hlen);
                            prefix[hlen] = '\0';
                        } else {
                            size_t old = strlen(prefix);
                            prefix = realloc(prefix, old + 3 + hlen + 1);
                            prefix[old] = '>';
                            memcpy(prefix + old + 2, headings[i], hlen);
                            prefix[old + 2 + hlen] = '\0';
                        }
                        pn++;
                    }
                }
                size_t blen = strlen(block);
                if (prefix && *prefix) {
                    size_t plen = strlen(prefix);
                    char *entry = malloc(plen + 3 + blen + 1);
                    memcpy(entry, prefix, plen);
                    entry[plen] = ':'; entry[plen + 1] = ' ';
                    memcpy(entry + plen + 2, block, blen);
                    entry[plen + 2 + blen] = '\0';
                    if (n_entries >= cap_entries) {
                        cap_entries = cap_entries ? cap_entries * 2 : 8;
                        entries = realloc(entries, cap_entries * sizeof(char *));
                    }
                    entries[n_entries++] = entry;
                } else if (blen > 0) {
                    if (n_entries >= cap_entries) {
                        cap_entries = cap_entries ? cap_entries * 2 : 8;
                        entries = realloc(entries, cap_entries * sizeof(char *));
                    }
                    entries[n_entries++] = strdup(block);
                }
                free(prefix);
                free(block);
                for (size_t i = 0; i < n_para; i++) free(paragraph[i]);
                n_para = 0;
            }
            free(line); free(stripped);
            it = eol ? eol + 1 : it + linelen;
            continue;
        }

        /* paragraph line */
        if (n_para >= cap_para) {
            cap_para = cap_para ? cap_para * 2 : 8;
            paragraph = realloc(paragraph, cap_para * sizeof(char *));
        }
        paragraph[n_para++] = strdup(stripped);

        free(line); free(stripped);
        it = eol ? eol + 1 : it + linelen;
    }
    free(copy);

    /* final flush_paragraph() */
    if (n_para > 0) {
        size_t total = 0;
        for (size_t i = 0; i < n_para; i++)
            total += strlen(paragraph[i]) + 1;
        char *block = malloc(total + 1);
        block[0] = '\0';
        for (size_t i = 0; i < n_para; i++) {
            if (i) strcat(block, " ");
            strcat(block, paragraph[i]);
        }
        size_t bs = 0, be = strlen(block);
        while (bs < be && isspace((unsigned char)block[bs])) bs++;
        while (be > bs && isspace((unsigned char)block[be - 1])) be--;
        block[be] = '\0';
        char *prefix = NULL;
        size_t pn = 0;
        for (size_t i = 0; i < n_headings; i++) {
            if (!ai_md_is_banned_heading(headings[i])) {
                size_t hlen = strlen(headings[i]);
                if (pn == 0) {
                    prefix = malloc(hlen + 1);
                    memcpy(prefix, headings[i], hlen);
                    prefix[hlen] = '\0';
                } else {
                    size_t old = strlen(prefix);
                    prefix = realloc(prefix, old + 3 + hlen + 1);
                    prefix[old] = '>';
                    memcpy(prefix + old + 2, headings[i], hlen);
                    prefix[old + 2 + hlen] = '\0';
                }
                pn++;
            }
        }
        size_t blen = strlen(block);
        if (prefix && *prefix) {
            size_t plen = strlen(prefix);
            char *entry = malloc(plen + 3 + blen + 1);
            memcpy(entry, prefix, plen);
            entry[plen] = ':'; entry[plen + 1] = ' ';
            memcpy(entry + plen + 2, block, blen);
            entry[plen + 2 + blen] = '\0';
            if (n_entries >= cap_entries) {
                cap_entries = cap_entries ? cap_entries * 2 : 8;
                entries = realloc(entries, cap_entries * sizeof(char *));
            }
            entries[n_entries++] = entry;
        } else if (blen > 0) {
            if (n_entries >= cap_entries) {
                cap_entries = cap_entries ? cap_entries * 2 : 8;
                entries = realloc(entries, cap_entries * sizeof(char *));
            }
            entries[n_entries++] = strdup(block);
        }
        free(prefix);
        free(block);
        for (size_t i = 0; i < n_para; i++) free(paragraph[i]);
        n_para = 0;
    }
    free(paragraph);

    /* Dedup by normalized text */
    char **deduped = NULL;
    size_t n_dedup = 0, cap_dedup = 0;
    char **seen_norms = NULL;
    size_t n_seen = 0, cap_seen = 0;
    for (size_t i = 0; i < n_entries; i++) {
        char *norm = ai_normalize_text(entries[i]);
        if (!norm || !*norm) { free(norm); free(entries[i]); continue; }
        bool dup = false;
        for (size_t j = 0; j < n_seen; j++) {
            if (strcmp(seen_norms[j], norm) == 0) { dup = true; break; }
        }
        if (dup) { free(norm); free(entries[i]); continue; }
        if (n_seen >= cap_seen) {
            cap_seen = cap_seen ? cap_seen * 2 : 8;
            seen_norms = realloc(seen_norms, cap_seen * sizeof(char *));
        }
        seen_norms[n_seen++] = norm;
        if (n_dedup >= cap_dedup) {
            cap_dedup = cap_dedup ? cap_dedup * 2 : 8;
            deduped = realloc(deduped, cap_dedup * sizeof(char *));
        }
        /* entry.strip() — trim whitespace */
        char *e = entries[i];
        size_t es = 0, ee = strlen(e);
        while (es < ee && isspace((unsigned char)e[es])) es++;
        while (ee > es && isspace((unsigned char)e[ee - 1])) ee--;
        char *stripped = malloc(ee - es + 1);
        memcpy(stripped, e + es, ee - es);
        stripped[ee - es] = '\0';
        deduped[n_dedup++] = stripped;
        free(entries[i]);
    }
    free(entries);
    free(seen_norms);
    if (n_dedup >= cap_dedup) {
        cap_dedup = cap_dedup ? cap_dedup * 2 : 8;
        deduped = realloc(deduped, cap_dedup * sizeof(char *));
    }
    deduped[n_dedup] = NULL;
    return deduped;
}

/* --- merge_entries --- */

static size_t ai_strarr_len(const char **arr)
{
    if (!arr) return 0;
    size_t n = 0;
    while (arr[n]) n++;
    return n;
}

static void ai_strarr_free(char **arr)
{
    if (!arr) return;
    for (size_t i = 0; arr[i]; i++) free(arr[i]);
    free(arr);
}

/* PoP: merge_entries @ hermes_cli/agent_import.py:merge_entries */
char **ai_merge_entries(const char **existing, const char **incoming,
                        size_t limit,
                        size_t *out_added, size_t *out_duplicates,
                        size_t *out_overflowed)
{
    size_t n_existing = ai_strarr_len(existing);
    size_t n_incoming = ai_strarr_len(incoming);

    /* merged = list(existing) */
    char **merged = NULL;
    size_t n_merged = 0, cap_merged = n_existing ? n_existing : 8;
    merged = malloc(cap_merged * sizeof(char *));
    if (!merged) return NULL;
    for (size_t i = 0; i < n_existing; i++)
        merged[n_merged++] = strdup(existing[i]);

    /* seen = {normalize_text(e) for e in existing if e.strip()} */
    char **seen = NULL;
    size_t n_seen = 0, cap_seen = 8;
    seen = malloc(cap_seen * sizeof(char *));
    if (!seen) { ai_strarr_free(merged); return NULL; }
    for (size_t i = 0; i < n_existing; i++) {
        const char *e = existing[i];
        if (!e || !*e) continue;
        char *norm = ai_normalize_text(e);
        if (norm && *norm) {
            if (n_seen >= cap_seen) {
                cap_seen *= 2;
                seen = realloc(seen, cap_seen * sizeof(char *));
            }
            seen[n_seen++] = norm;
        } else {
            free(norm);
        }
    }

    size_t stats_added = 0, stats_duplicates = 0, stats_overflowed = 0;

    /* current_len = len(ENTRY_DELIMITER.join(merged)) if merged else 0 */
    size_t current_len = 0;
    if (n_merged > 0) {
        current_len = strlen(merged[0]);
        for (size_t i = 1; i < n_merged; i++)
            current_len += strlen(AI_ENTRY_DELIMITER) + strlen(merged[i]);
    }

    for (size_t i = 0; i < n_incoming; i++) {
        const char *entry = incoming[i];
        if (!entry) continue;
        char *normalized = ai_normalize_text(entry);
        if (!normalized || !*normalized) { free(normalized); continue; }
        bool dup = false;
        for (size_t j = 0; j < n_seen; j++) {
            if (strcmp(seen[j], normalized) == 0) { dup = true; break; }
        }
        if (dup) { stats_duplicates++; free(normalized); continue; }
        size_t entry_len = strlen(entry);
        size_t candidate_len = (n_merged == 0) ? entry_len
            : current_len + strlen(AI_ENTRY_DELIMITER) + entry_len;
        if (candidate_len > limit) { stats_overflowed++; free(normalized); continue; }
        if (n_merged >= cap_merged) {
            cap_merged *= 2;
            merged = realloc(merged, cap_merged * sizeof(char *));
        }
        merged[n_merged++] = strdup(entry);
        if (n_seen >= cap_seen) {
            cap_seen *= 2;
            seen = realloc(seen, cap_seen * sizeof(char *));
        }
        seen[n_seen++] = normalized;
        current_len = candidate_len;
        stats_added++;
    }
    free(seen);

    if (n_merged >= cap_merged) {
        cap_merged = n_merged + 1;
        merged = realloc(merged, cap_merged * sizeof(char *));
    }
    merged[n_merged] = NULL;

    if (out_added) *out_added = stats_added;
    if (out_duplicates) *out_duplicates = stats_duplicates;
    if (out_overflowed) *out_overflowed = stats_overflowed;
    return merged;
}

/* PoP: claude_rule_to_command_pattern @ hermes_cli/agent_import.py:claude_rule_to_command_pattern */
char *ai_claude_rule_to_command_pattern(const char *rule)
{
    if (!rule) return NULL;
    const char *r = rule;
    while (*r == ' ' || *r == '\t') r++;
    size_t len = strlen(r);
    /* _BASH_RULE_RE = ^Bash\((.*)\)$ */
    if (len < 6 || strncmp(r, "Bash(", 5) != 0) return NULL;
    if (r[len - 1] != ')') return NULL;
    /* inner = group(1).strip() */
    size_t istart = 5, iend = len - 1;
    while (istart < iend && isspace((unsigned char)r[istart])) istart++;
    while (iend > istart && isspace((unsigned char)r[iend - 1])) iend--;
    if (istart >= iend) return NULL; /* empty inner */
    char *inner = malloc(iend - istart + 1);
    memcpy(inner, r + istart, iend - istart);
    inner[iend - istart] = '\0';
    /* if inner.endswith(":*"): inner = inner[:-2] + "*" */
    size_t ilen = strlen(inner);
    if (ilen >= 2 && inner[ilen - 2] == ':' && inner[ilen - 1] == '*') {
        inner[ilen - 2] = '*';
        inner[ilen - 1] = '\0';
    }
    return inner;
}

/* PoP: sanitize_mcp_env @ hermes_cli/agent_import.py:sanitize_mcp_env */
void ai_sanitize_mcp_env(const char *env_json,
                         char **out_kept, char ***out_stripped)
{
    char *kept = strdup("{}");
    char **stripped = NULL;
    size_t n_stripped = 0, cap_stripped = 0;

    if (env_json) {
        json_t *env = json_parse(env_json, NULL);
        if (env && env->type == JSON_OBJECT) {
            json_t *kept_obj = json_object();
            for (size_t i = 0; i < env->c.count; i++) {
                const char *key = env->c.keys[i];
                if (ai_is_secret_key(key)) {
                    if (n_stripped >= cap_stripped) {
                        cap_stripped = cap_stripped ? cap_stripped * 2 : 4;
                        stripped = realloc(stripped, cap_stripped * sizeof(char *));
                    }
                    stripped[n_stripped++] = strdup(key);
                } else {
                    json_set(kept_obj, key, json_copy(env->c.items[i]));
                }
            }
            free(kept);
            kept = json_serialize(kept_obj);
            json_free(kept_obj);
        }
        if (env) json_free(env);
    }
    if (n_stripped >= cap_stripped) {
        cap_stripped = n_stripped + 1;
        stripped = realloc(stripped, cap_stripped * sizeof(char *));
    }
    stripped[n_stripped] = NULL;

    if (out_kept) *out_kept = kept; else free(kept);
    if (out_stripped) *out_stripped = stripped; else ai_strarr_free(stripped);
}
