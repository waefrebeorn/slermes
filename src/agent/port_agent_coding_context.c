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
bool detect_project_facts(const char *root, json_t *out_facts) {
    if (!root || !out_facts) return false;
    
    json_t *facts = json_object();
    if (!facts) return false;
    json_t *manifests = json_array();
    if (!manifests) { json_free(facts); return false; }
    json_t *package_managers = json_array();
    if (!package_managers) { json_free(facts); json_free(manifests); return false; }
    json_t *verify_commands = json_array();
    if (!verify_commands) { json_free(facts); json_free(manifests); json_free(package_managers); return false; }
    json_t *context_files = json_array();
    if (!context_files) { json_free(facts); json_free(manifests); json_free(package_managers); json_free(verify_commands); return false; }
    
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
                const char *existing = json_string_value(item);
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
                    
                    /* Iterate through scripts object keys */
                    size_t script_count = json_len(scripts);
                    for (size_t s = 0; s < script_count; s++) {
                        json_t *key_item = json_get(scripts, s);
                        json_t *val = key_item; /* In our libjson, keys are stored in parallel array */
                        
                        const char *key = key_item->str_val;
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
        }
    } else if (f) fclose(f);
    
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
    
    json_object_set(facts, "manifests", manifests);
    json_object_set(facts, "package_managers", package_managers);
    json_object_set(facts, "verify_commands", verify_commands);
    json_object_set(facts, "context_files", context_files);
    
    /* Copy to output */
    json_object_set(out_facts, "manifests", manifests);
    json_object_set(out_facts, "package_managers", package_managers);
    json_object_set(out_facts, "verify_commands", verify_commands);
    json_object_set(out_facts, "context_files", context_files);
    
    return true;
}


/* Port of Python: project_facts_for */
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
    
    json_object_set(result, "manifests", json_obj_get(facts, "manifests"));
    json_object_set(result, "packageManagers", json_obj_get(facts, "package_managers"));
    json_object_set(result, "verifyCommands", json_obj_get(facts, "verify_commands"));
    json_object_set(result, "contextFiles", json_obj_get(facts, "context_files"));
    
    free(root);
    json_free(facts);
    
    return result;
}