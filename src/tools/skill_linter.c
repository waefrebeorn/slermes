/*
 * skill_linter.c — structural + convention linter for SKILL.md files.
 *
 * Self-contained module: cohesive C11 port of tools/skill_linter.py.
 * See skill_linter.h for the public API and PoP annotations.
 */

#include "skill_linter.h"
#include "hermes_regex.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <glob.h>

#define SKILL_PROMPT_DESC_LIMIT 60   /* agent/skill_utils.py */
#define SL_LINT_ERROR   "error"
#define SL_LINT_WARNING  "warning"

/* ── Rule data ──────────────────────────────────────────────────────────────── */

typedef struct {
    const char *token;
    const char *tool;
} shell_util_t;

static const shell_util_t SHELL_UTILS[] = {
    { "grep", "search_files" },
    { "rg", "search_files" },
    { "cat", "read_file" },
    { "head", "read_file" },
    { "tail", "read_file" },
    { "sed", "patch" },
    { "awk", "patch" },
    { "find", "search_files (target='files')" },
    { "ls", "search_files (target='files')" },
};
static const size_t SHELL_UTILS_N =
    sizeof(SHELL_UTILS) / sizeof(SHELL_UTILS[0]);

static const char *MARKETING_WORDS[] = {
    "powerful", "comprehensive", "seamless", "advanced",
    "cutting-edge", "state-of-the-art", "revolutionary", "robust",
};
static const size_t MARKETING_N =
    sizeof(MARKETING_WORDS) / sizeof(MARKETING_WORDS[0]);

static const char *POSIX_PRIMITIVES[] = {
    "fcntl", "termios", "os.setsid", "signal.SIGKILL",
    "osascript", "/proc/", "apt-get", "systemctl",
};
static const size_t POSIX_N =
    sizeof(POSIX_PRIMITIVES) / sizeof(POSIX_PRIMITIVES[0]);

static const char *FORBIDDEN_FILES[] = {
    "README.md", "CHANGELOG.md", "install.sh", ".env",
    ".env.example", ".gitignore",
};
static const size_t FORBIDDEN_N =
    sizeof(FORBIDDEN_FILES) / sizeof(FORBIDDEN_FILES[0]);

static const char *EXPECTED_SECTIONS[] = { "When to Use", "When to use" };
static const size_t EXPECTED_SECTIONS_N =
    sizeof(EXPECTED_SECTIONS) / sizeof(EXPECTED_SECTIONS[0]);

/* ── Append helper ─────────────────────────────────────────────────────────── */

static size_t append_finding(lint_finding_t *out, size_t n, size_t cap,
                             const char *severity, const char *rule,
                             const char *message)
{
    if (n >= cap) return n;
    out[n].severity = severity;
    strncpy(out[n].rule, rule, LINTER_MAX_RULE - 1);
    out[n].rule[LINTER_MAX_RULE - 1] = '\0';
    strncpy(out[n].message, message, LINTER_MAX_MSG - 1);
    out[n].message[LINTER_MAX_MSG - 1] = '\0';
    return n + 1;
}

/* word_contains: case-insensitive substring match across a word. */
static bool word_contains(const char *haystack, const char *needle)
{
    size_t hlen = strlen(haystack), nlen = strlen(needle);
    if (nlen == 0) return false;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        if (strncasecmp(haystack + i, needle, nlen) == 0)
            return true;
    }
    return false;
}

/* lowercpy: lowercase a string into buf (truncation-safe). */
static void lowercpy(const char *src, char *buf, size_t sz)
{
    if (sz == 0) return;
    size_t i;
    for (i = 0; i < sz - 1 && src[i]; i++)
        buf[i] = (char)tolower((unsigned char)src[i]);
    buf[i] = '\0';
}

/* ── LintFinding ─────────────────────────────────────────────────────────────── */

/* PoP: lint_finding_format @ tools/skill_linter.py:LintFinding.format */
void lint_finding_format(const lint_finding_t *f, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    /* Python uses U+2717 (✗) for errors, U+26A0 (⚠) for warnings. */
    const char *badge = (strcmp(f->severity, SL_LINT_ERROR) == 0)
                        ? "\xe2\x9c\x97"   /* ✗ */
                        : "\xe2\x9a\xa0";  /* ⚠ */
    snprintf(out, out_sz, "%s [%s] %s", badge, f->rule, f->message);
}

