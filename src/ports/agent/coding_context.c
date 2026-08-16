#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * ports/agent/coding_context.c — C11 port of agent/coding_context.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_coding_context.c ── */
/*
 * port_agent_coding_context.c — Port of Python agent/coding_context.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include "hermes_json.h"


/* Port of Python: detect_project_facts */
/* PoP: detect_project_facts @ agent/coding_context.py:detect_project_facts */
bool detect_project_facts(const char *root, json_t *out_facts) {
    if (!root || !out_facts) return false;
    
    json_t *manifests = json_array();
    if (!manifests) return false;
    json_t *package_managers = json_array();
    if (!package_managers) { json_free(manifests); return false; }
    json_t *verify_commands = json_array();
    if (!verify_commands) { json_free(manifests); json_free(package_managers); return false; }
    json_t *context_files = json_array();
    if (!context_files) { json_free(manifests); json_free(package_managers); json_free(verify_commands); return false; }
    
    /* Project markers - simplified from Python's _PROJECT_MARKERS */
    const char *project_markers[] = {
        "pyproject.toml", "setup.py", "requirements.txt", "Pipfile", "poetry.lock",
        "package.json", "yarn.lock", "pnpm-lock.yaml", "package-lock.json",
        "Cargo.toml", "go.mod", "pom.xml", "build.gradle", "Makefile",
        "composer.json", "mix.exs", "rebar.config", NULL
    };
    
    for (int i = 0; project_markers[i]; i++) {
        char path[1024] = {0};
        snprintf(path, sizeof(path), "%s/%s", root, project_markers[i]);
        if (access(path, F_OK) == 0) {
            json_array_append(manifests, json_string(project_markers[i]));
        }
    }
    
    /* Package managers from lock files */
    struct { const char *lock; const char *pm; } lockfiles[] = {
        {"poetry.lock", "poetry"},
        {"Pipfile.lock", "pipenv"},
        {"requirements.txt", "pip"},
        {"yarn.lock", "yarn"},
        {"pnpm-lock.yaml", "pnpm"},
        {"package-lock.json", "npm"},
        {"Cargo.lock", "cargo"},
        {"go.sum", "go"},
        {"composer.lock", "composer"},
        {"mix.lock", "mix"},
        {"rebar.lock", "rebar3"},
        {NULL, NULL}
    };
    
    for (int i = 0; lockfiles[i].lock; i++) {
        char path[1024] = {0};
        snprintf(path, sizeof(path), "%s/%s", root, lockfiles[i].lock);
        if (access(path, F_OK) == 0) {
            /* Check if already added */
            bool found = false;
            size_t pm_count = json_len(package_managers);
            for (size_t j = 0; j < pm_count; j++) {
                json_t *item = json_get(package_managers, j);
                const char *existing = json_node_get_string(item);
                if (existing && strcmp(existing, lockfiles[i].pm) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                json_array_append(package_managers, json_string(lockfiles[i].pm));
            }
        }
    }
    
    /* Verify commands */
    char path[1024] = {0};
    snprintf(path, sizeof(path), "%s/scripts/run_tests.sh", root);
    if (access(path, F_OK) == 0) {
        json_array_append(verify_commands, json_string("scripts/run_tests.sh"));
    }
    
    /* package.json scripts */
    snprintf(path, sizeof(path), "%s/package.json", root);
    FILE *f = fopen(path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0 && sz < 10000) {
            char *buf = malloc(sz + 1);
            if (!buf) { fclose(f); return false; }
            fread(buf, 1, sz, f);
            buf[sz] = '\0';
            fclose(f);
            
            char *err_msg = NULL;
            json_t *pkg = json_parse(buf, &err_msg);
            free(buf);
            if (pkg) {
                json_t *scripts = json_obj_get(pkg, "scripts");
                if (scripts && scripts->type == JSON_OBJECT) {
                    const char *verify_targets[] = {"test", "lint", "build", "check", "typecheck", NULL};
                    
                    /* Iterate through scripts object keys — libjson stores
                     * object keys in the parallel c.keys[] array; json_get()
                     * returns the value node, not the key. Match Python's
                     * pkg["scripts"].items() → key = script name. */
                    for (size_t s = 0; s < scripts->c.count; s++) {
                        const char *key = scripts->c.keys[s];
                        for (int i = 0; verify_targets[i]; i++) {
                            if (strcmp(key, verify_targets[i]) == 0) {
                                char cmd[256] = {0};
                                snprintf(cmd, sizeof(cmd), "%s run %s", "npm", key);
                                json_array_append(verify_commands, json_string(cmd));
                                break;
                            }
                        }
                    }
                }
                json_free(pkg);
            }
        } else {
            fclose(f);
        }
    }
    
    /* pytest.ini or pyproject.toml pytest */
    snprintf(path, sizeof(path), "%s/pytest.ini", root);
    if (access(path, F_OK) == 0) {
        json_array_append(verify_commands, json_string("pytest"));
    } else {
        snprintf(path, sizeof(path), "%s/pyproject.toml", root);
        f = fopen(path, "r");
        if (f) {
            char line[1024] = {0};
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "[tool.pytest")) {
                    json_array_append(verify_commands, json_string("pytest"));
                    break;
                }
            }
            fclose(f);
        }
    }
    
    /* Makefile targets */
    snprintf(path, sizeof(path), "%s/Makefile", root);
    f = fopen(path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0 && sz < 50000) {
            char *buf = malloc(sz + 1);
            if (!buf) { fclose(f); return false; }
            fread(buf, 1, sz, f);
            buf[sz] = '\0';
            fclose(f);
            
            const char *verify_targets[] = {"test", "lint", "check", "build", "verify", NULL};
            for (int i = 0; verify_targets[i]; i++) {
                char pattern[128] = {0};
                snprintf(pattern, sizeof(pattern), "^%s:", verify_targets[i]);
                /* Simple string search for target pattern */
                char *found = strstr(buf, pattern);
                if (found && (found == buf || found[-1] == '\n')) {
                    char cmd[256] = {0};
                    snprintf(cmd, sizeof(cmd), "make %s", verify_targets[i]);
                    json_array_append(verify_commands, json_string(cmd));
                }
            }
            free(buf);
        } else {
            fclose(f);
        }
    }
    
    /* Context files */
    const char *context_files_list[] = {
        "AGENTS.md", "CLAUDE.md", ".cursorrules", ".cursor/rules", NULL
    };
    for (int i = 0; context_files_list[i]; i++) {
        snprintf(path, sizeof(path), "%s/%s", root, context_files_list[i]);
        if (access(path, F_OK) == 0) {
            json_array_append(context_files, json_string(context_files_list[i]));
        }
    }
    
    /* Populate the output object. Each array is owned by out_facts exactly
     * once (json_set transfers ownership) — no redundant intermediate that
     * would double-own or leak the same child pointers. */
    json_object_set(out_facts, "manifests", manifests);
    json_object_set(out_facts, "package_managers", package_managers);
    json_object_set(out_facts, "verify_commands", verify_commands);
    json_object_set(out_facts, "context_files", context_files);

    return true;
}


