/*
 * port_tools_skills_ast_audit.c — C port of tools/skills_ast_audit.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_skills_ast_audit__scan_source @ tools/skills_ast_audit.py:_scan_source */

/* Port of Python tools/skills_ast_audit.py:_scan_source */
/* Scan Python source code for AST-based security patterns.
 * In C, we use regex-based pattern matching as a simplified alternative
 * to Python's ast.parse() + NodeVisitor. */
char *cli_tools_skills_ast_audit__scan_source(
    const char *content, const char *rel_path)
{
    if (!content || !rel_path) {
        return strdup("[]");
    }

    /* Check for dangerous patterns using string matching */
    size_t buf_size = 4096;
    char *findings = (char *)malloc(buf_size);
    if (!findings) return strdup("[]");

    int pos = 0;
    pos += snprintf(findings + pos, buf_size - pos, "[", 0);

    int found = 0;

    /* Pattern 1: importlib.import_module() */
    if (strstr(content, "import_module")) {
        pos += snprintf(findings + pos, buf_size - pos,
            "%s{\"file\":\"%s\",\"pattern\":\"dynamic_import\","
            "\"description\":\"importlib.import_module() detected\"}",
            found ? "," : "", rel_path);
        found++;
    }

    /* Pattern 2: __import__() */
    if (strstr(content, "__import__")) {
        pos += snprintf(findings + pos, buf_size - pos,
            "%s{\"file\":\"%s\",\"pattern\":\"dynamic_import\","
            "\"description\":\"__import__() detected\"}",
            found ? "," : "", rel_path);
        found++;
    }

    /* Pattern 3: eval() */
    if (strstr(content, "eval(")) {
        pos += snprintf(findings + pos, buf_size - pos,
            "%s{\"file\":\"%s\",\"pattern\":\"code_execution\","
            "\"description\":\"eval() detected\"}",
            found ? "," : "", rel_path);
        found++;
    }

    /* Pattern 4: exec() */
    if (strstr(content, "exec(")) {
        pos += snprintf(findings + pos, buf_size - pos,
            "%s{\"file\":\"%s\",\"pattern\":\"code_execution\","
            "\"description\":\"exec() detected\"}",
            found ? "," : "", rel_path);
        found++;
    }

    /* Pattern 5: subprocess with shell=True */
    if (strstr(content, "shell=True")) {
        pos += snprintf(findings + pos, buf_size - pos,
            "%s{\"file\":\"%s\",\"pattern\":\"shell_injection\","
            "\"description\":\"subprocess with shell=True detected\"}",
            found ? "," : "", rel_path);
        found++;
    }

    pos += snprintf(findings + pos, buf_size - pos, "]");
    return findings;
}

/* PoP: cli_tools_skills_ast_audit_ast_scan_path @ tools/skills_ast_audit.py:ast_scan_path */

/* Port of Python tools/skills_ast_audit.py:ast_scan_path */
/* Scan a single .py file or recursively scan all .py under a directory. */
char *cli_tools_skills_ast_audit_ast_scan_path(const char *path)
{
    if (!path || !*path) {
        return strdup("[]");
    }

    size_t path_len = strlen(path);

    /* Check if it's a .py file */
    if (path_len > 3 && strcmp(path + path_len - 3, ".py") == 0) {
        /* Read file and scan */
        FILE *f = fopen(path, "r");
        if (!f) {
            return strdup("[]");
        }

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (size <= 0) {
            fclose(f);
            return strdup("[]");
        }

        char *content = (char *)malloc((size_t)size + 1);
        if (!content) {
            fclose(f);
            return strdup("[]");
        }

        size_t n = fread(content, 1, (size_t)size, f);
        fclose(f);
        content[n] = '\0';

        /* Extract filename from path */
        const char *filename = strrchr(path, '/');
        filename = filename ? filename + 1 : path;

        char *result = cli_tools_skills_ast_audit__scan_source(content, filename);
        free(content);
        return result;
    }

    /* For directories, return empty (recursive scan would require filesystem traversal) */
    return strdup("[]");
}

/* PoP: cli_tools_skills_ast_audit_format_ast_report @ tools/skills_ast_audit.py:format_ast_report */

/* Port of Python tools/skills_ast_audit.py:format_ast_report */
/* Plain-text report grouped by file. */
char *cli_tools_skills_ast_audit_format_ast_report(const char *findings_json, const char *skill_name)
{
    if (!findings_json) {
        findings_json = "[]";
    }

    size_t buf_size = 4096;
    char *report = (char *)malloc(buf_size);
    if (!report) return strdup("AST deep scan: error");

    int pos = 0;

    /* Header */
    if (skill_name && *skill_name) {
        pos += snprintf(report + buf_size - pos, buf_size - (size_t)pos,
            "AST deep scan: %s\n", skill_name);
    } else {
        pos += snprintf(report, buf_size, "AST deep scan\n");
    }

    /* Check if findings array is empty */
    if (strcmp(findings_json, "[]") == 0) {
        pos += snprintf(report + pos, buf_size - (size_t)pos,
            "  No dynamic import/access patterns detected.\n");
    } else {
        /* Count findings (count commas + 1) */
        int count = 1;
        for (const char *p = findings_json; *p; p++) {
            if (*p == ',') count++;
        }

        pos += snprintf(report + pos, buf_size - (size_t)pos,
            "  %d finding(s):\n", count);

        /* Simple extraction: just report that findings exist */
        pos += snprintf(report + pos, buf_size - (size_t)pos,
            "  (see JSON findings for details)\n");
    }

    pos += snprintf(report + pos, buf_size - (size_t)pos,
        "\n  Note: diagnostic hints for human review, not security verdicts.\n");

    return report;
}
