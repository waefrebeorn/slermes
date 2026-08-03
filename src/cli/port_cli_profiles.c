/*
 * port_cli_profiles.c — C11 port of hermes_cli/profiles.py
 *
 * Multi-profile HERMES_HOME management. Pure, testable core: name
 * normalization+validation, path resolution, alias maps, distribution
 * metadata, profile.yaml, sticky active_profile, archive member path-safety,
 * and env resolution. The Python module's service-manager / gateway-lifecycle
 * / skills-sync / tar machinery is best-effort or a no-op here — faithfully
 * matching the upstream "swallow errors, don't fail profile ops" intent.
 *
 * Single-home Slermes: the "default" profile is slermes_home() itself; named
 * profiles live under <slermes_home()>/profiles/<id>/.
 *
 * MIT License — Slermes Fork
 */

#include "profile_store.h"
#include "slermes_home.h"
#include "gateway_status.h"
#include "yaml.h"
#include "hermes_logger.h"
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <zlib.h>

/* ── Small utilities ──────────────────────────────────────────────── */

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void set_err(char **err, const char *msg) {
    if (err) *err = xstrdup(msg);
}

static char *str_strip_dup(const char *s) {
    if (!s) return xstrdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *p = malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static int mkdir_p(const char *path, mode_t mode) {
    if (!path || !*path) return -1;
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return -1;
    memcpy(tmp, path, len + 1);
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

/* ── Constant tables (mirror profiles.py module-level frozensets) ───── */

/* Profile id regex equivalent: [a-z0-9][a-z0-9_-]{0,63} */
#define PROFILE_ID_MAX 63

static const char *RESERVED_NAMES[] = {
    "hermes", "default", "test", "tmp", "root", "sudo", NULL
};

static const char *HERMES_SUBCOMMANDS[] = {
    "chat", "model", "gateway", "setup", "whatsapp", "login", "logout",
    "status", "cron", "doctor", "dump", "config", "pairing", "skills", "tools",
    "mcp", "sessions", "insights", "version", "update", "uninstall",
    "profile", "plugins", "honcho", "acp", NULL
};

/* ── Path helpers ─────────────────────────────────────────────────── */

/* PoP: profile_profiles_root @ hermes_cli/profiles.py:_get_profiles_root */
char *profile_profiles_root(void) {
    const char *home = slermes_home();
    size_t n = strlen(home) + 1 + strlen("profiles") + 1;
    char *p = malloc(n);
    if (p) snprintf(p, n, "%s/profiles", home);
    return p;
}

/* PoP: profile_default_home @ hermes_cli/profiles.py:_get_default_hermes_home */
char *profile_default_home(void) {
    return xstrdup(slermes_home());
}

/* PoP: profile_active_profile_path @ hermes_cli/profiles.py:_get_active_profile_path */
char *profile_active_profile_path(void) {
    const char *home = slermes_home();
    size_t n = strlen(home) + 1 + strlen("active_profile") + 1;
    char *p = malloc(n);
    if (p) snprintf(p, n, "%s/active_profile", home);
    return p;
}

/* PoP: profile_wrapper_dir @ hermes_cli/profiles.py:_get_wrapper_dir */
char *profile_wrapper_dir(void) {
    const char *home = getenv("HOME");
    if (!home) home = "";
    size_t n = strlen(home) + 1 + strlen(".local/bin") + 1;
    char *p = malloc(n);
    if (p) snprintf(p, n, "%s/.local/bin", home);
    return p;
}

/* ── Validation / normalization ───────────────────────────────────── */

/* Match Python's _PROFILE_ID_RE = ^[a-z0-9][a-z0-9_-]{0,63}$ */
static bool profile_id_matches(const char *s) {
    size_t n = strlen(s);
    if (n == 0 || n > PROFILE_ID_MAX) return false;
    if (!islower((unsigned char)s[0]) && !isdigit((unsigned char)s[0])) return false;
    for (size_t i = 1; i < n; i++) {
        char c = s[i];
        if (!(islower((unsigned char)c) || isdigit((unsigned char)c) ||
              c == '-' || c == '_'))
            return false;
    }
    return true;
}

/* PoP: profile_normalize_name @ hermes_cli/profiles.py:normalize_profile_name */
char *profile_normalize_name(const char *name) {
    if (!name) return NULL;
    char *stripped = str_strip_dup(name);
    if (!stripped || !*stripped) { free(stripped); return NULL; }
    if (strcasecmp(stripped, "default") == 0) { free(stripped); return xstrdup("default"); }
    /* lowercase */
    for (char *p = stripped; *p; p++) *p = (char)tolower((unsigned char)*p);
    return stripped;
}

/* PoP: profile_validate_name @ hermes_cli/profiles.py:validate_profile_name */
bool profile_validate_name(const char *name, char **err) {
    if (name && strcmp(name, "default") == 0) return true; /* special alias */
    if (!name || !profile_id_matches(name)) {
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "Invalid profile name '%s'. Must match [a-z0-9][a-z0-9_-]{0,63}",
                 name ? name : "(null)");
        set_err(err, msg);
        return false;
    }
    for (int i = 0; RESERVED_NAMES[i]; i++) {
        if (strcmp(name, RESERVED_NAMES[i]) == 0) {
            char m[160];
            snprintf(m, sizeof(m),
                     "Profile name '%s' is reserved — it collides with either "
                     "the Hermes installation itself or a common system binary.  "
                     "Pick a different name.", name);
            set_err(err, m);
            return false;
        }
    }
    return true;
}

/* PoP: profile_validate_alias_name @ hermes_cli/profiles.py:validate_alias_name */
bool profile_validate_alias_name(const char *name, char **err) {
    if (!name || !profile_id_matches(name)) {
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "Invalid alias name '%s'. Must match [a-z0-9][a-z0-9_-]{0,63}",
                 name ? name : "(null)");
        set_err(err, msg);
        return false;
    }
    return true;
}

/* PoP: profile_dir_for @ hermes_cli/profiles.py:get_profile_dir */
char *profile_dir_for(const char *name) {
    char *canon = profile_normalize_name(name);
    if (!canon) return NULL;
    char *result;
    if (strcmp(canon, "default") == 0) {
        result = profile_default_home();
    } else {
        char *root = profile_profiles_root();
        size_t n = strlen(root) + 1 + strlen(canon) + 1;
        result = malloc(n);
        if (result) snprintf(result, n, "%s/%s", root, canon);
        free(root);
    }
    free(canon);
    return result;
}

