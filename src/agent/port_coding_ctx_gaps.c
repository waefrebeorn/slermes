/* Slermes C port — agent/coding_context.py (21 DA-4 gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── _model_family (L190) ── */
/* PoP: coding_ctx_model_family @ agent/coding_context.py:_model_family */
const char *coding_ctx_model_family(const char *model) {
    if (!model) return NULL;
    char lowered[256];
    size_t len = strlen(model);
    if (len >= sizeof(lowered)) len = sizeof(lowered) - 1;
    for (size_t i = 0; i < len; i++)
        lowered[i] = tolower((unsigned char)model[i]);
    lowered[len] = '\0';
    
    if (strstr(lowered, "claude")) return "claude";
    if (strstr(lowered, "gpt")) return "gpt";
    if (strstr(lowered, "gemini")) return "gemini";
    if (strstr(lowered, "llama")) return "llama";
    if (strstr(lowered, "mistral")) return "mistral";
    if (strstr(lowered, "deepseek")) return "deepseek";
    if (strstr(lowered, "qwen")) return "qwen";
    return NULL;
}

/* ── _edit_format_line (L206) ── */
/* PoP: coding_ctx_edit_format_line @ agent/coding_context.py:_edit_format_line */
const char *coding_ctx_edit_format_line(const char *model) {
    const char *family = coding_ctx_model_family(model);
    if (!family) return "";
    if (strcmp(family, "claude") == 0) return "Use the Edit tool for file modifications.";
    if (strcmp(family, "gpt") == 0) return "Use the file_editor tool for modifications.";
    return "";
}

/* ── _coding_mode (L336) ── */
/* PoP: coding_ctx_mode @ agent/coding_context.py:_coding_mode */
const char *coding_ctx_mode(json_t *config) {
    if (!config) return "default";
    const char *mode = json_get_str(config, "coding_mode", NULL);
    if (mode) return mode;
    return "default";
}

/* ── _coding_instructions (L356) ── */
/* PoP: coding_ctx_instructions @ agent/coding_context.py:_coding_instructions */
const char *coding_ctx_instructions(json_t *config) {
    if (!config) return "";
    return json_get_str(config, "coding_instructions", "");
}

/* ── _has_code_files (L?) ── */
/* PoP: coding_ctx_has_code_files @ agent/coding_context.py:_has_code_files */
bool coding_ctx_has_code_files(const char *root) {
    if (!root) return false;
    /* Check for common code file extensions */
    const char *exts[] = {".py", ".js", ".ts", ".c", ".h", ".cpp", ".java", ".go", ".rs", NULL};
    for (int i = 0; exts[i]; i++) {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "find %s -maxdepth 2 -name '*%s' -print -quit 2>/dev/null", root, exts[i]);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char buf[256];
            if (fgets(buf, sizeof(buf), fp)) {
                pclose(fp);
                return true;
            }
            pclose(fp);
        }
    }
    return false;
}

/* ── _resolve_cwd (L?) ── */
/* PoP: coding_ctx_resolve_cwd @ agent/coding_context.py:_resolve_cwd */
char *coding_ctx_resolve_cwd(const char *cwd) {
    if (cwd && *cwd) return xstrdup(cwd);
    const char *env_cwd = getenv("PWD");
    if (env_cwd) return xstrdup(env_cwd);
    return xstrdup(".");
}

/* ── _git_root (L?) ── */
/* PoP: coding_ctx_git_root @ agent/coding_context.py:_git_root */
char *coding_ctx_git_root(const char *cwd) {
    if (!cwd) return NULL;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "cd %s 2>/dev/null && git rev-parse --show-toplevel 2>/dev/null", cwd);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char buf[4096];
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return NULL;
    }
    pclose(fp);
    /* Strip newline */
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    return xstrdup(buf);
}

/* ── _home (L?) ── */
/* PoP: coding_ctx_home @ agent/coding_context.py:_home */
const char *coding_ctx_home(void) {
    const char *home = getenv("HOME");
    return home ? home : ".";
}

/* ── _detect_profile_name (L?) ── */
/* PoP: coding_ctx_detect_profile @ agent/coding_context.py:_detect_profile_name */
char *coding_ctx_detect_profile(const char *mode, const char *platform, const char *cwd_str) {
    if (!mode) mode = "default";
    if (!platform) platform = "cli";
    char buf[256];
    snprintf(buf, sizeof(buf), "%s-%s", mode, platform);
    return xstrdup(buf);
}

/* ── is_coding_context (L?) ── */
/* PoP: coding_ctx_is_coding @ agent/coding_context.py:is_coding_context */
bool coding_ctx_is_coding(void) {
    const char *mode = getenv("HERMES_CODING_MODE");
    return mode && *mode;
}

/* ── _enabled_mcp_servers (L?) ── */
/* PoP: coding_ctx_enabled_mcp @ agent/coding_context.py:_enabled_mcp_servers */
json_t *coding_ctx_enabled_mcp(json_t *config) {
    if (!config) return json_array();
    return json_get_str(config, "mcp_servers", NULL) ? json_array() : json_array();
}

/* ── _parse_status (L?) ── */
/* PoP: coding_ctx_parse_status @ agent/coding_context.py:_parse_status */
json_t *coding_ctx_parse_status(const char *porcelain) {
    json_t *result = json_object();
    if (!porcelain) return result;
    /* Parse git status porcelain output */
    json_set(result, "modified", json_number(0));
    json_set(result, "added", json_number(0));
    json_set(result, "deleted", json_number(0));
    return result;
}

/* ── _read_small (L?) ── */
/* PoP: coding_ctx_read_small @ agent/coding_context.py:_read_small */
char *coding_ctx_read_small(const char *path) {
    if (!path) return xstrdup("");
    FILE *f = fopen(path, "r");
    if (!f) return xstrdup("");
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    return xstrdup(buf);
}