/* PoP: lint_has_errors @ tools/skill_linter.py:has_errors */
int lint_has_errors(const lint_finding_t *findings, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (strcmp(findings[i].severity, SL_LINT_ERROR) == 0)
            return 1;
    return 0;
}

/* ── Frontmatter parsing ─────────────────────────────────────────────────────── */

/* Check if a top-level key exists in the frontmatter (including block-style
 * keys whose value is on subsequent indented lines). Matches Python's
 * ``key in frontmatter`` for presence-based checks. */
static bool frontmatter_has_key(const char *content, const char *key)
{
    if (!content || !key) return false;
    const char *f1 = strstr(content, "---");
    if (!f1) return false;
    const char *f2 = strstr(f1 + 4, "---");
    if (!f2) return false;
    size_t klen = strlen(key);
    const char *p = f1;
    while (p < f2) {
        const char *nl = strchr(p, '\n');
        const char *line = nl ? nl + 1 : p;
        if (line >= f2) break;
        const char *s = line;
        while (s < f2 && (*s == ' ' || *s == '\t')) s++;
        if ((size_t)(f2 - s) >= klen && strncmp(s, key, klen) == 0
            && (s + klen < f2 && (s[klen] == ':' || s[klen] == ' ' || s[klen] == '\t')))
            return true;
        p = line;
    }
    return false;
}

/* Check if sub_key exists as an indented child of parent_key's block.
 * E.g. does "hermes:" appear under the "metadata:" block? */
static bool frontmatter_has_subkey(const char *content, const char *parent_key,
                                   const char *sub_key)
{
    if (!content || !parent_key || !sub_key) return false;
    const char *f1 = strstr(content, "---");
    if (!f1) return false;
    const char *f2 = strstr(f1 + 4, "---");
    if (!f2) return false;
    size_t pklen = strlen(parent_key);
    size_t sklen = strlen(sub_key);
    /* Find the parent_key line */
    const char *p = f1;
    const char *parent_line = NULL;
    while (p < f2) {
        const char *nl = strchr(p, '\n');
        const char *line = nl ? nl + 1 : p;
        if (line >= f2) break;
        const char *s = line;
        while (s < f2 && (*s == ' ' || *s == '\t')) s++;
        if ((size_t)(f2 - s) >= pklen && strncmp(s, parent_key, pklen) == 0
            && (s + pklen < f2 && s[pklen] == ':')) {
            parent_line = line;
            break;
        }
        p = line;
    }
    if (!parent_line) return false;
    /* Scan indented lines after parent_line until we hit next top-level key */
    const char *cur = parent_line;
    while (cur < f2) {
        const char *nl = strchr(cur, '\n');
        cur = nl ? nl + 1 : f2;
        if (cur >= f2) break;
        /* top-level key = no leading whitespace */
        if (cur[0] != ' ' && cur[0] != '\t' && cur[0] != '\n') break;
        const char *s = cur;
        while (s < f2 && (*s == ' ' || *s == '\t')) s++;
        if ((size_t)(f2 - s) >= sklen && strncmp(s, sub_key, sklen) == 0
            && (s + sklen < f2 && s[sklen] == ':'))
            return true;
    }
    return false;
}

/* Extract a top-level YAML scalar value for *key* from *content* ...
 * SKILL.md text, including the frontmatter). Returns 0 and leaves buf empty
 * if not found. Matches agent/skill_utils.parse_frontmatter semantics closely
 * enough for lint: looks for "^key:" lines, strips surrounding quotes. */
