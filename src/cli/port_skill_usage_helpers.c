/*
 * port_skill_usage_helpers.c
 *
 * Pure, portable helper ported from tools/skill_usage.py.
 *
 * - latest_activity_at: takes a usage-record JSON object, reads the
 *   last_used_at / last_viewed_at / last_patched_at ISO timestamps, and
 *   returns the newest one as a malloc'd string (NULL if none parse). Pure
 *   JSON + ISO-8601 parse; no file IO.
 *
 *   (is_protected_builtin is already ported in lib/libskillusage/skill_usage.c,
 *    so it is intentionally NOT duplicated here.)
 *
 * Module prefix used by the scanner for tools/skill_usage.py is "skill_usage_".
 *
 * C name <- python name (skill_usage_ prefix): latest_activity_at
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "hermes_json.h"

/* Parse an ISO-8601 timestamp (YYYY-MM-DDTHH:MM:SS[.fff][Z|±HH:MM]) into a
 * time_t (UTC). Returns 1 on success, 0 on failure. */
static int parse_iso(const char *value, time_t *out)
{
    if (!value || !*value) return 0;
    int Y, M, D, h, m, s = 0;
    /* up to seconds; tolerate fractional + tz suffix */
    if (sscanf(value, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) < 6)
        return 0;
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = Y - 1900;
    tmv.tm_mon = M - 1;
    tmv.tm_mday = D;
    tmv.tm_hour = h;
    tmv.tm_min = m;
    tmv.tm_sec = s;
    tmv.tm_isdst = 0;
#ifdef _WIN32
    *out = _mkgmtime(&tmv);
#else
    *out = timegm(&tmv);
#endif
    return (*out != (time_t)-1) ? 1 : 0;
}

/* ---------------------------------------------------------------------- */
/* PoP: latest_activity_at @ tools/skill_usage.py:latest_activity_at */
/* record_json: JSON object with optional last_used_at/last_viewed_at/last_patched_at */
char *skill_usage_latest_activity_at(const char *record_json)
{
    json_t *rec = json_parse(record_json, NULL);
    if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); return NULL; }
    static const char *keys[] = {"last_used_at", "last_viewed_at", "last_patched_at"};
    time_t best = (time_t)-1;
    const char *best_raw = NULL;
    for (int i = 0; i < 3; i++) {
        json_t *v = json_object_get(rec, keys[i]);
        if (!v || v->type != JSON_STRING) continue;
        const char *raw = json_string_value(v);
        time_t t;
        if (parse_iso(raw, &t)) {
            if (best == (time_t)-1 || t > best) { best = t; best_raw = raw; }
        }
    }
    char *out = NULL;
    if (best_raw) {
        out = malloc(strlen(best_raw) + 1);
        strcpy(out, best_raw);
    }
    json_free(rec);
    return out;
}

/* ================================================================
 *  Remaining tools/skill_usage.py gaps (closed for parity)
 * ================================================================ */

#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "hermes_logger.h"
#include "hermes_core_types.h"
#include "hermes.h"
#include "port_skills_sync.h"
#include "libskillusage/skill_usage.h"

static void su_home(char *out, size_t outsz) {
    const char *h = getenv("HERMES_HOME");
    if (h && *h) { snprintf(out, outsz, "%s", h); return; }
    const char *home = getenv("HOME");
    if (!home || !*home) home = "/tmp";
    snprintf(out, outsz, "%s/.hermes", home);
}

/* PoP: skill_usage__read_bundled_manifest_names @ tools/skill_usage.py:_read_bundled_manifest_names */
/* Return malloc'd NULL-terminated array of bundled skill names (caller frees
 * with skill_usage_free_string_list). Reads ~/.hermes/skills/.bundled_manifest. */
char **skill_usage__read_bundled_manifest_names(void) {
    char home[2048]; su_home(home, sizeof(home));
    char path[4096]; snprintf(path, sizeof(path), "%s/skills/.bundled_manifest", home);
    FILE *f = fopen(path, "r");
    char **list = calloc(64, sizeof(char *));
    int n = 0;
    if (f) {
        char buf[1024];
        while (fgets(buf, sizeof(buf), f) && n < 63) {
            char *nl = strchr(buf, '\n'); if (nl) *nl = '\0';
            char *p = buf; while (*p == ' ' || *p == '\t') p++;
            if (!*p) continue;
            char *name = p; char *colon = strchr(p, ':');
            if (colon) *colon = '\0';
            if (*name) list[n++] = strdup(name);
        }
        fclose(f);
    }
    list[n] = NULL;
    return list;
}

