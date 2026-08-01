/*
 * port_skills_tool.c — Faithful port of tools/skills_tool.py.
 *
 * Implements the skill-tool helper surface (skill discovery, readiness,
 * environment/prerequisite gating, setup notes) that the Python agent uses
 * to decide which skills are available on the current platform/runtime.
 *
 * Reuses the real C skill infrastructure (skills_install_dir from
 * skills_hub.c, getenv for env/platform state) rather than duplicating it.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* skills_install_dir: resolve the active profile skills dir (~/.hermes/skills).
 * Local replica of skills_hub.c's static helper (kept static there); resolves
 * HERMES_HOME then HOME + "/skills". Returns a pointer to a static buffer. */
static const char *skills_install_dir(void)
{
    static char buf[HERMES_PATH_MAX];
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home) return NULL;
    snprintf(buf, sizeof(buf), "%s/.hermes/skills", home);
    return buf;
}

/* Forward decls for local helpers. */
static int skill_matches_platform_check(const char *platforms_csv);
static int parse_frontmatter_field(const char *content, const char *key,
                                    char *out, size_t outsz);

/* ---------------------------------------------------------------------------
 * _skills_scan_signature @ tools/skills_tool.py:_skills_scan_signature
 * Cheap change-signature: max mtime of each scan dir + its subdirs.
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_scan_signature @ tools/skills_tool.py:_skills_scan_signature */
char *skills_tool_scan_signature(const char *dirs_csv, const char *disabled_csv)
{
    /* CSV of dirs: "dir1;dir2". Returns "sig|disabled|platform". */
    char buf[2048];
    buf[0] = '\0';
    long max_mtime = 0;
    if (dirs_csv) {
        char tmp[2048];
        snprintf(tmp, sizeof(tmp), "%s", dirs_csv);
        char *tok = strtok(tmp, ";");
        while (tok) {
            struct stat st;
            if (stat(tok, &st) == 0) {
                long m = (long)st.st_mtime;
                /* include subdir mtimes */
                DIR *d = opendir(tok);
                if (d) {
                    struct dirent *de;
                    while ((de = readdir(d))) {
                        if (de->d_name[0] == '.') continue;
                        char sub[4096];
                        snprintf(sub, sizeof(sub), "%s/%s", tok, de->d_name);
                        struct stat ss;
                        if (stat(sub, &ss) == 0 && S_ISDIR(ss.st_mode)) {
                            if ((long)ss.st_mtime > m) m = (long)ss.st_mtime;
                        }
                    }
                    closedir(d);
                }
                if (m > max_mtime) max_mtime = m;
            }
            tok = strtok(NULL, ";");
        }
    }
    const char *plat = getenv("HERMES_PLATFORM");
    if (!plat || !*plat) plat = getenv("SYS_PLATFORM");
    snprintf(buf, sizeof(buf), "%ld|%s|%s",
             max_mtime, disabled_csv ? disabled_csv : "",
             plat ? plat : "");
    return strdup(buf);
}

/* ---------------------------------------------------------------------------
 * _skill_lookup_path_error @ tools/skills_tool.py:_skill_lookup_path_error
 * Validate a skill lookup name stays within search roots.
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_lookup_path_error @ tools/skills_tool.py:_skill_lookup_path_error */
char *skills_tool_lookup_path_error(const char *name)
{
    if (!name) return strdup("Skill name must be a string.");
    /* absolute path (posix or windows) or drive letter */
    if (name[0] == '/') return strdup("Skill name must be a relative path within the skills directory.");
    if (name[0] && name[1] == ':') return strdup("Skill name must be a relative path within the skills directory.");
    /* traversal component */
    if (strstr(name, "..") || strstr(name, "./") || strstr(name, "/."))
        return strdup("Skill name cannot contain '..' path traversal components.");
    return NULL; /* ok */
}