static int frontmatter_field(const char *content, const char *key,
                             char *buf, size_t bufsz)
{
    if (!content || !key || !buf || bufsz == 0) return 0;
    buf[0] = '\0';
    /* Must be inside the frontmatter block (between --- fences). */
    const char *f1 = strstr(content, "---");
    if (!f1) return 0;
    const char *f2 = strstr(f1 + 4, "---");
    if (!f2) return 0;

    size_t klen = strlen(key);
    const char *p = f1;
    while (p < f2) {
        /* find start of line */
        const char *nl = strchr(p, '\n');
        const char *line = nl ? nl + 1 : p;
        if (line >= f2) break;
        /* match "key:" at start of line (after indentation) */
        const char *s = line;
        while (s < f2 && (*s == ' ' || *s == '\t')) s++;
        if ((size_t)(f2 - s) >= klen && strncmp(s, key, klen) == 0
            && (s + klen < f2 && (s[klen] == ':' || s[klen] == ' ' || s[klen] == '\t'))) {
            /* skip to value after "key:" */
            const char *v = s + klen;
            while (v < f2 && *v != ':') v++;
            if (v < f2 && *v == ':') v++;
            while (v < f2 && (*v == ' ' || *v == '\t')) v++;
            /* value runs to end of line */
            const char *le = v;
            while (le < f2 && *le != '\n') le++;
            size_t vlen = (size_t)(le - v);
            /* strip surrounding quotes */
            if (vlen >= 2 && ((v[0] == '"' && v[vlen-1] == '"')
                             || (v[0] == '\'' && v[vlen-1] == '\''))) {
                v++; vlen -= 2;
            }
            /* strip inline comment */
            const char *hash = memchr(v, '#', vlen);
            if (hash) vlen = (size_t)(hash - v);
            if (vlen >= bufsz) vlen = bufsz - 1;
            memcpy(buf, v, vlen);
            buf[vlen] = '\0';
            /* trim trailing whitespace */
            while (vlen > 0 && (buf[vlen-1] == ' ' || buf[vlen-1] == '\t'))
                buf[--vlen] = '\0';
            return 1;
        }
        p = line;
    }
    return 0;
}

/* ── strip_code_blocks ───────────────────────────────────────────────────────── */

/* PoP: strip_code_blocks @ tools/skill_linter.py:_strip_code_blocks */
/* Strip fenced code blocks so prose-only checks don't fire on examples. */
static char *strip_code_blocks(const char *body)
{
    if (!body) return NULL;
    size_t blen = strlen(body);
    char *out = malloc(blen + 1);
    if (!out) return NULL;
    size_t oi = 0;
    const char *p = body;
    bool in_fence = false;
    while (*p) {
        if (!in_fence && p[0] == '`' && p[1] == '`' && p[2] == '`') {
            /* opening fence — skip until closing fence */
            in_fence = true;
            p += 3;
            while (*p && !(p[0] == '`' && p[1] == '`' && p[2] == '`'))
                p++;
            if (p[0] == '`') p += 3;
            in_fence = false;
            continue;
        }
        out[oi++] = *p++;
    }
    out[oi] = '\0';
    return out;
}

/* ── Checks ──────────────────────────────────────────────────────────────────── */

/* PoP: check_name_format @ tools/skill_linter.py:_check_name_format */
static size_t check_name_format(const char *name, lint_finding_t *out,
                                size_t n, size_t cap)
{
    if (!name || !*name) return n;
    /* regex: [a-z0-9][a-z0-9_-]*  (fullmatch) */
    size_t i;
    for (i = 0; name[i]; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '-' || c == '_'))
            break;
    }
    if (i == 0 || name[i] != '\0') {
        char msg[LINTER_MAX_MSG];
        snprintf(msg, LINTER_MAX_MSG,
            "name '%s' must be lowercase letters, digits, hyphens, "
            "and underscores only.", name);
        return append_finding(out, n, cap, SL_LINT_ERROR, "name-format", msg);
    }
    return n;
}

/* PoP: check_name_matches_dir @ tools/skill_linter.py:_check_name_matches_dir */
static size_t check_name_matches_dir(const char *name, const char *dir,
                                     lint_finding_t *out, size_t n, size_t cap)
{
    if (!dir || !*dir || !name || !*name) return n;
    const char *base = strrchr(dir, '/');
    base = base ? base + 1 : dir;
    if (strcmp(name, base) != 0) {
        char msg[LINTER_MAX_MSG];
        snprintf(msg, LINTER_MAX_MSG,
            "frontmatter name '%s' does not match directory '%s'; "
            "they must be identical.", name, base);
        return append_finding(out, n, cap, SL_LINT_ERROR, "name-dir-mismatch", msg);
    }
    return n;
}

