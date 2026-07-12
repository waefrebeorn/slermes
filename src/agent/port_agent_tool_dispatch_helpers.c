/* Slermes C port — agent/tool_dispatch_helpers.py (pure mutation-tracking helpers)
 *
 * Faithful port of _neutralize_delimiters and _extract_landed_file_mutation_paths.
 * _extract_landed_file_mutation_paths depends on _extract_file_mutation_targets
 * (re-implemented here) and a minimal JSON parse of the result string.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <regex.h>

/* File-mutating tool names (FILE_MUTATING_TOOL_NAMES in Python). */
static const char *FILE_MUTATING_TOOLS[] = { "write_file", "patch", NULL };

/* PoP: agent_tool_dispatch__neutralize_delimiters @ agent/tool_dispatch_helpers.py:_neutralize_delimiters */
char *agent_tool_dispatch_neutralize_delimiters(const char *content)
{
    if (!content) return NULL;
    /* Replace every case-insensitive occurrence of "untrusted_tool_result"
     * with "untrusted-tool-result" (only the underscore becomes a hyphen;
     * all other characters, including case, are preserved). */
    size_t clen = strlen(content);
    char *out = malloc(clen + 1);
    size_t o = 0;
    for (size_t i = 0; i < clen; ) {
        bool match = false;
        if (i + 21 <= clen) {
            const char *lit = "untrusted_tool_result";
            match = true;
            for (int k = 0; k < 21; k++) {
                char c = content[i + k];
                if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
                if (c != lit[k]) { match = false; break; }
            }
        }
        if (match) {
            /* The Python .sub() replaces with a FIXED lowercase string
             * "untrusted-tool-result" regardless of the matched case. */
            const char *rep = "untrusted-tool-result";
            for (int k = 0; k < 21; k++) out[o++] = rep[k];
            i += 21;
        } else {
            out[o++] = content[i++];
        }
    }
    out[o] = '\0';
    return out;
}

static bool is_file_mutating(const char *tool_name)
{
    for (int i = 0; FILE_MUTATING_TOOLS[i]; i++)
        if (strcmp(tool_name, FILE_MUTATING_TOOLS[i]) == 0) return true;
    return false;
}

/* Re-implementation of _extract_file_mutation_targets (needed by _extract_landed_file_mutation_paths). */
static char **extract_file_mutation_targets(const char *tool_name, const char *path,
                                            const char *mode, const char *patch, int *out_n)
{
    int cap = 4, n = 0;
    char **paths = malloc(sizeof(char *) * cap);
    if (!is_file_mutating(tool_name)) { *out_n = 0; return paths; }
    if (strcmp(tool_name, "write_file") == 0) {
        if (path && path[0]) { paths[n++] = strdup(path); }
    } else { /* patch */
        const char *m = mode ? mode : "replace";
        if (strcmp(m, "replace") == 0) {
            if (path && path[0]) paths[n++] = strdup(path);
        } else if (strcmp(m, "patch") == 0) {
            if (patch && patch[0]) {
                static regex_t re1, re2; static bool comp = false;
                if (!comp) {
                    regcomp(&re1, "^\\*\\*\\*[ \t]+(Update|Add|Delete)[ \t]+File:[ \t]*(.+)$", REG_EXTENDED | REG_NEWLINE);
                    regcomp(&re2, "^\\*\\*\\*[ \t]+Move[ \t]+File:[ \t]*(.+?)[ \t]*->[ \t]*(.+)$", REG_EXTENDED | REG_NEWLINE);
                    comp = true;
                }
                char *buf = strdup(patch);
                regmatch_t mm[3];
                size_t off = 0;
                while (regexec(&re1, buf + off, 3, mm, 0) == 0) {
                    /* find capture group 2 */
                    int so = mm[2].rm_so, eo = mm[2].rm_eo;
                    size_t l = (size_t)(eo - so);
                    char *pv = malloc(l + 1); memcpy(pv, buf + off + so, l); pv[l] = '\0';
                    /* trim trailing ws */
                    while (l > 0 && (pv[l-1]==' '||pv[l-1]=='\t')) pv[--l]='\0';
                    if (pv[0]) { if (n>=cap){cap*=2;paths=realloc(paths,sizeof(char*)*cap);} paths[n++]=pv; }
                    else free(pv);
                    off += (size_t)mm[0].rm_eo;
                }
                off = 0;
                while (regexec(&re2, buf + off, 3, mm, 0) == 0) {
                    int so = mm[1].rm_so, eo = mm[1].rm_eo;
                    size_t l = (size_t)(eo - so); char *sv = malloc(l+1); memcpy(sv, buf+off+so, l); sv[l]='\0';
                    while (l>0 && (sv[l-1]==' '||sv[l-1]=='\t')) sv[--l]='\0';
                    int so2 = mm[2].rm_so, eo2 = mm[2].rm_eo;
                    size_t l2 = (size_t)(eo2-so2); char *dv = malloc(l2+1); memcpy(dv, buf+off+so2, l2); dv[l2]='\0';
                    while (l2>0 && (dv[l2-1]==' '||dv[l2-1]=='\t')) dv[--l2]='\0';
                    if (sv[0]) { if (n>=cap){cap*=2;paths=realloc(paths,sizeof(char*)*cap);} paths[n++]=sv; } else free(sv);
                    if (dv[0]) { if (n>=cap){cap*=2;paths=realloc(paths,sizeof(char*)*cap);} paths[n++]=dv; } else free(dv);
                    off += (size_t)mm[0].rm_eo;
                }
                free(buf);
            }
        }
    }
    paths = realloc(paths, sizeof(char *) * (n + 1));
    paths[n] = NULL;
    *out_n = n;
    return paths;
}