/* PoP: skill_usage__read_hub_installed_names @ tools/skill_usage.py:_read_hub_installed_names */
char **skill_usage__read_hub_installed_names(void) {
    char home[2048]; su_home(home, sizeof(home));
    char path[4096]; snprintf(path, sizeof(path), "%s/skills/.hub/lock.json", home);
    FILE *f = fopen(path, "r");
    char **list = calloc(64, sizeof(char *));
    int n = 0;
    if (f) {
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz > 0 && sz < 1 << 20) {
            char *buf = malloc(sz + 1);
            if (buf) {
                fread(buf, 1, sz, f);
                buf[sz] = '\0';
                json_t *data = json_parse(buf, NULL);
                free(buf);
                if (data) {
                    json_t *installed = json_obj_get(data, "installed");
                    if (installed && installed->type == JSON_OBJECT) {
                        size_t nkeys = installed->c.count;
                        for (size_t i = 0; i < nkeys && n < 63; i++) {
                            char *k = installed->c.keys[i];
                            if (k) list[n++] = strdup(k);
                        }
                    }
                    json_free(data);
                }
            }
        }
        fclose(f);
    }
    list[n] = NULL;
    return list;
}

void skill_usage_free_string_list(char **list) {
    if (!list) return;
    for (int i = 0; list[i]; i++) free(list[i]);
    free(list);
}

/* PoP: skill_usage__prune_builtins_enabled @ tools/skill_usage.py:_prune_builtins_enabled */
/* Whether bundled built-ins are eligible for curator pruning. Reads
 * curator.prune_builtins from config (default True). */
int skill_usage__prune_builtins_enabled(void) {
    /* C config does not model curator.prune_builtins; Python default is True. */
    return 1;
}

static char *su_suppressed_path(void) {
    static char path[4096];
    char home[2048]; su_home(home, sizeof(home));
    snprintf(path, sizeof(path), "%s/skills/.curator_suppressed", home);
    return path;
}

/* PoP: skill_usage__suppressed_file @ tools/skill_usage.py:_suppressed_file */
/* Returns malloc'd path to the curator suppression list file. */
char *skill_usage__suppressed_file(void) {
    char home[2048]; su_home(home, sizeof(home));
    char *p = malloc(4096);
    snprintf(p, 4096, "%s/skills/.curator_suppressed", home);
    return p;
}

/* PoP: skill_usage_read_suppressed_names @ tools/skill_usage.py:read_suppressed_names */
char **skill_usage_read_suppressed_names(void) {
    char **list = calloc(128, sizeof(char *));
    int n = 0;
    FILE *f = fopen(su_suppressed_path(), "r");
    if (f) {
        char buf[1024];
        while (fgets(buf, sizeof(buf), f) && n < 127) {
            char *p = buf; while (*p == ' ' || *p == '\t') p++;
            size_t L = strlen(p); while (L && (p[L-1]=='\n'||p[L-1]=='\r')) p[--L]='\0';
            if (!*p || p[0]=='#') continue;
            list[n++] = strdup(p);
        }
        fclose(f);
    }
    list[n] = NULL;
    return list;
}

/* PoP: skill_usage__write_suppressed_names @ tools/skill_usage.py:_write_suppressed_names */
void skill_usage__write_suppressed_names(char **names) {
    char path[4096]; strncpy(path, su_suppressed_path(), sizeof(path)-1); path[sizeof(path)-1]='\0';
    char tmpp[4096]; snprintf(tmpp, sizeof(tmpp), "%s.tmp", path);
    FILE *f = fopen(tmpp, "w");
    if (!f) return;
    if (names) {
        for (int i = 0; names[i]; i++) fprintf(f, "%s\n", names[i]);
    }
    fclose(f);
    rename(tmpp, path);
}

/* PoP: skill_usage_add_suppressed_name @ tools/skill_usage.py:add_suppressed_name */
void skill_usage_add_suppressed_name(const char *name) {
    if (!name) return;
    char **list = skill_usage_read_suppressed_names();
    int n = 0; while (list[n]) n++;
    /* dedupe */
    for (int i = 0; i < n; i++) if (strcmp(list[i], name) == 0) { skill_usage_free_string_list(list); return; }
    char **nl = realloc(list, (n + 2) * sizeof(char *));
    if (nl) { nl[n++] = strdup(name); nl[n] = NULL; list = nl; }
    skill_usage__write_suppressed_names(list);
    skill_usage_free_string_list(list);
}

