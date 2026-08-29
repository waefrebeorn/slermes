/* ───────────────────────── gap fillers ────────────────────────── */
/* Implements hermes_cli/banner.py gap functions that have no C
 * equivalent yet (DA-1, c_location=None).                            */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "hermes_logger.h"
#include "slermes_home.h"

/* ================================================================== */
/*  Git helpers                                                        */
/* ================================================================== */

/* PoP: banner_upstream_main_sha @ hermes_cli/banner.py:_upstream_main_sha */
/* Read the upstream main SHA via `git ls-remote` (cached per-process
 * in Python; C uses a simple function-level static cache).
 * Returns malloc'd string (64 hex chars + NUL) or NULL on failure. */
char *banner_upstream_main_sha(void)
{
    static char cached_sha[65] = {0};
    if (cached_sha[0]) return strdup(cached_sha);

    const char *repo = getenv("HERMES_GIT_REMOTE_URL");
    if (!repo) repo = "https://github.com/NousResearch/hermes-agent.git";
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "git ls-remote %s refs/heads/main 2>/dev/null", repo);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char line[256] = {0};
    if (fgets(line, sizeof(line), fp)) {
        pclose(fp);
        /* First field is the SHA */
        char *sha = strtok(line, " \t\n");
        if (sha && strlen(sha) == 64) {
            strncpy(cached_sha, sha, sizeof(cached_sha)-1);
            cached_sha[sizeof(cached_sha)-1] = '\0';
            return strdup(cached_sha);
        }
    } else {
        pclose(fp);
    }
    return NULL;
}

/* PoP: banner_github_compare_behind @ hermes_cli/banner.py:_github_compare_behind */
/* Use the GitHub compare API to count commits behind upstream.
 * Returns the behind count, or -1 on failure/ambiguous. */
int banner_github_compare_behind(const char *local_rev, const char *upstream_rev)
{
    if (!local_rev || !upstream_rev) return -1;
    const char *repo = getenv("HERMES_GIT_REMOTE_URL");
    if (!repo) repo = "NousResearch/hermes-agent";
    /* Extract owner/repo from full URL if needed */
    char owner_repo[256];
    const char *slash = strrchr(repo, '/');
    if (slash && slash > repo) {
        strncpy(owner_repo, slash + 1, sizeof(owner_repo)-1);
        owner_repo[sizeof(owner_repo)-1] = '\0';
    } else {
        strncpy(owner_repo, repo, sizeof(owner_repo)-1);
        owner_repo[sizeof(owner_repo)-1] = '\0';
    }
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "curl -sf 'https://api.github.com/repos/%s/compare/%s...%s' 2>/dev/null | "
        "grep -o '\"behind_by\": *[0-9]*' | grep -o '[0-9]*' | head -1",
        owner_repo, upstream_rev, local_rev);
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    int behind = -1;
    char val[16] = {0};
    if (fgets(val, sizeof(val), fp)) {
        behind = atoi(val);
    }
    pclose(fp);
    return behind;
}

/* PoP: banner_get_latest_release_tag @ hermes_cli/banner.py:get_latest_release_tag */
/* Return the latest git tag (and canonical release URL) via
 * `git describe --tags --abbrev=0`. Returns malloc'd "tag|url" string
 * or NULL. */
char *banner_get_latest_release_tag(const char *repo_dir)
{
    const char *dir = repo_dir ? repo_dir : ".";
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cd %s && git describe --tags --abbrev=0 2>/dev/null", dir);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char tag[256] = {0};
    if (fgets(tag, sizeof(tag), fp)) {
        pclose(fp);
        /* Strip newline */
        char *nl = strchr(tag, '\n');
        if (nl) *nl = '\0';
        if (tag[0]) {
            char *result = malloc(strlen(tag) + 64);
            if (result) {
                snprintf(result, strlen(tag) + 64, "%s|https://github.com/NousResearch/hermes-agent/releases/tag/%s",
                         tag, tag);
            }
            return result;
        }
    }
    pclose(fp);
    return NULL;
}

/* PoP: banner_prefetch_banner_data @ hermes_cli/banner.py:prefetch_banner_data */
/* Prefetch git banner state + latest release tag. Best-effort;
 * caches results in function-level statics for subsequent calls. */
void banner_prefetch_banner_data(void)
{
    /* Trigger lazy computation by calling the cached helpers */
    banner_upstream_main_sha();
    banner_get_latest_release_tag(NULL);
}

/* ================================================================== */
/*  Banner git state                                                   */
/* ================================================================== */

/* PoP: banner_get_git_banner_state @ hermes_cli/banner.py:get_git_banner_state */
/* Return upstream/local git hashes for the startup banner as a
 * malloc'd JSON string. Format:
 * {"upstream":"<sha>","local":"<sha>","ahead":0}
 * Falls back to the baked-in build SHA when no checkout is available. */
char *banner_get_git_banner_state(const char *repo_dir)
{
    const char *dir = repo_dir ? repo_dir : ".";

    /* Local HEAD SHA */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cd %s && git rev-parse HEAD 2>/dev/null", dir);
    FILE *fp = popen(cmd, "r");
    char local_sha[65] = {0};
    if (fp) {
        if (fgets(local_sha, sizeof(local_sha), fp)) {
            /* strip newline */
            local_sha[strcspn(local_sha, "\n")] = '\0';
        }
        pclose(fp);
    }

    /* Upstream SHA */
    char *upstream = banner_upstream_main_sha();
    char upstream_sha[65] = {0};
    if (upstream) {
        strncpy(upstream_sha, upstream, sizeof(upstream_sha)-1);
        upstream_sha[sizeof(upstream_sha)-1] = '\0';
        free(upstream);
    }

    /* Ahead count */
    int ahead = 0;
    if (local_sha[0] && upstream_sha[0]) {
        if (strcmp(local_sha, upstream_sha) == 0) {
            ahead = 0;
        } else {
            snprintf(cmd, sizeof(cmd),
                "cd %s && git rev-list --count %s..HEAD 2>/dev/null",
                dir, upstream_sha);
            fp = popen(cmd, "r");
            if (fp) {
                char val[16] = {0};
                if (fgets(val, sizeof(val), fp)) ahead = atoi(val);
                pclose(fp);
            }
        }
    }

    /* Build JSON */
    char *result = malloc(512);
    if (result) {
        snprintf(result, 512,
            "{\"upstream\":\"%s\",\"local\":\"%s\",\"ahead\":%d}",
            upstream_sha[0] ? upstream_sha : "",
            local_sha[0] ? local_sha : "",
            ahead);
    }
    return result;
}
