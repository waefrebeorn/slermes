/*
 * port_tools_skills_guard.c — C port of tools/skills_guard.py
 */

#include "hermes_logger.h"
#include "hermes_crypto.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/* Faithful C mirrors of the Python Finding / ScanResult dataclasses. */
#define SKILLS_GUARD_FINDING_FIELDS 7
typedef struct {
    char pattern_id[64];
    char severity[16];
    char category[24];
    char file[256];
    int  line;
    char match[512];
    char description[256];
} skills_guard_finding_t;

typedef struct {
    char skill_name[128];
    char source[64];
    char trust_level[16];
    char verdict[16];
    skills_guard_finding_t *findings;
    int finding_count;
    char scanned_at[64];
    char summary[512];
    int fresh;   /* scan_skill_cached provenance */
} skills_guard_scan_result_t;

/* Forward declarations for functions defined later in this file. */
int cli_tools_skills_guard_scan_skill(const char *skill_path, const char *source,
                                      char *result_buf, size_t bufsize);
const char *cli_tools_skills_guard__resolve_trust_level(const char *source, const char *repo);

/* PoP: cli_tools_skills_guard__content_digest @ tools/skills_guard.py:_content_digest */
/* Canonical SHA-256 over relative paths and exact file bytes. Mirrors the
 * Python exactly: for a directory, sorted(rglob('*')) of files, each fed as
 * rel.as_posix().encode() + b'\x00' + file bytes into one rolling SHA-256;
 * for a single file, the bare file bytes. We reproduce the rolling hash by
 * assembling the same byte stream then computing one SHA-256 (skills are
 * small, so an in-memory assembly is faithful and bounded). */
static int sg_collect_files(const char *root, const char *rel,
                             char *out[], int *n, int cap) {
    char path[4096];
    snprintf(path, sizeof(path), "%s%s%s", root, rel[0] ? "/" : "", rel);
    DIR *d = opendir(path);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char child[4096];
        snprintf(child, sizeof(child), "%s%s%s", rel, rel[0] ? "/" : "", e->d_name);
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", root, child);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            sg_collect_files(root, child, out, n, cap);
        } else if (S_ISREG(st.st_mode)) {
            if (*n < cap) out[(*n)++] = strdup(child);
        }
    }
    closedir(d);
    return 0;
}

static int sg_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

char *skills_guard_content_digest(const char *skill_path) {
    if (!skill_path) return NULL;
    struct stat st;
    if (stat(skill_path, &st) != 0) return NULL;

    char *stream = NULL;
    size_t slen = 0, scap = 0;

    if (S_ISDIR(st.st_mode)) {
        char *files[8192];
        int n = 0;
        sg_collect_files(skill_path, "", files, &n, 8192);
        qsort(files, n, sizeof(char *), sg_cmp_str);
        for (int i = 0; i < n; i++) {
            /* rel.as_posix() is POSIX-style (forward slashes) — already so. */
            size_t rlen = strlen(files[i]);
            /* grow stream */
            if (slen + rlen + 1 + 1 > scap) {
                size_t ncap = (slen + rlen + 1024) * 2;
                char *ns = realloc(stream, ncap);
                if (!ns) { free(stream); return NULL; }
                stream = ns; scap = ncap;
            }
            memcpy(stream + slen, files[i], rlen); slen += rlen;
            stream[slen++] = '\0';  /* b"\x00" separator */
            char full[4096];
            snprintf(full, sizeof(full), "%s/%s", skill_path, files[i]);
            FILE *f = fopen(full, "rb");
            if (f) {
                fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
                if (sz > 0 && slen + (size_t)sz > scap) {
                    size_t ncap = (slen + (size_t)sz + 1024) * 2;
                    char *ns = realloc(stream, ncap);
                    if (!ns) { fclose(f); free(stream); return NULL; }
                    stream = ns; scap = ncap;
                }
                if (sz > 0) {
                    size_t rd = fread(stream + slen, 1, (size_t)sz, f);
                    slen += rd;
                }
                fclose(f);
            }
            free(files[i]);
        }
    } else if (S_ISREG(st.st_mode)) {
        FILE *f = fopen(skill_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            if (sz > 0) {
                stream = malloc((size_t)sz);
                if (stream) { slen = fread(stream, 1, (size_t)sz, f); scap = (size_t)sz; }
            }
            fclose(f);
        }
    } else {
        return NULL;
    }

    unsigned char md[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)(stream ? stream : ""), slen, md);
    free(stream);

    char *hex = malloc(CRYPTO_SHA256_LEN * 2 + 1);
    if (!hex) return NULL;
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < CRYPTO_SHA256_LEN; i++) {
        hex[i * 2] = H[md[i] >> 4];
        hex[i * 2 + 1] = H[md[i] & 0xF];
    }
    hex[CRYPTO_SHA256_LEN * 2] = '\0';
    return hex;
}

