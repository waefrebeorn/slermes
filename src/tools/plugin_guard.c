/*
 * plugin_guard.c — PoP of tools/plugin_guard.py (plugin install scanner).
 *
 * Python -> C symbol map (1:1):
 *   PLUGIN_SCANNER_VERSION        -> PLUGIN_GUARD_SCANNER_VERSION
 *   EXCLUDED_DIRS                 -> pg_excluded_dir (helper)
 *   CODE_FILE_EXTENSIONS          -> pg_code_file_ext (helper)
 *   CODE_EXEMPT_PATTERN_IDS       -> pg_pattern_exempt_on_code (helper)
 *   SEVERITY_REMAP                -> pg_severity_remap (helper)
 *   MAX_PLUGIN_FILE_COUNT         -> PG_MAX_PLUGIN_FILE_COUNT
 *   MAX_PLUGIN_TOTAL_SIZE_KB      -> PG_MAX_PLUGIN_TOTAL_SIZE_KB
 *   MAX_PLUGIN_SINGLE_FILE_KB     -> PG_MAX_PLUGIN_SINGLE_FILE_KB
 *   _is_excluded                  -> plugin_guard__is_excluded
 *   _filter_findings              -> plugin_guard__filter_findings
 *   _check_plugin_structure       -> plugin_guard__check_plugin_structure
 *   scan_plugin                   -> plugin_guard_scan_plugin
 *   should_allow_plugin_install   -> should_allow_plugin_install
 *
 * The shared skills_guard engine it imports in Python (scan_file,
 * SUSPICIOUS_BINARY_EXTENSIONS, _determine_verdict) is carried by the
 * GENERATED table src/tools/plugin_guard_patterns.h + the local
 * pg_scan_file port below — byte-faithful to tools/skills_guard.py.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "plugin_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <libgen.h>

#include "hermes_json.h"
#include "libregex/hermes_regex.h"
#include "plugin_guard_patterns.h"

const char *PLUGIN_GUARD_SCANNER_VERSION = "plugin-guard-v1";

const int PG_MAX_PLUGIN_FILE_COUNT = 400;
const int PG_MAX_PLUGIN_TOTAL_SIZE_KB = 10 * 1024;
const int PG_MAX_PLUGIN_SINGLE_FILE_KB = 1024;

static const char *const PG_EXCLUDED_DIRS[] = {
    ".git", "__pycache__", "node_modules", ".venv", "venv",
    ".mypy_cache", ".pytest_cache", ".ruff_cache", ".tox", NULL
};
static const char *const PG_CODE_FILE_EXTENSIONS[] = {
    ".py", ".js", ".ts", ".sh", ".bash", ".rb", ".pl", ".php", NULL
};
/* skills_guard.THREAT_PATTERNS ids exempt on code files (documented plugin
 * behavior: reads own env secrets / HTTP-with-key / agent-facing prose). */
static const char *const PG_CODE_EXEMPT_PATTERN_IDS[] = {
    "python_environ_get_secret", "python_getenv_secret", "python_os_environ",
    "node_process_env", "ruby_env_secret",
    "env_exfil_httpx", "env_exfil_requests", "env_exfil_fetch",
    "env_exfil_curl", "env_exfil_wget",
    "context_exfil", "send_to_url", "fake_policy",
    "agent_config_mod", "encoded_exfil", NULL
};
/* pattern_id -> remapped severity for plugins (NULL-terminated pairs) */
static const char *const PG_SEVERITY_REMAP[][2] = {
    { "binary_file", "high" },
    { "hermes_env_access", "medium" },
    { "curl_pipe_shell", "high" },
    { NULL, NULL }
};

int plugin_guard__is_excluded(const char *rel_parts[], size_t n)
{
    for (size_t i = 0; i < n; i++)
        for (int k = 0; PG_EXCLUDED_DIRS[k]; k++)
            if (strcmp(rel_parts[i], PG_EXCLUDED_DIRS[k]) == 0) return 1;
    return 0;
}

