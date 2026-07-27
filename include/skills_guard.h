/*
 * skills_guard.h — minimal declaration surface for the deterministic
 * skill-trust helpers ported from tools/skills_guard.py in
 * src/cli/port_tools_skills_guard.c. Opaque / minimal: no god-header.
 */

#ifndef SLERMES_SKILLS_GUARD_H
#define SLERMES_SKILLS_GUARD_H

#include <stddef.h>

/* Port of tools/skills_guard.py:_resolve_trust_level (source -> trust level).
 * The repo argument is accepted for API compatibility but ignored (Python
 * derives trust from source alone). */
const char *cli_tools_skills_guard__resolve_trust_level(const char *source, const char *repo);

/* Port of tools/skills_guard.py:_determine_verdict. Severity levels:
 * 3=critical, 2=high, 1=medium, 0=low. critical->dangerous, high->caution,
 * else safe (trust-independent, matching the Python findings logic). */
const char *cli_tools_skills_guard__determine_verdict(int max_severity, int total_findings);

/* ── Faithful C mirrors of the Python Finding / ScanResult dataclasses ───── */
/* Port of tools/skills_guard.py:Finding (7 fields). */
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
    int fresh;   /* scan_skill_cached provenance (0=cached, 1=fresh) */
} skills_guard_scan_result_t;

/* Port of tools/skills_guard.py:_content_digest — canonical SHA-256 over
 * relative paths + exact file bytes (directory) or bare file bytes. Caller frees. */
char *skills_guard_content_digest(const char *skill_path);

/* Port of tools/skills_guard.py:full_content_hash — "sha256:" + digest. Caller frees. */
char *skills_guard_full_content_hash(const char *skill_path);

/* Port of tools/skills_guard.py:_finding_dict — compact JSON of the 7
 * Finding fields. Caller frees. */
char *skills_guard_finding_dict(const skills_guard_finding_t *f);

/* Port of tools/skills_guard.py:scan_skill_cached — scan + attestation cache.
 * Returns 0 on success (out populated); out.fresh distinguishes cache hits. */
int skills_guard_scan_skill_cached(const char *skill_path, const char *source,
                                    const char *source_url, const char *cache_dir,
                                    skills_guard_scan_result_t *out);

#endif /* SLERMES_SKILLS_GUARD_H */