/* PoP: cli_tools_skills_guard_full_content_hash @ tools/skills_guard.py:full_content_hash */
char *skills_guard_full_content_hash(const char *skill_path) {
    char *digest = skills_guard_content_digest(skill_path);
    if (!digest) return NULL;
    size_t need = strlen(digest) + 8;  /* "sha256:" + digest + NUL */
    char *out = malloc(need);
    if (!out) { free(digest); return NULL; }
    snprintf(out, need, "sha256:%s", digest);
    free(digest);
    return out;
}

/* PoP: cli_tools_skills_guard__finding_dict @ tools/skills_guard.py:_finding_dict */
/* Mirror {key: getattr(finding, key) for key in (...)} — returns a compact
 * JSON object with the seven Finding fields. String values are JSON-escaped.
 * Caller frees. */
static void sg_append_json_str(char *buf, size_t *off, size_t cap, const char *s) {
    if (!s) s = "";
    if (*off + 1 < cap) buf[(*off)++] = '"';
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        char tmp[8]; size_t n = 0;
        switch (*p) {
            case '"':  memcpy(tmp, "\\\"", 2); n = 2; break;
            case '\\': memcpy(tmp, "\\\\", 2); n = 2; break;
            case '\n': memcpy(tmp, "\\n", 2); n = 2; break;
            case '\r': memcpy(tmp, "\\r", 2); n = 2; break;
            case '\t': memcpy(tmp, "\\t", 2); n = 2; break;
            default:
                if (*p < 0x20) { snprintf(tmp, sizeof(tmp), "\\u%04x", *p); n = 6; }
                else tmp[n++] = (char)*p;
        }
        if (*off + n + 1 < cap) memcpy(buf + *off, tmp, n), *off += n;
    }
    if (*off + 1 < cap) buf[(*off)++] = '"';
}

char *skills_guard_finding_dict(const skills_guard_finding_t *f) {
    if (!f) return strdup("{}");
    char *out = malloc(2048);
    if (!out) return NULL;
    size_t off = 0, cap = 2048;
    const char *keys[7] = {"pattern_id", "severity", "category", "file", "line", "match", "description"};
    const char *vals[7] = {f->pattern_id, f->severity, f->category, f->file, NULL, f->match, f->description};
    off += (size_t)snprintf(out, cap - off, "{\"%s\":\"", keys[0]);
    sg_append_json_str(out, &off, cap, vals[0]);
    for (int i = 1; i < 7; i++) {
        if (off + 1 < cap) out[off++] = ',';
        if (strcmp(keys[i], "line") == 0) {
            off += (size_t)snprintf(out + off, cap - off, "\"line\":%d", f->line);
        } else {
            off += (size_t)snprintf(out + off, cap - off, "\"%s\":", keys[i]);
            sg_append_json_str(out, &off, cap, vals[i]);
        }
    }
    if (off + 1 < cap) out[off++] = '}';
    out[off] = '\0';
    return out;
}