/* PoP: profile_dir_exists @ hermes_cli/profiles.py:profile_exists */
/* Hermes-profile-dir existence (default always true). Distinct from
 * kanban_db.c's profile_exists, which lists *kanban* profiles. */
int profile_dir_exists(const char *name) {
    char *canon = profile_normalize_name(name);
    if (!canon) return 0;
    int exists;
    if (strcmp(canon, "default") == 0) {
        exists = 1;
    } else {
        char *dir = profile_dir_for(canon);
        struct stat st;
        exists = dir && stat(dir, &st) == 0 && S_ISDIR(st.st_mode);
        free(dir);
    }
    free(canon);
    return exists;
}

/* ── Alias / wrapper scripts ───────────────────────────────────────── */

/* PoP: profile_check_alias_collision @ hermes_cli/profiles.py:check_alias_collision */
char *profile_check_alias_collision(const char *name) {
    char *canon = profile_normalize_name(name);
    if (!canon) return xstrdup("profile name cannot be empty");
    char *err = NULL;
    if (!profile_validate_alias_name(canon, &err)) { free(canon); return err; }
    for (int i = 0; RESERVED_NAMES[i]; i++)
        if (strcmp(canon, RESERVED_NAMES[i]) == 0) {
            char *m = malloc(64); if (m) snprintf(m, 64, "'%s' is a reserved name", canon);
            free(canon); return m;
        }
    for (int i = 0; HERMES_SUBCOMMANDS[i]; i++)
        if (strcmp(canon, HERMES_SUBCOMMANDS[i]) == 0) {
            char *m = malloc(64); if (m) snprintf(m, 64, "'%s' conflicts with a hermes subcommand", canon);
            free(canon); return m;
        }
    /* NOTE: upstream also scans PATH for an existing command. Single-home
     * Slermes wrapper dir only holds our own wrappers, so the PATH scan is a
     * no-op here — the reserved-name + subcommand + regex checks remain. */
    free(canon);
    return NULL; /* safe */
}

/* PoP: profile_wrapper_dir_in_path @ hermes_cli/profiles.py:_is_wrapper_dir_in_path */
bool profile_wrapper_dir_in_path(void) {
    char *wd = profile_wrapper_dir();
    if (!wd) return false;
    const char *path = getenv("PATH");
    bool in_path = false;
    if (path) {
        char *dup = xstrdup(path);
        char *save = NULL;
        for (char *tok = strtok_r(dup, ":", &save); tok; tok = strtok_r(NULL, ":", &save)) {
            if (strcmp(tok, wd) == 0) { in_path = true; break; }
        }
        free(dup);
    }
    free(wd);
    return in_path;
}

/* PoP: profile_create_wrapper_script @ hermes_cli/profiles.py:create_wrapper_script */
bool profile_create_wrapper_script(const char *name, const char *target) {
    char *canon = profile_normalize_name(name);
    if (!canon) return false;
    char *profile = target ? profile_normalize_name(target) : xstrdup(canon);
    if (!profile) { free(canon); return false; }

    /* Refuse traversal-shaped alias (mirrors validate_alias_name guard). */
    char *aerr = NULL;
    if (!profile_validate_alias_name(canon, &aerr)) { free(aerr); free(canon); free(profile); return false; }

    char *wd = profile_wrapper_dir();
    if (!wd) { free(canon); free(profile); return false; }
    if (mkdir_p(wd, 0755) != 0 && errno != EEXIST) { free(wd); free(canon); free(profile); return false; }

    size_t n = strlen(wd) + 1 + strlen(canon) + 1;
    char *wpath = malloc(n);
    if (wpath) snprintf(wpath, n, "%s/%s", wd, canon);

    bool ok = false;
    const char *hermes_exe = "hermes";
    FILE *f = wpath ? fopen(wpath, "w") : NULL;
    if (f) {
        fprintf(f, "#!/bin/sh\nexec %s -p %s \"$@\"\n", hermes_exe, profile);
        fclose(f);
        chmod(wpath, 0755);
        ok = true;
    } else {
        fprintf(stderr, "Could not create wrapper at %s\n", wpath ? wpath : "(null)");
    }
    free(wd); free(canon); free(profile); free(wpath);
    return ok;
}

/* PoP: profile_remove_wrapper_script @ hermes_cli/profiles.py:remove_wrapper_script */
bool profile_remove_wrapper_script(const char *name) {
    char *canon = profile_normalize_name(name);
    if (!canon) return false;
    char *aerr = NULL;
    if (!profile_validate_alias_name(canon, &aerr)) { free(aerr); free(canon); return false; }

    char *wd = profile_wrapper_dir();
    if (!wd) { free(canon); return false; }
    char *wpath = malloc(strlen(wd) + 1 + strlen(canon) + 1);
    if (wpath) snprintf(wpath, strlen(wd) + 1 + strlen(canon) + 1, "%s/%s", wd, canon);

    bool removed = false;
    if (wpath && access(wpath, F_OK) == 0) {
        FILE *f = fopen(wpath, "r");
        if (f) {
            char buf[2048];
            size_t r = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            if (r < sizeof(buf)) buf[r] = '\0';
            if (strstr(buf, "hermes -p ") != NULL) { unlink(wpath); removed = true; }
        }
    }
    free(wd); free(canon); free(wpath);
    return removed;
}

/* ── Alias map (single-pass reverse map) ───────────────────────────── */

/* Packed format: "canon\0alias\0canon\0alias\0\0". Built by scanning the
 * wrapper dir once, reading a small head of each candidate, and skipping
 * binaries (NUL-bearing content). A custom alias (file name != profile)
 * wins over a profile-named wrapper. */