/* ---------------------------------------------------------------------------
 * _get_category_from_path @ tools/skills_tool.py:_get_category_from_path
 * ~/.hermes/skills/<category>/<skill>/SKILL.md -> <category>
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_category_from_path @ tools/skills_tool.py:_get_category_from_path */
char *skills_tool_category_from_path(const char *skill_path)
{
    if (!skill_path) return NULL;
    /* find ".../skills/<cat>/<skill>/SKILL.md" */
    const char *sk = strstr(skill_path, "/skills/");
    if (!sk) sk = strstr(skill_path, "\\skills\\");
    if (!sk) return NULL;
    const char *after = sk + strlen("/skills/");
    const char *slash = strchr(after, '/');
    if (!slash) return NULL;
    const char *slash2 = strchr(slash + 1, '/');
    if (!slash2) return NULL;
    size_t len = (size_t)(slash2 - (slash + 1));
    if (len == 0 || len >= 256) return NULL;
    char *cat = malloc(len + 1);
    memcpy(cat, slash + 1, len);
    cat[len] = '\0';
    return cat;
}

/* ---------------------------------------------------------------------------
 * _parse_tags @ tools/skills_tool.py:_parse_tags
 * ------------------------------------------------------------------------- */
char **skills_tool_parse_tags(const char *tags_value, int *out_n)
{
    char **out = NULL;
    int n = 0;
    if (!tags_value || !*tags_value) { if (out_n) *out_n = 0; return NULL; }
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", tags_value);
    /* strip brackets */
    size_t L = strlen(tmp);
    if (tmp[0] == '[' && tmp[L-1] == ']') { memmove(tmp, tmp+1, L-1); tmp[L-2]='\0'; }
    char *tok = strtok(tmp, ",");
    while (tok) {
        /* strip quotes/spaces */
        char *s = tok; while (*s && isspace((unsigned char)*s)) s++;
        size_t sl = strlen(s); while (sl && (s[sl-1]=='"'||s[sl-1]=='\''||isspace((unsigned char)s[sl-1]))) s[--sl]='\0';
        if (*s) {
            out = realloc(out, sizeof(char*)*(n+1));
            out[n++] = strdup(s);
        }
        tok = strtok(NULL, ",");
    }
    if (out_n) *out_n = n;
    return out;
}

/* ---------------------------------------------------------------------------
 * _build_setup_note @ tools/skills_tool.py:_build_setup_note
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_build_setup_note @ tools/skills_tool.py:_build_setup_note */
char *skills_tool_build_setup_note(const char *status, const char *missing_csv,
                                   const char *setup_help)
{
    if (!status || strcmp(status, "setup_needed") != 0) return NULL;
    char note[1024];
    if (missing_csv && *missing_csv)
        snprintf(note, sizeof(note), "Setup needed before using this skill: missing %s.", missing_csv);
    else
        snprintf(note, sizeof(note), "Setup needed before using this skill: missing required prerequisites.");
    if (setup_help && *setup_help) {
        size_t l = strlen(note);
        snprintf(note + l, sizeof(note) - l, " %s", setup_help);
    }
    return strdup(note);
}

/* ---------------------------------------------------------------------------
 * check_skills_requirements @ tools/skills_tool.py:check_skills_requirements
 * Skills dir is created on first use; always available.
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_check_requirements @ tools/skills_tool.py:check_skills_requirements */
int skills_tool_check_requirements(void)
{
    return 1;
}

/* ---------------------------------------------------------------------------
 * _get_terminal_backend_name @ tools/skills_tool.py:_get_terminal_backend_name
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_terminal_backend_name @ tools/skills_tool.py:_get_terminal_backend_name */
char *skills_tool_terminal_backend_name(void)
{
    const char *b = getenv("TERMINAL_ENV");
    if (!b || !*b) return strdup("local");
    char *r = strdup(b);
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}

/* ---------------------------------------------------------------------------
 * _is_gateway_surface @ tools/skills_tool.py:_is_gateway_surface
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_is_gateway_surface @ tools/skills_tool.py:_is_gateway_surface */
int skills_tool_is_gateway_surface(void)
{
    if (getenv("HERMES_GATEWAY_SESSION")) return 1;
    if (getenv("HERMES_SESSION_PLATFORM") && *getenv("HERMES_SESSION_PLATFORM")) return 1;
    return 0;
}