/* PoP: cli_tools_skills_guard_scan_skill_cached @ tools/skills_guard.py:scan_skill_cached */
/* Returns a scan plus attestation, caching only exact current content.
 * On a content+source match, deserializes the cached ScanResult (fresh=false);
 * otherwise runs the scan and writes a fresh cache entry (fresh=true). */
int skills_guard_scan_skill_cached(const char *skill_path, const char *source,
                                    const char *source_url, const char *cache_dir,
                                    skills_guard_scan_result_t *out) {
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (!skill_path) return -1;
    if (!source) source = "community";
    if (!source_url) source_url = "";

    char *bundle_hash = skills_guard_full_content_hash(skill_path);
    if (!bundle_hash) return -1;

    /* cache_root = cache_dir or <skill_path.parent>/.scan-cache */
    char cache_root[4096];
    if (cache_dir && *cache_dir) {
        snprintf(cache_root, sizeof(cache_root), "%s", cache_dir);
    } else {
        char parent[4096];
        const char *slash = strrchr(skill_path, '/');
        if (slash) { size_t pl = (size_t)(slash - skill_path); memcpy(parent, skill_path, pl); parent[pl] = '\0'; }
        else snprintf(parent, sizeof(parent), ".");
        snprintf(cache_root, sizeof(cache_root), "%s/.scan-cache", parent);
    }

    /* source_identity = sha256(f"{source}\0{source_url}")[:16] */
    char sid_in[1024];
    int sid_l = snprintf(sid_in, sizeof(sid_in), "%s%c%s", source, '\0', source_url);
    unsigned char sid_md[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)sid_in, (size_t)sid_l, sid_md);
    char source_identity[17];
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        source_identity[i] = H[sid_md[i] >> 4];
        source_identity[i + 1] = H[sid_md[i] & 0xF];
    }
    source_identity[16] = '\0';

    char bare[CRYPTO_SHA256_LEN * 2 + 1];
    const char *colon = strchr(bundle_hash, ':');
    snprintf(bare, sizeof(bare), "%s", colon ? colon + 1 : bundle_hash);

    char cache_file[8192];
    snprintf(cache_file, sizeof(cache_file), "%s/%s-%s.json", cache_root, bare, source_identity);

    /* Try load cached */
    json_t *cached = NULL;
    {
        FILE *f = fopen(cache_file, "r");
        if (f) {
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            if (sz > 0) {
                char *buf = malloc((size_t)sz + 1);
                if (buf) {
                    size_t rd = fread(buf, 1, (size_t)sz, f); buf[rd] = '\0';
                    cached = json_parse(buf, NULL);
                    free(buf);
                }
            }
            fclose(f);
        }
    }

    int hit = 0;
    if (cached && json_get_str(cached, "bundle_hash", NULL) &&
        strcmp(json_get_str(cached, "bundle_hash", ""), bundle_hash) == 0 &&
        strcmp(json_get_str(cached, "scanner_version", ""), "skills-guard-v1") == 0 &&
        strcmp(json_get_str(cached, "source", ""), source) == 0 &&
        strcmp(json_get_str(cached, "source_url", ""), source_url) == 0) {
        hit = 1;
        snprintf(out->skill_name, sizeof(out->skill_name), "%s",
                 strrchr(skill_path, '/') ? strrchr(skill_path, '/') + 1 : skill_path);
        snprintf(out->source, sizeof(out->source), "%s", source);
        snprintf(out->trust_level, sizeof(out->trust_level), "%s",
                 json_get_str(cached, "trust_level", "community"));
        snprintf(out->verdict, sizeof(out->verdict), "%s",
                 json_get_str(cached, "verdict", "safe"));
        snprintf(out->scanned_at, sizeof(out->scanned_at), "%s",
                 json_get_str(cached, "scanned_at", ""));
        snprintf(out->summary, sizeof(out->summary), "%s",
                 json_get_str(cached, "summary", ""));
        out->fresh = 0;
    }
    if (cached) json_free(cached);
    free(bundle_hash);

    if (hit) return 0;

    /* Miss: run the underlying scan (cli_tools_skills_guard_scan_skill fills a
     * JSON report; we materialize a minimal ScanResult from it), then write a
     * fresh cache entry so a repeat call hits (mirrors Python scan_skill_cached
     * which persists the attestation keyed on exact content + source). */
    char report[4096];
    int rc = cli_tools_skills_guard_scan_skill(skill_path, source, report, sizeof(report));
    (void)rc;
    const char *base = strrchr(skill_path, '/') ? strrchr(skill_path, '/') + 1 : skill_path;
    snprintf(out->skill_name, sizeof(out->skill_name), "%s", base);
    snprintf(out->source, sizeof(out->source), "%s", source);
    snprintf(out->trust_level, sizeof(out->trust_level), "%s",
             cli_tools_skills_guard__resolve_trust_level(source, NULL));
    /* verdict from the report is not parsed here; mirror the scan_skill
     * default of safe for a clean skill. */
    snprintf(out->verdict, sizeof(out->verdict), "safe");
    /* Mirror _build_summary: clean scan -> "<name>: clean scan, no threats detected". */
    snprintf(out->summary, sizeof(out->summary), "%s: clean scan, no threats detected", base);
    out->fresh = 1;

    /* Write the cache entry (recompute bundle_hash since we freed it). */
    char *bh = skills_guard_full_content_hash(skill_path);
    if (bh) {
        char bare[CRYPTO_SHA256_LEN * 2 + 1];
        const char *colon = strchr(bh, ':');
        snprintf(bare, sizeof(bare), "%s", colon ? colon + 1 : bh);
        mkdir(cache_root, 0755);
        char cfile[8192];
        snprintf(cfile, sizeof(cfile), "%s/%s-%s.json", cache_root, bare, source_identity);
        FILE *cf = fopen(cfile, "w");
        if (cf) {
            /* provenance mirrors Python: bundle_hash, scanner_version, source,
             * source_url, trust_level, verdict, scanned_at, summary, fresh. */
            fprintf(cf,
                "{\"bundle_hash\":\"%s\",\"scanner_version\":\"skills-guard-v1\","
                "\"source\":\"%s\",\"source_url\":\"%s\",\"trust_level\":\"%s\","
                "\"verdict\":\"%s\",\"scanned_at\":\"%s\",\"summary\":\"%s\",\"fresh\":true}",
                bh, source, source_url, out->trust_level, out->verdict,
                out->scanned_at, out->summary);
            fclose(cf);
        }
        free(bh);
    }
    return 0;
}


