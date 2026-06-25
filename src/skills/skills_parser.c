/*
 * skills_parser.c — SKILL.md YAML frontmatter parser
 *
 * Parses SKILL.md files from ~/.slermes/skills/ (and upstream source).
 * Extracts name, description, version, author, tags, dependencies.
 * Serves results as JSON via /api/skills endpoint.
 *
 * MIT License — Slermes Fork
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
/* Use the home resolution from the CLI port layer */
extern void hermes_get_home(char *buf, size_t sz);

#define MAX_SKILLS      256
#define MAX_SKILL_NAME   64
#define MAX_DESC_LEN    512
#define MAX_VERSION_LEN  32
#define MAX_AUTHOR_LEN   64
#define MAX_TAGS        16
#define MAX_TAG_LEN      64
#define MAX_DEPS        16
#define MAX_DEP_LEN      64
#define MAX_PATH        1024
#define FRONTMATTER_SZ  4096

typedef struct {
    char name[MAX_SKILL_NAME];
    char description[MAX_DESC_LEN];
    char version[MAX_VERSION_LEN];
    char author[MAX_AUTHOR_LEN];
    char tags[MAX_TAGS][MAX_TAG_LEN];
    int  tag_count;
    char dependencies[MAX_DEPS][MAX_DEP_LEN];
    int  dep_count;
    char path[MAX_PATH];
} skill_info_t;

/* ── YAML frontmatter parser ──────────────────────────────────────── */

/* Strip leading and trailing whitespace in-place */
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    char *e = s + strlen(s) - 1;
    while (e >= s && (*e == ' ' || *e == '\t' || *e == '\r' || *e == '\n'))
        *e-- = '\0';
    return s;
}

/* Parse a YAML list item: "- value" → value */
static void parse_yaml_list_item(const char *line, char *out, size_t out_sz) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '-') {
        p++;
        while (*p == ' ' || *p == '\t') p++;
    }
    /* Strip quotes */
    if (*p == '"' || *p == '\'') {
        char q = *p++;
        const char *end = strchr(p, q);
        if (end) {
            size_t len = (size_t)(end - p);
            if (len >= out_sz) len = out_sz - 1;
            memcpy(out, p, len);
            out[len] = '\0';
            return;
        }
    }
    snprintf(out, out_sz, "%s", p);
    char *nl = strchr(out, '\n');
    if (nl) *nl = '\0';
}

/* Parse SKILL.md frontmatter into skill_info_t.
 * Returns 0 on success, -1 if no valid frontmatter found. */