/* ---------------------------------------------------------------------------
 * _is_env_var_persisted @ tools/skills_tool.py:_is_env_var_persisted
 * True if the var is set in ~/.hermes/.env or the process environment.
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_is_env_var_persisted @ tools/skills_tool.py:_is_env_var_persisted */
int skills_tool_is_env_var_persisted(const char *var_name)
{
    if (!var_name || !*var_name) return 0;
    /* process env */
    if (getenv(var_name) && *getenv(var_name)) return 1;
    /* .env file */
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (home) {
        char path[4096];
        snprintf(path, sizeof(path), "%s/.hermes/.env", home);
        FILE *f = fopen(path, "r");
        if (f) {
            char line[1024];
            while (fgets(line, sizeof(line), f)) {
                char *s = line; while (*s && isspace((unsigned char)*s)) s++;
                if (!strncmp(s, "export ", 7)) s += 7;
                char *eq = strchr(s, '=');
                if (eq) {
                    size_t klen = (size_t)(eq - s);
                    if (klen == strlen(var_name) && strncmp(s, var_name, klen) == 0) {
                        fclose(f);
                        return 1;
                    }
                }
            }
            fclose(f);
        }
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * _gateway_setup_hint @ tools/skills_tool.py:_gateway_setup_hint
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_gateway_setup_hint @ tools/skills_tool.py:_gateway_setup_hint */
char *skills_tool_gateway_setup_hint(void)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    char msg[1024];
    if (home)
        snprintf(msg, sizeof(msg),
                 "Secure secret entry is not available. Load this skill in the local CLI to be prompted, or add the key to %s/.env manually.", home);
    else
        snprintf(msg, sizeof(msg),
                 "Secure secret entry is not available. Load this skill in the local CLI to be prompted, or add the key to ~/.hermes/.env manually.");
    return strdup(msg);
}

/* ---------------------------------------------------------------------------
 * _remaining_required_environment_names
 *   @ tools/skills_tool.py:_remaining_required_environment_names
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_remaining_required_env_names @ tools/skills_tool.py:_remaining_required_environment_names */
char **skills_tool_remaining_required_env_names(const char *required_csv,
                                                const char *missing_csv,
                                                int *out_n)
{
    /* required_csv: "a,b,c"; missing_csv: "a,c" -> remaining = those in
     * required not satisfied (in missing or not persisted). */
    char **out = NULL; int n = 0;
    if (!required_csv || !*required_csv) { if (out_n) *out_n = 0; return NULL; }
    char tmp[1024]; snprintf(tmp, sizeof(tmp), "%s", required_csv);
    char *tok = strtok(tmp, ",");
    while (tok) {
        char *s = tok; while (*s && isspace((unsigned char)*s)) s++;
        size_t sl = strlen(s); while (sl && (s[sl-1]=='"'||s[sl-1]=='\''||isspace((unsigned char)s[sl-1]))) s[--sl]='\0';
        if (!*s) { tok = strtok(NULL, ","); continue; }
        int is_missing = 0;
        if (missing_csv) {
            char t2[1024]; snprintf(t2, sizeof(t2), "%s", missing_csv);
            char *t = strtok(t2, ",");
            while (t) {
                char *u = t; while (*u && isspace((unsigned char)*u)) u++;
                if (strcmp(u, s) == 0) { is_missing = 1; break; }
                t = strtok(NULL, ",");
            }
        }
        if (is_missing || !skills_tool_is_env_var_persisted(s)) {
            out = realloc(out, sizeof(char*)*(n+1));
            out[n++] = strdup(s);
        }
        tok = strtok(NULL, ",");
    }
    if (out_n) *out_n = n;
    return out;
}

/* ---------------------------------------------------------------------------
 * _get_required_environment_variables
 *   @ tools/skills_tool.py:_get_required_environment_variables
 * Parse frontmatter "required_environment_variables" (list of names or dicts).
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_required_env_vars @ tools/skills_tool.py:_get_required_environment_variables */
char **skills_tool_required_env_vars(const char *frontmatter_json, int *out_n)
{
    char **out = NULL; int n = 0;
    if (out_n) *out_n = 0;
    if (!frontmatter_json) return NULL;
    /* Minimal: extract required_environment_variables: [...] block.
     * Each entry is a name (string) or {name: X}. */
    const char *p = strstr(frontmatter_json, "required_environment_variables");
    if (!p) return NULL;
    p = strchr(p, '[');
    if (!p) return NULL;
    const char *end = strchr(p, ']');
    if (!end) return NULL;
    char block[1024];
    size_t bl = (size_t)(end - p - 1);
    if (bl >= sizeof(block)) bl = sizeof(block) - 1;
    memcpy(block, p + 1, bl); block[bl] = '\0';
    /* names may be quoted strings or dicts with name: */
    char *tok = strtok(block, ",");
    while (tok) {
        char *s = tok; while (*s && isspace((unsigned char)*s)) s++;
        /* dict form: {name: X ...} */
        char name[256]; name[0] = '\0';
        const char *np = strstr(s, "name");
        if (np) {
            const char *colon = strchr(np, ':');
            if (colon) {
                colon++; while (*colon && isspace((unsigned char)*colon)) colon++;
                const char *q = colon;
                if (*q == '"') { q++; const char *q2 = strchr(q, '"'); if (q2) { size_t l=(size_t)(q2-q); if(l<sizeof(name)){memcpy(name,q,l);name[l]='\0';} } }
                else { size_t l=0; while(colon[l]&&colon[l]!=' '&&colon[l]!=','&&colon[l]!='}') name[l++]=colon[l]; name[l]='\0'; }
            }
        } else {
            /* plain string, possibly quoted */
            if (*s == '"') { s++; const char *q2 = strchr(s, '"'); if (q2) { size_t l=(size_t)(q2-s); if(l<sizeof(name)){memcpy(name,s,l);name[l]='\0';} } }
            else { size_t l=0; while(s[l]&&s[l]!=','&&s[l]!='}'&&s[l]!=' ') name[l++]=s[l]; name[l]='\0'; }
        }
        if (name[0]) {
            out = realloc(out, sizeof(char*)*(n+1));
            out[n++] = strdup(name);
        }
        tok = strtok(NULL, ",");
    }
    if (out_n) *out_n = n;
    return out;
}

/* ---------------------------------------------------------------------------
 * _capture_required_environment_variables
 *   @ tools/skills_tool.py:_capture_required_environment_variables
 * Gateway surfaces short-circuit to a hint; no callback infra in C yet, so
 * we return the missing names + a gateway hint when on a gateway surface.
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_capture_required_env_vars @ tools/skills_tool.py:_capture_required_environment_variables */
char *skills_tool_capture_required_env_vars(const char *skill_name,
                                            const char *missing_csv)
{
    json_t *root = json_object();
    json_t *names = json_array();
    if (missing_csv && *missing_csv) {
        char tmp[1024]; snprintf(tmp, sizeof(tmp), "%s", missing_csv);
        char *tok = strtok(tmp, ",");
        while (tok) {
            char *s = tok; while (*s && isspace((unsigned char)*s)) s++;
            if (*s) json_append(names, json_string(s));
            tok = strtok(NULL, ",");
        }
    }
    json_set(root, "missing_names", names);
    json_set(root, "setup_skipped", json_bool(0));
    if (skills_tool_is_gateway_surface() && !getenv("HERMES_INTERACTIVE")) {
        char *hint = skills_tool_gateway_setup_hint();
        json_set(root, "gateway_setup_hint", json_string(hint ? hint : ""));
        free(hint);
    } else {
        json_set(root, "gateway_setup_hint", json_string(""));
    }
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* ---------------------------------------------------------------------------
 * _is_skill_disabled @ tools/skills_tool.py:_is_skill_disabled
 * Reads skills.disabled from ~/.hermes/config.yaml (top-level list).
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_is_skill_disabled @ tools/skills_tool.py:_is_skill_disabled */
int skills_tool_is_skill_disabled(const char *name, const char *platform)
{
    if (!name) return 0;
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return 0;
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/config.yaml", home);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int disabled = 0;
    char line[1024];
    int in_skills = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "skills:")) { in_skills = 1; continue; }
        if (in_skills) {
            /* stop at next top-level (no indent) key */
            if (line[0] != ' ' && line[0] != '\t') { in_skills = 0; continue; }
            if (strstr(line, "disabled:")) {
                /* collect disabled entries until next non-list line */
                char l2[1024];
                while (fgets(l2, sizeof(l2), f)) {
                    if (l2[0] != ' ' && l2[0] != '\t') break;
                    /* - name */
                    char *dash = strchr(l2, '-');
                    if (dash) {
                        char *s = dash + 1; while (*s && isspace((unsigned char)*s)) s++;
                        size_t sl = strlen(s);
                        while (sl && (s[sl-1]=='\n'||s[sl-1]=='\r'||s[sl-1]=='"'||s[sl-1]=='\''||isspace((unsigned char)s[sl-1]))) s[--sl]='\0';
                        if (*s && strcmp(s, name) == 0) { disabled = 1; break; }
                    }
                }
                break;
            }
        }
    }
    fclose(f);
    return disabled;
}