/* Minimal JSON string/array extraction for files_modified / resolved_path.
 * Only handles the specific shapes the Python code reads. */
static bool json_get_string_field(const char *json, const char *key, char *out, size_t outsz)
{
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"[ \t]*:[ \t]*\"", key);
    regex_t re; if (regcomp(&re, pat, REG_EXTENDED) != 0) return false;
    regmatch_t m; char *buf = strdup(json); bool found = false;
    if (regexec(&re, buf, 1, &m, 0) == 0) {
        char *start = buf + m.rm_eo - 1;  /* the value's opening quote */
        if (start >= buf && *start == '"') {
            size_t i = 1, o = 0;
            while (start[i] && start[i] != '"') {
                if (start[i] == '\\' && start[i+1]) {
                    char e = start[i+1];
                    if (e == 'n') out[o++] = '\n';
                    else if (e == 't') out[o++] = '\t';
                    else if (e == 'r') out[o++] = '\r';
                    else out[o++] = e;   /* \\ and \" and \/ */
                    i += 2;
                } else {
                    if (o < outsz - 1) out[o++] = start[i];
                    i++;
                }
            }
            out[o] = '\0'; found = true;
        }
    }
    free(buf); regfree(&re); return found;
}

/* PoP: agent_tool_dispatch__extract_landed_file_mutation_paths @ agent/tool_dispatch_helpers.py:_extract_landed_file_mutation_paths */
char **agent_tool_dispatch_extract_landed_file_mutation_paths(const char *tool_name,
                                                              const char *args_json,
                                                              const char *result, int *out_n)
{
    /* Parse path/mode/patch from args_json (mirrors Python's args dict). */
    char path[1024]; path[0] = '\0';
    char mode[64]; mode[0] = '\0';
    char patch[8192]; patch[0] = '\0';
    if (args_json) {
        json_get_string_field(args_json, "path", path, sizeof(path));
        json_get_string_field(args_json, "mode", mode, sizeof(mode));
        json_get_string_field(args_json, "patch", patch, sizeof(patch));
    }
    int targets_n = 0;
    char **targets = extract_file_mutation_targets(tool_name, path[0]?path:NULL,
                                                   mode[0]?mode:NULL, patch[0]?patch:NULL, &targets_n);
    if (!is_file_mutating(tool_name) || !result) { *out_n = targets_n; return targets; }
    /* try JSON parse of result */
    char *rbuf = strdup(result);
    /* trim leading whitespace */
    char *rp = rbuf; while (*rp == ' ' || *rp == '\t' || *rp == '\n' || *rp == '\r') rp++;
    if (rp[0] == '{') {
        /* files_modified array */
        regex_t re; regcomp(&re, "\"files_modified\"[ \t]*:[ \t]*\\[", REG_EXTENDED);
        regmatch_t m;
        if (regexec(&re, rp, 1, &m, 0) == 0) {
            char *arr = rp + m.rm_eo;
            /* collect strings until matching ] */
            int cap = targets_n + 4, n = targets_n;
            targets = realloc(targets, sizeof(char *) * cap);
            char *p = arr;
            while (*p && *p != ']') {
                if (*p == '"') {
                    p++;
                    size_t i = 0; char val[1024];
                    while (*p && *p != '"') {
                        if (*p == '\\' && p[1]) { val[i++] = p[1]; p += 2; } else val[i++] = *p++;
                    }
                    val[i] = '\0'; p++;
                    if (val[0]) { if (n>=cap){cap*=2;targets=realloc(targets,sizeof(char*)*cap);} targets[n++]=strdup(val); }
                } else p++;
            }
            targets = realloc(targets, sizeof(char *) * (n + 1));
            targets[n] = NULL;
            *out_n = n;
            free(rbuf);
            regfree(&re);
            return targets;
        }
        regfree(&re);
        /* resolved_path */
        char rv[1024];
        if (json_get_string_field(rp, "resolved_path", rv, sizeof(rv)) && rv[0]) {
            targets = realloc(targets, sizeof(char *) * (targets_n + 2));
            targets[targets_n++] = strdup(rv);
            targets[targets_n] = NULL;
            *out_n = targets_n;
            free(rbuf);
            return targets;
        }
    }
    free(rbuf);
    *out_n = targets_n;
    return targets;
}