static int skill_parse(const char *filepath, skill_info_t *info) {
    FILE *f = fopen(filepath, "r");
    if (!f) return -1;

    char buf[FRONTMATTER_SZ];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    /* Must start with "---" */
    const char *p = buf;
    while (*p == ' ' || *p == '\n' || *p == '\r') p++;
    if (strncmp(p, "---", 3) != 0) {
        /* Try starting from position 0 */
        if (strncmp(buf, "---", 3) != 0) return -1;
        p = buf;
    }
    p += 3;
    while (*p == '\n' || *p == '\r') p++;

    /* Find closing "---" */
    const char *close = strstr(p, "\n---");
    if (!close) close = strstr(p, "\r\n---");
    if (!close) return -1;

    /* Extract frontmatter text */
    size_t fm_len = (size_t)(close - p);
    char *frontmatter = (char *)malloc(fm_len + 1);
    if (!frontmatter) return -1;
    memcpy(frontmatter, p, fm_len);
    frontmatter[fm_len] = '\0';

    memset(info, 0, sizeof(*info));
    strncpy(info->path, filepath, sizeof(info->path) - 1);

    /* Parse line by line (simple key: value, with list support) */
    char *saveptr = NULL;
    char *line = strtok_r(frontmatter, "\n", &saveptr);
    char current_key[64] = "";

    while (line) {
        char *t = trim(line);
        if (t[0] == '\0') { line = strtok_r(NULL, "\n", &saveptr); continue; }

        if (t[0] == '-') {
            /* List item under current key */
            if (strcmp(current_key, "tags") == 0 && info->tag_count < MAX_TAGS) {
                parse_yaml_list_item(t, info->tags[info->tag_count], MAX_TAG_LEN);
                if (info->tags[info->tag_count][0])
                    info->tag_count++;
            } else if (strcmp(current_key, "dependencies") == 0 && info->dep_count < MAX_DEPS) {
                parse_yaml_list_item(t, info->dependencies[info->dep_count], MAX_DEP_LEN);
                if (info->dependencies[info->dep_count][0])
                    info->dep_count++;
            }
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        /* key: value */
        char *colon = strchr(t, ':');
        if (!colon) { line = strtok_r(NULL, "\n", &saveptr); continue; }

        *colon = '\0';
        char *key = trim(t);
        char *val = trim(colon + 1);
        strncpy(current_key, key, sizeof(current_key) - 1);

        if (strcmp(key, "name") == 0) {
            strncpy(info->name, val, sizeof(info->name) - 1);
        } else if (strcmp(key, "description") == 0) {
            /* Strip wrapping quotes */
            if ((val[0] == '"' && val[strlen(val)-1] == '"') ||
                (val[0] == '\'' && val[strlen(val)-1] == '\'')) {
                val[strlen(val)-1] = '\0';
                val++;
            }
            strncpy(info->description, val, sizeof(info->description) - 1);
        } else if (strcmp(key, "version") == 0) {
            strncpy(info->version, val, sizeof(info->version) - 1);
        } else if (strcmp(key, "author") == 0) {
            strncpy(info->author, val, sizeof(info->author) - 1);
        }
        /* tags and dependencies handled via list items above */

        line = strtok_r(NULL, "\n", &saveptr);
    }

    free(frontmatter);

    /* Must have at least name or description to be valid */
    if (info->name[0] == '\0' && info->description[0] == '\0') return -1;
    if (info->name[0] == '\0') {
        /* Derive name from directory */
        const char *bn = strrchr(filepath, '/');
        if (bn) {
            bn++; /* skip slash */
            strncpy(info->name, bn, sizeof(info->name) - 1);
        }
    }
    if (info->version[0] == '\0') {
        strncpy(info->version, "1.0.0", sizeof(info->version) - 1);
    }

    return 0;
}

/* ── Skill discovery ─────────────────────────────────────────────── */

static int json_escape_str(char *buf, size_t bufsz, const char *src) {
    size_t pos = 0;
    while (*src && pos + 6 < bufsz) {
        switch (*src) {
            case '\\': memcpy(buf+pos,"\\\\",2); pos+=2; break;
            case '"':  memcpy(buf+pos,"\\\"",2); pos+=2; break;
            case '\n': memcpy(buf+pos,"\\n",2);  pos+=2; break;
            case '\r': memcpy(buf+pos,"\\r",2);  pos+=2; break;
            case '\t': memcpy(buf+pos,"\\t",2);  pos+=2; break;
            default:   buf[pos++] = *src; break;
        }
        src++;
    }
    buf[pos] = '\0';
    return (int)pos;
}

/* Scan a category directory for skill subdirs containing SKILL.md */
static void scan_category_dir(const char *cat_dir, skill_info_t *skills, int max_skills, int *count) {
    DIR *d = opendir(cat_dir);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) && *count < max_skills) {
        if (de->d_name[0] == '.') continue;
        char skill_md[MAX_PATH];
        snprintf(skill_md, sizeof(skill_md), "%s/%s/SKILL.md", cat_dir, de->d_name);
        struct stat st;
        if (stat(skill_md, &st) == 0) {
            skill_info_t info;
            if (skill_parse(skill_md, &info) == 0) {
                skills[(*count)++] = info;
            }
        }
    }
    closedir(d);
}