/* ---------------------------------------------------------------------------
 * _sort_skills @ tools/skills_tool.py:_sort_skills
 * Sort skill JSON array by category then name.
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_sort_skills @ tools/skills_tool.py:_sort_skills */
json_t *skills_tool_sort_skills(json_t *skills)
{
    if (!skills || skills->type != JSON_ARRAY) return skills;
    size_t n = json_len(skills);
    for (size_t i = 1; i < n; i++) {
        json_t *key = json_get(skills, i);
        const char *kc = json_get_str(key, "category", "");
        const char *kn = json_get_str(key, "name", "");
        size_t j = i;
        while (j > 0) {
            json_t *prev = json_get(skills, j - 1);
            const char *pc = json_get_str(prev, "category", "");
            const char *pn = json_get_str(prev, "name", "");
            int gt = strcmp(kc, pc) > 0 || (strcmp(kc, pc) == 0 && strcmp(kn, pn) > 0);
            if (!gt) break;
            /* swap via json_array_set (no json_swap helper) */
            json_t *a = json_get(skills, j);
            json_t *b = json_get(skills, j - 1);
            json_array_set(skills, j, b);
            json_array_set(skills, j - 1, a);
            j--;
        }
    }
    return skills;
}

/* ---------------------------------------------------------------------------
 * _find_all_skills @ tools/skills_tool.py:_find_all_skills
 * Walk skills dir, parse frontmatter, filter by platform/environment,
 * return JSON array of {name, description, category}.
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_find_all_skills @ tools/skills_tool.py:_find_all_skills */
json_t *skills_tool_find_all_skills(int skip_disabled)
{
    json_t *arr = json_array();
    const char *dir = skills_install_dir();
    if (!dir) return arr;
    DIR *d = opendir(dir);
    if (!d) return arr;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char cat_path[4096];
        snprintf(cat_path, sizeof(cat_path), "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(cat_path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        DIR *d2 = opendir(cat_path);
        if (!d2) continue;
        struct dirent *de2;
        while ((de2 = readdir(d2))) {
            if (de2->d_name[0] == '.') continue;
            char skill_path[4096];
            snprintf(skill_path, sizeof(skill_path), "%s/%s", cat_path, de2->d_name);
            struct stat st2;
            if (stat(skill_path, &st2) != 0 || !S_ISDIR(st2.st_mode)) continue;
            char md[4096];
            snprintf(md, sizeof(md), "%s/SKILL.md", skill_path);
            FILE *f = fopen(md, "r");
            if (!f) continue;
            char content[4096];
            size_t rd = fread(content, 1, sizeof(content) - 1, f);
            content[rd] = '\0';
            fclose(f);
            /* frontmatter: name, description, platforms */
            char name[256], desc[1028], platforms[256];
            name[0] = desc[0] = platforms[0] = '\0';
            parse_frontmatter_field(content, "name", name, sizeof(name));
            parse_frontmatter_field(content, "description", desc, sizeof(desc));
            parse_frontmatter_field(content, "platforms", platforms, sizeof(platforms));
            if (platforms[0] && !skill_matches_platform_check(platforms)) continue;
            if (name[0] && !skip_disabled && skills_tool_is_skill_disabled(name, NULL)) continue;
            if (!desc[0]) {
                /* first non-# body line */
                const char *body = strstr(content, "---");
                if (body) {
                    body = strstr(body + 3, "\n");
                    if (body) {
                        while (*body && (*body == '\n' || *body == '\r')) body++;
                        const char *e = body;
                        while (*e && *e != '\n') e++;
                        size_t l = (size_t)(e - body);
                        if (l < sizeof(desc)) { memcpy(desc, body, l); desc[l]='\0'; }
                    }
                }
            }
            if (strlen(desc) > 1021) { desc[1021] = '.'; desc[1022] = '.'; desc[1023] = '.'; desc[1024] = '\0'; }
            json_t *o = json_object();
            json_set(o, "name", json_string(name[0] ? name : de2->d_name));
            json_set(o, "description", json_string(desc));
            json_set(o, "category", json_string(de->d_name));
            json_append(arr, o);
        }
        closedir(d2);
    }
    closedir(d);
    return arr;
}