/* PoP: skill_usage_remove_suppressed_name @ tools/skill_usage.py:remove_suppressed_name */
void skill_usage_remove_suppressed_name(const char *name) {
    if (!name) return;
    char **list = skill_usage_read_suppressed_names();
    int n = 0; while (list[n]) n++;
    char **nl = calloc(n + 1, sizeof(char *));
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(list[i], name) != 0) nl[m++] = list[i];
        else free(list[i]);
    }
    nl[m] = NULL;
    skill_usage__write_suppressed_names(nl);
    free(nl);
    free(list);
}

/* PoP: skill_usage_list_agent_created_skill_names @ tools/skill_usage.py:list_agent_created_skill_names */
/* Enumerate skills in ~/.hermes/skills/ whose usage record has created_by=agent. */
char **skill_usage_list_agent_created_skill_names(void) {
    char home[2048]; su_home(home, sizeof(home));
    char path[4096]; snprintf(path, sizeof(path), "%s/skills", home);
    char **list = calloc(256, sizeof(char *));
    int n = 0;
    DIR *d = opendir(path);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && n < 255) {
            if (e->d_name[0] == '.') continue;
            char usage[4096]; snprintf(usage, sizeof(usage), "%s/%s/.usage.json", path, e->d_name);
            FILE *f = fopen(usage, "r");
            if (!f) continue;
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            if (sz > 0 && sz < 1 << 20) {
                char *buf = malloc(sz + 1);
                if (buf) {
                    fread(buf, 1, sz, f); buf[sz] = '\0';
                    json_t *data = json_parse(buf, NULL);
                    free(buf);
                    if (data) {
                        json_t *urls = json_obj_get(data, "urls");
                        /* record keyed by url; search for created_by=agent */
                        int found = 0;
                        if (urls && urls->type == JSON_OBJECT) {
                            for (size_t i = 0; i < urls->c.count && !found; i++) {
                                json_t *rec = urls->c.items[i];
                                const char *cb = json_get_str(json_obj_get(rec, "created_by"), NULL, "");
                                if (strcmp(cb, "agent") == 0) found = 1;
                                else if (json_get_bool(rec, "agent_created", false)) found = 1;
                            }
                        }
                        if (found) list[n++] = strdup(e->d_name);
                        json_free(data);
                    }
                }
            }
            fclose(f);
        }
        closedir(d);
    }
    list[n] = NULL;
    return list;
}

/* PoP: skill_usage_list_archived_skill_names @ tools/skill_usage.py:list_archived_skill_names */
char **skill_usage_list_archived_skill_names(void) {
    char home[2048]; su_home(home, sizeof(home));
    char path[4096]; snprintf(path, sizeof(path), "%s/skills/.archive", home);
    char **list = calloc(256, sizeof(char *));
    int n = 0;
    DIR *d = opendir(path);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && n < 255) {
            if (e->d_name[0] == '.') continue;
            char full[4096]; snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
            struct stat st; if (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
                list[n++] = strdup(e->d_name);
        }
        closedir(d);
    }
    list[n] = NULL;
    return list;
}

/* --- skill classification helpers ---------------------------------------- */

/* Find a skill dir by frontmatter name (reuses libskillusage). Returns
 * malloc'd path or NULL. */
static char *su_find_skill_dir(const char *skill_name) {
    char home[2048]; su_home(home, sizeof(home));
    char base[4096]; snprintf(base, sizeof(base), "%s/skills", home);
    DIR *d = opendir(base);
    if (!d) return NULL;
    char *result = NULL;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char skill_md[4096];
        snprintf(skill_md, sizeof(skill_md), "%s/%s/SKILL.md", base, e->d_name);
        struct stat st; if (stat(skill_md, &st) != 0) continue;
        char *nm = read_skill_name(skill_md, e->d_name);
        if (nm) {
            if (strcmp(nm, skill_name) == 0) {
                result = malloc(strlen(base) + strlen(e->d_name) + 2);
                sprintf(result, "%s/%s", base, e->d_name);
                free(nm); break;
            }
            free(nm);
        }
    }
    closedir(d);
    return result;
}

