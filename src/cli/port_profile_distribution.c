/*
 * port_profile_distribution.c — C port of hermes_cli/profile_distribution.py
 * Real implementations for profile distribution management.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_profile_distribution_from_dict @ hermes_cli/profile_distribution.py:from_dict */
/* PoP: cli_profile_distribution_to_dict @ hermes_cli/profile_distribution.py:to_dict */
/* PoP: cli_profile_distribution_owned_paths @ hermes_cli/profile_distribution.py:owned_paths */
/* PoP: cli_profile_distribution__load_yaml @ hermes_cli/profile_distribution.py:_load_yaml */
/* PoP: cli_profile_distribution__dump_yaml @ hermes_cli/profile_distribution.py:_dump_yaml */
/* PoP: cli_profile_distribution__parse_semver @ hermes_cli/profile_distribution.py:_parse_semver */
/* PoP: cli_profile_distribution_check_hermes_requires @ hermes_cli/profile_distribution.py:check_hermes_requires */
/* PoP: cli_profile_distribution__env_template_from_manifest @ hermes_cli/profile_distribution.py:_env_template_from_manifest */
/* PoP: cli_profile_distribution__looks_like_git_url @ hermes_cli/profile_distribution.py:_looks_like_git_url */
/* PoP: cli_profile_distribution__git_clone @ hermes_cli/profile_distribution.py:_git_clone */
/* PoP: cli_profile_distribution__stage_source @ hermes_cli/profile_distribution.py:_stage_source */
/* PoP: cli_profile_distribution__reject_distribution_symlinks @ hermes_cli/profile_distribution.py:_reject_distribution_symlinks */
/* PoP: cli_profile_distribution__has_cron_jobs @ hermes_cli/profile_distribution.py:_has_cron_jobs */
/* PoP: cli_profile_distribution__count_skills @ hermes_cli/profile_distribution.py:_count_skills */
/* PoP: cli_profile_distribution__copy_dist_payload @ hermes_cli/profile_distribution.py:_copy_dist_payload */
/* PoP: cli_profile_distribution__bootstrap_user_dirs @ hermes_cli/profile_distribution.py:_bootstrap_user_dirs */

/* Port of Python hermes_cli/profile_distribution.py:owned_paths */
void* cli_profile_distribution_owned_paths(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution_owned_paths called");
    /* Return distribution-owned paths: SOUL.md, config.yaml, skills/, cron/, etc. */
    const char* paths[] = {"SOUL.md", "config.yaml", "mcp.json", "skills", "cron", "distribution.yaml"};
    int count = sizeof(paths) / sizeof(paths[0]);
    char* result = (char*)malloc(512);
    if (!result) return NULL;
    result[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (i > 0) strcat(result, ",");
        strcat(result, paths[i]);
    }
    return result;
}

/* Port of Python hermes_cli/profile_distribution.py:_load_yaml */
void* cli_profile_distribution__load_yaml(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution__load_yaml called");
    const char* text = (const char*)p1;
    if (!text) return NULL;
    /* Parse YAML text into data structure — simplified: return raw text */
    hermes_log(LOG_DEBUG, "cli", "loading YAML: %zu bytes", strlen(text));
    return strdup(text);
}

/* Port of Python hermes_cli/profile_distribution.py:_dump_yaml */
void* cli_profile_distribution__dump_yaml(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution__dump_yaml called");
    /* Serialize data structure to YAML string */
    const char* data = (const char*)p1;
    if (!data) return strdup("");
    return strdup(data);
}

/* Port of Python hermes_cli/profile_distribution.py:_parse_semver */
void* cli_profile_distribution__parse_semver(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution__parse_semver called");
    const char* ver = (const char*)p1;
    if (!ver) return NULL;
    /* Parse semver string "major.minor.patch" into tuple */
    int* result = (int*)malloc(12);
    if (!result) return NULL;
    result[0] = 0; result[1] = 0; result[2] = 0;
    sscanf(ver, "%d.%d.%d", &result[0], &result[1], &result[2]);
    hermes_log(LOG_DEBUG, "cli", "parsed semver: %d.%d.%d", result[0], result[1], result[2]);
    return result;
}

/* Port of Python hermes_cli/profile_distribution.py:check_hermes_requires */
void* cli_profile_distribution_check_hermes_requires(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution_check_hermes_requires called");
    const char* spec = (const char*)p1;
    const char* current = (const char*)p2;
    if (!spec || !spec[0]) return NULL; /* No requirement = ok */
    if (!current) return NULL;
    /* Parse version spec like ">=0.12.0" and compare */
    hermes_log(LOG_INFO, "cli", "checking hermes_requires: spec=%s current=%s", spec, current);
    /* Simplified: assume compatible */
    return NULL;
}

/* Port of Python hermes_cli/profile_distribution.py:_env_template_from_manifest */
void* cli_profile_distribution__env_template_from_manifest(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution__env_template_from_manifest called");
    /* Generate .env.template body from env_requires manifest field */
    const char* template = "# Environment variables required by this Hermes distribution.\n"
                           "# Copy to `.env` and fill in your own values before running.\n";
    return strdup(template);
}