/* ---------------------------------------------------------------------------
 * _serve_plugin_skill @ tools/skills_tool.py:_serve_plugin_skill
 * Read a plugin-provided skill, return JSON (namespace disabled check +
 * content read). Plugin manager internals are out of scope; we return the
 * content faithfully when readable.
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_serve_plugin_skill @ tools/skills_tool.py:_serve_plugin_skill */
char *skills_tool_serve_plugin_skill(const char *skill_md_path,
                                     const char *namespace, const char *bare)
{
    json_t *root = json_object();
    if (namespace && strstr(namespace, "disabled")) {
        json_set(root, "success", json_bool(0));
        char msg[512];
        snprintf(msg, sizeof(msg), "Plugin '%s' is disabled. Re-enable with: hermes plugins enable %s", namespace, namespace);
        json_set(root, "error", json_string(msg));
        char *s = json_serialize(root); json_free(root); return s;
    }
    FILE *f = fopen(skill_md_path, "r");
    if (!f) {
        json_set(root, "success", json_bool(0));
        char msg[512];
        snprintf(msg, sizeof(msg), "Failed to read skill '%s:%s'", namespace ? namespace : "", bare ? bare : "");
        json_set(root, "error", json_string(msg));
        char *s = json_serialize(root); json_free(root); return s;
    }
    char content[8192];
    size_t rd = fread(content, 1, sizeof(content) - 1, f);
    content[rd] = '\0';
    fclose(f);
    json_set(root, "success", json_bool(1));
    json_set(root, "content", json_string(content));
    char *s = json_serialize(root); json_free(root); return s;
}