static int str_in(const char *const *list, const char *s)
{
    for (int i = 0; list[i]; i++)
        if (strcmp(list[i], s) == 0) return 1;
    return 0;
}

static const char *pg_remap_severity(const char *pattern_id)
{
    for (int i = 0; PG_SEVERITY_REMAP[i][0]; i++)
        if (strcmp(PG_SEVERITY_REMAP[i][0], pattern_id) == 0)
            return PG_SEVERITY_REMAP[i][1];
    return NULL;
}

/* ── pg_scan_file: faithful port of tools/skills_guard.py:scan_file ────── */

static const char *pg_unicode_char_name(unsigned cp)
{
    /* names mirror skills_guard._unicode_char_name (lowercase) */
    switch (cp) {
    case 0x200b: return "zero-width space";
    case 0x200c: return "zero-width non-joiner";
    case 0x200d: return "zero-width joiner";
    case 0x2060: return "word joiner";
    case 0x2062: return "invisible times";
    case 0x2063: return "invisible separator";
    case 0xfeff: return "zero-width no-break space";
    default: return "invisible character";
    }
}

static void utf8_next(const char **p, unsigned *cp)
{
    const unsigned char c = (unsigned char)**p;
    if (c < 0x80) { *cp = c; *p += 1; }
    else if ((c >> 5) == 0x6 && (*p)[1]) { *cp = c & 0x1f; *p += 1; *cp = (*cp << 6) | ((unsigned char)**p & 0x3f); *p += 1; }
    else if ((c >> 4) == 0xe && (*p)[1] && (*p)[2]) {
        *cp = c & 0x0f; *p += 1; *cp = (*cp << 6) | ((unsigned char)**p & 0x3f); *p += 1;
        *cp = (*cp << 6) | ((unsigned char)**p & 0x3f); *p += 1;
    } else if ((c >> 3) == 0x1e && (*p)[1] && (*p)[2] && (*p)[3]) {
        *cp = c & 0x07; *p += 1;
        for (int k = 0; k < 3; k++) { *cp = (*cp << 6) | ((unsigned char)**p & 0x3f); *p += 1; }
    } else { *cp = c; *p += 1; }
}

static void pg_add_finding(pg_finding_list_t *out, const char *pid,
                           const char *severity, const char *category,
                           const char *file, int line, const char *match,
                           const char *description)
{
    if (out->n >= out->cap) {
        out->cap = out->cap ? out->cap * 2 : 32;
        out->f = realloc(out->f, (size_t)out->cap * sizeof *out->f);
    }
    pg_finding_t *fd = &out->f[out->n++];
    snprintf(fd->pattern_id, sizeof fd->pattern_id, "%s", pid);
    snprintf(fd->severity, sizeof fd->severity, "%s", severity);
    snprintf(fd->category, sizeof fd->category, "%s", category);
    snprintf(fd->file, sizeof fd->file, "%s", file);
    fd->line = line;
    snprintf(fd->match, sizeof fd->match, "%s", match);
    snprintf(fd->description, sizeof fd->description, "%s", description);
}

static int pg_ext_of(const char *path, char *ext, size_t ext_sz)
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    const char *dot = strrchr(base, '.');
    if (!dot) { if (ext_sz) ext[0] = '\0'; return 0; }
    snprintf(ext, ext_sz, "%s", dot);
    for (char *q = ext; *q; q++) *q = (char)tolower((unsigned char)*q);
    return 1;
}

