/*
 * port_tools_skills_guard.c — C port of tools/skills_guard.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_skills_guard_scan_file @ tools/skills_guard.py:scan_file */
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
    if (strcmp(source, "builtin") == 0) return "builtin";
    if (strcmp(source, "trusted") == 0) return "trusted";
    if (strcmp(source, "agent-created") == 0) return "agent-created";
    if (repo) {
        if (strcmp(repo, "openai/skills") == 0) return "trusted";
        if (strcmp(repo, "anthropics/skills") == 0) return "trusted";
        if (strcmp(repo, "huggingface/skills") == 0) return "trusted";
        if (strcmp(repo, "NVIDIA/skills") == 0) return "trusted";
    }
    hermes_log(LOG_DEBUG, "skills_guard", "_resolve_trust_level: source=%s -> community", source);
    return "community";
}

/* PoP: cli_tools_skills_guard__determine_verdict @ tools/skills_guard.py:_determine_verdict */
const char* cli_tools_skills_guard__determine_verdict(int max_severity, int total_findings, const char *trust_level) {
    if (!trust_level) {
        trust_level = "community";
    }
    int is_trusted = (strcmp(trust_level, "builtin") == 0 || strcmp(trust_level, "trusted") == 0);
    if (max_severity >= 3) return "dangerous";
    if (max_severity >= 2) return "dangerous";
    if (max_severity >= 1 && !is_trusted) return "dangerous";
    if (max_severity >= 1) return "caution";
    if (total_findings > 0) return "caution";
    hermes_log(LOG_DEBUG, "skills_guard", "_determine_verdict: sev=%d findings=%d trust=%s -> safe",
               max_severity, total_findings, trust_level);
    return "safe";
}
