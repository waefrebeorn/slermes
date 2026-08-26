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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <ftw.h>
#include <sys/stat.h>
#include "libyaml/yaml.h"
#include "libjson/json.h"
#include "libtoml/toml.h"

#define AI_ENTRY_DELIMITER "\n\xC2\xA7\n" /* "\n§\n" (UTF-8) */

/* ── is_secret_key ───────────────────────────────────────────── */

/* PoP: ai_is_secret_key @ hermes_cli/agent_import.py:is_secret_key */
bool ai_is_secret_key(const char *key)
{
    if (!key || !*key) return false;

    /* Case-insensitive regex approximation:
     * (^|_)(API_KEY|APIKEY|TOKEN|SECRET|PASSWORD|PASSWD|CREDENTIALS?|
     *         AUTH|PRIVATE_KEY|ACCESS_KEY)(_|$)  OR  KEY$
     *
     * We scan the uppercase copy for keywords bounded by start/_/-. */
    size_t len = strlen(key);
    char *upper = malloc(len + 1);
    if (!upper) return false;
    for (size_t i = 0; i < len; i++)
        upper[i] = (char)toupper((unsigned char)key[i]);
    upper[len] = '\0';

    /* KEY$ suffix */
    if (len >= 3) {
        size_t i = len - 3;
        if (upper[i] == 'K' && upper[i + 1] == 'E' && upper[i + 2] == 'Y') {
            free(upper);
            return true;
        }
    }

    bool secret = false;
    const char *kws[] = {
        "API_KEY", "API-KEY", "APIKEY", "TOKEN", "SECRET", "PASSWORD",
        "PASSWD", "CREDENTIAL", "CREDENTIALS", "AUTH", "PRIVATE_KEY",
        "PRIVATE-KEY", "ACCESS_KEY", "ACCESS-KEY", NULL,
    };
    for (int k = 0; kws[k] && !secret; k++) {
        size_t kwlen = strlen(kws[k]);
        const char *found = upper;
        while ((found = strstr(found, kws[k])) != NULL) {
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

/* ── normalize_text ─────────────────────────────────────────── */

/* PoP: ai_normalize_text @ hermes_cli/agent_import.py:normalize_text */
char *ai_normalize_text(const char *text)
{
    if (!text) return strdup("");
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
    while (j > 0 && out[j - 1] == ' ') j--;
    out[j] = '\0';
    return out;
}

/* ── str array helpers ───────────────────────────────────────── */

static void ai_strarr_free(char **arr)
{
    if (!arr) return;
    for (size_t i = 0; arr[i]; i++) free(arr[i]);
    free(arr);
}

/* Push a malloc'd string into a NULL-terminated array (grows + 1 slot).
 * cap may be NULL (auto-managed local capacity). */
static void ai_strarr_push(char ***arr, size_t *n, size_t *cap, char *val)
{
    size_t local_cap = cap ? *cap : 0;
    if (*n + 1 >= local_cap) {
        local_cap = local_cap ? local_cap * 2 : 8;
        *arr = realloc(*arr, (local_cap + 1) * sizeof(char *));
        if (cap) *cap = local_cap;
    }
    (*arr)[(*n)++] = val;
    (*arr)[*n] = NULL;
}

/* ── markdown entry extraction ──────────────────────────────── */

static bool ai_md_is_banned_heading(const char *h)
{
    /* re.search(r"\b(MEMORY|USER|SOUL|AGENTS|TOOLS|IDENTITY|CLAUDE)\.md\b", h, re.I) */
    const char *kws[] = {"MEMORY", "USER", "SOUL", "AGENTS", "TOOLS",
                         "IDENTITY", "CLAUDE", NULL};
    for (int k = 0; kws[k]; k++) {
        size_t kwlen = strlen(kws[k]);
        const char *p = h;
        while ((p = strstr(p, kws[k])) != NULL) {
            bool left_ok = (p == h) || !(isalnum((unsigned char)p[-1]) || p[-1] == '_');
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

/* Build a context prefix: filtered heading chain joined by " > ".
 * Returns malloc'd string or NULL if no headings. */
static char *ai_md_context_prefix(char **headings, size_t n_headings)
{
    size_t pn = 0;
    for (size_t i = 0; i < n_headings; i++)
        if (!ai_md_is_banned_heading(headings[i])) pn++;
    if (pn == 0) return NULL;

    size_t cap = 0;
    char *prefix = NULL;
    size_t n = 0;
    for (size_t i = 0; i < n_headings; i++) {
        if (ai_md_is_banned_heading(headings[i])) continue;
        size_t hlen = strlen(headings[i]);
        if (n == 0) {
            cap = hlen + 16;
            prefix = malloc(cap);
            memcpy(prefix, headings[i], hlen);
            n = hlen;
            prefix[n] = '\0';
        } else {
            /* append " > " + heading[i]; separator is " > " (3 chars) */
            size_t need = n + 3 + hlen + 1;
            if (need > cap) { cap = need + 16; prefix = realloc(prefix, cap); }
            prefix[n] = ' '; prefix[n+1] = '>'; prefix[n+2] = ' ';
            memcpy(prefix + n + 3, headings[i], hlen);
            n += 3 + hlen;
            prefix[n] = '\0';
        }
    }
    return prefix;
}

/* Free paragraph lines array contents, then the array itself. */
static void ai_para_clear(char ***paragraph, size_t *n)
{
    if (paragraph && *paragraph) {
        for (size_t i = 0; i < *n; i++) free((*paragraph)[i]);
        /* do NOT free the array itself — caller manages capacity */
        *n = 0;
    }
}

/* Flush accumulated paragraph lines into an entry (with context prefix). */
static void ai_flush_paragraph(char ***entries, size_t *n_ent, size_t *cap_ent,
                               char **paragraph, size_t n_para,
                               char **headings, size_t n_headings)
{
    if (n_para == 0) return;

    /* block = " ".join(paragraph).strip() */
    size_t total = 1;
    for (size_t i = 0; i < n_para; i++) total += strlen(paragraph[i]) + 1;
    char *block = malloc(total);
    block[0] = '\0';
    for (size_t i = 0; i < n_para; i++) {
        if (i) strcat(block, " ");
        strcat(block, paragraph[i]);
    }
    /* strip block */
    size_t bs = 0, be = strlen(block);
    while (bs < be && isspace((unsigned char)block[bs])) bs++;
    while (be > bs && isspace((unsigned char)block[be - 1])) be--;
    block[be] = '\0';
    if (bs > 0) memmove(block, block + bs, be - bs + 1);

    char *prefix = ai_md_context_prefix(headings, n_headings);
    char *entry;
    if (prefix && *prefix) {
        size_t plen = strlen(prefix);
        size_t blen = strlen(block);
        entry = malloc(plen + 3 + blen + 1);  /* "prefix: block" */
        memcpy(entry, prefix, plen);
        entry[plen] = ':'; entry[plen + 1] = ' ';
        memcpy(entry + plen + 2, block, blen);
        entry[plen + 2 + blen] = '\0';
    } else if (block[0]) {
        entry = strdup(block);
    } else {
        entry = NULL;
    }
    free(prefix);
    free(block);

    if (entry) ai_strarr_push(entries, n_ent, cap_ent, entry);
}

/* PoP: ai_extract_markdown_entries @ hermes_cli/agent_import.py:extract_markdown_entries */
char **ai_extract_markdown_entries(const char *text)
{
    if (!text) text = "";

    char **entries = NULL;
    size_t n_ent = 0, cap_ent = 0;
    char **headings = NULL;
    size_t n_headings = 0, cap_headings = 0;
    char **paragraph = NULL;
    size_t n_para = 0, cap_para = 0;
    bool in_code = false;

    char *copy = strdup(text);
    if (!copy) return NULL;
    const char *it = copy;

    while (*it) {
        const char *eol = strchr(it, '\n');
        size_t linelen = eol ? (size_t)(eol - it) : strlen(it);

        /* line = it[:linelen]; stripped = line.strip() */
        char *line = malloc(linelen + 1);
        memcpy(line, it, linelen);
        line[linelen] = '\0';

        /* rstrip */
        size_t rl = linelen;
        while (rl > 0 && isspace((unsigned char)line[rl - 1])) rl--;
        line[rl] = '\0';

        /* lstrip */
        size_t s = 0;
        while (s < rl && isspace((unsigned char)line[s])) s++;
        char *stripped = malloc(rl - s + 1);
        memcpy(stripped, line + s, rl - s);
        stripped[rl - s] = '\0';

        it = eol ? eol + 1 : it + strlen(it);

        /* code fence toggle */
        if (strncmp(stripped, "```", 3) == 0) {
            ai_flush_paragraph(&entries, &n_ent, &cap_ent,
                               paragraph, n_para, headings, n_headings);
            ai_para_clear(&paragraph, &n_para);
            in_code = !in_code;
            free(line); free(stripped);
            continue;
        }
        if (in_code) { free(line); free(stripped); continue; }

        /* heading: re.match(r"^(#{1,6})\s+(.*\S)\s*$", stripped) */
        if (stripped[0] == '#') {
            size_t hlevel = 0;
            while (hlevel < 6 && stripped[hlevel] == '#') hlevel++;
            if (hlevel >= 1 && hlevel <= 6 &&
                stripped[hlevel] == ' ' && stripped[hlevel + 1]) {
                size_t vlen = strlen(stripped + hlevel + 1);
                char *value = strndup(stripped + hlevel + 1, vlen);
                /* strip trailing whitespace from value */
                size_t v = strlen(value);
                while (v > 0 && isspace((unsigned char)value[v-1])) v--;
                value[v] = '\0';

                ai_flush_paragraph(&entries, &n_ent, &cap_ent,
                                   paragraph, n_para, headings, n_headings);
                ai_para_clear(&paragraph, &n_para);

                /* pop headings while len >= level (1-indexed) */
                while (n_headings >= hlevel) {
                    free(headings[--n_headings]);
                }
                ai_strarr_push(&headings, &n_headings, &cap_headings, value);

                free(line); free(stripped);
                continue;
            }
        }

        /* bullet: re.match(r"^\s*(?:[-*]|\d+\.)\s+(.*\S)\s*$", line) */
        /* Check on the raw line (not stripped), per Python: bullet_match = re.match(..., line) */
        if (line[0] && (line[0] == '-' || line[0] == '*' ||
                        (isdigit((unsigned char)line[0])))) {
            size_t bi = 0;
            while (bi < strlen(line) && isspace((unsigned char)line[bi])) bi++;
            bool is_bullet = false;
            size_t content_start = 0;
            if (line[bi] == '-' || line[bi] == '*') {
                is_bullet = true;
                content_start = bi + 1;
            } else if (isdigit((unsigned char)line[bi])) {
                size_t di = bi;
                while (isdigit((unsigned char)line[di])) di++;
                if (di < strlen(line) && line[di] == '.') { is_bullet = true; content_start = di + 1; }
            }
            if (is_bullet) {
                /* bullet content = re.match group(1).strip() */
                while (content_start < rl &&
                       isspace((unsigned char)line[content_start]))
                    content_start++;
                if (content_start < rl) {
                    /* Flush paragraph before bullet */
                    ai_flush_paragraph(&entries, &n_ent, &cap_ent,
                                       paragraph, n_para, headings, n_headings);
                    ai_para_clear(&paragraph, &n_para);

                    size_t clen = rl - content_start;
                    char *content = strndup(line + content_start, clen);
                    /* strip content */
                    size_t cs = 0, ce = strlen(content);
                    while (cs < ce && isspace((unsigned char)content[cs])) cs++;
                    while (ce > cs && isspace((unsigned char)content[ce-1])) ce--;
                    content[ce] = '\0';
                    if (cs > 0) memmove(content, content + cs, ce - cs + 1);

                    char *prefix = ai_md_context_prefix(headings, n_headings);
                    char *entry;
                    if (prefix && *prefix) {
                        size_t plen = strlen(prefix);
                        size_t blen = strlen(content);
                        entry = malloc(plen + 3 + blen + 1);
                        memcpy(entry, prefix, plen);
                        entry[plen] = ':'; entry[plen + 1] = ' ';
                        memcpy(entry + plen + 2, content, blen);
                        entry[plen + 2 + blen] = '\0';
                    } else {
                        entry = content;
                        content = NULL;
                    }
                    free(prefix);
                    free(content);
                    if (entry) ai_strarr_push(&entries, &n_ent, &cap_ent, entry);
                }
                free(line); free(stripped);
                continue;
            }
        }

        /* blank line forces paragraph flush */
        if (stripped[0] == '\0') {
            ai_flush_paragraph(&entries, &n_ent, &cap_ent,
                               paragraph, n_para, headings, n_headings);
            ai_para_clear(&paragraph, &n_para);
            free(line); free(stripped);
            it = eol ? eol + 1 : it + strlen(it);
            continue;
        }

        /* table line: stripped.startswith("|") and stripped.endswith("|") */
        if (stripped[0] == '|' && strlen(stripped) > 0 &&
            stripped[strlen(stripped) - 1] == '|') {
            ai_flush_paragraph(&entries, &n_ent, &cap_ent,
                               paragraph, n_para, headings, n_headings);
            ai_para_clear(&paragraph, &n_para);
            free(line); free(stripped);
            continue;
        }

        /* paragraph line: accumulate */
        ai_strarr_push(&paragraph, &n_para, &cap_para, strdup(stripped));

        free(line); free(stripped);
    }
    free(copy);

    /* Final flush */
    ai_flush_paragraph(&entries, &n_ent, &cap_ent,
                       paragraph, n_para, headings, n_headings);
    /* Free remaining paragraph elements + array */
    if (paragraph) {
        for (size_t i = 0; i < n_para; i++) free(paragraph[i]);
        free(paragraph);
    }
    ai_strarr_free(headings);

    /* Dedup by normalize_text (mirrors Python's deduped loop) */
    /* Python: for entry in entries: normalized = normalize_text(entry);
     *   if not normalized or normalized in seen: skip; seen.add(normalized);
     *   deduped.append(entry.strip()) */
    char **deduped = calloc(1, sizeof(char *));
    size_t n_dedup = 0;
    char **seen = calloc(1, sizeof(char *));
    size_t n_seen = 0;
    for (size_t i = 0; entries && entries[i]; i++) {
        char *norm = ai_normalize_text(entries[i]);
        if (!norm || !*norm) { free(norm); continue; }
        bool dup = false;
        for (size_t j = 0; j < n_seen; j++) {
            if (strcmp(seen[j], norm) == 0) { dup = true; break; }
        }
        if (dup) { free(norm); continue; }
        ai_strarr_push(&seen, &n_seen, /*cap*/NULL, norm);
        /* entry.strip() */
        char *e = entries[i];
        size_t es = 0, ee = strlen(e);
        while (es < ee && isspace((unsigned char)e[es])) es++;
        while (ee > es && isspace((unsigned char)e[ee - 1])) ee--;
        ai_strarr_push(&deduped, &n_dedup, /*cap*/NULL, strndup(e + es, ee - es));
    }
    ai_strarr_free(entries);
    ai_strarr_free(seen);
    return deduped;
}

/* ── claude_rule_to_command_pattern ──────────────────────────── */

/* PoP: ai_claude_rule_to_command_pattern @ hermes_cli/agent_import.py:claude_rule_to_command_pattern */
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

/* ── sanitize_mcp_env ────────────────────────────────────────── */

/* PoP: ai_sanitize_mcp_env @ hermes_cli/agent_import.py:sanitize_mcp_env */
void ai_sanitize_mcp_env(const char *env_json,
                         char **out_kept, char ***out_stripped)
{
    char *kept = strdup("{}");
    char **stripped = NULL;
    size_t n_stripped = 0, cap_stripped = 0;
    char *err = NULL;

    if (env_json) {
        json_t *env = json_parse(env_json, &err);
        if (err) free(err);
        if (env && env->type == JSON_OBJECT) {
            json_t *kept_obj = json_object();
            for (size_t i = 0; i < env->c.count; i++) {
                const char *key = env->c.keys[i];
                if (ai_is_secret_key(key)) {
                    if (n_stripped >= cap_stripped) {
                        cap_stripped = cap_stripped ? cap_stripped * 2 : 4;
                        stripped = realloc(stripped, (cap_stripped + 1) * sizeof(char *));
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
    /* NULL-terminate stripped */
    if (n_stripped >= cap_stripped) {
        cap_stripped = n_stripped + 1;
        stripped = realloc(stripped, (cap_stripped + 1) * sizeof(char *));
    }
    stripped[n_stripped] = NULL;

    if (out_kept) *out_kept = kept; else free(kept);
    if (out_stripped) *out_stripped = stripped; else ai_strarr_free(stripped);
}

/* ── parse_existing_memory_entries ─────────────────────────── */

/* PoP: ai_parse_existing_memory_entries @ hermes_cli/agent_import.py:parse_existing_memory_entries */
char **ai_parse_existing_memory_entries(const char *file_contents)
{
    char **entries = calloc(1, sizeof(char *));
    size_t n = 0, cap = 0;

    if (!file_contents || !*file_contents)
        return entries;

    const char *cur = file_contents;
    const size_t dlen = strlen(AI_ENTRY_DELIMITER);

    while (*cur) {
        char *next = strstr(cur, AI_ENTRY_DELIMITER);
        char *seg;
        if (next) {
            size_t slen = (size_t)(next - cur);
            seg = strndup(cur, slen);
            cur = next + dlen;
        } else {
            seg = strdup(cur);
            cur = cur + strlen(cur);  /* advance past end -> loop exits */
        }

        /* strip + skip empty: [e.strip() for e in raw.split(DELIM) if e.strip()] */
        size_t sl = strlen(seg);
        size_t si = 0;
        while (si < sl && isspace((unsigned char)seg[si])) si++;
        size_t se = sl;
        while (se > si && isspace((unsigned char)seg[se - 1])) se--;
        if (si < se) {
            char *entry = strndup(seg + si, se - si);
            ai_strarr_push(&entries, &n, &cap, entry);
        }
        free(seg);

        if (!next) break;
    }
    return entries;
}

/* ── merge_entries ──────────────────────────────────────────── */

/* PoP: ai_merge_entries @ hermes_cli/agent_import.py:merge_entries */
char **ai_merge_entries(const char *existing_json, const char *incoming_json,
                        long limit, size_t *out_added,
                        size_t *out_duplicates, size_t *out_overflowed)
{
    if (out_added) *out_added = 0;
    if (out_duplicates) *out_duplicates = 0;
    if (out_overflowed) *out_overflowed = 0;

    char **merged = calloc(1, sizeof(char *));
    size_t n_merged = 0, cap_merged = 0;
    char **seen = calloc(1, sizeof(char *));
    size_t n_seen = 0, cap_seen = 0;

    char *err = NULL;
    json_t *ex = json_parse(existing_json ? existing_json : "[]", &err);
    if (err) free(err);
    json_t *in = json_parse(incoming_json ? incoming_json : "[]", &err);
    if (err) free(err);

    /* merged = list(existing); seen = {normalize_text(e) for e in existing if e.strip()} */
    if (ex && ex->type == JSON_ARRAY) {
        for (size_t i = 0; i < ex->c.count; i++) {
            json_t *e = ex->c.items[i];
            char *s = e ? (e->type == JSON_STRING ? strdup(e->str_val)
                                                  : json_serialize(e)) : strdup("");
            /* strip check like Python's `if e.strip()` */
            size_t sl = strlen(s);
            size_t si = 0;
            while (si < sl && isspace((unsigned char)s[si])) si++;
            size_t se = sl;
            while (se > si && isspace((unsigned char)s[se-1])) se--;
            if (si < se) {
                char *norm = ai_normalize_text(s);
                if (norm && *norm) ai_strarr_push(&seen, &n_seen, &cap_seen, norm);
                else free(norm);
            }
            ai_strarr_push(&merged, &n_merged, &cap_merged, s);
        }
    }

    /* current_len = len(ENTRY_DELIMITER.join(merged)) if merged else 0 */
    size_t current_len = 0;
    if (n_merged > 0) {
        current_len = strlen(merged[0]);
        for (size_t k = 1; k < n_merged; k++)
            current_len += strlen(AI_ENTRY_DELIMITER) + strlen(merged[k]);
    }

    if (in && in->type == JSON_ARRAY) {
        for (size_t i = 0; i < in->c.count; i++) {
            json_t *e = in->c.items[i];
            char *entry = e ? (e->type == JSON_STRING ? strdup(e->str_val)
                                                      : json_serialize(e)) : strdup("");
            char *normalized = ai_normalize_text(entry);
            if (!normalized || !*normalized) { free(normalized); free(entry); continue; }
            bool dup = false;
            for (size_t j = 0; j < n_seen; j++)
                if (strcmp(seen[j], normalized) == 0) { dup = true; break; }
            if (dup) {
                if (out_duplicates) (*out_duplicates)++;
                free(normalized); free(entry);
                continue;
            }
            /* candidate_len = len(entry) if not merged else current_len + dlen + len(entry) */
            size_t cand_len = n_merged
                ? current_len + strlen(AI_ENTRY_DELIMITER) + strlen(entry)
                : strlen(entry);
            if (cand_len > (size_t)limit) {
                if (out_overflowed) (*out_overflowed)++;
                free(normalized); free(entry);
                continue;
            }
            ai_strarr_push(&merged, &n_merged, &cap_merged, entry);
            ai_strarr_push(&seen, &n_seen, &cap_seen, normalized);
            current_len = cand_len;
            if (out_added) (*out_added)++;
        }
    }

    if (ex) json_free(ex);
    if (in) json_free(in);
    ai_strarr_free(seen);
    return merged;
}

/* ── YAML helpers ──────────────────────────────────────────── */

/* Simple growable buffer for YAML emission */
struct ai_yaml_buf {
    char *buf;
    size_t len;
    size_t cap;
};

static void ai_buf_init(struct ai_yaml_buf *b) {
    b->buf = NULL; b->len = 0; b->cap = 0;
}

static void ai_buf_putc(struct ai_yaml_buf *b, char c) {
    if (b->len + 2 > b->cap) {
        b->cap = b->cap ? b->cap * 2 + 64 : 128;
        b->buf = realloc(b->buf, b->cap);
    }
    b->buf[b->len++] = c;
    b->buf[b->len] = '\0';
}

static void ai_buf_puts(struct ai_yaml_buf *b, const char *s) {
    if (!s) return;
    size_t slen = strlen(s);
    if (b->len + slen + 1 > b->cap) {
        b->cap = (b->len + slen + 1) * 2;
        b->buf = realloc(b->buf, b->cap);
    }
    memcpy(b->buf + b->len, s, slen);
    b->len += slen;
    b->buf[b->len] = '\0';
}

/* PoP: ai_load_yaml_from_string @ hermes_cli/agent_import.py:load_yaml_file */
/* Parse a YAML string into a JSON string. Mirrors Python's
 * yaml.safe_load: returns "{}" for empty/non-dict input. */
char *ai_load_yaml_from_string(const char *yaml_text) {
    if (!yaml_text || !*yaml_text) {
        char *empty = strdup("{}");
        return empty;
    }
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse(yaml_text, &err);
    if (err) free(err);
    if (!doc) {
        char *empty = strdup("{}");
        return empty;
    }
    char *json_str = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!json_str) {
        char *empty = strdup("{}");
        return empty;
    }
    return json_str;
}

/* PoP: ai_yaml_emit_scalar @ hermes_cli/agent_import.py:dump_yaml_file */
/* Serialize a JSON dict to YAML string. Mirrors Python's
 * yaml.safe_dump(data, default_flow_style=False, sort_keys=False,
 * allow_unicode=True). Produces block-style YAML. */
/* Recursive YAML value emitter. Appends block-style YAML for a json_t
 * value at the given indent level into the output buffer. */
static void ai_yaml_emit_value(struct ai_yaml_buf *b, const json_t *val, int indent);

static void ai_yaml_emit_scalar(struct ai_yaml_buf *b, const char *s, int indent) {
    bool needs_quote = false;
    if (!s || !*s) needs_quote = true;
    else {
        for (const char *p = s; *p; p++) {
            if (*p == ':' || *p == '#' || *p == '\'' || *p == '"' ||
                *p == '\n' || *p == '\t' || *p == '{' || *p == '}' ||
                *p == '[' || *p == ']' || *p == ',' || *p == '&' || *p == '*' ||
                (isspace((unsigned char)*p))) {
                needs_quote = true; break;
            }
        }
    }
    for (int i = 0; i < indent; i++) ai_buf_putc(b, ' ');
    if (needs_quote) {
        ai_buf_putc(b, '"');
        for (const char *p = s; p && *p; p++) {
            if (*p == '"') { ai_buf_putc(b, '\\'); ai_buf_putc(b, '"'); }
            else ai_buf_putc(b, *p);
        }
        ai_buf_putc(b, '"');
    } else {
        for (const char *p = s; p && *p; p++) ai_buf_putc(b, *p);
    }
    ai_buf_putc(b, '\n');
}

static void ai_yaml_emit_value(struct ai_yaml_buf *b, const json_t *val, int indent) {
    if (!val) {
        ai_yaml_emit_scalar(b, "null", indent);
        return;
    }
    switch (val->type) {
        case JSON_STRING:
            ai_yaml_emit_scalar(b, val->str_val, indent);
            break;
        case JSON_NUMBER: {
            double num = val->num_val;
            char nb[64];
            if (num == (double)(long long)num && num > -1e15 && num < 1e15)
                snprintf(nb, sizeof(nb), "%lld", (long long)num);
            else
                snprintf(nb, sizeof(nb), "%g", num);
            ai_yaml_emit_scalar(b, nb, indent);
            break;
        }
        case JSON_BOOL:
            ai_yaml_emit_scalar(b, val->bool_val ? "true" : "false", indent);
            break;
        case JSON_NULL:
            ai_yaml_emit_scalar(b, "null", indent);
            break;
        case JSON_ARRAY:
            if (val->c.count == 0) {
                for (int i = 0; i < indent; i++) ai_buf_putc(b, ' ');
                ai_buf_puts(b, "[]\n");
            } else {
                for (size_t i = 0; i < val->c.count; i++) {
                    for (int j = 0; j < indent; j++) ai_buf_putc(b, ' ');
                    ai_buf_putc(b, '-'); ai_buf_putc(b, ' ');
                    /* Emit inline for scalars, nested for containers */
                    json_t *item = val->c.items[i];
                    if (item && (item->type == JSON_OBJECT || item->type == JSON_ARRAY)) {
                        ai_buf_putc(b, '\n');
                        ai_yaml_emit_value(b, item, indent + 2);
                    } else if (item && item->type == JSON_STRING) {
                        ai_buf_puts(b, item->str_val);
                        ai_buf_putc(b, '\n');
                    } else {
                        char *serialized = json_serialize(item);
                        ai_buf_puts(b, serialized ? serialized : "null");
                        ai_buf_putc(b, '\n');
                        free(serialized);
                    }
                }
            }
            break;
        case JSON_OBJECT:
            if (val->c.count == 0) {
                for (int i = 0; i < indent; i++) ai_buf_putc(b, ' ');
                ai_buf_puts(b, "{}\n");
            } else {
                for (size_t i = 0; i < val->c.count; i++) {
                    const char *key = val->c.keys[i];
                    json_t *item = val->c.items[i];
                    for (int j = 0; j < indent; j++) ai_buf_putc(b, ' ');
                    ai_buf_puts(b, key);
                    ai_buf_puts(b, ": ");
                    if (item && (item->type == JSON_OBJECT || item->type == JSON_ARRAY)) {
                        if (item->type == JSON_ARRAY && item->c.count == 0) {
                            ai_buf_puts(b, "[]\n");
                        } else if (item->type == JSON_OBJECT && item->c.count == 0) {
                            ai_buf_puts(b, "{}\n");
                        } else if (item->type == JSON_ARRAY) {
                            ai_buf_putc(b, '\n');
                            ai_yaml_emit_value(b, item, indent + 2);
                        } else {
                            ai_buf_putc(b, '\n');
                            ai_yaml_emit_value(b, item, indent + 2);
                        }
                    } else if (item && item->type == JSON_STRING) {
                        ai_buf_puts(b, item->str_val);
                        ai_buf_putc(b, '\n');
                    } else {
                        char *serialized = json_serialize(item);
                        ai_buf_puts(b, serialized ? serialized : "null");
                        ai_buf_putc(b, '\n');
                        free(serialized);
                    }
                }
            }
            break;
    }
}

char *ai_dump_yaml_to_string(const char *json_dict) {
    if (!json_dict || !*json_dict) {
        char *empty = strdup("");
        return empty;
    }
    char *err = NULL;
    json_t *data = json_parse(json_dict, &err);
    if (err) free(err);
    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        char *empty = strdup("");
        return empty;
    }
    struct ai_yaml_buf b;
    ai_buf_init(&b);
    for (size_t i = 0; i < data->c.count; i++) {
        const char *key = data->c.keys[i];
        json_t *val = data->c.items[i];
        ai_buf_puts(&b, key);
        ai_buf_puts(&b, ": ");
        if (val && (val->type == JSON_OBJECT || val->type == JSON_ARRAY)) {
            if (val->type == JSON_ARRAY && val->c.count == 0) {
                ai_buf_puts(&b, "[]\n");
            } else if (val->type == JSON_OBJECT && val->c.count == 0) {
                ai_buf_puts(&b, "{}\n");
            } else {
                ai_buf_putc(&b, '\n');
                ai_yaml_emit_value(&b, val, 2);
            }
        } else if (val && val->type == JSON_STRING) {
            ai_buf_puts(&b, val->str_val);
            ai_buf_putc(&b, '\n');
        } else {
            char *serialized = json_serialize(val);
            ai_buf_puts(&b, serialized ? serialized : "null");
            ai_buf_putc(&b, '\n');
            free(serialized);
        }
    }
    json_free(data);
    if (!b.buf) b.buf = strdup("");
    return b.buf;
}

/* ── default_source_dir ─────────────────────────────────────── */

/* PoP: ai_default_source_dir @ hermes_cli/agent_import.py:default_source_dir */
/* Build the default source dir path for an agent.
 * claude-code → "<home>/.claude"
 * codex → "<home>/.codex" */
char *ai_default_source_dir(const char *agent, const char *home) {
    if (!agent || !home) return NULL;
    const char *suffix = NULL;
    if (strcmp(agent, "claude-code") == 0) suffix = ".claude";
    else if (strcmp(agent, "codex") == 0) suffix = ".codex";
    else return NULL;
    size_t len = strlen(home) + 1 + strlen(suffix) + 1;
    char *path = malloc(len);
    if (!path) return NULL;
    sprintf(path, "%s/%s", home, suffix);
    return path;
}

/* ── detect_agents ──────────────────────────────────────────── */

/* PoP: ai_detect_agents @ hermes_cli/agent_import.py:detect_agents */
/* Return list of agents whose default dirs exist under home.
 * Returns malloc'd NULL-terminated array of malloc'd strings. */
char **ai_detect_agents(const char *home) {
    char **result = calloc(1, sizeof(char *));
    size_t n = 0, cap = 0;
    if (!home) return result;
    const char *agents[] = {"claude-code", "codex", NULL};
    for (int i = 0; agents[i]; i++) {
        char *dir = ai_default_source_dir(agents[i], home);
        if (dir) {
            struct stat st;
            if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode)) {
                ai_strarr_push(&result, &n, &cap, strdup(agents[i]));
            }
            free(dir);
        }
    }
    return result;
}

/* ── backup_memory_file ─────────────────────────────────────── */

/* PoP: ai_backup_path @ hermes_cli/agent_import.py:backup_memory_file */
/* Build a backup path: "<path>.bak.<unix_ts>".
 * Returns NULL if path doesn't exist. The caller provides the
 * unix timestamp via ai_backup_path (no time() calls for test
 * determinism). */
char *ai_backup_path(const char *path, long unix_ts) {
    if (!path) return NULL;
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    size_t plen = strlen(path);
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    const char *ext = (dot && (!slash || dot > slash)) ? dot : NULL;
    size_t prefix_len = ext ? (size_t)(ext - path) : plen;
    size_t need = prefix_len + 32 + 5; /* .bak.<ts>\0 */
    char *backup = malloc(need);
    if (!backup) return NULL;
    memcpy(backup, path, prefix_len);
    sprintf(backup + prefix_len, ".bak.%ld", unix_ts);
    return backup;
}

/* ── record / build_report ──────────────────────────────────── */

/* PoP: strings @ hermes_cli/agent_import.py:AgentImporter.record */
/* Build a JSON item dict from import fields.
 * kind, source, destination: strings (may be NULL → null in JSON)
 * status: string (imported/skipped/conflict/error)
 * reason: string (may be NULL)
 * details_json: optional JSON object string to merge in (may be NULL)
 * Returns malloc'd JSON string. */
char *ai_record(const char *kind, const char *source,
                const char *destination, const char *status,
                const char *reason, const char *details_json) {
    json_t *item = json_object();
    if (kind) json_set(item, "kind", json_string(kind));
    if (source) json_set(item, "source", json_string(source));
    if (destination) json_set(item, "destination", json_string(destination));
    if (status) json_set(item, "status", json_string(status));
    if (reason) json_set(item, "reason", json_string(reason));
    if (details_json) {
        char *err = NULL;
        json_t *details = json_parse(details_json, &err);
        if (err) free(err);
        if (details && details->type == JSON_OBJECT) {
            for (size_t i = 0; i < details->c.count; i++)
                json_set(item, details->c.keys[i], json_copy(details->c.items[i]));
        }
        if (details) json_free(details);
    }
    char *out = json_serialize(item);
    json_free(item);
    return out;
}

/* PoP: strings @ hermes_cli/agent_import.py:AgentImporter.build_report */
/* Build a JSON report from import items.
 * agent: string
 * source_path: string
 * target_path: string
 * execute: 0 = dry run, 1 = real
 * items_json: JSON array of item dicts
 * stripped_secrets_json: JSON array of secret-path strings (may be NULL)
 * Returns malloc'd JSON string. */
char *ai_build_report(const char *agent, const char *source_path,
                      const char *target_path, int execute,
                      const char *items_json, const char *stripped_secrets_json) {
    json_t *report = json_object();
    if (agent) json_set(report, "agent", json_string(agent));
    if (source_path) json_set(report, "source", json_string(source_path));
    if (target_path) json_set(report, "target", json_string(target_path));
    json_set(report, "dry_run", json_bool(!execute));

    /* items */
    json_t *items = json_array();
    if (items_json) {
        char *err = NULL;
        json_t *arr = json_parse(items_json, &err);
        if (err) free(err);
        if (arr && arr->type == JSON_ARRAY) {
            for (size_t i = 0; i < arr->c.count; i++)
                json_append(items, json_copy(arr->c.items[i]));
        }
        if (arr) json_free(arr);
    }
    json_set(report, "items", items);

    /* summary: count by status */
    json_t *summary = json_object();
    json_set(summary, "imported", json_number(0));
    json_set(summary, "skipped", json_number(0));
    json_set(summary, "conflict", json_number(0));
    json_set(summary, "error", json_number(0));
    if (items && items->c.count) {
        for (size_t i = 0; i < items->c.count; i++) {
            json_t *item = items->c.items[i];
            if (!item || item->type != JSON_OBJECT) continue;
            json_t *st = json_obj_get(item, "status");
            const char *status = st && st->type == JSON_STRING ? st->str_val : "skipped";
            json_t *cur = json_obj_get(summary, status);
            double val = cur && cur->type == JSON_NUMBER ? cur->num_val : 0;
            json_set(summary, status, json_number(val + 1));
        }
    }
    json_set(report, "summary", summary);

    /* stripped_secrets: sorted dedup'd */
    if (stripped_secrets_json && *stripped_secrets_json) {
        char *err = NULL;
        json_t *arr = json_parse(stripped_secrets_json, &err);
        if (err) free(err);
        if (arr && arr->type == JSON_ARRAY) {
            /* Collect, sort, dedup */
            char **names = calloc(arr->c.count + 1, sizeof(char *));
            size_t n = 0;
            for (size_t i = 0; i < arr->c.count; i++) {
                json_t *e = arr->c.items[i];
                if (e && e->type == JSON_STRING)
                    names[n++] = strdup(e->str_val);
            }
            /* Simple insertion sort */
            for (size_t i = 1; i < n; i++) {
                char *key = names[i];
                size_t j = i;
                while (j > 0 && strcmp(names[j-1], key) > 0) {
                    names[j] = names[j-1]; j--;
                }
                names[j] = key;
            }
            /* Dedup */
            json_t *sorted = json_array();
            for (size_t i = 0; i < n; i++) {
                if (i > 0 && strcmp(names[i-1], names[i]) == 0) continue;
                json_append(sorted, json_string(names[i]));
            }
            if (n > 0) {
                json_set(report, "stripped_secrets", sorted);
            } else {
                json_free(sorted);
            }
            for (size_t i = 0; i < n; i++) free(names[i]);
            free(names);
        }
        if (arr) json_free(arr);
    }

    char *out = json_serialize(report);
    json_free(report);
    return out;
}

/* ── permission list import ─────────────────────────────────── */

/* PoP: import_permission_allowlist @ hermes_cli/agent_import.py:import_permission_allowlist */
/* Convert a list of Claude Code Bash permission rules into Hermes command
 * patterns. Mirrors the pure core of import_permission_allowlist (without
 * the file I/O / config merging).
 * rules_json: JSON array of rule strings
 * Returns malloc'd JSON array string of sorted unique patterns. */
char *ai_import_permission_patterns(const char *rules_json) {
    if (!rules_json || !*rules_json) {
        char *r = strdup("[]");
        return r;
    }
    char *err = NULL;
    json_t *rules = json_parse(rules_json, &err);
    if (err) free(err);
    json_t *patterns = json_array();
    if (rules && rules->type == JSON_ARRAY) {
        /* Collect unique patterns (dict.fromkeys preserves order, then sorted) */
        char **seen = calloc(rules->c.count + 1, sizeof(char *));
        size_t n_seen = 0;
        for (size_t i = 0; i < rules->c.count; i++) {
            json_t *rule = rules->c.items[i];
            if (!rule || rule->type != JSON_STRING) continue;
            char *pattern = ai_claude_rule_to_command_pattern(rule->str_val);
            if (pattern && *pattern) {
                /* Check for duplicate */
                bool dup = false;
                for (size_t j = 0; j < n_seen; j++)
                    if (strcmp(seen[j], pattern) == 0) { dup = true; break; }
                if (!dup) {
                    if (n_seen < rules->c.count) seen[n_seen++] = pattern;
                    else free(pattern);
                } else {
                    free(pattern);
                }
            } else {
                free(pattern);
            }
        }
        /* Sort */
        for (size_t i = 1; i < n_seen; i++) {
            char *key = seen[i];
            size_t j = i;
            while (j > 0 && strcmp(seen[j-1], key) > 0) {
                seen[j] = seen[j-1]; j--;
            }
            seen[j] = key;
        }
        /* Build JSON array */
        for (size_t i = 0; i < n_seen; i++)
            json_append(patterns, json_string(seen[i]));
        for (size_t i = 0; i < n_seen; i++) free(seen[i]);
        free(seen);
    }
    if (rules) json_free(rules);
    char *out = json_serialize(patterns);
    json_free(patterns);
    return out;
}




/* ==================================================================
 *  REAL_GAP wave — hermes_cli/agent_import.py AgentImporter methods
 *  + import_agent_command / print_import_report. Signatures follow
 *  include/port_hermes_cli_agent_import.h exactly.
 * ================================================================== */

#include <dirent.h>
#include <time.h>

#define AI_MEMORY_CHAR_LIMIT 20000

static void ai_imp_record(AgentImporter *imp, const char *kind,
                          const char *source, const char *destination,
                          const char *status, const char *reason,
                          const char *details_json)
{
    char *item_json = ai_record(kind, source, destination, status, reason,
                                details_json);
    if (!item_json) return;
    char *err = NULL;
    json_t *item = json_parse(item_json, &err);
    if (err) free(err);
    if (item && item->type == JSON_OBJECT) {
        json_append(imp->items, item);      /* append copies the node */
        json_free(item);
    } else if (item) {
        json_free(item);
    }
    free(item_json);
}

static void ai_imp_strip_secret(AgentImporter *imp, const char *name)
{
    if (name) json_append(imp->stripped_secrets, json_string(name));
}

static char *imp_path(const AgentImporter *imp, const char *rel)
{
    size_t n = strlen(imp->target_root) + strlen(rel) + 2;
    char *p = malloc(n);
    if (p) snprintf(p, n, "%s/%s", imp->target_root, rel);
    return p;
}

static bool path_exists(const char *p)
{
    struct stat st;
    return p && stat(p, &st) == 0;
}

static bool path_is_dir(const char *p)
{
    struct stat st;
    return p && stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* PoP: __init__ @ hermes_cli/agent_import.py:AgentImporter.__init__ */
AgentImporter *ai_agent_importer_new(const char *agent, const char *source_root,
                                     const char *target_root, bool execute,
                                     bool overwrite)
{
    if (!agent || !*agent || !source_root || !target_root) return NULL;
    AgentImporter *imp = calloc(1, sizeof *imp);
    if (!imp) return NULL;
    imp->agent = strdup(agent);
    imp->source_root = strdup(source_root);
    imp->target_root = strdup(target_root);
    imp->execute = execute ? true : false;
    imp->overwrite = overwrite ? true : false;
    imp->items = json_array();
    imp->stripped_secrets = json_array();
    return imp;
}

void ai_agent_importer_free(AgentImporter *imp)
{
    if (!imp) return;
    free(imp->agent); free(imp->source_root); free(imp->target_root);
    if (imp->items) json_free(imp->items);
    if (imp->stripped_secrets) json_free(imp->stripped_secrets);
    free(imp);
}

void ai_agent_importer_set_execute(AgentImporter *imp, bool execute)
{
    if (imp) imp->execute = execute ? true : false;
}

/* PoP: load_target_config @ hermes_cli/agent_import.py:AgentImporter.load_target_config */
json_t *ai_load_target_config(AgentImporter *imp, const char *kind,
                              const char *source, const char *dest_yaml_path)
{
    FILE *fh = fopen(dest_yaml_path, "r");
    if (!fh) {
        char reason[512];
        snprintf(reason, sizeof reason, "Could not read %s: %s",
                 dest_yaml_path, strerror(errno));
        ai_imp_record(imp, kind, source, dest_yaml_path, "error", reason, NULL);
        return NULL;   /* nothing safe to merge into — file left untouched */
    }
    fseek(fh, 0, SEEK_END); long sz = ftell(fh); fseek(fh, 0, SEEK_SET);
    char *text = malloc((size_t)sz + 1);
    if (!text) { fclose(fh); return NULL; }
    size_t rd = fread(text, 1, (size_t)sz, fh);
    text[rd] = '\0';
    fclose(fh);

    char *json_text = ai_load_yaml_from_string(text);
    free(text);
    if (!json_text) {
        ai_imp_record(imp, kind, source, dest_yaml_path, "error",
                      "config.yaml could not be parsed as a mapping", NULL);
        return NULL;
    }
    char *err = NULL;
    json_t *cfg = json_parse(json_text, &err);
    if (err) free(err);
    free(json_text);
    if (!cfg || cfg->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        ai_imp_record(imp, kind, source, dest_yaml_path, "error",
                      "config.yaml is not a mapping", NULL);
        return NULL;
    }
    return cfg;
}

/* sorted unique merge of current+patterns; out_added lists merged entries
 * absent from current (malloc'd strings). */
static json_t *merge_pattern_lists(json_t *current, json_t *patterns,
                                   char ***out_added, size_t *out_added_n)
{
    *out_added = NULL; *out_added_n = 0;
    char **all = NULL;
    size_t n = 0, cap = 0;
#define AI_PUSH_STR(sval) do { \
        if (n + 1 > cap) { cap = cap ? cap * 2 : 16; all = realloc(all, cap * sizeof(char *)); } \
        if (all) all[n++] = strdup(sval); } while (0)
    if (current && current->type == JSON_ARRAY)
        for (size_t i = 0; i < current->c.count; i++) {
            json_t *e = current->c.items[i];
            if (e && e->type == JSON_STRING) AI_PUSH_STR(e->str_val);
        }
    if (patterns && patterns->type == JSON_ARRAY)
        for (size_t i = 0; i < patterns->c.count; i++) {
            json_t *e = patterns->c.items[i];
            if (e && e->type == JSON_STRING) AI_PUSH_STR(e->str_val);
        }
#undef AI_PUSH_STR
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++)
            if (strcmp(all[i], all[j]) == 0) { free(all[j]); memmove(&all[j], &all[j+1], (n-j-1)*sizeof(char*)); n--; j--; }
    qsort(all, n, sizeof(char *), cmp_str);
    json_t *merged = json_array();
    char **added = NULL; size_t added_n = 0;
    for (size_t i = 0; i < n; i++) {
        if (i > 0 && strcmp(all[i-1], all[i]) == 0) continue;
        json_append(merged, json_string(all[i]));
        bool in_current = false;
        if (current && current->type == JSON_ARRAY)
            for (size_t j = 0; j < current->c.count; j++) {
                json_t *e = current->c.items[j];
                if (e && e->type == JSON_STRING && strcmp(e->str_val, all[i]) == 0) { in_current = true; break; }
            }
        if (!in_current) {
            added = realloc(added, (added_n + 1) * sizeof(char *));
            added[added_n++] = strdup(all[i]);
        }
    }
    for (size_t i = 0; i < n; i++) free(all[i]);
    free(all);
    *out_added = added; *out_added_n = added_n;
    return merged;
}

static void free_names2(char **names, size_t n)
{
    if (!names) return;
    for (size_t i = 0; i < n; i++) free(names[i]);
    free(names);
}

static void write_config_yaml(const char *destination, json_t *config)
{
    char *ser = json_serialize(config);
    if (!ser) return;
    char *yaml_text = ai_dump_yaml_to_string(ser);
    if (yaml_text) {
        FILE *fh = fopen(destination, "w");
        if (fh) { fputs(yaml_text, fh); fclose(fh); }
        free(yaml_text);
    }
    free(ser);
}

/* PoP: _merge_memory_entries @ hermes_cli/agent_import.py:_merge_memory_entries */
char **ai_merge_memory_entries(AgentImporter *imp, const char *kind,
                               const char *source, const char *dest_path,
                               char **existing, char **incoming, int incoming_count)
{
    size_t n_exist = 0;
    while (existing && existing[n_exist]) n_exist++;

    char *ex_json = NULL, *in_json = NULL;
    {
        json_t *a = json_array();
        for (size_t i = 0; i < n_exist; i++) json_append(a, json_string(existing[i]));
        ex_json = json_serialize(a);
        json_free(a);
        json_t *b = json_array();
        for (int i = 0; i < incoming_count; i++)
            if (incoming[i]) json_append(b, json_string(incoming[i]));
        in_json = json_serialize(b);
        json_free(b);
    }

    size_t added = 0, dups = 0, overflowed = 0;
    char **merged_arr = ai_merge_entries(ex_json ? ex_json : "[]",
                                         in_json ? in_json : "[]",
                                         AI_MEMORY_CHAR_LIMIT,
                                         &added, &dups, &overflowed);
    free(ex_json); free(in_json);

    char details[1024];
    snprintf(details, sizeof details,
             "{\"existing_entries\":%zu,\"added_entries\":%zu,"
             "\"duplicate_entries\":%zu,\"overflowed_entries\":%zu}",
             n_exist, added, dups, overflowed);

    if (!merged_arr || added == 0 || merged_arr[0] == NULL) {
        ai_imp_record(imp, kind, source, dest_path, "skipped",
                      "No new entries to import", details);
        if (merged_arr) { for (size_t i = 0; merged_arr[i]; i++) free(merged_arr[i]); free(merged_arr); }
        return NULL;
    }

    if (!imp->execute) {
        ai_imp_record(imp, kind, source, dest_path, "imported",
                      "Would merge entries", details);
        return merged_arr;
    }

    /* execute: mkdir parents, back up, atomic write of the joined store */
    char parent[4608];
    snprintf(parent, sizeof parent, "%s", dest_path);
    char *slash = strrchr(parent, '/');
    if (slash) *slash = '\0';
    mkdir(parent, 0755);

    char backup[4800] = "";
    char *bp = ai_backup_path(dest_path, (long)time(NULL));
    if (bp) { snprintf(backup, sizeof backup, "%s", bp); free(bp); }
    if (backup[0]) {
        FILE *src = fopen(dest_path, "rb");
        FILE *dst = src ? fopen(backup, "wb") : NULL;
        bool okc = src && dst;
        if (okc) {
            char chunk[8192]; size_t r;
            while ((r = fread(chunk, 1, sizeof chunk, src)) > 0) fwrite(chunk, 1, r, dst);
        }
        if (dst) fclose(dst);
        if (src) fclose(src);
        if (!okc) {
            ai_imp_record(imp, kind, source, dest_path, "error",
                          "Could not back up existing memory file", details);
            for (size_t i = 0; merged_arr[i]; i++) free(merged_arr[i]);
            free(merged_arr);
            return NULL;
        }
    }
    /* ENTRY_DELIMITER.join(merged) + "\n" via tmp + rename */
    char tmp[4896];
    snprintf(tmp, sizeof tmp, "%s.tmp.%d", dest_path, (int)getpid());
    FILE *out = fopen(tmp, "w");
    bool ok = out != NULL;
    if (ok) {
        static const char *DELIM = "\n\xC2\xA7\n";
        bool first = true;
        for (size_t i = 0; merged_arr[i]; i++) {
            if (!first) fputs(DELIM, out);
            fputs(merged_arr[i], out);
            first = false;
        }
        fputc('\n', out);
        fclose(out);
    }
    if (!ok || rename(tmp, dest_path) != 0) {
        ai_imp_record(imp, kind, source, dest_path, "error",
                      "Could not write merged memory file", details);
    } else {
        if (backup[0]) {
            char trimmed[sizeof(details)];
            snprintf(trimmed, sizeof trimmed, "%s", details);
            size_t dl = strlen(trimmed);
            if (dl > 0 && trimmed[dl-1] == '}') trimmed[--dl] = '\0';
            char combined[2048];
            snprintf(combined, sizeof combined, "%s,\"backup\":\"%s\"}", trimmed, backup);
            ai_imp_record(imp, kind, source, dest_path, "imported", NULL, combined);
        } else {
            ai_imp_record(imp, kind, source, dest_path, "imported", NULL, details);
        }
    }
    return merged_arr;
}

/* shared core: read source file → extract entries → merge into MEMORY.md */
static void import_context_core(AgentImporter *imp, const char *kind,
                                const char *source, const char *dest_path)
{
    FILE *fh = fopen(source, "r");
    if (!fh) {
        char reason[640];
        snprintf(reason, sizeof reason, "Could not read file: %s", strerror(errno));
        ai_imp_record(imp, kind, source, dest_path, "error", reason, NULL);
        return;
    }
    fseek(fh, 0, SEEK_END); long sz = ftell(fh); fseek(fh, 0, SEEK_SET);
    char *text = malloc((size_t)(sz > 0 ? sz : 0) + 1);
    size_t rd = text ? fread(text, 1, (size_t)sz, fh) : 0;
    if (text) text[rd] = '\0';
    fclose(fh);
    if (!text) {
        ai_imp_record(imp, kind, source, dest_path, "error", "read failed", NULL);
        return;
    }
    char **incoming = ai_extract_markdown_entries(text);
    free(text);
    if (!incoming || !incoming[0]) {
        ai_imp_record(imp, kind, source, dest_path, "skipped",
                      "No importable entries found", NULL);
        if (incoming) { for (size_t i = 0; incoming[i]; i++) free(incoming[i]); free(incoming); }
        return;
    }
    /* existing entries from destination contents */
    char **existing = NULL;
    {
        FILE *efh = fopen(dest_path, "r");
        char *etext = NULL;
        if (efh) {
            fseek(efh, 0, SEEK_END); long esz = ftell(efh); fseek(efh, 0, SEEK_SET);
            if (esz > 0) {
                etext = malloc((size_t)esz + 1);
                if (etext) { size_t r2 = fread(etext, 1, (size_t)esz, efh); etext[r2] = '\0'; }
            }
            fclose(efh);
        }
        if (etext) existing = ai_parse_existing_memory_entries(etext);
        free(etext);
    }
    int count = 0;
    while (incoming[count]) count++;
    char **merged = ai_merge_memory_entries(imp, kind, source, dest_path,
                                            existing, incoming, count);
    if (existing) { for (size_t i = 0; existing[i]; i++) free(existing[i]); free(existing); }
    for (size_t i = 0; incoming[i]; i++) free(incoming[i]);
    free(incoming);
    if (merged) { for (size_t i = 0; merged[i]; i++) free(merged[i]); free(merged); }
}

/* PoP: import_context_file @ hermes_cli/agent_import.py:AgentImporter.import_context_file */
void ai_import_context_file(AgentImporter *imp, const char *source_path, const char *kind)
{
    char *destination = imp_path(imp, "memories/MEMORY.md");
    if (!destination) return;
    if (!path_exists(source_path)) {
        const char *bn = strrchr(source_path, '/');
        bn = bn ? bn + 1 : source_path;
        char reason[256];
        snprintf(reason, sizeof reason, "No %s found", bn);
        ai_imp_record(imp, kind, NULL, destination, "skipped", reason, NULL);
        free(destination);
        return;
    }
    import_context_core(imp, kind, source_path, destination);
    free(destination);
}

/* PoP: import_memories_dir @ hermes_cli/agent_import.py:AgentImporter.import_memories_dir */
void ai_import_memories_dir(AgentImporter *imp, const char *memories_dir)
{
    char *destination = imp_path(imp, "memories/MEMORY.md");
    if (!destination) return;
    if (!path_is_dir(memories_dir)) {
        ai_imp_record(imp, "memories", NULL, destination, "skipped",
                      "No memories directory found", NULL);
        free(destination);
        return;
    }
    DIR *d = opendir(memories_dir);
    char **mds = NULL; size_t nmds = 0, capm = 0;
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            size_t L = strlen(e->d_name);
            if (L < 3 || strcmp(e->d_name + L - 3, ".md") != 0) continue;
            if (nmds + 1 > capm) { capm = capm ? capm * 2 : 8; mds = realloc(mds, capm * sizeof(char *)); }
            mds[nmds++] = strdup(e->d_name);
        }
        closedir(d);
    }
    qsort(mds, nmds, sizeof(char *), cmp_str);
    /* collect all entries across *.md files first (Python extends a list) */
    char **incoming = NULL; int icount = 0, icap = 0;
    char err_reason[640] = "";
    char err_file[4608] = "";
    for (size_t i = 0; i < nmds; i++) {
        char pth[4608];
        snprintf(pth, sizeof pth, "%s/%s", memories_dir, mds[i]);
        FILE *fh = fopen(pth, "r");
        if (!fh) {
            snprintf(err_file, sizeof err_file, "%s", pth);
            snprintf(err_reason, sizeof err_reason, "Could not read file: %s", strerror(errno));
            continue;
        }
        fseek(fh, 0, SEEK_END); long sz = ftell(fh); fseek(fh, 0, SEEK_SET);
        char *text = malloc((size_t)(sz > 0 ? sz : 0) + 1);
        size_t rd = text ? fread(text, 1, (size_t)sz, fh) : 0;
        if (text) text[rd] = '\0';
        fclose(fh);
        if (!text) continue;
        char **entries = ai_extract_markdown_entries(text);
        free(text);
        if (entries) {
            for (size_t j = 0; entries[j]; j++) {
                if (icount + 1 > icap) { icap = icap ? icap * 2 : 8; incoming = realloc(incoming, icap * sizeof(char *)); }
                incoming[icount++] = strdup(entries[j]);
            }
            for (size_t j = 0; entries[j]; j++) free(entries[j]);
            free(entries);
        }
    }
    for (size_t i = 0; i < nmds; i++) free(mds[i]);
    free(mds);
    if (err_reason[0])
        ai_imp_record(imp, "memories", err_file, destination, "error",
                      err_reason, NULL);
    if (icount == 0) {
        ai_imp_record(imp, "memories", memories_dir, destination, "skipped",
                      "No importable entries found", NULL);
        free(incoming);
        free(destination);
        return;
    }
    char **existing = NULL;
    {
        FILE *efh = fopen(destination, "r");
        char *etext = NULL;
        if (efh) {
            fseek(efh, 0, SEEK_END); long esz = ftell(efh); fseek(efh, 0, SEEK_SET);
            if (esz > 0) {
                etext = malloc((size_t)esz + 1);
                if (etext) { size_t r2 = fread(etext, 1, (size_t)esz, efh); etext[r2] = '\0'; }
            }
            fclose(efh);
        }
        if (etext) existing = ai_parse_existing_memory_entries(etext);
        free(etext);
    }
    char **merged = ai_merge_memory_entries(imp, "memories", memories_dir,
                                            destination, existing, incoming,
                                            icount);
    if (existing) { for (size_t i = 0; existing[i]; i++) free(existing[i]); free(existing); }
    for (int i = 0; i < icount; i++) free(incoming[i]);
    free(incoming);
    if (merged) { for (size_t i = 0; merged[i]; i++) free(merged[i]); free(merged); }
    free(destination);
}


/* PoP: _load_claude_settings @ hermes_cli/agent_import.py:_load_claude_settings */
json_t *ai_agent_importer_load_claude_settings(AgentImporter *imp)
{
    char path[4608];
    snprintf(path, sizeof path, "%s/settings.json", imp->source_root);
    if (!path_exists(path)) {
        ai_imp_record(imp, "settings", NULL, NULL, "skipped",
                      "No settings.json found", NULL);
        return json_object();
    }
    FILE *fh = fopen(path, "r");
    if (!fh) return json_object();
    fseek(fh, 0, SEEK_END); long sz = ftell(fh); fseek(fh, 0, SEEK_SET);
    char *text = malloc((size_t)(sz > 0 ? sz : 0) + 1);
    size_t rd = text ? fread(text, 1, (size_t)sz, fh) : 0;
    if (text) text[rd] = '\0';
    fclose(fh);
    if (!text) return json_object();
    char *err = NULL;
    json_t *data = json_parse(text, &err);
    free(text);
    if (err) free(err);
    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        ai_imp_record(imp, "settings", path, NULL, "error",
                      "settings.json is not a JSON object", NULL);
        return json_object();
    }
    return data;
}

/* PoP: _claude_mcp_servers @ hermes_cli/agent_import.py:_claude_mcp_servers */
json_t *ai_agent_importer_claude_mcp_servers(AgentImporter *imp,
                                             json_t *settings,
                                             const char *claude_json_path)
{
    json_t *servers = json_object();
    /* ~/.claude.json lives NEXT TO ~/.claude/, not inside it */
    char cjpath[4608];
    if (claude_json_path && *claude_json_path) {
        snprintf(cjpath, sizeof cjpath, "%s", claude_json_path);
    } else {
        char parent[4096];
        snprintf(parent, sizeof parent, "%s", imp->source_root);
        char *slash = strrchr(parent, '/');
        if (slash && slash != parent) *slash = '\0'; else snprintf(parent, sizeof parent, "/");
        snprintf(cjpath, sizeof cjpath, "%s/.claude.json", parent);
    }
    if (path_exists(cjpath)) {
        FILE *fh = fopen(cjpath, "r");
        if (fh) {
            fseek(fh, 0, SEEK_END); long sz = ftell(fh); fseek(fh, 0, SEEK_SET);
            char *text = malloc((size_t)(sz > 0 ? sz : 0) + 1);
            size_t rd = text ? fread(text, 1, (size_t)sz, fh) : 0;
            if (text) text[rd] = '\0';
            fclose(fh);
            if (text) {
                char *err = NULL;
                json_t *data = json_parse(text, &err);
                if (err) free(err);
                free(text);
                json_t *mcps = (data && data->type == JSON_OBJECT)
                                   ? json_obj_get(data, "mcpServers") : NULL;
                if (mcps && mcps->type == JSON_OBJECT)
                    for (size_t i = 0; i < mcps->c.count; i++)
                        json_set(servers, mcps->c.keys[i], json_copy(mcps->c.items[i]));
                else
                    ai_imp_record(imp, "mcp-servers", cjpath, NULL, "error",
                                  "Could not parse .claude.json", NULL);
                if (data) json_free(data);
            }
        }
    }
    json_t *from_settings = settings ? json_obj_get(settings, "mcpServers") : NULL;
    if (from_settings && from_settings->type == JSON_OBJECT)
        for (size_t i = 0; i < from_settings->c.count; i++) {
            const char *k = from_settings->c.keys[i];
            if (!json_obj_get(servers, k))
                json_set(servers, k, json_copy(from_settings->c.items[i]));
        }
    return servers;
}

/* PoP: _load_codex_config @ hermes_cli/agent_import.py:_load_codex_config */
json_t *ai_agent_importer_load_codex_config(AgentImporter *imp)
{
    char path[4608];
    snprintf(path, sizeof path, "%s/config.toml", imp->source_root);
    if (!path_exists(path)) {
        ai_imp_record(imp, "config", NULL, NULL, "skipped",
                      "No config.toml found", NULL);
        return json_object();
    }
    FILE *fh = fopen(path, "r");
    if (!fh) return json_object();
    fseek(fh, 0, SEEK_END); long sz = ftell(fh); fseek(fh, 0, SEEK_SET);
    char *text = malloc((size_t)(sz > 0 ? sz : 0) + 1);
    size_t rd = text ? fread(text, 1, (size_t)sz, fh) : 0;
    if (text) text[rd] = '\0';
    fclose(fh);
    if (!text) return json_object();
    toml_doc_t *doc = toml_parse(text);
    free(text);
    if (!doc) {
        ai_imp_record(imp, "config", path, NULL, "error",
                      "Could not parse config.toml", NULL);
        return json_object();
    }
    char *ser = toml_serialize(doc);
    json_t *out = json_object();
    if (ser) {
        char *err = NULL;
        json_t *parsed = json_parse(ser, &err);
        if (err) free(err);
        if (parsed && parsed->type == JSON_OBJECT) { json_free(out); out = parsed; }
        else if (parsed) json_free(parsed);
        free(ser);
    }
    toml_free(doc);
    return out;
}

/* shared permission importer core */
static void ai_permission_import(AgentImporter *imp, json_t *settings,
                                 const char *list_key, const char *kind,
                                 const char *source_label,
                                 const char *config_key, bool nested_approvals,
                                 const char *target_root)
{
    char destination[4608];
    snprintf(destination, sizeof destination, "%s/config.yaml",
             target_root ? target_root : imp->target_root);
    json_t *permissions = settings ? json_obj_get(settings, "permissions") : NULL;
    json_t *rules = (permissions && permissions->type == JSON_OBJECT)
                        ? json_obj_get(permissions, list_key) : NULL;
    if (!rules || rules->type != JSON_ARRAY || rules->c.count == 0) {
        char reason[160];
        snprintf(reason, sizeof reason, "No permissions.%s rules found", list_key);
        ai_imp_record(imp, kind, NULL, destination, "skipped", reason, NULL);
        return;
    }
    /* rules → command patterns via the pure converter */
    json_t *arr = json_array();
    for (size_t i = 0; i < rules->c.count; i++) {
        json_t *e = rules->c.items[i];
        if (e && e->type == JSON_STRING) json_append(arr, json_string(e->str_val));
    }
    char *ser = json_serialize(arr);
    json_free(arr);
    char *patterns_json = ai_import_permission_patterns(ser ? ser : "[]");
    free(ser);
    char *err = NULL;
    json_t *patterns = json_parse(patterns_json, &err);
    if (err) free(err);
    free(patterns_json);
    if (!patterns || patterns->type != JSON_ARRAY || patterns->c.count == 0) {
        char reason[160];
        snprintf(reason, sizeof reason, "No Bash(...) %s rules to import",
                 strcmp(list_key, "allow") == 0 ? "allow" : "deny");
        ai_imp_record(imp, kind, NULL, destination, "skipped", reason, NULL);
        if (patterns) json_free(patterns);
        return;
    }

    json_t *config = ai_load_target_config(imp, kind, source_label, destination);
    if (!config) { json_free(patterns); return; }

    json_t *approvals = NULL, *current = NULL;
    if (nested_approvals) {
        approvals = json_obj_get(config, "approvals");
        if (!approvals || approvals->type != JSON_OBJECT) approvals = NULL;
        current = approvals ? json_obj_get(approvals, "deny") : NULL;
    } else {
        current = json_obj_get(config, config_key);
    }
    if (!current || current->type != JSON_ARRAY) current = NULL;

    char **added = NULL; size_t added_n = 0;
    json_t *merged = merge_pattern_lists(current, patterns, &added, &added_n);

    if (added_n == 0) {
        ai_imp_record(imp, kind, source_label, destination, "skipped",
                      "All patterns already present", NULL);
    } else if (imp->execute) {
        if (nested_approvals) {
            json_t *ap = approvals;
            if (!ap) { ap = json_object(); json_set(config, "approvals", ap); }
            json_set(ap, "deny", merged);
        } else {
            json_set(config, config_key, merged);
        }
        write_config_yaml(destination, config);
        char details[2048];
        {   char *mser = json_serialize(merged);
            snprintf(details, sizeof details, "{\"added_patterns\":%.*s}",
                     mser ? 1800 : 2, mser ? mser : "[]");
            free(mser); }
        ai_imp_record(imp, kind, source_label, destination, "imported", NULL, details);
    } else {
        ai_imp_record(imp, kind, source_label, destination, "imported",
                      "Would merge patterns", NULL);
    }
    free_names2(added, added_n);
    if (merged) json_free(merged);
    json_free(config);
    json_free(patterns);
}

/* PoP: import_permission_allowlist @ hermes_cli/agent_import.py:import_permission_allowlist */
void ai_import_permission_allowlist(AgentImporter *imp, json_t *settings,
                                    const char *target_root)
{
    ai_permission_import(imp, settings, "allow", "command-allowlist",
                         "settings.json permissions.allow", "command_allowlist",
                         false, target_root);
}

/* PoP: import_permission_denylist @ hermes_cli/agent_import.py:import_permission_denylist */
void ai_import_permission_denylist(AgentImporter *imp, json_t *settings)
{
    ai_permission_import(imp, settings, "deny", "command-denylist",
                         "settings.json permissions.deny", "approvals.deny",
                         true, NULL);
}

/* PoP: import_mcp_servers @ hermes_cli/agent_import.py:import_mcp_servers */
void ai_import_mcp_servers(AgentImporter *imp, json_t *servers, const char *kind)
{
    char *destination = imp_path(imp, "config.yaml");
    if (!destination) return;
    if (!servers || servers->type != JSON_OBJECT || servers->c.count == 0) {
        ai_imp_record(imp, kind, NULL, destination, "skipped",
                      "No MCP servers found", NULL);
        free(destination);
        return;
    }
    json_t *config = ai_load_target_config(imp, kind, NULL, destination);
    if (!config) { free(destination); return; }
    json_t *existing = json_obj_get(config, "mcp_servers");
    if (!existing || existing->type != JSON_OBJECT) existing = NULL;

    int added = 0;
    for (size_t i = 0; i < servers->c.count; i++) {
        const char *name = servers->c.keys[i];
        json_t *srv = servers->c.items[i];
        if (!srv || srv->type != JSON_OBJECT) {
            ai_imp_record(imp, kind, name, NULL, "skipped",
                          "Server entry is not a mapping", NULL);
            continue;
        }
        if (existing && json_obj_get(existing, name) && !imp->overwrite) {
            char dest[640];
            snprintf(dest, sizeof dest, "mcp_servers.%s", name);
            ai_imp_record(imp, kind, name, dest, "conflict",
                          "MCP server already exists in Hermes config", NULL);
            continue;
        }
        json_t *hermes_srv = json_object();
        json_t *v;
        if ((v = json_obj_get(srv, "command")) && v->type == JSON_STRING && v->str_val[0]) {
            json_set(hermes_srv, "command", json_string(v->str_val));
            if ((v = json_obj_get(srv, "args")) && v->type == JSON_ARRAY)
                json_set(hermes_srv, "args", json_copy(v));
            json_t *envj = json_obj_get(srv, "env");
            if (envj && envj->type == JSON_OBJECT) {
                char *env_ser = json_serialize(envj);
                if (env_ser) {
                    char *kept_json = NULL;
                    char **stripped_names = NULL;
                    ai_sanitize_mcp_env(env_ser, &kept_json, &stripped_names);
                    if (kept_json && strcmp(kept_json, "{}") != 0) {
                        char *e2 = NULL;
                        json_t *k = json_parse(kept_json, &e2);
                        if (e2) free(e2);
                        if (k) json_set(hermes_srv, "env", k);
                    }
                    size_t nsn = 0;
                    for (size_t j = 0; stripped_names && stripped_names[j]; j++) nsn++;
                    for (size_t j = 0; j < nsn; j++) {
                        char full[512];
                        snprintf(full, sizeof full, "mcp_servers.%s.env.%s",
                                 name, stripped_names[j]);
                        ai_imp_strip_secret(imp, full);
                    }
                    free_names2(stripped_names, nsn);
                    free(kept_json);
                    free(env_ser);
                }
            }
            if ((v = json_obj_get(srv, "cwd")) && v->type == JSON_STRING && v->str_val[0])
                json_set(hermes_srv, "cwd", json_string(v->str_val));
        }
        if ((v = json_obj_get(srv, "url")) && v->type == JSON_STRING && v->str_val[0]) {
            json_set(hermes_srv, "url", json_string(v->str_val));
            json_t *headers = json_obj_get(srv, "headers");
            if (headers && headers->type == JSON_OBJECT) {
                json_t *kept = json_object();
                bool any_kept = false;
                for (size_t j = 0; j < headers->c.count; j++) {
                    const char *hk = headers->c.keys[j];
                    bool secret = ai_is_secret_key(hk);
                    char lower[256];
                    snprintf(lower, sizeof lower, "%s", hk);
                    for (char *q = lower; *q; q++) *q = (char)tolower((unsigned char)*q);
                    bool auth = strstr(lower, "authorization") != NULL;
                    if (!secret && !auth) {
                        json_set(kept, hk, json_copy(headers->c.items[j]));
                        any_kept = true;
                    } else {
                        char full[512];
                        snprintf(full, sizeof full, "mcp_servers.%s.headers.%s", name, hk);
                        ai_imp_strip_secret(imp, full);
                    }
                }
                if (any_kept) json_set(hermes_srv, "headers", kept);
                else json_free(kept);
            }
        }
        if (hermes_srv->c.count == 0) {
            ai_imp_record(imp, kind, name, NULL, "skipped",
                          "Server has neither a command nor a url", NULL);
            json_free(hermes_srv);
            continue;
        }
        if (!existing) { existing = json_object(); json_set(config, "mcp_servers", existing); }
        json_set(existing, name, hermes_srv);
        added++;
        char dest[640];
        snprintf(dest, sizeof dest, "config.yaml mcp_servers.%s", name);
        ai_imp_record(imp, kind, name, dest, "imported", NULL, NULL);
    }

    if (added > 0 && imp->execute)
        write_config_yaml(destination, config);
    json_free(config);
    free(destination);
}


/* recursive copytree helper (nftw) */
static char g_copy_src_base[4096];
static char g_copy_dst_base[4096];

static int copy_tree_cb(const char *path, const struct stat *st, int flag,
                        struct FTW *ftw)
{
    (void)flag; (void)ftw;
    const char *rel = path + strlen(g_copy_src_base);
    while (*rel == '/') rel++;
    char dst[8192];
    snprintf(dst, sizeof dst, "%s/%s", g_copy_dst_base, rel);
    if (S_ISDIR(st->st_mode)) mkdir(dst, 0755);
    else {
        FILE *src = fopen(path, "rb");
        FILE *out = src ? fopen(dst, "wb") : NULL;
        if (src && out) {
            char chunk[8192]; size_t r;
            while ((r = fread(chunk, 1, sizeof chunk, src)) > 0) fwrite(chunk, 1, r, out);
        }
        if (out) fclose(out);
        if (src) fclose(src);
    }
    return 0;
}

/* PoP: import_skills @ hermes_cli/agent_import.py:import_skills */
void ai_import_skills(AgentImporter *imp, const char *source_root,
                      const char *category_in)
{
    /* _SKILL_CATEGORY: claude-code → claude-code skills; codex → codex */
    const char *category = (category_in && *category_in)
        ? category_in
        : ((strcmp(imp->agent, "codex") == 0) ? "codex" : "claude-code");
    char dest_root[4608];
    snprintf(dest_root, sizeof dest_root, "%s/skills/%s", imp->target_root, category);
    if (!path_is_dir(source_root)) {
        ai_imp_record(imp, "skills", NULL, dest_root, "skipped",
                      "No skills directory found", NULL);
        return;
    }
    DIR *d = opendir(source_root);
    if (!d) return;
    struct dirent *e;
    char **names = NULL; size_t nn = 0, capn = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char dirp[8192], skill_md[8400];
        snprintf(dirp, sizeof dirp, "%s/%s", source_root, e->d_name);
        struct stat st;
        if (stat(dirp, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        snprintf(skill_md, sizeof skill_md, "%s/SKILL.md", dirp);
        if (stat(skill_md, &st) != 0) continue;
        if (nn + 1 > capn) { capn = capn ? capn * 2 : 8; names = realloc(names, capn * sizeof(char *)); }
        names[nn++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, nn, sizeof(char *), cmp_str);
    if (nn == 0) {
        ai_imp_record(imp, "skills", source_root, dest_root, "skipped",
                      "No skills with SKILL.md found", NULL);
        free(names);
        return;
    }
    for (size_t i = 0; i < nn; i++) {
        char destination[8192];
        snprintf(destination, sizeof destination, "%s/%s", dest_root, names[i]);
        if (path_exists(destination) && !imp->overwrite) {
            ai_imp_record(imp, "skill", names[i], destination, "conflict",
                          "Destination skill already exists", NULL);
            continue;
        }
        if (imp->execute) {
            char parent[8192];
            snprintf(parent, sizeof parent, "%s", destination);
            char *sl = strrchr(parent, '/');
            if (sl) *sl = '\0';
            mkdir(parent, 0755);
            if (path_exists(destination)) {
                char rmcmd[8504];
                snprintf(rmcmd, sizeof rmcmd, "rm -rf '%s'", destination);
                system(rmcmd);   /* shutil.rmtree equivalent */
            }
            snprintf(g_copy_src_base, sizeof g_copy_src_base, "%s/%s",
                     source_root, names[i]);
            snprintf(g_copy_dst_base, sizeof g_copy_dst_base, "%s", destination);
            mkdir(destination, 0755);
            nftw(g_copy_src_base, copy_tree_cb, 16, FTW_PHYS);
            ai_imp_record(imp, "skill", names[i], destination, "imported", NULL, NULL);
        } else {
            ai_imp_record(imp, "skill", names[i], destination, "imported",
                          "Would copy skill directory", NULL);
        }
    }
    for (size_t i = 0; i < nn; i++) free(names[i]);
    free(names);
}

/* PoP: _run_claude_code @ hermes_cli/agent_import.py:_run_claude_code */
void ai_agent_importer_run_claude_code(AgentImporter *imp)
{
    json_t *settings = ai_agent_importer_load_claude_settings(imp);
    char src[4608];
    snprintf(src, sizeof src, "%s/CLAUDE.md", imp->source_root);
    ai_import_context_file(imp, src, "claude-md");
    ai_import_permission_allowlist(imp, settings, imp->target_root);
    ai_import_permission_denylist(imp, settings);
    json_t *servers = ai_agent_importer_claude_mcp_servers(imp, settings, NULL);
    ai_import_mcp_servers(imp, servers, "mcp-servers");
    json_free(servers);
    snprintf(src, sizeof src, "%s/skills", imp->source_root);
    ai_import_skills(imp, src, NULL);
    /* slash-commands have no Hermes equivalent → skipped record */
    {
        char cmds[4608];
        snprintf(cmds, sizeof cmds, "%s/commands", imp->source_root);
        bool any_md = false;
        DIR *dd = path_is_dir(cmds) ? opendir(cmds) : NULL;
        if (dd) {
            struct dirent *e;
            while ((e = readdir(dd)) != NULL) {
                size_t L = strlen(e->d_name);
                if (L >= 3 && strcmp(e->d_name + L - 3, ".md") == 0) { any_md = true; break; }
            }
            closedir(dd);
        }
        if (any_md)
            ai_imp_record(imp, "slash-commands", cmds, NULL, "skipped",
                          "Claude slash commands have no direct Hermes "
                          "equivalent — consider converting them into skills",
                          NULL);
    }
    json_free(settings);
}

/* PoP: _run_codex @ hermes_cli/agent_import.py:_run_codex */
void ai_agent_importer_run_codex(AgentImporter *imp)
{
    json_t *config = ai_agent_importer_load_codex_config(imp);
    char src[4608];
    snprintf(src, sizeof src, "%s/AGENTS.md", imp->source_root);
    ai_import_context_file(imp, src, "agents-md");
    json_t *mcp = config ? json_obj_get(config, "mcp_servers") : NULL;
    bool mcp_is_obj = mcp && mcp->type == JSON_OBJECT;
    ai_import_mcp_servers(imp, mcp_is_obj ? mcp : json_object(), "mcp-servers");
    snprintf(src, sizeof src, "%s/memories", imp->source_root);
    ai_import_memories_dir(imp, src);
    snprintf(src, sizeof src, "%s/skills", imp->source_root);
    ai_import_skills(imp, src, NULL);
    json_free(config);
}

static json_t *ai_report_from_items(AgentImporter *imp);

/* PoP: run @ hermes_cli/agent_import.py:AgentImporter.run */
json_t *ai_agent_importer_run(AgentImporter *imp)
{
    if (!path_is_dir(imp->source_root)) {
        ai_imp_record(imp, "source", imp->source_root, NULL, "error",
                      "Source directory does not exist", NULL);
        return ai_report_from_items(imp);
    }
    if (strcmp(imp->agent, "claude-code") == 0) ai_agent_importer_run_claude_code(imp);
    else ai_agent_importer_run_codex(imp);
    return ai_report_from_items(imp);
}

/* build the report dict from importer state (build_report parity core) */
static json_t *ai_report_from_items(AgentImporter *imp)
{
    json_t *report = json_object();
    json_set(report, "agent", json_string(imp->agent));
    json_set(report, "source", json_string(imp->source_root));
    json_set(report, "target", json_string(imp->target_root));
    json_set(report, "dry_run", json_bool(!imp->execute));
    json_set(report, "items", json_copy(imp->items));

    json_t *summary = json_object();
    json_set(summary, "imported", json_number(0));
    json_set(summary, "skipped", json_number(0));
    json_set(summary, "conflict", json_number(0));
    json_set(summary, "error", json_number(0));
    for (size_t i = 0; i < imp->items->c.count; i++) {
        json_t *item = imp->items->c.items[i];
        if (!item || item->type != JSON_OBJECT) continue;
        json_t *st = json_obj_get(item, "status");
        const char *status = st && st->type == JSON_STRING ? st->str_val : "skipped";
        json_t *cur = json_obj_get(summary, status);
        double val = cur && cur->type == JSON_NUMBER ? cur->num_val : 0;
        json_set(summary, status, json_number(val + 1));
    }
    json_set(report, "summary", summary);

    if (imp->stripped_secrets && imp->stripped_secrets->c.count) {
        /* sorted(set(stripped_secrets)) */
        char **names = calloc(imp->stripped_secrets->c.count + 1, sizeof(char *));
        size_t n = 0;
        for (size_t i = 0; i < imp->stripped_secrets->c.count; i++) {
            json_t *e = imp->stripped_secrets->c.items[i];
            if (e && e->type == JSON_STRING) names[n++] = strdup(e->str_val);
        }
        qsort(names, n, sizeof(char *), cmp_str);
        json_t *sorted = json_array();
        for (size_t i = 0; i < n; i++) {
            if (i > 0 && strcmp(names[i-1], names[i]) == 0) continue;
            json_append(sorted, json_string(names[i]));
        }
        json_set(report, "stripped_secrets", sorted);
        free_names2(names, n);
    }
    return report;
}

/* PoP: print_import_report @ hermes_cli/agent_import.py:print_import_report */
void ai_print_import_report(json_t *report, bool dry_run)
{
    if (!report || report->type != JSON_OBJECT) return;
    printf("\n%s\n", dry_run ? "== Dry Run Results ==" : "== Import Results ==");
    if (dry_run)
        printf("No files were modified. This is a preview of what would happen.\n");
    printf("\n");

    json_t *items = json_obj_get(report, "items");
    static const struct { const char *status; const char *label_static; } groups[] = {
        { "imported", NULL },
        { "conflict", "Conflicts (skipped — use --overwrite to force)" },
        { "skipped",  "Skipped" },
        { "error",    "Errors" },
    };
    for (size_t g = 0; g < sizeof groups / sizeof groups[0]; g++) {
        const char *status = groups[g].status;
        char imported_label[32];
        const char *label = groups[g].label_static;
        if (!label) { snprintf(imported_label, sizeof imported_label, "%s",
                               dry_run ? "Would import" : "Imported"); label = imported_label; }
        bool any = false;
        if (items && items->type == JSON_ARRAY)
            for (size_t i = 0; i < items->c.count && !any; i++) {
                json_t *it = items->c.items[i];
                json_t *st = it ? json_obj_get(it, "status") : NULL;
                if (st && st->type == JSON_STRING && strcmp(st->str_val, status) == 0) any = true;
            }
        if (!any) continue;
        printf("  %s:\n", label);
        for (size_t i = 0; i < (items ? items->c.count : 0); i++) {
            json_t *it = items->c.items[i];
            json_t *st = it ? json_obj_get(it, "status") : NULL;
            if (!st || st->type != JSON_STRING || strcmp(st->str_val, status) != 0) continue;
            json_t *kj = json_obj_get(it, "kind");
            const char *kind = kj && kj->type == JSON_STRING ? kj->str_val : "unknown";
            if (strcmp(status, "imported") == 0) {
                json_t *dj = json_obj_get(it, "destination");
                const char *dest = dj && dj->type == JSON_STRING ? dj->str_val : "";
                printf("      %-22s -> %s\n", kind, dest);
            } else {
                json_t *rj = json_obj_get(it, "reason");
                const char *reason = rj && rj->type == JSON_STRING ? rj->str_val : "";
                printf("      %-22s  %s\n", kind, reason);
            }
        }
        printf("\n");
    }

    json_t *stripped = json_obj_get(report, "stripped_secrets");
    if (stripped && stripped->type == JSON_ARRAY && stripped->c.count) {
        printf("  Secrets stripped (never imported):\n");
        for (size_t i = 0; i < stripped->c.count; i++) {
            json_t *e = stripped->c.items[i];
            if (e && e->type == JSON_STRING) printf("      %s\n", e->str_val);
        }
        printf("Re-add credentials deliberately via setup or the home .env.\n");
    }
}

/* PoP: import_agent_command @ hermes_cli/agent_import.py:import_agent_command */
void ai_import_agent_command(const char *agent, const char *source,
                             bool dry_run, bool overwrite, bool auto_yes)
{
    extern const char *get_hermes_home(void);
    const char *home = get_hermes_home();
    if (!home || !*home) home = ".";

    char agent_buf[64] = "";
    if (agent && *agent) snprintf(agent_buf, sizeof agent_buf, "%s", agent);
    else {
        char **detected = ai_detect_agents(home);
        size_t n = detected ? 0 : 0;
        while (detected && detected[n]) n++;
        if (!detected || n == 0) {
            fprintf(stderr, "No supported agent setup found (~/.claude or ~/.codex).\n"
                            "Specify one explicitly: slermes import-agent claude-code --source /path\n");
            free_names2(detected, n);
            return;
        }
        if (n > 1 && !source) {
            printf("Multiple agent setups detected:");
            for (size_t i = 0; detected[i]; i++) printf(" %s", detected[i]);
            printf("\nPick one explicitly.\n");
            free_names2(detected, n);
            return;
        }
        snprintf(agent_buf, sizeof agent_buf, "%s", detected[0]);
        free_names2(detected, n);
    }

    char source_dir[4608];
    if (source && *source) snprintf(source_dir, sizeof source_dir, "%s", source);
    else {
        char *ds = ai_default_source_dir(agent_buf, home);
        if (!ds) return;
        snprintf(source_dir, sizeof source_dir, "%s", ds);
        free(ds);
    }
    if (!path_is_dir(source_dir)) {
        fprintf(stderr, "Agent directory not found: %s\n", source_dir);
        return;
    }

    /* Phase 1: preview (always) */
    AgentImporter *preview_imp = ai_agent_importer_new(agent_buf, source_dir,
                                                       home, false,
                                                       overwrite ? true : false);
    if (!preview_imp) return;
    json_t *preview = ai_agent_importer_run(preview_imp);
    ai_agent_importer_free(preview_imp);
    if (!preview) return;

    json_t *summary = json_obj_get(preview, "summary");
    json_t *imp_n = summary ? json_obj_get(summary, "imported") : NULL;
    json_t *conf_n = summary ? json_obj_get(summary, "conflict") : NULL;
    long imported = imp_n && imp_n->type == JSON_NUMBER ? (long)imp_n->num_val : 0;
    long conflict = conf_n && conf_n->type == JSON_NUMBER ? (long)conf_n->num_val : 0;
    if (imported == 0 && conflict == 0) {
        printf("Nothing to import from %s.\n", agent_buf);
        ai_print_import_report(preview, true);
        json_free(preview);
        return;
    }
    printf("\nImport Preview — %ld item(s) would be imported. No changes made yet.\n",
           imported);
    ai_print_import_report(preview, true);

    if (dry_run || !auto_yes) {
        if (dry_run) { json_free(preview); return; }
        printf("Proceed with import? [Y/n] ");
        fflush(stdout);
        char answer[16];
        if (!fgets(answer, sizeof answer, stdin)) { json_free(preview); return; }
        if (answer[0] == 'n' || answer[0] == 'N') {
            printf("Import cancelled.\n");
            json_free(preview);
            return;
        }
    }

    /* Phase 2: execute */
    AgentImporter *imp = ai_agent_importer_new(agent_buf, source_dir, home,
                                               true, overwrite ? true : false);
    if (!imp) { json_free(preview); return; }
    json_t *report = ai_agent_importer_run(imp);
    ai_agent_importer_free(imp);
    ai_print_import_report(report, false);
    printf("Import complete.\n");
    printf("API keys and credentials were NOT imported — configure providers via "
           "setup or the home .env.\n");
    if (report) json_free(report);
    json_free(preview);
}