void pg_scan_file(const char *filepath, const char *rel_path,
                  pg_finding_list_t *out)
{
    char ext[32], base[512];
    const char *slash = strrchr(filepath, '/');
    snprintf(base, sizeof base, "%s", slash ? slash + 1 : filepath);

    /* suffix filter: scannable text types only (SKILL.md always scanned) */
    pg_ext_of(filepath, ext, sizeof ext);
    int scannable = str_in(PG_SCANNABLE_EXTENSIONS, ext);
    if (!scannable && strcmp(base, "SKILL.md") != 0) return;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) return;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { fclose(fp); return; }
    char *content = malloc((size_t)sz + 1);
    size_t rd = fread(content, 1, (size_t)sz, fp);
    content[rd] = '\0';
    fclose(fp);
    /* UnicodeDecodeError -> [] : reject invalid UTF-8 */
    if (strlen(content) != rd) { free(content); return; }

    if (!rel_path || !*rel_path) rel_path = base;

    /* split into lines */
    size_t nlines = 1;
    for (size_t i = 0; i < rd; i++) if (content[i] == '\n') nlines++;
    char **lines = malloc(nlines * sizeof *lines);
    size_t ln = 0, start = 0;
    for (size_t i = 0; i <= rd; i++) {
        if (content[i] == '\n' || content[i] == '\0') {
            content[i] = '\0';
            lines[ln++] = content + start;
            start = i + 1;
            if (i == rd) break;
        }
    }

    /* regex patterns: one finding max per (pattern, line) */
    for (int t = 0; PG_THREAT_PATTERNS[t].pattern; t++) {
        if (PG_THREAT_PATTERNS[t].flags == PG_PATTERN_FLAGS_SKIP) continue;
        hregex_t *re = regex_compile(PG_THREAT_PATTERNS[t].pattern, 0);
        if (!re) continue;
        for (size_t i = 0; i < ln; i++) {
            regex_match_t *m = regex_search(re, lines[i]);
            if (m && m->matched) {
                char matched_text[256];
                const char *ls = lines[i];
                while (*ls && isspace((unsigned char)*ls)) ls++;
                const char *le = ls + strlen(ls);
                while (le > ls && isspace((unsigned char)le[-1])) le--;
                size_t ml = (size_t)(le - ls);
                int trunc = ml > 120;
                if (trunc) ml = 117;
                memcpy(matched_text, ls, ml);
                matched_text[ml] = '\0';
                if (trunc) strcat(matched_text, "...");
                pg_add_finding(out, PG_THREAT_PATTERNS[t].id,
                               PG_THREAT_PATTERNS[t].severity,
                               PG_THREAT_PATTERNS[t].category, rel_path,
                               (int)i + 1, matched_text,
                               PG_THREAT_PATTERNS[t].description);
            }
            regex_match_free(m);
        }
        regex_free(re);
    }

    /* invisible unicode detection: one finding per line */
    for (size_t i = 0; i < ln; i++) {
        for (int k = 0; PG_INVISIBLE_CHARS[k].name; k++) {
            /* search the UTF-8 encoding of this codepoint */
            unsigned cp = PG_INVISIBLE_CHARS[k].cp;
            char enc[5]; int el = 0;
            if (cp < 0x80) enc[el++] = (char)cp;
            else if (cp < 0x800) { enc[el++] = (char)(0xc0 | (cp >> 6)); enc[el++] = (char)(0x80 | (cp & 0x3f)); }
            else if (cp < 0x10000) { enc[el++] = (char)(0xe0 | (cp >> 12)); enc[el++] = (char)(0x80 | ((cp >> 6) & 0x3f)); enc[el++] = (char)(0x80 | (cp & 0x3f)); }
            else { enc[el++] = (char)(0xf0 | (cp >> 18)); enc[el++] = (char)(0x80 | ((cp >> 12) & 0x3f)); enc[el++] = (char)(0x80 | ((cp >> 6) & 0x3f)); enc[el++] = (char)(0x80 | (cp & 0x3f)); }
            enc[el] = '\0';
            if (strstr(lines[i], enc)) {
                char match[64], desc[192];
                snprintf(match, sizeof match, "U+%04X (%s)", cp,
                         pg_unicode_char_name(cp));
                snprintf(desc, sizeof desc,
                         "invisible unicode character %s "
                         "(possible text hiding/injection)",
                         pg_unicode_char_name(cp));
                pg_add_finding(out, "invisible_unicode", "high", "injection",
                               rel_path, (int)i + 1, match, desc);
                break;
            }
        }
    }

    free(lines);
    free(content);
}