/* PoP: check_description @ tools/skill_linter.py:_check_description */
static size_t check_description(const char *desc, lint_finding_t *out,
                                size_t n, size_t cap)
{
    if (!desc || !*desc) return n;
    size_t len = strlen(desc);
    if (len > SKILL_PROMPT_DESC_LIMIT) {
        char msg[LINTER_MAX_MSG];
        snprintf(msg, LINTER_MAX_MSG,
            "description is %zu chars; the skill index truncates past %d chars + '...', losing routing signal. Keep it to one sentence.",
            len, SKILL_PROMPT_DESC_LIMIT);
        n = append_finding(out, n, cap, SL_LINT_WARNING, "description-length", msg);
    }
    char lower[2048];
    lowercpy(desc, lower, sizeof(lower));
    /* Collect all marketing-word hits, emit ONE batched finding (matches
     * Python which uses a single LintFinding with the full hits list). */
    char hits[512] = "";
    size_t nhits = 0;
    for (size_t w = 0; w < MARKETING_N; w++) {
        if (nhits >= 16) break;
        if (word_contains(lower, MARKETING_WORDS[w])) {
            /* build a Python-style list repr: ['w1', 'w2'] */
            char tmp[512];
            if (nhits == 0)
                snprintf(tmp, sizeof(tmp), "'%s'", MARKETING_WORDS[w]);
            else
                snprintf(tmp, sizeof(tmp), "%s, '%s'", hits, MARKETING_WORDS[w]);
            strncpy(hits, tmp, sizeof(hits) - 1);
            hits[sizeof(hits) - 1] = '\0';
            nhits++;
        }
    }
    if (nhits > 0) {
        char msg[LINTER_MAX_MSG];
        snprintf(msg, LINTER_MAX_MSG,
            "description contains marketing words [%s]; state the capability, not adjectives.",
            hits);
        n = append_finding(out, n, cap, SL_LINT_WARNING, "description-marketing", msg);
    }
    return n;
}

/* PoP: check_metadata_block @ tools/skill_linter.py:_check_metadata_block */
static size_t check_metadata_block(const char *version, const char *author,
                                   const char *license_s, bool has_metadata,
                                   bool has_hermes_meta,
                                   const char *hermes_tags,
                                   lint_finding_t *out, size_t n, size_t cap)
{
    if (!version || !*version)
        n = append_finding(out, n, cap, SL_LINT_WARNING, "missing-metadata",
                          "frontmatter is missing 'version'; every peer skill has it.");
    if (!author || !*author)
        n = append_finding(out, n, cap, SL_LINT_WARNING, "missing-metadata",
                          "frontmatter is missing 'author'; every peer skill has it.");
    if (!license_s || !*license_s)
        n = append_finding(out, n, cap, SL_LINT_WARNING, "missing-metadata",
                          "frontmatter is missing 'license'; every peer skill has it.");
    if (!has_metadata || !has_hermes_meta) {
        n = append_finding(out, n, cap, SL_LINT_WARNING, "missing-metadata",
            "frontmatter is missing metadata.hermes.{tags, related_skills}.");
    }
    if (has_hermes_meta && hermes_tags && !*hermes_tags)
        n = append_finding(out, n, cap, SL_LINT_WARNING, "missing-metadata",
            "metadata.hermes.tags is missing.");
    return n;
}

/* PoP: check_author_caps @ tools/skill_linter.py:_check_metadata_block(author) */
static size_t check_author_caps(const char *author, lint_finding_t *out,
                                size_t n, size_t cap)
{
    if (!author || !*author) return n;
    char lower[256];
    lowercpy(author, lower, sizeof(lower));
    if (strcmp(lower, "hermes") == 0 ||
        strcmp(lower, "agent") == 0 ||
        strcmp(lower, "hermes agent") == 0) {
        if (strcmp(author, "Hermes Agent") != 0) {
            char msg[LINTER_MAX_MSG];
            snprintf(msg, LINTER_MAX_MSG,
                "author '%s' should be 'Hermes Agent' (proper caps) "
                "or a real contributor name.", author);
            n = append_finding(out, n, cap, SL_LINT_WARNING, "author-caps", msg);
        }
    }
    return n;
}