int skills_discover(skill_info_t *skills, int max_skills, const char *extra_path) {
    int count = 0;
    char base_dir[512];
    char home[512];

    /* Primary: ~/.slermes/skills/ — use hermes_get_home for compatibility with main binary */
    hermes_get_home(home, sizeof(home));
    snprintf(base_dir, sizeof(base_dir), "%s/skills", home);

    DIR *d = opendir(base_dir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) && count < max_skills) {
            if (de->d_name[0] == '.') continue;
            char cat_dir[MAX_PATH];
            snprintf(cat_dir, sizeof(cat_dir), "%s/%s", base_dir, de->d_name);
            struct stat st;
            if (stat(cat_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
                scan_category_dir(cat_dir, skills, max_skills, &count);
            }
        }
        closedir(d);
    }

    /* Extra path: also scan category subdirs */
    if (extra_path && extra_path[0]) {
        d = opendir(extra_path);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) && count < max_skills) {
                if (de->d_name[0] == '.') continue;
                char cat_dir[MAX_PATH];
                snprintf(cat_dir, sizeof(cat_dir), "%s/%s", extra_path, de->d_name);
                struct stat st;
                if (stat(cat_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
                    scan_category_dir(cat_dir, skills, max_skills, &count);
                }
            }
            closedir(d);
        }
    }

    return count;
}

/* ── Skills endpoint ─────────────────────────────────────────────── */

/* Global buffer for skills JSON — persisted across requests */
static char g_skills_json[65536];
static int  g_skills_count = 0;
static int  g_skills_valid = 0;

void skills_invalidate_cache(void) {
    g_skills_valid = 0;
}

void skills_build_json(int count, skill_info_t *skills) {
    char *p = g_skills_json;
    size_t remain = sizeof(g_skills_json);
    int written;

    written = snprintf(p, remain,
        "{\n"
        "  \"total\": %d,\n"
        "  \"skills\": [", count);
    p += written; remain -= (size_t)written;

    for (int i = 0; i < count && remain > 512; i++) {
        char esc_name[128], esc_desc[1024], esc_version[64], esc_author[128];
        json_escape_str(esc_name, sizeof(esc_name), skills[i].name);
        json_escape_str(esc_desc, sizeof(esc_desc), skills[i].description);
        json_escape_str(esc_version, sizeof(esc_version), skills[i].version);
        json_escape_str(esc_author, sizeof(esc_author), skills[i].author);

        written = snprintf(p, remain,
            "%s{\n"
            "    \"name\": \"%s\",\n"
            "    \"description\": \"%s\",\n"
            "    \"version\": \"%s\",\n"
            "    \"author\": \"%s\",\n"
            "    \"tags\": [",
            i > 0 ? ",\n" : "\n",
            esc_name, esc_desc, esc_version, esc_author);
        p += written; remain -= (size_t)written;

        for (int t = 0; t < skills[i].tag_count && remain > 256; t++) {
            char esc_tag[128];
            json_escape_str(esc_tag, sizeof(esc_tag), skills[i].tags[t]);
            written = snprintf(p, remain, "%s\"%s\"",
                t > 0 ? ", " : "", esc_tag);
            p += written; remain -= (size_t)written;
        }

        written = snprintf(p, remain, "],\n    \"dependencies\": [");
        p += written; remain -= (size_t)written;

        for (int d = 0; d < skills[i].dep_count && remain > 256; d++) {
            char esc_dep[128];
            json_escape_str(esc_dep, sizeof(esc_dep), skills[i].dependencies[d]);
            written = snprintf(p, remain, "%s\"%s\"",
                d > 0 ? ", " : "", esc_dep);
            p += written; remain -= (size_t)written;
        }

        written = snprintf(p, remain, "]\n  }");
        p += written; remain -= (size_t)written;
    }

    written = snprintf(p, remain, "\n  ]\n}");
    p += written; remain -= (size_t)written;

    g_skills_count = count;
    g_skills_valid = 1;
}

/* Called from h_skills handler in web_server.c */
int skills_get_json(char *buf, size_t bufsz) {
    if (!g_skills_valid) {
        skill_info_t skills[MAX_SKILLS];
        int count = skills_discover(skills, MAX_SKILLS,
            "/home/wubu/hermes-agent-dev/skills");
        skills_build_json(count, skills);
    }
    strncpy(buf, g_skills_json, bufsz - 1);
    buf[bufsz - 1] = '\0';
    return g_skills_count;
}