/* ── plugin-specific logic (tools/plugin_guard.py proper) ─────────────── */

const char *const *plugin_guard_excluded_dirs(void) { return PG_EXCLUDED_DIRS; }
const char *const *plugin_guard_code_extensions(void) { return PG_CODE_FILE_EXTENSIONS; }

int plugin_guard__pattern_exempt_on_code(const char *pattern_id)
{
    return str_in(PG_CODE_EXEMPT_PATTERN_IDS, pattern_id);
}

void plugin_guard__filter_findings(const pg_finding_list_t *in,
                                   const char *rel_path,
                                   pg_finding_list_t *out)
{
    char ext[32];
    pg_ext_of(rel_path, ext, sizeof ext);
    int is_code = str_in(PG_CODE_FILE_EXTENSIONS, ext);
    memset(out, 0, sizeof *out);
    for (int i = 0; i < in->n; i++) {
        pg_finding_t f = in->f[i]; /* copy; severity may be remapped */
        if (is_code &&
            plugin_guard__pattern_exempt_on_code(f.pattern_id))
            continue;
        const char *remapped = pg_remap_severity(f.pattern_id);
        if (remapped)
            snprintf(f.severity, sizeof f.severity, "%s", remapped);
        pg_add_finding(out, f.pattern_id, f.severity, f.category, f.file,
                       f.line, f.match, f.description);
    }
}

const char *plugin_guard_determine_verdict(const pg_finding_list_t *findings)
{
    /* skills_guard._determine_verdict: critical->dangerous, high->caution */
    for (int i = 0; i < findings->n; i++)
        if (strcmp(findings->f[i].severity, "critical") == 0)
            return "dangerous";
    for (int i = 0; i < findings->n; i++)
        if (strcmp(findings->f[i].severity, "high") == 0)
            return "caution";
    return "safe";
}

/* walk helper: collect regular files under root, sorted by relative path */
typedef struct { char rel[1024]; char abs[2048]; off_t size; } pg_file_t;

static int cmp_files(const void *a, const void *b)
{
    return strcmp(((const pg_file_t *)a)->rel, ((const pg_file_t *)b)->rel);
}

static void walk(const char *root, const char *prefix, pg_file_t **arr,
                 size_t *n, size_t *cap)
{
    char dirpath[2048];
    if (prefix[0])
        snprintf(dirpath, sizeof dirpath, "%s/%s", root, prefix);
    else
        snprintf(dirpath, sizeof dirpath, "%s", root);
    DIR *d = opendir(dirpath);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        char rel[1024];
        if (prefix[0])
            snprintf(rel, sizeof rel, "%s/%s", prefix, e->d_name);
        else
            snprintf(rel, sizeof rel, "%s", e->d_name);
        char absp[2048];
        snprintf(absp, sizeof absp, "%s/%s", root, rel);
        struct stat st;
        if (lstat(absp, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            const char *parts[1] = { e->d_name };
            if (plugin_guard__is_excluded(parts, 1)) continue;
            walk(root, rel, arr, n, cap);
        } else if (S_ISREG(st.st_mode)) {
            if (*n >= *cap) {
                *cap = *cap ? *cap * 2 : 64;
                *arr = realloc(*arr, *cap * sizeof **arr);
            }
            pg_file_t *f = &(*arr)[(*n)++];
            snprintf(f->rel, sizeof f->rel, "%s", rel);
            snprintf(f->abs, sizeof f->abs, "%s", absp);
            f->size = st.st_size;
        }
    }
    closedir(d);
}