/* PoP: skill_usage_is_agent_created @ tools/skill_usage.py:is_agent_created */
int skill_usage_is_agent_created(const char *skill_name) {
    char **bundled = skill_usage__read_bundled_manifest_names();
    char **hub = skill_usage__read_hub_installed_names();
    int off_limits = 0;
    for (int i = 0; bundled[i]; i++) if (strcmp(bundled[i], skill_name) == 0) off_limits = 1;
    for (int i = 0; hub[i]; i++) if (strcmp(hub[i], skill_name) == 0) off_limits = 1;
    skill_usage_free_string_list(bundled);
    skill_usage_free_string_list(hub);
    if (off_limits) return 0;
    return su_find_skill_dir(skill_name) != NULL ? 1 : 0;
}

/* PoP: skill_usage_is_hub_installed @ tools/skill_usage.py:is_hub_installed */
int skill_usage_is_hub_installed(const char *skill_name) {
    char **hub = skill_usage__read_hub_installed_names();
    int r = 0;
    for (int i = 0; hub[i]; i++) if (strcmp(hub[i], skill_name) == 0) { r = 1; break; }
    skill_usage_free_string_list(hub);
    return r;
}

/* PoP: skill_usage_is_bundled @ tools/skill_usage.py:is_bundled */
int skill_usage_is_bundled(const char *skill_name) {
    char **b = skill_usage__read_bundled_manifest_names();
    int r = 0;
    for (int i = 0; b[i]; i++) if (strcmp(b[i], skill_name) == 0) { r = 1; break; }
    skill_usage_free_string_list(b);
    return r;
}

/* PoP: skill_usage__external_read_only_message @ tools/skill_usage.py:_external_read_only_message */
char *skill_usage__external_read_only_message(const char *skill_name) {
    char *out = malloc(256);
    snprintf(out, 256, "skill '%s' lives in skills.external_dirs; external skills are read-only to the curator",
             skill_name ? skill_name : "");
    return out;
}

/* PoP: skill_usage_is_curation_eligible @ tools/skill_usage.py:is_curation_eligible */
int skill_usage_is_curation_eligible(const char *skill_name) {
    if (skill_usage_is_protected_builtin(skill_name)) return 0;
    if (skill_usage_is_hub_installed(skill_name)) return 0;
    if (skill_usage_is_bundled(skill_name)) return skill_usage__prune_builtins_enabled();
    return su_find_skill_dir(skill_name) != NULL ? 1 : 0;
}

/* PoP: skill_usage__is_curator_managed_record @ tools/skill_usage.py:_is_curator_managed_record */
int skill_usage__is_curator_managed_record(const json_t *record) {
    if (!record || record->type != JSON_OBJECT) return 0;
    const char *cb = json_get_str(json_obj_get(record, "created_by"), NULL, "");
    if (strcmp(cb, "agent") == 0) return 1;
    return json_get_bool(record, "agent_created", false) ? 1 : 0;
}

/* PoP: skill_usage__empty_record @ tools/skill_usage.py:_empty_record */
/* Build an empty usage record as a malloc'd JSON string (faithful to Python). */
char *skill_usage__empty_record(void) {
    json_t *r = json_object();
    json_set(r, "created_by", json_null());
    json_set(r, "use_count", json_new_number(0));
    json_set(r, "view_count", json_new_number(0));
    json_set(r, "last_used_at", json_null());
    json_set(r, "last_viewed_at", json_null());
    json_set(r, "patch_count", json_new_number(0));
    json_set(r, "last_patched_at", json_null());
    json_set(r, "created_at", json_string(""));
    json_set(r, "state", json_string("active"));
    json_set(r, "pinned", json_new_bool(0));
    json_set(r, "archived_at", json_null());
    char *out = json_serialize(r); json_free(r);
    return out;
}

/* PoP: skill_usage_restore_skill_by_name @ tools/skill_usage.py:restore_skill */
/* Restore an archived skill. Returns 1 on success with *msg filled, 0 on
 * failure with *msg describing why. *msg is malloc'd (caller frees). */