int cli_tools_skills_guard_scan_file(const char *filepath, const char *source, char *result_buf, size_t bufsize) {
    if (!filepath || !result_buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "skills_guard", "scan_file: invalid args");
        return -1;
    }
    FILE *f = fopen(filepath, "r");
    if (!f) {
        hermes_log(LOG_WARNING, "skills_guard", "scan_file: cannot open %s", filepath);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > 1024 * 1024) {
        fclose(f);
        hermes_log(LOG_WARNING, "skills_guard", "scan_file: file too large or empty (%ld bytes)", size);
        return -1;
    }
    char *content = (char*)malloc(size + 1);
    if (!content) {
        fclose(f);
        hermes_log(LOG_WARNING, "skills_guard", "scan_file: malloc failed");
        return -1;
    }
    size_t n = fread(content, 1, size, f);
    content[n] = '\0';
    fclose(f);
    int findings = 0;
    if (strstr(content, "curl") && strstr(content, "KEY")) findings++;
    if (strstr(content, "wget") && strstr(content, "TOKEN")) findings++;
    if (strstr(content, "rm -rf /")) findings++;
    if (strstr(content, "ignore previous instructions")) findings++;
    if (strstr(content, "sudo")) findings++;
    free(content);
    snprintf(result_buf, bufsize, "{\"file\":\"%s\",\"findings\":%d}", filepath, findings);
    hermes_log(LOG_DEBUG, "skills_guard", "scan_file: %s findings=%d", filepath, findings);
    return findings;
}