pg_finding_list_t *plugin_guard__check_plugin_structure(const char *dir)
{
    pg_finding_list_t *out = calloc(1, sizeof *out);
    int file_count = 0;
    long total_size = 0;

    pg_file_t *files = NULL;
    size_t n = 0, cap = 0;
    walk(dir, "", &files, &n, &cap);

    for (size_t i = 0; i < n; i++) {
        file_count++;
        long size = (long)files[i].size;
        total_size += size;

        if (size > (long)PG_MAX_PLUGIN_SINGLE_FILE_KB * 1024) {
            char match[64], desc[192];
            snprintf(match, sizeof match, "%ldKB", size / 1024);
            snprintf(desc, sizeof desc, "file is %ldKB (limit: %dKB)",
                     size / 1024, PG_MAX_PLUGIN_SINGLE_FILE_KB);
            pg_add_finding(out, "oversized_file", "medium", "structural",
                           files[i].rel, 0, match, desc);
        }
        char ext[32];
        pg_ext_of(files[i].abs, ext, sizeof ext);
        if (str_in(PG_SUSPICIOUS_BINARY_EXTENSIONS, ext)) {
            char match[64], desc[192];
            snprintf(match, sizeof match, "binary: %s", ext);
            snprintf(desc, sizeof desc,
                     "binary/executable file (%s) bundled in plugin "
                     "(cannot be scanned)", ext);
            pg_add_finding(out, "binary_file", "high", "structural",
                           files[i].rel, 0, match, desc);
        }
    }

    if (file_count > PG_MAX_PLUGIN_FILE_COUNT) {
        char match[64], desc[192];
        snprintf(match, sizeof match, "%d files", file_count);
        snprintf(desc, sizeof desc, "plugin has %d files (limit: %d)",
                 file_count, PG_MAX_PLUGIN_FILE_COUNT);
        pg_add_finding(out, "too_many_files", "medium", "structural",
                       "(directory)", 0, match, desc);
    }
    if (total_size > (long)PG_MAX_PLUGIN_TOTAL_SIZE_KB * 1024) {
        char match[64], desc[192];
        snprintf(match, sizeof match, "%ldKB", total_size / 1024);
        snprintf(desc, sizeof desc, "plugin is %ldKB total (limit: %dKB)",
                 total_size / 1024, PG_MAX_PLUGIN_TOTAL_SIZE_KB);
        pg_add_finding(out, "oversized_bundle", "medium", "structural",
                       "(directory)", 0, match, desc);
    }

    free(files);
    return out;
}