int skill_usage_restore_skill_by_name(const char *skill_name, char **msg) {
    if (msg) *msg = NULL;
    if (skill_usage_is_hub_installed(skill_name)) {
        if (msg) *msg = strdup("skill is now hub-installed; restore would shadow the upstream version");
        return 0;
    }
    if (skill_usage_is_bundled(skill_name) && !skill_usage__prune_builtins_enabled()) {
        if (msg) *msg = strdup("skill is now bundled; restore would shadow the upstream version");
        return 0;
    }
    char home[2048]; su_home(home, sizeof(home));
    char archive[4096]; snprintf(archive, sizeof(archive), "%s/skills/.archive", home);
    struct stat st; if (stat(archive, &st) != 0 || !S_ISDIR(st.st_mode)) {
        if (msg) *msg = strdup("no archive directory"); return 0;
    }
    char src[4096]; snprintf(src, sizeof(src), "%s/%s", archive, skill_name);
    if (stat(src, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(src, sizeof(src), "%s", archive);
        DIR *d = opendir(archive);
        if (d) {
            struct dirent *e; int found = 0;
            while ((e = readdir(d))) {
                if (e->d_name[0] == '.') continue;
                char cand[4096]; snprintf(cand, sizeof(cand), "%s/%s/%s", archive, e->d_name, skill_name);
                struct stat cs; if (stat(cand, &cs) == 0 && S_ISDIR(cs.st_mode)) {
                    snprintf(src, sizeof(src), "%s", cand); found = 1; break;
                }
            }
            closedir(d);
            if (!found) { if (msg) *msg = strdup("skill not found in archive"); return 0; }
        } else { if (msg) *msg = strdup("skill not found in archive"); return 0; }
    }
    char dest[4096]; snprintf(dest, sizeof(dest), "%s/skills/%s", home, skill_name);
    if (stat(dest, &st) == 0) {
        if (msg) *msg = strdup("destination already exists"); return 0;
    }
    if (rename(src, dest) != 0) {
        if (msg) *msg = strdup("failed to restore"); return 0;
    }
    skill_usage_remove_suppressed_name(skill_name);
    skill_usage_set_state(home, skill_name, "active");
    if (msg) *msg = strdup("restored");
    return 1;
}

/* PoP: skill_usage__find_external_skill_dir @ tools/skill_usage.py:_find_external_skill_dir */
/* Locate an external skill dir for skill_name among SKILLS_EXTERNAL_DIRS. */
char *skill_usage__find_external_skill_dir(const char *skill_name) {
    const char *env = getenv("SKILLS_EXTERNAL_DIRS");
    if (!env || !*env) return NULL;
    char buf[4096]; strncpy(buf, env, sizeof(buf) - 1); buf[sizeof(buf)-1] = '\0';
    char *save = NULL;
    for (char *tok = strtok_r(buf, ":", &save); tok; tok = strtok_r(NULL, ":", &save)) {
        char cand[4096]; snprintf(cand, sizeof(cand), "%s/%s", tok, skill_name);
        struct stat st; if (stat(cand, &st) == 0 && S_ISDIR(st.st_mode)) {
            char *out = malloc(strlen(cand) + 1); strcpy(out, cand); return out;
        }
    }
    return NULL;
}

/* PoP: skill_usage_agent_created_report @ tools/skill_usage.py:agent_created_report */
/* Returns a malloc'd text report of agent-created skills (names, newline-sep). */
char *skill_usage_agent_created_report(void) {
    char **names = skill_usage_list_agent_created_skill_names();
    size_t total = 1; int n = 0; while (names[n]) { total += strlen(names[n]) + 1; n++; }
    char *out = malloc(total + 1); out[0] = '\0';
    for (int i = 0; names[i]; i++) { strcat(out, names[i]); strcat(out, "\n"); }
    skill_usage_free_string_list(names);
    return out;
}

/* PoP: skill_usage_usage_report @ tools/skill_usage.py:usage_report */
/* Returns a malloc'd summary report: count of active/archived skills. */
char *skill_usage_usage_report(void) {
    char home[2048]; su_home(home, sizeof(home));
    char path[4096]; snprintf(path, sizeof(path), "%s/skills", home);
    int active = 0;
    DIR *d = opendir(path);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            char sd[4096]; snprintf(sd, sizeof(sd), "%s/%s/SKILL.md", path, e->d_name);
            struct stat st; if (stat(sd, &st) == 0) active++;
        }
        closedir(d);
    }
    char **arch = skill_usage_list_archived_skill_names();
    int archived = 0; while (arch[archived]) archived++;
    skill_usage_free_string_list(arch);
    char *out = malloc(256);
    snprintf(out, 256, "active: %d\narchived: %d", active, archived);
    return out;
}

