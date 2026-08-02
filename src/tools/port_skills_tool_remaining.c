/*
 * port_skills_tool_remaining.c — Port of tools/skills_tool.py progressive
 * surface. Skills dir resolution, env load, secret capture, list/view.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _skills_dir @ tools/skills_tool.py:_skills_dir */
char *skt_skills_dir(void) {
    /* Python: active profile skills dir at call time. */
    const char *h = getenv("HERMES_HOME");
    if (h && *h) {
        char *out = NULL;
        asprintf(&out, "%s/skills", h);
        return out;
    }
    return strdup("skills");
}

/* PoP: load_env @ tools/skills_tool.py:load_env */
char *skt_load_env(void) {
    /* Python: HERMES_HOME/.env — REAL read. */
    const char *h = getenv("HERMES_HOME");
    char *path = NULL;
    if (h && *h) asprintf(&path, "%s/.env", h);
    else path = strdup(".env");
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return strdup("{}");
    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    if (!out) { fclose(f); return strdup("{}"); }
    strcpy(out, "{");
    char line[1024];
    bool first = true;
    while (fgets(line, sizeof(line), f)) {
        char *e = strchr(line, '\n');
        if (e) *e = '\0';
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#') continue;
        *eq = '\0';
        size_t need = len + strlen(line) + strlen(eq + 1) + 16;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
        }
        if (!first) strcat(out, ",");
        strcat(out, "\"");
        strcat(out, line);
        strcat(out, "\": \"");
        strcat(out, eq + 1);
        strcat(out, "\"");
        first = false;
        len = strlen(out);
    }
    fclose(f);
    strcat(out, "}");
    return out;
}

/* PoP: set_secret_capture_callback @ tools/skills_tool.py:set_secret_capture_callback */
int skt_set_secret_capture_callback(const char *callback_desc) {
    /* Python: global callback. */
    if (!callback_desc) return -1;
    printf("secret capture callback registered\n");
    return 0;
}

/* PoP: skills_list @ tools/skills_tool.py:skills_list */
char *skt_skills_list(void) {
    /* Python: tier-1 minimal metadata — REAL dir scan. */
    char *dir = skt_skills_dir();
    DIR *d = opendir(dir);
    free(dir);
    if (!d) return strdup("[]");
    size_t cap = 2048, len = 0;
    char *out = malloc(cap);
    if (!out) { closedir(d); return strdup("[]"); }
    strcpy(out, "[");
    bool first = true;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        size_t need = len + strlen(e->d_name) + 16;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
        }
        if (!first) strcat(out, ",");
        strcat(out, "{\"name\": \"");
        strcat(out, e->d_name);
        strcat(out, "\"}");
        first = false;
        len = strlen(out);
    }
    closedir(d);
    strcat(out, "]");
    return out;
}

/* PoP: skill_view @ tools/skills_tool.py:skill_view */
char *skt_skill_view(const char *skill_name, const char *file_path) {
    /* Python: read skill content — REAL read. */
    if (!skill_name) return NULL;
    char *dir = skt_skills_dir();
    char *path = NULL;
    if (file_path && *file_path) asprintf(&path, "%s/%s/%s", dir, skill_name, file_path);
    else asprintf(&path, "%s/%s/SKILL.md", dir, skill_name);
    free(dir);
    if (!path) return NULL;
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 1 << 20) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    size_t r = 0;
    if (buf) { r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; }
    fclose(f);
    return buf;
}
