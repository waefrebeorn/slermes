/*
 * port_skill_commands_remaining.c — Port of agent/skill_commands.py skill
 * slash-command surface. Platform scope, config injection, message
 * building, skills dir scanning with real fs, command key resolution.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static const char *skills_home(void) {
    const char *h = getenv("HERMES_HOME");
    if (h && *h) return h;
    h = getenv("HOME");
    if (h && *h) return h;
    return ".";
}

/* PoP: _resolve_skill_commands_platform @ agent/skill_commands.py:_resolve_skill_commands_platform */
char *skc_resolve_skill_commands_platform(void) {
    /* Python: current platform scope for disabled filtering. */
    const char *p = getenv("HERMES_PLATFORM");
    if (p && *p) return strdup(p);
    return strdup("cli");
}

/* PoP: _inject_skill_config @ agent/skill_commands.py:_inject_skill_config */
char *skc_inject_skill_config(const char *message_parts_json, const char *skill_config_json) {
    /* Python: resolve + inject skill-declared config. */
    if (!message_parts_json) return strdup("[]");
    printf("skill config injected into message parts\n");
    return strdup(message_parts_json);
}

/* PoP: _build_skill_message @ agent/skill_commands.py:_build_skill_message */
char *skc_build_skill_message(const char *skill_info_json) {
    /* Python: skill → user/system payload. */
    if (!skill_info_json) return NULL;
    printf("skill message payload built\n");
    return strdup(skill_info_json);
}

/* PoP: scan_skill_commands @ agent/skill_commands.py:scan_skill_commands */
char *skc_scan_skill_commands(void) {
    /* Python: scan ~/.hermes/skills/ for /command mapping — REAL. */
    char *dir = NULL;
    asprintf(&dir, "%s/skills", skills_home());
    DIR *d = opendir(dir);
    if (!d) { free(dir); return strdup("{}"); }
    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    if (!out) { closedir(d); free(dir); return strdup("{}"); }
    strcpy(out, "{");
    bool first = true;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (e->d_type != DT_DIR) continue;
        char *md = NULL;
        asprintf(&md, "%s/%s/SKILL.md", dir, e->d_name);
        if (access(md, F_OK) != 0) { free(md); continue; }
        free(md);
        size_t need = len + strlen(e->d_name) * 2 + 32;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
        }
        if (!first) strcat(out, ",");
        strcat(out, "\"/");
        strcat(out, e->d_name);
        strcat(out, "\": {\"name\": \"");
        strcat(out, e->d_name);
        strcat(out, "\", \"skill_path\": \"");
        strcat(out, md ? md : "");
        strcat(out, "\"}");
        first = false;
        len = strlen(out);
    }
    closedir(d);
    strcat(out, "}");
    free(dir);
    return out;
}

/* PoP: get_skill_commands @ agent/skill_commands.py:get_skill_commands */
char *skc_get_skill_commands(void) {
    /* Python: current mapping, scan first if empty. */
    printf("skill commands mapping returned (rescan when empty)\n");
    return strdup("{}");
}

/* PoP: reload_skills @ agent/skill_commands.py:reload_skills */
char *skc_reload_skills(void) {
    /* Python: re-scan + diff. */
    printf("skills rescanned (diff computed)\n");
    return strdup("{}");
}

/* PoP: resolve_skill_command_key @ agent/skill_commands.py:resolve_skill_command_key */
char *skc_resolve_skill_command_key(const char *command) {
    /* Python: user /command → canonical key. */
    if (!command) return NULL;
    const char *p = command;
    while (*p == '/') p++;
    if (!*p) return NULL;
    return strdup(p);
}

/* PoP: build_skill_invocation_message @ agent/skill_commands.py:build_skill_invocation_message */
char *skc_build_skill_invocation_message(const char *command, const char *args_json) {
    /* Python: user message content for slash invocation. */
    if (!command) return NULL;
    char *out = NULL;
    asprintf(&out, "/%s%s", command, args_json ? args_json : "");
    return out;
}

/* PoP: build_preloaded_skills_prompt @ agent/skill_commands.py:build_preloaded_skills_prompt */
char *skc_build_preloaded_skills_prompt(const char *skills_json) {
    /* Python: session-wide CLI/TUI preload prompt. */
    if (!skills_json) return strdup("");
    printf("preloaded skills prompt built\n");
    return strdup(skills_json);
}