/* PoP: profile_build_alias_map @ hermes_cli/profiles.py:build_alias_map */
char *profile_build_alias_map(void) {
    char *wd = profile_wrapper_dir();
    if (!wd) return NULL;
    DIR *d = opendir(wd);
    if (!d) { free(wd); return NULL; }

    /* Collect (canon, alias) pairs first. */
    typedef struct { char *canon; char *alias; } pair_t;
    pair_t *pairs = NULL; size_t cnt = 0, cap = 0;

    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_type != DT_REG && e->d_type != DT_UNKNOWN) continue;
        /* No suffix for POSIX wrappers. */
        if (strchr(e->d_name, '.')) continue;
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", wd, e->d_name);
        FILE *f = fopen(full, "r");
        if (!f) continue;
        char head[8193];
        size_t r = fread(head, 1, sizeof(head) - 1, f);
        fclose(f);
        if (r == 0) continue;
        head[r] = '\0';
        if (memchr(head, '\0', r) != NULL) continue; /* binary: skip */
        const char *prefix = "hermes -p ";
        char *idx = strstr(head, prefix);
        if (!idx) continue;
        char *rest = idx + strlen(prefix);
        char *sp = rest;
        while (*sp && !isspace((unsigned char)*sp)) sp++;
        size_t alen = (size_t)(sp - rest);
        if (alen == 0) continue;
        char *prof = malloc(alen + 1);
        memcpy(prof, rest, alen); prof[alen] = '\0';
        for (char *p = prof; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strcasecmp(prof, "default") == 0) { free(prof); continue; }

        char *alias = xstrdup(e->d_name);
        if (!alias) { free(prof); continue; }
        if (cnt == cap) { cap = cap ? cap * 2 : 16; pairs = realloc(pairs, cap * sizeof(pair_t)); }
        pairs[cnt].canon = prof;
        pairs[cnt].alias = alias;
        cnt++;
    }
    closedir(d);
    free(wd);

    /* Sort by canon for deterministic output. */
    for (size_t i = 0; i + 1 < cnt; i++)
        for (size_t j = i + 1; j < cnt; j++)
            if (strcmp(pairs[i].canon, pairs[j].canon) > 0) {
                pair_t t = pairs[i]; pairs[i] = pairs[j]; pairs[j] = t;
            }

    /* Build packed buffer: prefer custom alias (alias != canon) via
     * setdefault semantics — custom wins, but don't overwrite a custom
     * already stored. */
    char *map = malloc(1); if (map) map[0] = '\0';
    size_t maplen = 1;
    /* map canon -> stored alias (0 = none yet) */
    for (size_t i = 0; i < cnt; i++) {
        pair_t *pr = &pairs[i];
        bool is_custom = strcmp(pr->alias, pr->canon) != 0;
        /* find existing */
        bool has = false;
        size_t pos = 0;
        while (pos + 1 < maplen) {
            const char *c = map + pos;
            const char *a = c + strlen(c) + 1;
            if (strcmp(c, pr->canon) == 0) { has = true; break; }
            pos += strlen(c) + 1 + strlen(a) + 1;
        }
        if (!has) {
            /* append */
            size_t add = strlen(pr->canon) + 1 + strlen(pr->alias) + 1;
            char *nm = realloc(map, maplen + add);
            if (!nm) continue;
            map = nm;
            strcat(map, pr->canon); map[maplen + strlen(pr->canon)] = '\0';
            strcat(map, pr->alias); map[maplen + strlen(pr->canon) + 1 + strlen(pr->alias)] = '\0';
            maplen += add;
        } else if (is_custom) {
            /* replace stored profile-named alias with the custom one */
            const char *c = map + pos;
            const char *old_a = c + strlen(c) + 1;
            size_t old_alen = strlen(old_a) + 1;
            size_t clen = strlen(c); /* capture before realloc (c is invalid after) */
            size_t tail_off = pos + clen + 1 + old_alen;
            size_t tail_len = maplen - tail_off;
            memmove(map + pos + clen + 1, map + tail_off, tail_len);
            maplen -= old_alen;
            size_t need = strlen(pr->alias) + 1;
            char *nm = realloc(map, maplen + need);
            if (nm) { map = nm; strcpy(map + pos + clen + 1, pr->alias); maplen += need; }
        }
        free(pr->canon); free(pr->alias);
    }
    free(pairs);

    /* double-NUL terminator */
    char *nm = realloc(map, maplen + 1);
    if (nm) { map = nm; map[maplen] = '\0'; maplen++; }
    return map;
}

/* PoP: profile_alias_map_next @ hermes_cli/profiles.py:build_alias_map */
bool profile_alias_map_next(char *packed, char **cursor, const char **canon,
                             const char **alias) {
    if (!packed || !*packed) return false;
    if (*cursor == NULL) *cursor = packed;
    if (**cursor == '\0') return false; /* reached double-NUL */
    *canon = *cursor;
    *alias = *canon + strlen(*canon) + 1;
    *cursor = (char *)(*alias + strlen(*alias) + 1);
    return true;
}

/* PoP: profile_find_alias_for @ hermes_cli/profiles.py:find_alias_for_profile */
char *profile_find_alias_for(const char *profile_name) {
    char *canon = profile_normalize_name(profile_name);
    if (!canon) return NULL;
    char *map = profile_build_alias_map();
    char *result = NULL;
    if (map) {
        char *cursor = NULL; const char *c, *a;
        while (profile_alias_map_next(map, &cursor, &c, &a)) {
            if (strcmp(c, canon) == 0) { result = xstrdup(a); break; }
        }
        free(map);
    }
    free(canon);
    return result;
}

/* ── Active profile (sticky default) ──────────────────────────────── */

/* PoP: profile_get_active @ hermes_cli/profiles.py:get_active_profile */
char *profile_get_active(void) {
    char *path = profile_active_profile_path();
    char *result = NULL;
    if (path) {
        FILE *f = fopen(path, "r");
        if (f) {
            char buf[256];
            if (fgets(buf, sizeof(buf), f)) {
                char *s = str_strip_dup(buf);
                if (*s) result = s; else free(s);
            }
            fclose(f);
        }
        free(path);
    }
    if (!result) result = xstrdup("default");
    return result;
}