pg_plugin_scan_result_t *plugin_guard_scan_plugin(const char *plugin_dir,
                                                  const char *source)
{
    pg_plugin_scan_result_t *r = calloc(1, sizeof *r);
    pg_finding_list_t all = {0};

    struct stat st;
    if (stat(plugin_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        pg_finding_list_t *struct_f =
            plugin_guard__check_plugin_structure(plugin_dir);
        for (int i = 0; i < struct_f->n; i++)
            pg_add_finding(&all, struct_f->f[i].pattern_id,
                           struct_f->f[i].severity, struct_f->f[i].category,
                           struct_f->f[i].file, struct_f->f[i].line,
                           struct_f->f[i].match, struct_f->f[i].description);
        free(struct_f->f);
        free(struct_f);

        /* content scan: sorted rglob of regular files */
        pg_file_t *files = NULL;
        size_t n = 0, cap = 0;
        walk(plugin_dir, "", &files, &n, &cap);
        qsort(files, n, sizeof *files, cmp_files);
        for (size_t i = 0; i < n; i++) {
            pg_finding_list_t raw = {0};
            pg_scan_file(files[i].abs, files[i].rel, &raw);
            pg_finding_list_t filtered;
            plugin_guard__filter_findings(&raw, files[i].rel, &filtered);
            for (int k = 0; k < filtered.n; k++)
                pg_add_finding(&all, filtered.f[k].pattern_id,
                               filtered.f[k].severity,
                               filtered.f[k].category, filtered.f[k].file,
                               filtered.f[k].line, filtered.f[k].match,
                               filtered.f[k].description);
            free(raw.f);
            free(filtered.f);
        }
        free(files);
    }

    snprintf(r->verdict, sizeof r->verdict, "%s",
             plugin_guard_determine_verdict(&all));

    /* summary must be built while all.f is alive (category pointers) */
    /* Path(dir).name: strip trailing slashes, take the last component */
    char dirbuf[2048];
    snprintf(dirbuf, sizeof dirbuf, "%s", plugin_dir);
    size_t dl = strlen(dirbuf);
    while (dl > 1 && dirbuf[dl - 1] == '/') dirbuf[--dl] = '\0';
    const char *base = strrchr(dirbuf, '/');
    base = base ? base + 1 : dirbuf;
    snprintf(r->skill_name, sizeof r->skill_name, "%s", base);
    snprintf(r->source, sizeof r->source, "%s",
             source && *source ? source : base);
    snprintf(r->trust_level, sizeof r->trust_level, "community");

    if (all.n > 0) {
        /* categories set -> sorted unique join */
        const char *cats[64];
        int ncats = 0;
        for (int i = 0; i < all.n; i++) {
            int seen = 0;
            for (int k = 0; k < ncats; k++)
                if (strcmp(cats[k], all.f[i].category) == 0) { seen = 1; break; }
            if (!seen && ncats < 64) cats[ncats++] = all.f[i].category;
        }
        /* insertion sort (small n) */
        for (int i = 1; i < ncats; i++) {
            const char *key = cats[i];
            int j = i - 1;
            while (j >= 0 && strcmp(cats[j], key) > 0) { cats[j + 1] = cats[j]; j--; }
            cats[j + 1] = key;
        }
        char joined[256] = "";
        for (int i = 0; i < ncats; i++) {
            if (i) strncat(joined, ", ", sizeof joined - strlen(joined) - 1);
            strncat(joined, cats[i], sizeof joined - strlen(joined) - 1);
        }
        snprintf(r->summary, sizeof r->summary,
                 "%s: %s \xe2\x80\x94 %d finding(s) in %s",
                 r->skill_name, r->verdict, all.n, joined);
    } else {
        snprintf(r->summary, sizeof r->summary,
                 "%s: clean scan, no threats detected", r->skill_name);
    }

    r->findings = calloc((size_t)(all.n ? all.n : 1), sizeof *r->findings);
    if (all.n)
        memcpy(r->findings, all.f, (size_t)all.n * sizeof *r->findings);
    r->finding_count = all.n;
    free(all.f);
    return r;
}

void plugin_guard_scan_result_free(pg_plugin_scan_result_t *r)
{
    if (!r) return;
    free(r->findings);
    free(r);
}

/* PoP: should_allow_plugin_install @ tools/plugin_guard.py:should_allow_plugin_install */
void should_allow_plugin_install(const pg_plugin_scan_result_t *result,
                                 int force, int *allowed, char *reason,
                                 size_t reason_sz)
{
    /* allowed: 1 = proceed, 0 = blocked, -1 = needs confirmation (None) */
    if (strcmp(result->verdict, "safe") == 0) {
        if (allowed) *allowed = 1;
        snprintf(reason, reason_sz, "Allowed (clean scan)");
    } else if (strcmp(result->verdict, "caution") == 0) {
        if (force) {
            if (allowed) *allowed = 1;
            snprintf(reason, reason_sz,
                     "Force-installed despite caution verdict (%d findings)",
                     result->finding_count);
        } else {
            if (allowed) *allowed = -1;
            snprintf(reason, reason_sz,
                     "Requires confirmation (caution verdict, %d findings)",
                     result->finding_count);
        }
    } else {
        if (allowed) *allowed = 0;
        snprintf(reason, reason_sz,
                 "Blocked (dangerous verdict, %d findings). --force does not "
                 "override a dangerous verdict.", result->finding_count);
    }
}