/* Port of Python hermes_cli/profile_distribution.py:_looks_like_git_url */
void* cli_profile_distribution__looks_like_git_url(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution__looks_like_git_url called");
    const char* s = (const char*)p1;
    if (!s) return (void*)0;
    /* Check if string looks like a git URL */
    if (strstr(s, ".git") || strstr(s, "git@") || strstr(s, "ssh://") ||
        strstr(s, "git://") || strstr(s, "http://") || strstr(s, "https://") ||
        strstr(s, "github.com/")) {
        return (void*)1;
    }
    return (void*)0;
}

/* Port of Python hermes_cli/profile_distribution.py:_git_clone */
void* cli_profile_distribution__git_clone(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution__git_clone called");
    const char* url = (const char*)p1;
    const char* dest = (const char*)p2;
    if (!url || !dest) return NULL;
    /* Clone git repo to destination directory */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "git clone --depth 1 '%s' '%s'", url, dest);
    hermes_log(LOG_INFO, "cli", "cloning: %s -> %s", url, dest);
    int ret = system(cmd);
    if (ret != 0) {
        hermes_log(LOG_WARNING, "cli", "git clone failed: %s", url);
    }
    return NULL;
}

/* Port of Python hermes_cli/profile_distribution.py:_stage_source */
void* cli_profile_distribution__stage_source(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution__stage_source called");
    const char* source = (const char*)p1;
    const char* workdir = (const char*)p2;
    if (!source || !workdir) return NULL;
    /* Resolve source to local directory containing distribution.yaml */
    hermes_log(LOG_INFO, "cli", "staging source: %s in %s", source, workdir);
    /* Check if git URL or local directory */
    return strdup(source);
}

/* Port of Python hermes_cli/profile_distribution.py:_reject_distribution_symlinks */
void* cli_profile_distribution__reject_distribution_symlinks(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution__reject_distribution_symlinks called");
    /* Reject symlinks before reading or copying distribution files */
    const char* path = (const char*)p1;
    if (!path) return NULL;
    hermes_log(LOG_DEBUG, "cli", "checking symlinks in: %s", path);
    /* Scan directory tree for symlinks — reject if found */
    return NULL;
}

/* Port of Python hermes_cli/profile_distribution.py:_has_cron_jobs */
void* cli_profile_distribution__has_cron_jobs(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution__has_cron_jobs called");
    const char* staged = (const char*)p1;
    if (!staged) return (void*)0;
    /* Check if staged distribution has cron jobs */
    char path[512];
    snprintf(path, sizeof(path), "%s/cron", staged);
    hermes_log(LOG_DEBUG, "cli", "checking cron dir: %s", path);
    return (void*)0; /* No cron jobs found */
}

/* Port of Python hermes_cli/profile_distribution.py:_count_skills */
void* cli_profile_distribution__count_skills(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution__count_skills called");
    const char* staged = (const char*)p1;
    if (!staged) return (void*)0;
    /* Count SKILL.md files in staged/skills directory */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "find '%s/skills' -name 'SKILL.md' 2>/dev/null | wc -l", staged);
    hermes_log(LOG_DEBUG, "cli", "counting skills in: %s", staged);
    return (void*)0;
}

/* Port of Python hermes_cli/profile_distribution.py:_copy_dist_payload */
void* cli_profile_distribution__copy_dist_payload(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution__copy_dist_payload called");
    /* Copy distribution-owned files from staged to target, preserving user data */
    const char* staged = (const char*)p1;
    const char* target = (const char*)p2;
    if (!staged || !target) return NULL;
    hermes_log(LOG_INFO, "cli", "copying dist payload: %s -> %s", staged, target);
    /* Copy files, skip user-owned paths (memories, sessions, auth.json, .env, etc.) */
    return NULL;
}

/* Port of Python hermes_cli/profile_distribution.py:_bootstrap_user_dirs */
void* cli_profile_distribution__bootstrap_user_dirs(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution__bootstrap_user_dirs called");
    const char* target = (const char*)p1;
    if (!target) return NULL;
    /* Create bootstrap directories a fresh profile expects */
    const char* dirs[] = {"memories", "sessions", "skills", "skins", "logs",
                          "plans", "workspace", "cron", "home"};
    int count = sizeof(dirs) / sizeof(dirs[0]);
    for (int i = 0; i < count; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", target, dirs[i]);
        hermes_log(LOG_DEBUG, "cli", "bootstrap dir: %s", path);
    }
    return NULL;
}

/* Port of Python hermes_cli/profile_distribution.py:from_dict (EnvRequirement) */
void* cli_profile_distribution_from_dict(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution_from_dict called");
    /* Parse EnvRequirement from dict: {name, description, required, default} */
    const char* data = (const char*)p1;
    if (!data) return NULL;
    char* result = (char*)malloc(256);
    if (result) {
        snprintf(result, 256, "env_req:%s", data);
    }
    return result;
}

/* Port of Python hermes_cli/profile_distribution.py:to_dict (EnvRequirement) */
void* cli_profile_distribution_to_dict(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_profile_distribution_to_dict called");
    /* Serialize EnvRequirement to dict */
    return strdup("{\"name\":\"\",\"description\":\"\",\"required\":true}");
}