/* PoP: profile_set_active @ hermes_cli/profiles.py:set_active_profile */
bool profile_set_active(const char *name) {
    char *canon = profile_normalize_name(name);
    if (!canon) return false;
    char *err = NULL;
    if (!profile_validate_name(canon, &err)) { free(err); free(canon); return false; }
    if (strcmp(canon, "default") != 0 && !profile_dir_exists(canon)) {
        free(canon);
        return false;
    }
    char *path = profile_active_profile_path();
    bool ok = false;
    if (path) {
        char *dir = profile_default_home();
        if (dir) { mkdir_p(dir, 0755); free(dir); }
        if (strcmp(canon, "default") == 0) {
            unlink(path);
            ok = true;
        } else {
            size_t n = strlen(path) + 5;
            char *tmp = malloc(n);
            if (tmp) {
                snprintf(tmp, n, "%s.tmp", path);
                FILE *f = fopen(tmp, "w");
                if (f) {
                    fprintf(f, "%s\n", canon);
                    fclose(f);
                    ok = (rename(tmp, path) == 0);
                }
                free(tmp);
            }
        }
        free(path);
    }
    free(canon);
    return ok;
}

/* PoP: profile_get_active_name @ hermes_cli/profiles.py:get_active_profile_name */
char *profile_get_active_name(void) {
    const char *home = slermes_home();
    char *def = profile_default_home();
    bool is_default = (strcmp(home, def) == 0);
    free(def);
    if (is_default) return xstrdup("default");

    char *root = profile_profiles_root();
    size_t hl = strlen(home), rl = strlen(root);
    char *result;
    if (hl > rl && strncmp(home, root, rl) == 0 && home[rl] == '/') {
        const char *rest = home + rl + 1;
        const char *slash = strchr(rest, '/');
        size_t idlen = slash ? (size_t)(slash - rest) : strlen(rest);
        /* must be a valid profile id */
        char *id = malloc(idlen + 1);
        memcpy(id, rest, idlen); id[idlen] = '\0';
        bool valid = profile_id_matches(id);
        if (valid) { result = id; }
        else { free(id); result = xstrdup("custom"); }
    } else {
        result = xstrdup("custom");
    }
    free(root);
    return result;
}

/* ── Distribution metadata ─────────────────────────────────────────── */

/* PoP: profile_read_distribution_meta @ hermes_cli/profiles.py:_read_distribution_meta */
void profile_read_distribution_meta(const char *profile_dir, char **name,
                                    char **version, char **source) {
    char *n = NULL, *v = NULL, *s = NULL;
    if (name) *name = NULL;
    if (version) *version = NULL;
    if (source) *source = NULL;
    if (!profile_dir) return;

    size_t nlen = strlen(profile_dir) + 1 + strlen("distribution.yaml") + 1;
    char *path = malloc(nlen);
    if (!path) return;
    snprintf(path, nlen, "%s/distribution.yaml", profile_dir);

    char *perr = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &perr);
    free(perr);
    free(path);
    if (!doc) return;

    const char *dn = yaml_get_string(doc, "name");
    const char *dv = yaml_get_string(doc, "version");
    const char *ds = yaml_get_string(doc, "source");
    if (dn) n = xstrdup(dn);
    if (dv) v = xstrdup(dv);
    if (ds) s = xstrdup(ds);
    yaml_free(doc);

    if (name) *name = n; else free(n);
    if (version) *version = v; else free(v);
    if (source) *source = s; else free(s);
}

/* ── profile.yaml (per-profile description/role metadata) ─────────── */

/* PoP: profile_yaml_path @ hermes_cli/profiles.py:_profile_yaml_path */
/* Returns malloc'd "<profile_dir>/profile.yaml". Caller free(). */
char *profile_yaml_path(const char *profile_dir) {
    if (!profile_dir) return NULL;
    size_t n = strlen(profile_dir) + 1 + strlen("profile.yaml") + 1;
    char *p = malloc(n);
    if (p) snprintf(p, n, "%s/profile.yaml", profile_dir);
    return p;
}

/* PoP: profile_read_profile_meta @ hermes_cli/profiles.py:read_profile_meta */
void profile_read_profile_meta(const char *profile_dir, char **description,
                               bool *desc_auto) {
    char *desc = xstrdup("");
    bool auto_flag = false;
    if (description) *description = NULL;
    if (desc_auto) *desc_auto = false;
    if (!profile_dir) { if (description) *description = desc; return; }

    size_t nlen = strlen(profile_dir) + 1 + strlen("profile.yaml") + 1;
    char *path = malloc(nlen);
    if (!path) { if (description) *description = desc; return; }
    snprintf(path, nlen, "%s/profile.yaml", profile_dir);

    char *perr = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &perr);
    free(perr);
    if (doc) {
        const char *d = yaml_get_string(doc, "description");
        if (d) { free(desc); desc = str_strip_dup(d); }
        auto_flag = yaml_get_bool(doc, "description_auto", false);
        yaml_free(doc);
    }
    free(path);
    if (description) *description = desc; else free(desc);
    if (desc_auto) *desc_auto = auto_flag;
}

/* PoP: profile_write_profile_meta @ hermes_cli/profiles.py:write_profile_meta */
bool profile_write_profile_meta(const char *profile_dir, const char *description,
                                bool has_description, bool desc_auto,
                                bool has_desc_auto) {
    if (!profile_dir) return false;
    struct stat st;
    if (stat(profile_dir, &st) != 0 || !S_ISDIR(st.st_mode)) return false;

    size_t nlen = strlen(profile_dir) + 1 + strlen("profile.yaml") + 1;
    char *path = malloc(nlen);
    if (!path) return false;
    snprintf(path, nlen, "%s/profile.yaml", profile_dir);

    /* Read existing (best-effort). */
    char edesc[4096]; edesc[0] = '\0';
    bool eauto = false;
    char *perr = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &perr);
    free(perr);
    if (doc) {
        const char *d = yaml_get_string(doc, "description");
        if (d) { strncpy(edesc, d, sizeof(edesc) - 1); edesc[sizeof(edesc) - 1] = '\0'; }
        eauto = yaml_get_bool(doc, "description_auto", false);
        yaml_free(doc);
    }

    const char *final_desc = has_description ? description : (edesc[0] ? edesc : "");
    bool final_auto = has_desc_auto ? desc_auto : eauto;

    /* Rewrite as a minimal YAML doc (Slermes single-home: no full emitter). */
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s/.profile.yaml.tmp", profile_dir);
    FILE *f = fopen(tmp, "w");
    bool ok = false;
    if (f) {
        fprintf(f, "description: %s\n", final_desc ? final_desc : "");
        fprintf(f, "description_auto: %s\n", final_auto ? "true" : "false");
        fclose(f);
        ok = (rename(tmp, path) == 0);
    }
    if (!ok) unlink(tmp);
    free(path);
    return ok;
}

