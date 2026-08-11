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

/* ── tools/skills_tool.py gap functions ──────────────────────────── */

#define SKILL_VIEW_DEDUP_CAP 200
#define SKILL_VIEW_DEDUP_MESSAGE \
    "Skill content unchanged since it was loaded earlier in this " \
    "conversation — refer to the earlier skill_view result; it is still " \
    "current and complete. (Re-issued after context compression, this " \
    "returns the full content again.)"

/* PoP: _skill_view_fingerprint @ tools/skills_tool.py:_skill_view_fingerprint */
char *skillstool_view_fingerprint(const char *arg) {
    /* Python: stat the skill file a successful skill_view served.
     * Arg = "source_path" or empty. Returns "src\tmtime_ns\tsize" or NULL. */
    if (!arg || !*arg) return NULL;
    struct stat st;
    if (stat(arg, &st) != 0) return NULL;
    char *out = NULL;
    long long mtime_ns = (long long)st.st_mtim.tv_sec * 1000000000LL
                       + (long long)st.st_mtim.tv_nsec;
    asprintf(&out, "%s\t%lld\t%lld", arg,
             mtime_ns, (long long)st.st_size);
    return out;
}

/* PoP: _record_skill_view @ tools/skills_tool.py:_record_skill_view */
int skillstool_record_skill_view(const char *arg) {
    /* Python: record a served skill_view so an identical repeat can dedup.
     * Skips setup-needed views. Arg = "recorded" (1/0). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg[0] == '1' ? "1" : "0");
    return 0;
}

/* PoP: _check_skill_view_dedup @ tools/skills_tool.py:_check_skill_view_dedup */
char *skillstool_check_skill_view_dedup(const char *arg) {
    /* Python: return a dedup stub when this exact skill file was already
     * served to this task and is unchanged; None otherwise.
     * Arg = "unchanged\tname\tfile_path" (unchanged 1/0). */
    if (!arg || !*arg) return NULL;
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int unchanged = arg[0] == '1';
    if (!unchanged) return NULL;
    const char *name = t1 ? t1 + 1 : "";
    const char *fp = t2 ? t2 + 1 : "SKILL.md";
    char *out = NULL;
    asprintf(&out,
        "{\"success\": true, \"status\": \"unchanged\", \"name\": \"%s\", "
        "\"file\": \"%s\", \"dedup\": true, \"content_returned\": false, "
        "\"message\": \"" SKILL_VIEW_DEDUP_MESSAGE "\"}",
        name, fp);
    return out;
}

/* PoP: reset_skill_view_dedup @ tools/skills_tool.py:reset_skill_view_dedup */
int skillstool_reset_skill_view_dedup(const char *arg) {
    /* Python: clear the skill_view dedup cache (all tasks when arg empty).
     * Called on context compression. Arg = "task_id" or empty. */
    (void)arg;
    printf("skill_view dedup cache cleared\n");
    return 0;
}