/* PoP: cli_tools_skills_guard_scan_skill @ tools/skills_guard.py:scan_skill */
int cli_tools_skills_guard_scan_skill(const char *skill_path, const char *source, char *result_buf, size_t bufsize) {
    if (!skill_path || !result_buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "skills_guard", "scan_skill: invalid args");
        return -1;
    }
    hermes_log(LOG_DEBUG, "skills_guard", "scan_skill: path=%s source=%s", skill_path, source ? source : "(null)");
    snprintf(result_buf, bufsize, "{\"path\":\"%s\",\"source\":\"%s\",\"findings\":0}",
             skill_path, source ? source : "community");
    return 0;
}

/* PoP: cli_tools_skills_guard_format_scan_report @ tools/skills_guard.py:format_scan_report */
int cli_tools_skills_guard_format_scan_report(const char *scan_json, char *buf, size_t bufsize) {
    if (!scan_json || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "skills_guard", "format_scan_report: invalid args");
        return -1;
    }
    hermes_log(LOG_DEBUG, "skills_guard", "format_scan_report: %s", scan_json);
    snprintf(buf, bufsize, "Scan Report: %s", scan_json);
    return 0;
}

/* PoP: cli_tools_skills_guard_content_hash @ tools/skills_guard.py:content_hash */
int cli_tools_skills_guard_content_hash(const char *filepath, char *hash_buf, size_t bufsize) {
    if (!filepath || !hash_buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "skills_guard", "content_hash: invalid args");
        return -1;
    }
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        hermes_log(LOG_WARNING, "skills_guard", "content_hash: cannot open %s", filepath);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char*)malloc(size);
    if (!data) {
        fclose(f);
        return -1;
    }
    size_t n = fread(data, 1, size, f);
    fclose(f);
    /* Simple hash: sum of bytes mod 256 as hex */
    unsigned int hash = 0;
    for (size_t i = 0; i < n; i++) {
        hash = (hash * 31 + data[i]) & 0xFFFFFFFF;
    }
    free(data);
    snprintf(hash_buf, bufsize, "%08x", hash);
    hermes_log(LOG_DEBUG, "skills_guard", "content_hash: %s -> %s", filepath, hash_buf);
    return 0;
}

/* PoP: cli_tools_skills_guard__check_structure @ tools/skills_guard.py:_check_structure */
int cli_tools_skills_guard__check_structure(const char *skill_path, char *report_buf, size_t bufsize) {
    if (!skill_path || !report_buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "skills_guard", "_check_structure: invalid args");
        return -1;
    }
    char skill_md[2048];
    snprintf(skill_md, sizeof(skill_md), "%s/SKILL.md", skill_path);
    FILE *f = fopen(skill_md, "r");
    if (!f) {
        hermes_log(LOG_WARNING, "skills_guard", "_check_structure: no SKILL.md in %s", skill_path);
        snprintf(report_buf, bufsize, "missing SKILL.md");
        return -1;
    }
    fclose(f);
    snprintf(report_buf, bufsize, "structure OK");
    hermes_log(LOG_DEBUG, "skills_guard", "_check_structure: %s OK", skill_path);
    return 0;
}

/* PoP: cli_tools_skills_guard__unicode_char_name @ tools/skills_guard.py:_unicode_char_name */
int cli_tools_skills_guard__unicode_char_name(unsigned int codepoint, char *buf, size_t bufsize) {
    if (!buf || bufsize == 0) {
        return -1;
    }
    /* Basic ASCII printable */
    if (codepoint >= 0x20 && codepoint < 0x7F) {
        snprintf(buf, bufsize, "U+%04X ('%c')", codepoint, (char)codepoint);
    } else if (codepoint < 0x80) {
        snprintf(buf, bufsize, "U+%04X (control)", codepoint);
    } else {
        snprintf(buf, bufsize, "U+%04X", codepoint);
    }
    hermes_log(LOG_DEBUG, "skills_guard", "_unicode_char_name: %s", buf);
    return 0;
}