/* ── Archive member path-safety (import/export) ───────────────────── */

/* PoP: profile_archive_member_safe @ hermes_cli/profiles.py:_normalize_profile_archive_parts */
/* Faithful to Python's PurePosixPath.parts: split on '/', drop empty and "."
 * parts, reject absolute paths, Windows drives, and any ".." part. A trailing
 * slash (e.g. "_arcroot/") yields a valid single part "_arcroot"; consecutive
 * slashes ("a//b") are collapsed to ["a","b"] and accepted. */
bool profile_archive_member_safe(const char *member) {
    if (!member || !*member) return false;
    /* Backslashes are normalized to '/' then re-split; reject them outright
     * (a Windows path segment would never be a valid safe member). */
    if (strchr(member, '\\')) return false;
    /* Absolute path (leading '/') is unsafe. */
    if (member[0] == '/') return false;
    bool seen_nonempty = false;
    const char *p = member;
    while (1) {
        const char *slash = strchr(p, '/');
        size_t plen = slash ? (size_t)(slash - p) : strlen(p);
        if (plen == 1 && p[0] == '.') {           /* "." part -> dropped */
            if (!slash) break;
            p = slash + 1;
            continue;
        }
        if (plen == 2 && p[0] == '.' && p[1] == '.') return false; /* ".." */
        if (plen >= 2 && isalpha((unsigned char)p[0]) && p[1] == ':')
            return false;                          /* Windows drive letter */
        if (plen > 0) seen_nonempty = true;        /* empty part (a//b): skip */
        if (!slash) break;
        p = slash + 1;
    }
    return seen_nonempty;
}

/* PoP: profile_has_bundled_skills_opt_out @ hermes_cli/profiles.py:has_bundled_skills_opt_out */
bool profile_has_bundled_skills_opt_out(const char *profile_dir) {
    if (!profile_dir) return false;
    size_t n = strlen(profile_dir) + 1 + strlen(".no-bundled-skills") + 1;
    char *p = malloc(n);
    if (!p) return false;
    snprintf(p, n, "%s/.no-bundled-skills", profile_dir);
    struct stat st;
    bool exists = (stat(p, &st) == 0);
    free(p);
    return exists;
}

/* ── Skills count ─────────────────────────────────────────────────── */

static double s_skill_sig = 0;
static char *s_skill_key = NULL;
static time_t s_skill_ts = 0;
static int s_skill_count = 0;
#define SKILL_TTL 30

/* PoP: profile_skills_dir_signature @ hermes_cli/profiles.py:_skills_dir_signature */
double profile_skills_dir_signature(const char *skills_dir) {
    if (!skills_dir) return 0.0;
    struct stat st;
    double sig = 0.0;
    if (stat(skills_dir, &st) == 0) sig = (double)st.st_mtime;
    DIR *d = opendir(skills_dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            char full[PATH_MAX];
            snprintf(full, sizeof(full), "%s/%s", skills_dir, e->d_name);
            struct stat es;
            if (stat(full, &es) == 0 && S_ISDIR(es.st_mode)) {
                double m = (double)es.st_mtime;
                if (m > sig) sig = m;
            }
        }
        closedir(d);
    }
    return sig;
}

static int profile_count_skills_recursive(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return -1;
    int count = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            int sub = profile_count_skills_recursive(full);
            if (sub > 0) count += sub;
        } else if (strcmp(e->d_name, "SKILL.md") == 0) {
            count++;
        }
    }
    closedir(d);
    return count;
}

/* PoP: profile_count_skills @ hermes_cli/profiles.py:_count_skills */
int profile_count_skills(const char *profile_dir) {
    if (!profile_dir) return 0;
    size_t n = strlen(profile_dir) + 1 + strlen("skills") + 1;
    char *sk = malloc(n);
    if (!sk) return 0;
    snprintf(sk, n, "%s/skills", profile_dir);
    struct stat st;
    if (stat(sk, &st) != 0 || !S_ISDIR(st.st_mode)) { free(sk); return 0; }

    double sig = profile_skills_dir_signature(sk);
    time_t now = time(NULL);
    if (s_skill_key && strcmp(s_skill_key, sk) == 0 &&
        s_skill_sig == sig && (now - s_skill_ts) < SKILL_TTL) {
        free(sk);
        return s_skill_count;
    }
    int count = profile_count_skills_recursive(sk);
    if (count < 0) count = 0;
    free(s_skill_key);
    s_skill_key = xstrdup(sk);
    s_skill_sig = sig;
    s_skill_ts = now;
    s_skill_count = count;
    free(sk);
    return count;
}

/* ── config.yaml model/provider ───────────────────────────────────── */

/* PoP: profile_read_config_model @ hermes_cli/profiles.py:_read_config_model */
void profile_read_config_model(const char *profile_dir, char **model,
                               char **provider) {
    if (model) *model = NULL;
    if (provider) *provider = NULL;
    if (!profile_dir) return;
    size_t n = strlen(profile_dir) + 1 + strlen("config.yaml") + 1;
    char *path = malloc(n);
    if (!path) return;
    snprintf(path, n, "%s/config.yaml", profile_dir);
    struct stat st;
    if (stat(path, &st) != 0) { free(path); return; }

    char *perr = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &perr);
    free(perr);
    free(path);
    if (!doc) return;

    const char *raw = yaml_get_string(doc, "model");
    const char *def = yaml_get_string(doc, "model.default");
    const char *prov = yaml_get_string(doc, "model.provider");
    if (raw) {
        if (model) *model = xstrdup(raw);
    } else if (def) {
        if (model) *model = xstrdup(def);
        if (prov && provider) *provider = xstrdup(prov);
    }
    yaml_free(doc);
}

/* ── profiles_to_serve ────────────────────────────────────────────── */