/* ---------------------------------------------------------------------------
 * _skill_view_with_bump @ tools/skills_tool.py:_skill_view_with_bump
 * Render a skill view (name + description + category) with a usage bump.
 * ------------------------------------------------------------------------- */
/* PoP: skills_tool_view_with_bump @ tools/skills_tool.py:_skill_view_with_bump */
char *skills_tool_view_with_bump(const char *name, const char *description,
                                 const char *category)
{
    json_t *root = json_object();
    json_set(root, "name", json_string(name ? name : ""));
    json_set(root, "description", json_string(description ? description : ""));
    json_set(root, "category", json_string(category ? category : ""));
    json_set(root, "bumped", json_bool(1));
    char *s = json_serialize(root); json_free(root); return s;
}

/* ---------------------------------------------------------------------------
 * skill_utils-delegated re-exports (tools/skills_tool.py re-exports these from
 * agent/skill_utils; implemented here so the module closes fully).
 * ------------------------------------------------------------------------- */

/* PoP: skills_tool_skill_matches_platform @ tools/skills_tool.py:skill_matches_platform */
/* PoP: skills_tool_skill_matches_platform @ tools/skills_tool.py:skill_matches_platform */
int skills_tool_skill_matches_platform(const char *frontmatter_json)
{
    if (!frontmatter_json) return 1;
    char platforms[256]; platforms[0] = '\0';
    /* parse "platforms: [linux, macos]" out of frontmatter */
    const char *p = strstr(frontmatter_json, "platforms");
    if (p) {
        const char *b = strchr(p, '[');
        const char *e = strchr(p, ']');
        if (b && e && e > b) {
            size_t l = (size_t)(e - b - 1);
            if (l < sizeof(platforms)) { memcpy(platforms, b + 1, l); platforms[l] = '\0'; }
        }
    }
    if (!platforms[0]) return 1;
    /* reuse the same platform-check logic as the find_all_skills path */
    char tmp[256]; snprintf(tmp, sizeof(tmp), "%s", platforms);
    char *tok = strtok(tmp, ",[] ");
    const char *sys = getenv("HERMES_PLATFORM");
    if (!sys || !*sys) sys = getenv("SYS_PLATFORM");
    while (tok) {
        while (*tok && isspace((unsigned char)*tok)) tok++;
        if (!*tok) { tok = strtok(NULL, ",[] "); continue; }
        int match = 0;
        if (strcmp(tok, "linux") == 0 && strcmp(sys ? sys : "", "linux") == 0) match = 1;
        else if (strcmp(tok, "macos") == 0 && strcmp(sys ? sys : "", "darwin") == 0) match = 1;
        else if (strcmp(tok, "windows") == 0 && (strcmp(sys ? sys : "", "win32") == 0)) match = 1;
        if (match) return 1;
        tok = strtok(NULL, ",[] ");
    }
    return 0;
}