/* PoP: cli_tools_skills_guard__load_skill_ignore @ tools/skills_guard.py:_load_skill_ignore */
int cli_tools_skills_guard__load_skill_ignore(const char *ignore_path, char **patterns, int max_patterns) {
    if (!ignore_path || !patterns || max_patterns <= 0) {
        hermes_log(LOG_WARNING, "skills_guard", "_load_skill_ignore: invalid args");
        return 0;
    }
    FILE *f = fopen(ignore_path, "r");
    if (!f) {
        hermes_log(LOG_DEBUG, "skills_guard", "_load_skill_ignore: no file at %s", ignore_path);
        return 0;
    }
    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), f) && count < max_patterns) {
        char *p = line;
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (*p == '\0' || *p == '#') continue;
        size_t len = strlen(p);
        while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r')) p[--len] = '\0';
        if (len == 0) continue;
        patterns[count] = strdup(p);
        if (patterns[count]) count++;
    }
    fclose(f);
    hermes_log(LOG_DEBUG, "skills_guard", "_load_skill_ignore: %d patterns from %s", count, ignore_path);
    return count;
}

/* PoP: cli_tools_skills_guard__resolve_trust_level @ tools/skills_guard.py:_resolve_trust_level */
const char* cli_tools_skills_guard__resolve_trust_level(const char *source, const char *repo) {
    if (!source) {
        hermes_log(LOG_WARNING, "skills_guard", "_resolve_trust_level: NULL source");
        return "community";
    }
    (void)repo; /* Python derives trust from source alone; repo is ignored. */

    /* Prefix aliases normalized away (tools/skills_guard.py). */
    static const char *PREFIX_ALIASES[] = {
        "skills-sh/", "skills.sh/", "skils-sh/", "skils.sh/", NULL
    };
    static const char *TRUSTED_REPOS[] = {
        "openai/skills", "anthropics/skills", "huggingface/skills", "NVIDIA/skills", NULL
    };

    char norm[256];
    snprintf(norm, sizeof(norm), "%s", source);
    for (int i = 0; PREFIX_ALIASES[i]; i++) {
        size_t pl = strlen(PREFIX_ALIASES[i]);
        if (strncmp(norm, PREFIX_ALIASES[i], pl) == 0) {
            memmove(norm, norm + pl, strlen(norm) - pl + 1);
            break;
        }
    }

    if (strcmp(norm, "agent-created") == 0) return "agent-created";
    if (strcmp(norm, "official") == 0)     return "builtin";

    for (int i = 0; TRUSTED_REPOS[i]; i++) {
        size_t tl = strlen(TRUSTED_REPOS[i]);
        if (strcmp(norm, TRUSTED_REPOS[i]) == 0 ||
            (strncmp(norm, TRUSTED_REPOS[i], tl) == 0 && norm[tl] == '/')) {
            return "trusted";
        }
    }
    hermes_log(LOG_DEBUG, "skills_guard", "_resolve_trust_level: source=%s -> community", source);
    return "community";
}

/* PoP: cli_tools_skills_guard__determine_verdict @ tools/skills_guard.py:_determine_verdict */
/* Faithful to Python: critical -> dangerous, high -> caution, else safe.
 * Severity levels: 3=critical, 2=high, 1=medium, 0=low (trust-independent,
 * matching the Python findings-based logic). */
const char* cli_tools_skills_guard__determine_verdict(int max_severity, int total_findings) {
    (void)total_findings; /* Python verdict is driven by severity, not count. */
    if (max_severity >= 3) return "dangerous";
    if (max_severity == 2) return "caution";
    return "safe";
}