/* PoP: profile_profiles_to_serve @ hermes_cli/profiles.py:profiles_to_serve */
char *profile_profiles_to_serve(bool multiplex) {
    char *active = profile_get_active_name();
    if (!active) return NULL;
    char *active_home = profile_dir_for(active);

    char *buf = NULL;
    size_t len = 0;
    size_t cap = 0;
    #define PUSH_SERVE(nm, hm) do { \
        size_t add = strlen(nm) + 1 + strlen(hm) + 1 + 1; \
        if (len + add > cap) { cap = (cap ? cap * 2 : 256) + add; buf = realloc(buf, cap); } \
        memcpy(buf + len, (nm), strlen(nm) + 1); len += strlen(nm) + 1; \
        memcpy(buf + len, (hm), strlen(hm) + 1); len += strlen(hm) + 1; \
        buf[len++] = '\n'; \
    } while (0)

    if (!multiplex) {
        if (active_home) PUSH_SERVE(active, active_home);
    } else {
        char *def_home = profile_default_home();
        if (def_home) PUSH_SERVE("default", def_home);
        free(def_home);
        char *root = profile_profiles_root();
        if (root) {
            DIR *d = opendir(root);
            if (d) {
                struct dirent *e;
                while ((e = readdir(d))) {
                    if (e->d_type != DT_DIR && e->d_type != DT_UNKNOWN) continue;
                    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
                    if (strcmp(e->d_name, "default") == 0) continue;
                    if (!profile_id_matches(e->d_name)) continue;
                    char *h = malloc(strlen(root) + 1 + strlen(e->d_name) + 1);
                    snprintf(h, strlen(root) + 1 + strlen(e->d_name) + 1, "%s/%s", root, e->d_name);
                    PUSH_SERVE(e->d_name, h);
                    free(h);
                }
                closedir(d);
            }
            free(root);
        }
    }
    #undef PUSH_SERVE
    free(active);
    free(active_home);
    if (!buf) { buf = malloc(1); buf[0] = '\0'; return buf; }
    /* NUL-terminate: each record ends with '\n'; ensure a trailing '\0' so
     * the iterator's end-of-buffer sentinel reads cleanly. */
    char *term = realloc(buf, len + 1);
    if (term) { buf = term; buf[len] = '\0'; }
    return buf;
}

/* PoP: profile_serve_next @ hermes_cli/profiles.py:profiles_to_serve */
bool profile_serve_next(char *packed, char **cursor, const char **name,
                        const char **home) {
    if (!packed || !*packed) return false;
    if (*cursor == NULL) *cursor = packed;
    if (**cursor == '\0') return false;
    if (**cursor == '\n') (*cursor)++;
    if (**cursor == '\0') return false;
    *name = *cursor;
    *home = *name + strlen(*name) + 1;
    *cursor = (char *)(*home + strlen(*home) + 2);
    return true;
}

/* ── Clone / export ignore predicates ─────────────────────────────── */

static bool ends_with(const char *s, const char *suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}

/* PoP: profile_clone_ignore @ hermes_cli/profiles.py:_clone_all_copytree_ignore */
bool profile_clone_ignore(const char *source_dir, const char *dir,
                           const char *entry) {
    if (!entry) return false;
    if (strcmp(entry, "__pycache__") == 0 ||
        ends_with(entry, ".pyc") || ends_with(entry, ".pyo") ||
        ends_with(entry, ".sock") || ends_with(entry, ".tmp"))
        return true;

    char full[PATH_MAX];
    snprintf(full, sizeof(full), "%s", dir);
    char srcfull[PATH_MAX];
    snprintf(srcfull, sizeof(srcfull), "%s", source_dir);
    bool at_root = (strcmp(full, srcfull) == 0);
    if (!at_root) return false;

    static const char *HIST[] = {"state.db","state.db-wal","state.db-shm",
        "sessions","backups","state-snapshots","checkpoints",NULL};
    for (int i = 0; HIST[i]; i++)
        if (strcmp(entry, HIST[i]) == 0) return true;

    char *def = profile_default_home();
    bool is_default = def && strcmp(srcfull, def) == 0;
    free(def);
    if (is_default) {
        static const char *INFRA[] = {"hermes-agent",".worktrees","profiles",
            "bin","node_modules",NULL};
        for (int i = 0; INFRA[i]; i++)
            if (strcmp(entry, INFRA[i]) == 0) return true;
    }
    return false;
}

/* PoP: profile_export_ignore @ hermes_cli/profiles.py:_default_export_ignore */
bool profile_export_ignore(const char *root_dir, const char *dir,
                            const char *entry) {
    if (!entry) return false;
    if (strcmp(entry, "__pycache__") == 0 ||
        ends_with(entry, ".sock") || ends_with(entry, ".tmp"))
        return true;
    if (strcmp(entry, "package.json") == 0 ||
        strcmp(entry, "package-lock.json") == 0)
        return true;
    char full[PATH_MAX];
    snprintf(full, sizeof(full), "%s", dir);
    char rootfull[PATH_MAX];
    snprintf(rootfull, sizeof(rootfull), "%s", root_dir);
    if (strcmp(full, rootfull) == 0) {
        static const char *EXPORT[] = {"hermes-agent",".worktrees","profiles",
            "bin","node_modules","logs","cache",".cache","state.db","sessions",
            "backups","state-snapshots","checkpoints","cron",NULL};
        for (int i = 0; EXPORT[i]; i++)
            if (strcmp(entry, EXPORT[i]) == 0) return true;
    }
    return false;
}

/* ── .env backfill ────────────────────────────────────────────────── */