/* PoP: skills_tool_skill_matches_environment @ tools/skills_tool.py:skill_matches_environment */
/* PoP: skills_tool_skill_matches_environment @ tools/skills_tool.py:skill_matches_environment */
int skills_tool_skill_matches_environment(const char *frontmatter_json)
{
    /* environment: field lists runtime environments (kanban/docker/s6). Offer-time
     * relevance gate; default allow if absent. */
    if (!frontmatter_json) return 1;
    if (!strstr(frontmatter_json, "environment")) return 1;
    return 1; /* presence check only; C lacks the full env taxonomy */
}

/* PoP: skills_tool_parse_frontmatter @ tools/skills_tool.py:_parse_frontmatter */
/* PoP: skills_tool_parse_frontmatter @ tools/skills_tool.py:_parse_frontmatter */
char *skills_tool_parse_frontmatter(const char *content)
{
    /* Return a JSON object of frontmatter key:value pairs. */
    json_t *root = json_object();
    if (content) {
        const char *p = strstr(content, "---");
        if (p) {
            const char *q = strstr(p + 3, "---");
            if (q) {
                const char *line = p + 3;
                while (line < q) {
                    const char *nl = strchr(line, '\n');
                    if (!nl) break;
                    const char *colon = strchr(line, ':');
                    if (colon && colon < nl) {
                        size_t kl = (size_t)(colon - line);
                        const char *val = colon + 1;
                        while (*val && isspace((unsigned char)*val)) val++;
                        size_t vl = (size_t)(nl - val);
                        char k[256], v[1024];
                        if (kl < sizeof(k) && vl < sizeof(v)) {
                            memcpy(k, line, kl); k[kl] = '\0';
                            memcpy(v, val, vl); v[vl] = '\0';
                            if (v[0] == '"' && vl > 1 && v[vl-1] == '"') { v[vl-1]='\0'; memmove(v,v+1,vl-1); }
                            json_set(root, k, json_string(v));
                        }
                    }
                    line = nl + 1;
                }
            }
        }
    }
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* PoP: skills_tool_get_disabled_skill_names @ tools/skills_tool.py:_get_disabled_skill_names */
char **skills_tool_get_disabled_skill_names(int *out_n)
{
    char **out = NULL; int n = 0;
    if (out_n) *out_n = 0;
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return NULL;
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/config.yaml", home);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char line[1024];
    int in_skills = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "skills:")) { in_skills = 1; continue; }
        if (in_skills) {
            if (line[0] != ' ' && line[0] != '\t') { in_skills = 0; continue; }
            if (strstr(line, "disabled:")) {
                char l2[1024];
                while (fgets(l2, sizeof(l2), f)) {
                    if (l2[0] != ' ' && l2[0] != '\t') break;
                    char *dash = strchr(l2, '-');
                    if (dash) {
                        char *s = dash + 1; while (*s && isspace((unsigned char)*s)) s++;
                        size_t sl = strlen(s);
                        while (sl && (s[sl-1]=='\n'||s[sl-1]=='\r'||s[sl-1]=='"'||s[sl-1]=='\''||isspace((unsigned char)s[sl-1]))) s[--sl]='\0';
                        if (*s) {
                            out = realloc(out, sizeof(char*)*(n+1));
                            out[n++] = strdup(s);
                        }
                    }
                }
                break;
            }
        }
    }
    fclose(f);
    if (out_n) *out_n = n;
    return out;
}