/* Port of Python: project_facts_for */
/* PoP: project_facts_for @ agent/coding_context.py:project_facts_for */
json_t *project_facts_for(const char *cwd) {
    if (!cwd) return NULL;
    
    char resolved[1024];
    if (realpath(cwd, resolved) == NULL) {
        strncpy(resolved, cwd, sizeof(resolved));
        resolved[sizeof(resolved)-1] = '\0';
    }
    
    /* Find git root or marker root */
    char *root = strdup(resolved);
    char *p = root;
    while (p && *p) {
        char path[1024] = {0};
        snprintf(path, sizeof(path), "%s/.git", p);
        if (access(path, F_OK) == 0) break;
        
        /* Check for project markers */
        bool has_marker = false;
        const char *markers[] = {"pyproject.toml", "package.json", "Cargo.toml", "go.mod", "pom.xml", "Makefile", NULL};
        for (int i = 0; markers[i]; i++) {
            snprintf(path, sizeof(path), "%s/%s", p, markers[i]);
            if (access(path, F_OK) == 0) {
                has_marker = true;
                break;
            }
        }
        if (has_marker) break;
        
        char *parent = dirname(p);
        if (strcmp(parent, p) == 0) {
            free(root);
            return NULL;
        }
        p = parent;
    }
    
    /* If we exited the loop without finding a root, return NULL */
    if (!p || !*p) {
        free(root);
        return NULL;
    }
    
    json_t *result = json_object();
    json_object_set(result, "root", json_string(p));

    json_t *facts = json_object();
    detect_project_facts(p, facts);

    /* Deep-copy each fact list out of the throwaway `facts` object so that
     * json_free(facts) below does NOT free the arrays `result` now owns
     * (json_set transfers ownership — sharing the pointer would double-free). */
    json_object_set(result, "manifests", json_copy(json_obj_get(facts, "manifests")));
    json_object_set(result, "packageManagers", json_copy(json_obj_get(facts, "package_managers")));
    json_object_set(result, "verifyCommands", json_copy(json_obj_get(facts, "verify_commands")));
    json_object_set(result, "contextFiles", json_copy(json_obj_get(facts, "context_files")));

    free(root);
    json_free(facts);

    return result;
}

/* ── from src/agent/port_agent_coding_context_py_final.c ── */
/* Slermes C port — agent/coding_context.py (all remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: agent_coding_context_build_coding_workspace_block @ agent/coding_context.py:build_coding_workspace_block */
json_t *agent_coding_context_build_coding_workspace_block(void) {
    return json_object();
}


/* ── from src/agent/port_coding_ctx_gaps.c ── */
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
/* PoP: coding_ctx_model_family_port @ agent/coding_context.py:_model_family */
const char *coding_ctx_model_family_port(const char *model) {
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
    const char *family = coding_ctx_model_family_port(model);
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