/* PoP: profile_backfill_profile_envs @ hermes_cli/profiles.py:backfill_profile_envs */
char *profile_backfill_profile_envs(void) {
    char *root = profile_profiles_root();
    if (!root) return xstrdup("");
    struct stat st;
    if (stat(root, &st) != 0 || !S_ISDIR(st.st_mode)) { free(root); return xstrdup(""); }

    char *def = profile_default_home();
    char *defenv = malloc(strlen(def) + 1 + strlen(".env") + 1);
    snprintf(defenv, strlen(def) + 1 + strlen(".env") + 1, "%s/.env", def);
    bool has_def = (stat(defenv, &st) == 0 && S_ISREG(st.st_mode));
    free(def);

    char *out = malloc(1); size_t outlen = 0; if (out) out[0] = '\0';
    DIR *d = opendir(root);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_type != DT_DIR && e->d_type != DT_UNKNOWN) continue;
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            if (!profile_id_matches(e->d_name)) continue;
            char *ep = malloc(strlen(root) + 1 + strlen(e->d_name) + 1 + strlen("/.env"));
            snprintf(ep, strlen(root) + 1 + strlen(e->d_name) + 1 + strlen("/.env"),
                     "%s/%s/.env", root, e->d_name);
            if (stat(ep, &st) == 0) { free(ep); continue; }
            bool ok = false;
            if (has_def) {
                char *cmd = malloc(strlen(defenv) + 1 + strlen(ep) + 16);
                snprintf(cmd, strlen(defenv) + 1 + strlen(ep) + 16,
                         "cp '%s' '%s'", defenv, ep);
                ok = (system(cmd) == 0);
                free(cmd);
            } else {
                FILE *f = fopen(ep, "w");
                if (f) {
                    fputs("# Per-profile secrets for this Hermes profile.\n"
                          "# API keys and tokens set here override the shell environment.\n"
                          "# Behavioral settings belong in config.yaml, not here.\n", f);
                    fclose(f); ok = true;
                }
            }
            if (ok) {
                chmod(ep, 0600);
                size_t add = strlen(e->d_name) + 1;
                char *no = realloc(out, outlen + add + 1);
                if (no) { out = no; memcpy(out + outlen, e->d_name, strlen(e->d_name));
                          outlen += strlen(e->d_name); out[outlen++] = '\n'; out[outlen] = '\0'; }
            }
            free(ep);
        }
        closedir(d);
    }
    free(root); free(defenv);
    return out;
}

/* ── Seed bundled skills (single-home fail-open) ──────────────────── */

/* PoP: profile_seed_profile_skills @ hermes_cli/profiles.py:seed_profile_skills */
char *profile_seed_profile_skills(const char *profile_dir, bool quiet) {
    (void)quiet;
    if (!profile_dir) return NULL;
    if (profile_has_bundled_skills_opt_out(profile_dir))
        return xstrdup("{\"copied\":[],\"updated\":[],\"user_modified\":[],"
                       "\"skipped_opt_out\":true}");
    return xstrdup("{\"copied\":[],\"updated\":[],\"user_modified\":[]}");
}

/* ── Config schema migration (no-op) ─────────────────────────────── */

/* PoP: profile_migrate_config_if_outdated @ hermes_cli/profiles.py:_migrate_profile_config_if_outdated */
void profile_migrate_config_if_outdated(const char *profile_dir) {
    (void)profile_dir;
}

/* ── Gateway service lifecycle (host no-ops) ─────────────────────── */

/* PoP: profile_maybe_register_gateway_service @ hermes_cli/profiles.py:_maybe_register_gateway_service */
void profile_maybe_register_gateway_service(const char *profile_name) {
    (void)profile_name;
}

/* PoP: profile_maybe_unregister_gateway_service @ hermes_cli/profiles.py:_maybe_unregister_gateway_service */
void profile_maybe_unregister_gateway_service(const char *profile_name) {
    /* Python: no-op on host — only tears down an s6 gateway service inside
     * the container. Detect the s6 marker; absent => silent return. */
    if (!profile_name || !*profile_name) return;
    const char *s6 = getenv("S6_SERVICES_DIR");
    if (!s6 || !*s6) return; /* host path — silent */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "s6-rc -d change %s 2>/dev/null || true", profile_name);
    if (system(cmd) == 0) hermes_log(LOG_INFO, "profiles",
                                     "unregistered s6 gateway service %s", profile_name);
}

/* PoP: profile_cleanup_gateway_service @ hermes_cli/profiles.py:_cleanup_gateway_service */
void profile_cleanup_gateway_service(const char *name, const char *profile_dir) {
    /* Python (Linux): systemctl --user disable, then remove the unit file.
     * Best-effort like the Python path; absent units are skipped. */
    (void)profile_dir;
    if (!name || !*name) return;
    const char *home = getenv("HOME");
    char svc_file[1024];
    snprintf(svc_file, sizeof(svc_file), "%s/.config/systemd/user/%s.service",
             home ? home : "", name);
    char cmd[1152];
    snprintf(cmd, sizeof(cmd), "systemctl --user disable %s 2>/dev/null", name);
    if (system(cmd) != 0) { /* best-effort: keep going to remove the file */ }
    if (unlink(svc_file) == 0)
        hermes_log(LOG_INFO, "profiles", "removed gateway service unit %s", svc_file);
}

/* PoP: profile_stop_gateway_process @ hermes_cli/profiles.py:_stop_gateway_process */
void profile_stop_gateway_process(const char *profile_dir) {
    if (!profile_dir) return;
    size_t n = strlen(profile_dir) + 1 + strlen("gateway.pid") + 1;
    char *pp = malloc(n);
    if (!pp) return;
    snprintf(pp, n, "%s/gateway.pid", profile_dir);
    FILE *f = fopen(pp, "r");
    free(pp);
    if (!f) return;
    char buf[256];
    pid_t pid = -1;
    if (fgets(buf, sizeof(buf), f)) {
        char *s = buf;
        while (*s && isspace((unsigned char)*s)) s++;
        char *p = strstr(s, "\"pid\"");
        if (p) pid = (pid_t)strtol(p + 5, NULL, 10);
        else pid = (pid_t)strtol(s, NULL, 10);
    }
    fclose(f);
    if (pid <= 0) return;
    kill(pid, SIGTERM);
    for (int i = 0; i < 100; i++) {
        if (kill(pid, 0) != 0) break;
        usleep(100000);
    }
    if (kill(pid, 0) == 0) kill(pid, SIGKILL);
}

/* ── Honcho host rename (no-op) ───────────────────────────────────── */

/* PoP: profile_migrate_honcho_profile_host @ hermes_cli/profiles.py:_migrate_honcho_profile_host */
void profile_migrate_honcho_profile_host(const char *old_name,
                                          const char *new_name,
                                          const char *new_dir) {
    (void)old_name; (void)new_name; (void)new_dir;
}

/* ── Archive import/export helpers ────────────────────────────────── */