/* PoP: check_shell_utilities @ tools/skill_linter.py:_check_shell_utilities */
static size_t check_shell_utilities(const char *prose, lint_finding_t *out,
                                    size_t n, size_t cap)
{
    if (!prose || !*prose) return n;
    for (size_t w = 0; w < SHELL_UTILS_N; w++) {
        /* backtick-wrapped: `grep` */
        char pat[64];
        snprintf(pat, sizeof(pat), "`%s`", SHELL_UTILS[w].token);
        if (strstr(prose, pat)) {
            char msg[LINTER_MAX_MSG];
            snprintf(msg, LINTER_MAX_MSG,
                "prose references `%s`; name the native tool `%s` instead.",
                SHELL_UTILS[w].token, SHELL_UTILS[w].tool);
            n = append_finding(out, n, cap, SL_LINT_WARNING,
                "shell-utility-reference", msg);
        }
    }
    return n;
}

/* PoP: check_sections @ tools/skill_linter.py:_check_sections */
static size_t check_sections(const char *body, lint_finding_t *out,
                             size_t n, size_t cap)
{
    if (!body || !*body) {
        return append_finding(out, n, cap, SL_LINT_WARNING, "missing-section",
            "no '# When to Use' section found; skills need explicit "
            "trigger conditions near the top.");
    }
    bool found = false;
    const char *p = body;
    while (p && *p) {
        const char *nl = strchr(p, '\n');
        size_t llen = nl ? (size_t)(nl - p) : strlen(p);
        if (llen > 0 && p[0] == '#') {
            /* count leading '#' */
            size_t h = 0;
            while (h < llen && p[h] == '#') h++;
            if (h >= 2) {
                size_t off = h;
                while (off < llen && (p[off] == ' ' || p[off] == '\t')) off++;
                if (off < llen) {
                    for (size_t i = 0; i < EXPECTED_SECTIONS_N; i++) {
                        size_t rlen = strlen(EXPECTED_SECTIONS[i]);
                        if (off + rlen <= llen
                            && strncmp(p + off, EXPECTED_SECTIONS[i], rlen) == 0
                            && (off + rlen == llen)) {
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
            }
        }
        p = nl ? (nl + 1) : p + llen;
    }
    if (!found) {
        n = append_finding(out, n, cap, SL_LINT_WARNING, "missing-section",
            "no '## When to Use' section found; skills need explicit trigger conditions near the top.");
    }
    return n;
}

/* PoP: check_reference_links @ tools/skill_linter.py:_check_reference_links */
static size_t check_reference_links(const char *body, const char *skill_dir,
                                    lint_finding_t *out, size_t n, size_t cap)
{
    if (!body || !skill_dir || !*skill_dir) return n;
    /* Scan for the earliest match of references/ templates/ or assets/. */
    const char *p = body;
    char seen[256][256];
    size_t nseen = 0;
    while (p && *p) {
        const char *best = NULL;
        const char *found;
        found = strstr(p, "references/");
        if (found && (!best || found < best)) best = found;
        found = strstr(p, "templates/");
        if (found && (!best || found < best)) best = found;
        found = strstr(p, "assets/");
        if (found && (!best || found < best)) best = found;
        if (!best) break;
        p = best;
        /* extract the path token: [A-Za-z0-9_./-]+ */
        const char *start = p;
        const char *end = p;
        while (*end && (isalnum((unsigned char)*end) || *end == '_' || *end == '.'
                         || *end == '/' || *end == '-'))
            end++;
        size_t rlen = (size_t)(end - start);
        if (rlen == 0) { p = end; continue; }
        /* dedup */
        bool dup = false;
        for (size_t i = 0; i < nseen; i++) {
            if (strncmp(seen[i], start, rlen) == 0 && seen[i][rlen] == '\0') {
                dup = true; break;
            }
        }
        if (dup) { p = end; continue; }
        if (nseen < 256) {
            memcpy(seen[nseen], start, rlen);
            seen[nseen][rlen] = '\0';
            nseen++;
        }
        /* skip placeholders/globs */
        if (memchr(start, '*', rlen) || (rlen > 0 && start[rlen - 1] == '/')) {
            p = end; continue;
        }
        char full[1024];
        snprintf(full, sizeof(full), "%s/%.*s", skill_dir, (int)rlen, start);
        struct stat st;
        if (stat(full, &st) != 0) {
            char msg[LINTER_MAX_MSG];
            snprintf(msg, LINTER_MAX_MSG,
                "body references '%.*s' but that file does not exist "
                "in the skill directory.", (int)rlen, start);
            n = append_finding(out, n, cap, SL_LINT_WARNING,
                "dangling-reference", msg);
        }
        p = end;
    }
    return n;
}

/* PoP: check_platforms_gating @ tools/skill_linter.py:_check_platforms_gating */
static size_t check_platforms_gating(const char *skill_dir, lint_finding_t *out,
                                     size_t n, size_t cap)
{
    if (!skill_dir || !*skill_dir) return n;
    char scripts_dir[1024];
    snprintf(scripts_dir, sizeof(scripts_dir), "%s/scripts", skill_dir);
    struct stat st;
    if (stat(scripts_dir, &st) != 0 || !S_ISDIR(st.st_mode)) return n;

    /* Walk scripts_dir/* (non-recursive like the Python rglob over scripts) */
    char globpat[1024];
    snprintf(globpat, sizeof(globpat), "%s/*", scripts_dir);
    glob_t g;
    if (glob(globpat, GLOB_NOSORT, NULL, &g) != 0) return n;

    bool found_offender = false;
    char detail[512];
    detail[0] = '\0';
    for (size_t i = 0; i < g.gl_pathc; i++) {
        const char *script = g.gl_pathv[i];
        const char *dot = strrchr(script, '.');
        if (!dot || (strcmp(dot, ".py") != 0 && strcmp(dot, ".sh") != 0
                     && strcmp(dot, ".bash") != 0))
            continue;
        FILE *f = fopen(script, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        char *buf = malloc((size_t)sz + 1);
        if (!buf) { fclose(f); continue; }
        size_t got = fread(buf, 1, (size_t)sz, f);
        buf[got] = '\0';
        fclose(f);
        /* Collect ALL primitives found in this script into a Python-style
         * list repr: ['prim1', 'prim2'] (matches Python offenders[k] = hit). */
        char hits[256] = "";
        size_t nhits = 0;
        for (size_t w = 0; w < POSIX_N; w++) {
            if (strstr(buf, POSIX_PRIMITIVES[w])) {
                char tmp[256];
                if (nhits == 0)
                    snprintf(tmp, sizeof(tmp), "'%s'", POSIX_PRIMITIVES[w]);
                else
                    snprintf(tmp, sizeof(tmp), "%s, '%s'", hits, POSIX_PRIMITIVES[w]);
                strncpy(hits, tmp, sizeof(hits) - 1);
                hits[sizeof(hits) - 1] = '\0';
                nhits++;
            }
        }
        free(buf);
        if (nhits > 0) {
            const char *bn = strrchr(script, '/');
            bn = bn ? bn + 1 : script;
            char entry[256];
            snprintf(entry, sizeof(entry), "%s: [%s]", bn, hits);
            if (detail[0]) {
                strncat(detail, "; ", sizeof(detail) - strlen(detail) - 1);
            }
            strncat(detail, entry, sizeof(detail) - strlen(detail) - 1);
            found_offender = true;
        }
    }
    globfree(&g);
    if (found_offender) {
        char msg[LINTER_MAX_MSG];
        snprintf(msg, LINTER_MAX_MSG,
            "scripts use POSIX-only primitives (%s) but no 'platforms:' frontmatter is declared. Fix cross-platform or gate with platforms: [linux, macos].",
            detail);
        n = append_finding(out, n, cap, SL_LINT_WARNING, "platforms-gating", msg);
    }
    return n;
}

/* PoP: check_forbidden_files @ tools/skill_linter.py:_check_forbidden_files */
static size_t check_forbidden_files(const char *skill_dir, lint_finding_t *out,
                                    size_t n, size_t cap)
{
    if (!skill_dir || !*skill_dir) return n;
    for (size_t i = 0; i < FORBIDDEN_N; i++) {
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", skill_dir, FORBIDDEN_FILES[i]);
        struct stat st;
        if (stat(full, &st) == 0) {
            char msg[LINTER_MAX_MSG];
            snprintf(msg, LINTER_MAX_MSG,
                "skill ships '%s'; skills should not include scaffolding/config files.",
                FORBIDDEN_FILES[i]);
            n = append_finding(out, n, cap, SL_LINT_WARNING,
                "forbidden-file", msg);
        }
    }
    return n;
}

/* PoP: check_platform_list_valid @ tools/skill_linter.py:_check_platform_list_valid */
static size_t check_platform_list_valid(const char *platforms, lint_finding_t *out,
                                        size_t n, size_t cap)
{
    if (!platforms || !*platforms) return n;
    static const char *VALID[] = { "linux", "macos", "windows", "darwin" };
    static const size_t VALID_N = 4;
    /* platforms may be a comma-separated list or a single value */
    char copy[512];
    strncpy(copy, platforms, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';
    char badbuf[512] = "";
    char *tok = strtok(copy, ",");
    int bad_count = 0;
    while (tok) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && (*end == ' ' || *end == '\t' || *end == '"')) *end-- = '\0';
        bool ok = false;
        for (size_t i = 0; i < VALID_N; i++)
            if (strcasecmp(tok, VALID[i]) == 0) { ok = true; break; }
        if (!ok) {
            if (bad_count > 0) strncat(badbuf, ", ", sizeof(badbuf) - strlen(badbuf) - 1);
            /* reproduce Python's repr: ['val', 'val2'] */
            char entry[128];
            snprintf(entry, sizeof(entry), "'%s'", tok);
            strncat(badbuf, entry, sizeof(badbuf) - strlen(badbuf) - 1);
            bad_count++;
        }
        tok = strtok(NULL, ",");
    }
    if (bad_count > 0) {
        char msg[LINTER_MAX_MSG];
        snprintf(msg, LINTER_MAX_MSG,
            "platforms contains unrecognized value(s) [%s]; expected a subset of {darwin, linux, macos, windows}.",
            badbuf);
        n = append_finding(out, n, cap, SL_LINT_WARNING, "platforms-value", msg);
    }
    return n;
}

/* ── Public API ─────────────────────────────────────────────────────────────── */

/* PoP: lint_content @ tools/skill_linter.py:lint_content */
size_t lint_content(const char *content, const char *skill_dir,
                    lint_finding_t *out, size_t cap)
{
    size_t n = 0;
    char name[256], desc[1024], author[256], version[128], license_s[128],
         platforms[512], meta_hermes[256], tags[512];
    name[0] = desc[0] = author[0] = version[0] = license_s[0] =
        platforms[0] = meta_hermes[0] = tags[0] = '\0';

    frontmatter_field(content, "name", name, sizeof(name));
    frontmatter_field(content, "description", desc, sizeof(desc));
    frontmatter_field(content, "author", author, sizeof(author));
    frontmatter_field(content, "version", version, sizeof(version));
    frontmatter_field(content, "license", license_s, sizeof(license_s));
    frontmatter_field(content, "platforms", platforms, sizeof(platforms));
    bool has_metadata = frontmatter_has_key(content, "metadata");
    bool has_hermes_meta = frontmatter_has_subkey(content, "metadata", "hermes");
    /* metadata.hermes.tags — simple: look for "tags:" inside metadata block */
    frontmatter_field(content, "tags", tags, sizeof(tags));

    const char *body = content;
    /* body = content after the closing fence */
    const char *f2 = strstr(content, "\n---");
    if (f2) {
        const char *after = f2 + 4;
        while (*after == '\n' || *after == '\r') after++;
        body = after;
    }

    n = check_name_format(name, out, n, cap);
    n = check_name_matches_dir(name, skill_dir, out, n, cap);
    n = check_description(desc, out, n, cap);
    n = check_metadata_block(
        version[0] ? version : NULL,
        author[0] ? author : NULL,
        license_s[0] ? license_s : NULL,
        has_metadata,
        has_hermes_meta,
        tags,
        out, n, cap);
    n = check_author_caps(author, out, n, cap);
    n = check_platform_list_valid(platforms, out, n, cap);

    char *prose = strip_code_blocks(body);
    n = check_shell_utilities(prose, out, n, cap);
    n = check_sections(body, out, n, cap);
    n = check_reference_links(body, skill_dir, out, n, cap);
    n = check_platforms_gating(skill_dir, out, n, cap);
    n = check_forbidden_files(skill_dir, out, n, cap);
    free(prose);
    return n;
}

/* PoP: lint_skill @ tools/skill_linter.py:lint_skill */
size_t lint_skill(const char *skill_md_path, lint_finding_t *out, size_t cap)
{
    if (!skill_md_path || !out) return 0;
    FILE *f = fopen(skill_md_path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 0; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    buf[got] = '\0';
    fclose(f);

    /* skill_dir = parent directory of the SKILL.md path. */
    char dir[1024];
    strncpy(dir, skill_md_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *slash = strrchr(dir, '/');
    if (slash) *slash = '\0';
    else strncpy(dir, ".", sizeof(dir) - 1);

    size_t n = lint_content(buf, dir, out, cap);
    free(buf);
    return n;
}

/* PoP: lint_format_findings @ tools/skill_linter.py:format_findings */
size_t lint_format_findings(const lint_finding_t *findings, size_t n,
                            char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return 0;
    out[0] = '\0';
    size_t off = 0;
    char buf[512];
    for (size_t i = 0; i < n; i++) {
        lint_finding_format(&findings[i], buf, sizeof(buf));
        size_t blen = strlen(buf);
        if (i > 0) {
            if (off + 1 >= out_sz) break;
            out[off++] = '\n';
            out[off] = '\0';
        }
        size_t copy = blen;
        if (out_sz - off - 1 < copy) copy = out_sz - off - 1;
        memcpy(out + off, buf, copy);
        off += copy;
        out[off] = '\0';
    }
    return off;
}

/* PoP: skill_linter_main @ tools/skill_linter.py:_main */
int skill_linter_main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr,
            "usage: python -m tools.skill_linter <SKILL.md | skill-dir> ...\n");
        return 2;
    }
    bool any_error = false;
    int total = 0;
    int ntargets = 0;
    char buf[16384];
    for (int a = 1; a < argc; a++) {
        const char *arg = argv[a];
        struct stat st;
        if (stat(arg, &st) != 0) {
            fprintf(stderr, "skip (not a SKILL.md or dir): %s\n", arg);
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            char pattern[1024];
            snprintf(pattern, sizeof(pattern), "%s/**/SKILL.md", arg);
            glob_t g;
            if (glob(pattern, GLOB_MARK, NULL, &g) == 0) {
                for (size_t i = 0; i < g.gl_pathc; i++) {
                    const char *p = g.gl_pathv[i];
                    size_t plen = strlen(p);
                    if (plen > 1 && p[plen - 1] == '/') continue;
                    lint_finding_t findings[64];
                    size_t nf = lint_skill(p, findings, 64);
                    if (nf == 0) continue;
                    total += (int)nf;
                    ntargets++;
                    char parent[1024];
                    strncpy(parent, p, sizeof(parent) - 1);
                    parent[sizeof(parent) - 1] = '\0';
                    char *slash = strrchr(parent, '/');
                    if (slash) *slash = '\0';
                    else strncpy(parent, ".", sizeof(parent) - 1);
                    const char *pname = strrchr(parent, '/');
                    pname = pname ? pname + 1 : parent;
                    printf("\n%s (%s):\n", pname, p);
                    if (lint_has_errors(findings, nf))
                        any_error = true;
                    lint_format_findings(findings, nf, buf, sizeof(buf));
                    printf("%s\n", buf);
                }
                globfree(&g);
            }
        } else {
            const char *base = strrchr(arg, '/');
            base = base ? base + 1 : arg;
            if (strcmp(base, "SKILL.md") != 0) {
                fprintf(stderr, "skip (not a SKILL.md or dir): %s\n", arg);
                continue;
            }
            lint_finding_t findings[64];
            size_t nf = lint_skill(arg, findings, 64);
            if (nf == 0) continue;
            total += (int)nf;
            ntargets++;
            char parent[1024];
            strncpy(parent, arg, sizeof(parent) - 1);
            parent[sizeof(parent) - 1] = '\0';
            char *slash = strrchr(parent, '/');
            if (slash) *slash = '\0';
            else strncpy(parent, ".", sizeof(parent) - 1);
            const char *pname = strrchr(parent, '/');
            pname = pname ? pname + 1 : parent;
            printf("\n%s (%s):\n", pname, arg);
            if (lint_has_errors(findings, nf))
                any_error = true;
            lint_format_findings(findings, nf, buf, sizeof(buf));
            printf("%s\n", buf);
        }
    }
    if (total == 0)
        printf("All %d skill(s) clean.\n", ntargets);
    else
        printf("\n%d finding(s) across %d skill(s).\n", total, ntargets);
    return any_error ? 1 : 0;
}
