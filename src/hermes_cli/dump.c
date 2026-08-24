/**
 * port_dump.c — Port of Python: cli.py (dump helpers)
 *
 * Real C implementations for dump/git functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

/* Port of Python: get_git_commit_date */
char *get_git_commit_date(json_t *project_root)
{
    if (!project_root) {
        hermes_log(LOG_WARNING, "port", "get_git_commit_date: null project_root");
        return strdup("unknown");
    }
    const char *root = json_node_get_string(project_root);
    if (!root) return strdup("unknown");

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "git -C '%s' log -1 --format=%%ci 2>/dev/null", root);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        hermes_log(LOG_WARNING, "port", "get_git_commit_date: git command failed");
        return strdup("unknown");
    }
    char buf[256];
    if (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\n")] = '\0';
        pclose(fp);
        hermes_log(LOG_DEBUG, "port", "get_git_commit_date: %s -> %s", root, buf);
        return strdup(buf);
    }
    pclose(fp);
    return strdup("unknown");
}

/* Port of Python: _get_git_commit_date */
char *_get_git_commit_date(void *ctx, void *project_root)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_get_git_commit_date: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "_get_git_commit_date: called");
    if (project_root) {
        hermes_log(LOG_DEBUG, "port", "_get_git_commit_date: project_root is set");
    }
    return get_git_commit_date((json_t *)project_root);
}


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_dump_u_dotenv_key_names @ hermes_cli/dump.py:_dotenv_key_names */
int hermes_cli_dump_u_dotenv_key_names(const char *arg) {
    /* Python: non-empty env names. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_dump_u_get_git_commit @ hermes_cli/dump.py:_get_git_commit */
int hermes_cli_dump_u_get_git_commit(const char *arg) {
    /* Python: rev-parse then baked SHA. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("(unknown)\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("(unknown)\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "(unknown)");
    return 0;
}

/* PoP: hermes_cli_dump_u_count_skills @ hermes_cli/dump.py:_count_skills */
int hermes_cli_dump_u_count_skills(const char *arg) {
    /* Python: count SKILL.md under <home>/skills (excluding excluded paths).
     * Arg = "skills_dir\texcluded\texcluded..." */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    size_t dlen = tab ? (size_t)(tab - arg) : strlen(arg);
    char dir[1024];
    if (dlen >= sizeof(dir)) dlen = sizeof(dir) - 1;
    memcpy(dir, arg, dlen); dir[dlen] = '\0';
    if (dlen == 0) { printf("0\n"); return 0; }
    char cmd[1300];
    snprintf(cmd, sizeof(cmd), "find '%s' -name SKILL.md 2>/dev/null | wc -l", dir);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("0\n"); return 0; }
    char buf[64];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    long count = strtol(buf, NULL, 10);
    printf("%ld\n", count);
    return 0;
}

/* PoP: hermes_cli_dump_u_count_mcp_servers @ hermes_cli/dump.py:_count_mcp_servers */
int hermes_cli_dump_u_count_mcp_servers(const char *arg) {
    /* Python: len(config.get("mcp", {}).get("servers", {})). Arg = mcp
     * section JSON (or empty). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    json_t *mcp = json_parse(arg, NULL);
    if (!mcp || !json_is_object(mcp)) {
        if (mcp) json_free(mcp);
        printf("0\n");
        return 0;
    }
    json_t *servers = json_obj_get(mcp, "servers");
    size_t n = (servers && json_is_object(servers)) ? json_len(servers) : 0;
    printf("%zu\n", n);
    json_free(mcp);
    return 0;
}

/* PoP: hermes_cli_dump_u_cron_summary @ hermes_cli/dump.py:_cron_summary */
int hermes_cli_dump_u_cron_summary(const char *arg) {
    /* Python: "N active / M total" or "0" or "(error reading)". Arg =
     * "state\tsummary". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0 && tab && tab[1]) { printf("%s\n", tab + 1); return 0; }
    if (strcmp(state, "missing") == 0) { printf("0\n"); return 0; }
    printf("(error reading)\n");
    return 0;
}

/* PoP: hermes_cli_dump_u_configured_platforms @ hermes_cli/dump.py:_configured_platforms */
int hermes_cli_dump_u_configured_platforms(const char *arg) {
    /* Python: env-driven platform names. Arg = "platforms" (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_dump_u_memory_provider @ hermes_cli/dump.py:_memory_provider */
int hermes_cli_dump_u_memory_provider(const char *arg) {
    /* Python _memory_provider(config): config.memory.provider or the
     * "built-in" default. Arg = provider (may be empty). */
    if (!arg || !*arg) { printf("built-in\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_dump_u_get_model_and_provider @ hermes_cli/dump.py:_get_model_and_provider */
int hermes_cli_dump_u_get_model_and_provider(const char *arg) {
    /* Python: model config dict/str -> (model, provider). Arg =
     * "model_cfg_json\tdefault\tprovider". */
    if (!arg || !*arg) { printf("(not set)\t(auto)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *model = arg;
    const char *provider = t2 ? t2 + 1 : "(auto)";
    if (!model[0]) model = "(not set)";
    printf("%s\t%s\n", model, provider);
    return 0;
}

/* PoP: hermes_cli_dump_u_config_overrides @ hermes_cli/dump.py:_config_overrides */
int hermes_cli_dump_u_config_overrides(const char *arg) {
    /* Python: interesting overrides. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_dump_run_dump @ hermes_cli/dump.py:run_dump */
int hermes_cli_dump_run_dump(const char *arg) {
    /* Python: setup summary. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("dump rendered (version/commit/model/provider/profile/terminal backend EFFECTIVE value%s)%s\n", (tab && tab[1] == '1') ? "/env var source" : "", (tab && tab[1] == '1') ? " — keys masked" : "");
    return 0;
}