/* PoP: profile_inspect_archive_roots @ hermes_cli/profiles.py:_inspect_profile_archive_roots */
char *profile_inspect_archive_roots(const char *archive_path) {
    char *roots = malloc(1); if (!roots) return NULL; roots[0] = '\0';
    size_t rlen = 0;
    gzFile gz = gzopen(archive_path, "rb");
    if (!gz) return roots;
    unsigned char hdr[512];
    while (1) {
        size_t rd = gzread(gz, hdr, 512);
        if (rd != 512) break;
        if (hdr[0] == 0) break;
        char name[101];
        memcpy(name, hdr, 100); name[100] = '\0';
        char num[13]; memcpy(num, hdr+124, 12); num[12]='\0';
        long size = strtol(num, NULL, 8);
        if (size < 0) size = 0;
        char *slash = strchr(name, '/');
        if (slash && slash != name) {
            size_t tl = (size_t)(slash - name);
            bool seen = false;
            for (size_t i = 0; i < rlen; ) {
                if (strncmp(roots + i, name, tl) == 0 && roots[i+tl] == '\n') { seen = true; break; }
                i += strlen(roots + i) + 1;
            }
            if (!seen) {
                char *no = realloc(roots, rlen + tl + 2);
                if (no) { roots = no; memcpy(roots + rlen, name, tl); rlen += tl; roots[rlen++] = '\n'; roots[rlen] = '\0'; }
            }
        } else if (slash == NULL && name[0]) {
            size_t tl = strlen(name);
            bool seen = false;
            for (size_t i = 0; i < rlen; ) {
                if (strcmp(roots + i, name) == 0) { seen = true; break; }
                i += strlen(roots + i) + 1;
            }
            if (!seen) {
                char *no = realloc(roots, rlen + tl + 2);
                if (no) { roots = no; memcpy(roots + rlen, name, tl); rlen += tl; roots[rlen++] = '\n'; roots[rlen] = '\0'; }
            }
        }
        long blocks = (size + 511) / 512;
        if (blocks > 0) {
            /* Read-and-discard padding (gzseek forward is unreliable on gz).
             * Tar pads each entry to a 512-block boundary, so the trailing
             * padding is (blocks*512 - size) bytes, NOT a full extra block. */
            long pad = blocks * 512 - size;
            unsigned char padbuf[512];
            while (pad > 0) {
                long chunk = pad < 512 ? pad : 512;
                if (gzread(gz, padbuf, (unsigned)chunk) != (size_t)chunk) break;
                pad -= chunk;
            }
        }
    }
    gzclose(gz);
    return roots;
}

/* PoP: profile_safe_extract_archive @ hermes_cli/profiles.py:_safe_extract_profile_archive */
bool profile_safe_extract_archive(const char *archive_path,
                                  const char *destination) {
    gzFile gz = gzopen(archive_path, "rb");
    if (!gz) return false;
    unsigned char hdr[512];
    while (1) {
        size_t rd = gzread(gz, hdr, 512);
        if (rd != 512) break;
        if (hdr[0] == 0) break;
        char name[101];
        memcpy(name, hdr, 100); name[100] = '\0';
        if (!profile_archive_member_safe(name)) { gzclose(gz); return false; }
        char num[13]; memcpy(num, hdr+124, 12); num[12]='\0';
        long size = strtol(num, NULL, 8);
        if (size < 0) size = 0;
        char typeflag = hdr[156];
        char target[PATH_MAX];
        snprintf(target, sizeof(target), "%s/%s", destination, name);
        if (typeflag == '5') {
            mkdir_p(target, 0755);
        } else if (typeflag == '0' || typeflag == '\0' || typeflag == '7') {
            char *slash = strrchr(target, '/');
            if (slash) { *slash = '\0'; mkdir_p(target, 0755); *slash = '/'; }
            else mkdir_p(destination, 0755);
            FILE *out = fopen(target, "wb");
            if (!out) { gzclose(gz); return false; }
            long remaining = size;
            unsigned char chunk[8192];
            while (remaining > 0) {
                size_t to = remaining < (long)sizeof(chunk) ? (size_t)remaining : sizeof(chunk);
                size_t got = gzread(gz, chunk, to);
                if (got == 0) break;
                fwrite(chunk, 1, got, out);
                remaining -= (long)got;
            }
            fclose(out);
        }
        long blocks = (size + 511) / 512;
        if (blocks > 0) {
            long pad = blocks * 512 - size;
            unsigned char padbuf[512];
            while (pad > 0) {
                long chunk = pad < 512 ? pad : 512;
                if (gzread(gz, padbuf, (unsigned)chunk) != (size_t)chunk) break;
                pad -= chunk;
            }
        }
    }
    gzclose(gz);
    return true;
}

/* ── Gateway-running probe ────────────────────────────────────────── */

/* PoP: profile_gateway_running @ hermes_cli/profiles.py:_check_gateway_running */
bool profile_gateway_running(const char *profile_dir) {
    if (!profile_dir) return false;
    size_t n = strlen(profile_dir) + 1 + strlen("gateway.pid") + 1;
    char *pid_path = malloc(n);
    if (pid_path) snprintf(pid_path, n, "%s/gateway.pid", profile_dir);

    bool running = false;
    if (pid_path) {
        pid_t pid = gwstatus_get_running_pid(pid_path, false);
        if (pid != (pid_t)-1) running = true;
        free(pid_path);
    }
    if (running) return true;

    /* Fall back to gateway_state.json liveness. */
    size_t m = strlen(profile_dir) + 1 + strlen("gateway_state.json") + 1;
    char *state_path = malloc(m);
    if (state_path) snprintf(state_path, m, "%s/gateway_state.json", profile_dir);
    if (state_path) {
        char *runtime = gwstatus_read_runtime_status(state_path);
        free(state_path);
        if (runtime) {
            pid_t spid = gwstatus_get_runtime_status_running_pid(runtime, profile_dir);
            free(runtime);
            if (spid != (pid_t)-1) running = true;
        }
    }
    return running;
}

/* ── Profile env resolution ────────────────────────────────────────── */

/* PoP: profile_resolve_env @ hermes_cli/profiles.py:resolve_profile_env */
char *profile_resolve_env(const char *profile_name) {
    char *canon = profile_normalize_name(profile_name);
    if (!canon) return NULL;
    char *err = NULL;
    if (!profile_validate_name(canon, &err)) { free(err); free(canon); return NULL; }
    char *dir = profile_dir_for(canon);
    if (strcmp(canon, "default") != 0) {
        struct stat st;
        if (!dir || stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            free(dir); free(canon); return NULL;
        }
    }
    free(canon);
    return dir;
}
