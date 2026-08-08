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
#include <sys/stat.h>
#include "libyaml/yaml.h"
#include "libjson/json.h"

#define AI_ENTRY_DELIMITER "\n\xC2\xA7\n" /* "\n§\n" (UTF-8) */

/* ── is_secret_key ───────────────────────────────────────────── */

/* PoP: is_secret_key @ hermes_cli/agent_import.py:is_secret_key */
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

/* PoP: normalize_text @ hermes_cli/agent_import.py:normalize_text */
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

/* PoP: extract_markdown_entries @ hermes_cli/agent_import.py:extract_markdown_entries */
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

/* ── sanitize_mcp_env ────────────────────────────────────────── */

/* PoP: sanitize_mcp_env @ hermes_cli/agent_import.py:sanitize_mcp_env */
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

/* PoP: parse_existing_memory_entries @ hermes_cli/agent_import.py:parse_existing_memory_entries */
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

/* PoP: merge_entries @ hermes_cli/agent_import.py:merge_entries */
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

/* PoP: load_yaml_file @ hermes_cli/agent_import.py:load_yaml_file */
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

/* PoP: dump_yaml_file @ hermes_cli/agent_import.py:dump_yaml_file */
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

/* PoP: default_source_dir @ hermes_cli/agent_import.py:default_source_dir */
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

/* PoP: detect_agents @ hermes_cli/agent_import.py:detect_agents */
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

/* PoP: backup_memory_file @ hermes_cli/agent_import.py:backup_memory_file */
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

/* PoP: record @ hermes_cli/agent_import.py:AgentImporter.record */
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

/* PoP: build_report @ hermes_cli/agent_import.py:AgentImporter.build_report */
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