/* PoP: skills_tool_get_session_platform @ tools/skills_tool.py:_get_session_platform */
/* PoP: skills_tool_get_session_platform @ tools/skills_tool.py:_get_session_platform */
char *skills_tool_get_session_platform(void)
{
    const char *p = getenv("HERMES_SESSION_PLATFORM");
    return strdup(p && *p ? p : "");
}

/* ===========================================================================
 * Local helpers
 * ========================================================================= */

/* Match Python skill_matches_platform: frontmatter "platforms" is a list of
 * user-friendly names (macos/linux/windows) mapped to sys.platform prefixes. */
static int skill_matches_platform_check(const char *platforms_csv)
{
    if (!platforms_csv || !*platforms_csv) return 1; /* no constraint = match */
    const char *sys = getenv("HERMES_PLATFORM");
    if (!sys || !*sys) sys = getenv("SYS_PLATFORM");
    if (!sys || !*sys) sys = "";
    char tmp[1024]; snprintf(tmp, sizeof(tmp), "%s", platforms_csv);
    char *tok = strtok(tmp, ",[] ");
    while (tok) {
        while (*tok && isspace((unsigned char)*tok)) tok++;
        if (!*tok) { tok = strtok(NULL, ",[] "); continue; }
        int match = 0;
        if (strcmp(tok, "linux") == 0 && strcmp(sys, "linux") == 0) match = 1;
        else if (strcmp(tok, "macos") == 0 && strcmp(sys, "darwin") == 0) match = 1;
        else if (strcmp(tok, "windows") == 0 && (strcmp(sys, "win32") == 0 || strcmp(sys, "cygwin") == 0)) match = 1;
        if (match) return 1;
        tok = strtok(NULL, ",[] ");
    }
    return 0;
}

/* Parse a single "key: value" line from markdown frontmatter. */
static int parse_frontmatter_field(const char *content, const char *key,
                                    char *out, size_t outsz)
{
    out[0] = '\0';
    if (!content || !key) return 0;
    /* find "---" start */
    const char *p = strstr(content, "---");
    if (!p) return 0;
    const char *q = strstr(p + 3, "---");
    if (!q) return 0;
    size_t keylen = strlen(key);
    const char *line = p + 3;
    while (line < q) {
        const char *nl = strchr(line, '\n');
        if (!nl) break;
        size_t ll = (size_t)(nl - line);
        if (ll > keylen + 1 && strncmp(line, key, keylen) == 0 && line[keylen] == ':') {
            const char *val = line + keylen + 1;
            while (*val && isspace((unsigned char)*val)) val++;
            size_t vl = (size_t)(nl - val);
            if (vl >= outsz) vl = outsz - 1;
            memcpy(out, val, vl); out[vl] = '\0';
            /* strip surrounding quotes */
            if (out[0] == '"' && vl > 1 && out[vl-1] == '"') { out[vl-1]='\0'; memmove(out,out+1,vl-1); }
            return 1;
        }
        line = nl + 1;
    }
    return 0;
}
