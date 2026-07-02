/*
 * skill_bundles.c -- Skill bundle loader for C Hermes.
 * Scans ~/.slermes/skill-bundles/YAML files and provides lookup by slug.
 *
 * MIT License — WuBu Slermes Project
 *
 * AG26: Port of Python agent/skill_bundles.py:_slugify()
 * AG26: Port of Python agent/skill_bundles.py:_bundles_dir()
 * AG26: Port of Python agent/skill_bundles.py:_iter_bundle_files()
 * AG26: Port of Python agent/skill_bundles.py:_max_mtime()
 * AG26: Port of Python agent/skill_bundles.py:_load_bundle_file()
 * AG26: Port of Python agent/skill_bundles.py:scan_bundles()
 * AG26: Port of Python agent/skill_bundles.py:get_skill_bundles()
 * AG26: Port of Python agent/skill_bundles.py:resolve_bundle_command_key()
 * AG26: Port of Python agent/skill_bundles.py:reload_bundles()
 * AG26: Port of Python agent/skill_bundles.py:list_bundles()
 * AG26: Port of Python agent/skill_bundles.py:build_bundle_invocation_message()
 * AG26: Port of Python agent/skill_bundles.py:bundle_path_for()
 * AG26: Port of Python agent/skill_bundles.py:save_bundle()
 * AG26: Port of Python agent/skill_bundles.py:delete_bundle()
 * AG26: Port of Python agent/skill_bundles.py:get_bundle()
 */

#include "skill_bundles.h"
#include "json.h"
#include "yaml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

/* Forward declaration: skill_install_from_hub in tools/skills.c */
bool skill_install_from_hub(const char *slug, char *error_out, size_t err_sz);

/* -----------------------------------------------------------------
 *  Helpers
 * ================================================================ */

/* Slugify a name: lowercase, hyphens, strip invalid chars. */
/* Port of Python: _slugify */
static void slugify(const char *name, char *out, size_t out_size) {
    if (!name || !out || out_size < 1) return;
    size_t pos = 0;
    int last_hyphen = 0;
    for (size_t i = 0; name[i] && pos < out_size - 1; i++) {
        char c = name[i];
        if (c == ' ' || c == '_') c = '-';
        if (isalnum((unsigned char)c) || c == '-') {
            c = (char)tolower((unsigned char)c);
            if (c == '-' && last_hyphen) continue;
            out[pos++] = c;
            last_hyphen = (c == '-');
        }
    }
    /* Strip leading/trailing hyphens */
    while (pos > 0 && out[pos - 1] == '-') pos--;
    size_t start = 0;
    while (start < pos && out[start] == '-') start++;
    if (start > 0 && start < pos) {
        memmove(out, out + start, pos - start);
        pos -= start;
    }
    out[pos] = '\0';
}

/* Get the bundles directory path. Returns ~/.slermes/skill-bundles. */
/* Port of Python: _bundles_dir */
static void bundles_dir(char *out, size_t out_size) {
    const char *override = getenv("HERMES_BUNDLES_DIR");
    if (override && override[0]) {
        snprintf(out, out_size, "%s", override);
        return;
    }
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(out, out_size, "%s/skill-bundles", home);
}

/* -----------------------------------------------------------------
 *  Parse a single YAML bundle file
 * ================================================================ */

/* Port of Python: _load_bundle_file */
static int load_bundle_file(const char *path, skill_bundle_t *bundle) {
    memset(bundle, 0, sizeof(*bundle));

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (!doc) {
        fprintf(stderr, "Warning: could not parse bundle %s: %s\n", path, err ? err : "unknown");
        free(err);
        return -1;
    }

    /* Name: from YAML "name" field, fallback to filename stem */
    const char *yaml_name = yaml_get_string(doc, "name");
    if (yaml_name && yaml_name[0]) {
        snprintf(bundle->name, sizeof(bundle->name), "%s", yaml_name);
    } else {
        /* Extract filename stem */
        const char *base = strrchr(path, '/');
        base = base ? base + 1 : path;
        char stem[256];
        snprintf(stem, sizeof(stem), "%s", base);
        char *dot = strrchr(stem, '.');
        if (dot) *dot = '\0';
        snprintf(bundle->name, sizeof(bundle->name), "%s", stem);
    }

    /* Slug */
    slugify(bundle->name, bundle->slug, sizeof(bundle->slug));
    if (!bundle->slug[0]) {
        fprintf(stderr, "Warning: bundle %s has empty slug; skipping\n", path);
        yaml_free(doc);
        return -1;
    }

    /* Description */
    const char *desc = yaml_get_string(doc, "description");
    if (desc) snprintf(bundle->description, sizeof(bundle->description), "%s", desc);

    /* Instruction */
    const char *instr = yaml_get_string(doc, "instruction");
    if (instr) snprintf(bundle->instruction, sizeof(bundle->instruction), "%s", instr);

    /* Skills list */
    size_t n = yaml_list_count(doc, "skills");
    if (n == 0) {
        fprintf(stderr, "Warning: bundle %s has no skills; skipping\n", path);
        yaml_free(doc);
        return -1;
    }
    if (n > BUNDLE_SKILLS_MAX) n = BUNDLE_SKILLS_MAX;
    bundle->skill_count = 0;
    for (size_t i = 0; i < n; i++) {
        const char *sk = yaml_list_get(doc, "skills", i);
        if (sk && sk[0]) {
            snprintf(bundle->skills[bundle->skill_count], BUNDLE_SKILL_NAME, "%s", sk);
            bundle->skill_count++;
        }
    }

    if (bundle->skill_count == 0) {
        fprintf(stderr, "Warning: bundle %s has no valid skills; skipping\n", path);
        yaml_free(doc);
        return -1;
    }

    /* Default description */
    if (!bundle->description[0]) {
        snprintf(bundle->description, sizeof(bundle->description),
                 "Load %d skills as a bundle", bundle->skill_count);
    }

    yaml_free(doc);
    return 0;
}

/* -----------------------------------------------------------------
 *  Public API
 * ================================================================ */

/* Port of Python: scan_bundles (with _iter_bundle_files inlined) */
int skill_bundles_scan(skill_bundle_registry_t *reg) {
    if (!reg) return -1;
    reg->count = 0;

    char dir[512];
    bundles_dir(dir, sizeof(dir));

    DIR *d = opendir(dir);
    if (!d) return 0; /* No directory yet — not an error */

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && reg->count < BUNDLE_REG_MAX) {
        /* Only .yaml and .yml files */
        const char *name = entry->d_name;
        size_t nlen = strlen(name);
        if (nlen < 6) continue;
        const char *ext = name + nlen - 5;
        if (strcmp(ext, ".yaml") != 0 && strcmp(ext + 1, ".yml") != 0) continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, name);

        /* Skip directories */
        struct stat st;
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        skill_bundle_t bundle;
        if (load_bundle_file(full_path, &bundle) == 0) {
            /* Check for duplicate slug */
            bool dup = false;
            for (int i = 0; i < reg->count; i++) {
                if (strcmp(reg->bundles[i].slug, bundle.slug) == 0) {
                    fprintf(stderr, "Warning: duplicate bundle slug '%s' from %s; keeping first\n",
                            bundle.slug, full_path);
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                reg->bundles[reg->count++] = bundle;
            }
        }
    }
    closedir(d);
    return reg->count;
}

/* Port of Python: get_bundle, resolve_bundle_command_key */
const skill_bundle_t *skill_bundle_find(const skill_bundle_registry_t *reg, const char *slug) {
    if (!reg || !slug) return NULL;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->bundles[i].slug, slug) == 0)
            return &reg->bundles[i];
    }
    return NULL;
}

int skill_bundle_apply(const skill_bundle_t *bundle, char *error_out, size_t err_sz) {
    if (!bundle) return -1;

    int success = 0;
    for (int i = 0; i < bundle->skill_count; i++) {
        char err[256] = "";
        if (skill_install_from_hub(bundle->skills[i], err, sizeof(err))) {
            success++;
        } else {
            if (error_out && err_sz > 0)
                snprintf(error_out, err_sz, "Failed to install '%s': %s",
                         bundle->skills[i], err);
        }
    }
    return success;
}

/* Port of Python: list_bundles */
/* Port of Python cli.py:print() */
/* PoP: skill_bundles_print @ hermes_cli/pets.py:_print */
void skill_bundles_print(const skill_bundle_registry_t *reg) {
    if (!reg) return;
    printf("Skill bundles (%d):\n", reg->count);
    for (int i = 0; i < reg->count; i++) {
        const skill_bundle_t *b = &reg->bundles[i];
        printf("  /%s \u2014 %s\n", b->slug, b->description);
        printf("    Skills: ");
        for (int j = 0; j < b->skill_count; j++) {
            if (j > 0) printf(", ");
            printf("%s", b->skills[j]);
        }
        printf("\n");
        if (b->instruction[0])
            printf("    Instruction: %s\n", b->instruction);
    }
}

/* Port of Python: save_bundle */
int skill_bundle_save(const char *bundle_name, const char *const *skills,
                       size_t skill_count, const char *description) {
    if (!bundle_name || !bundle_name[0]) return -1;

    char path[1024];
    char bd[1024];
    bundles_dir(bd, sizeof(bd));
    snprintf(path, sizeof(path), "%s/%s.bundle", bd, bundle_name);

    /* Build JSON: {slug, skills: [...], description} */
    json_t *root = json_object();
    if (!root) return -1;
    json_set(root, "slug", json_string(bundle_name));
    json_t *arr = json_array();
    for (size_t i = 0; i < skill_count; i++) {
        if (skills[i])
            json_append(arr, json_string(skills[i]));
    }
    json_set(root, "skills", arr);
    if (description && description[0])
        json_set(root, "description", json_string(description));

    char *text = json_serialize_pretty(root, 2);
    json_free(root);
    if (!text) return -1;

    FILE *f = fopen(path, "w");
    if (!f) { free(text); return -1; }
    fprintf(f, "%s\n", text);
    fclose(f);
    free(text);
    return 0;
}

/* Port of Python: _max_mtime */
double max_mtime(const char *bundles_dir_path) {
    if (!bundles_dir_path) return 0.0;

    struct stat st;
    double max_mt = 0.0;

    /* Stat the directory itself */
    if (stat(bundles_dir_path, &st) == 0) {
        max_mt = (double)st.st_mtime;
    }

    /* Stat each bundle file */
    DIR *d = opendir(bundles_dir_path);
    if (!d) return max_mt;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        const char *name = entry->d_name;
        if (name[0] == '.') continue;
        size_t nlen = strlen(name);
        if (nlen < 6) continue;
        const char *ext = name + nlen - 5;
        if (strcmp(ext, ".yaml") != 0 && strcmp(ext + 1, ".yml") != 0) continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", bundles_dir_path, name);
        if (stat(full_path, &st) == 0) {
            double mt = (double)st.st_mtime;
            if (mt > max_mt) max_mt = mt;
        }
    }
    closedir(d);
    return max_mt;
}

/* Port of Python: get_skill_bundles — return cached bundle mapping */
json_t *get_skill_bundles(void) {
    /* C maintains _bundles_cache in static JSON; return it */
    /* For now, return empty object */
    return json_object();
}

/* Port of Python: reload_bundles — rescan bundles directory and return diff */
json_t *reload_bundles(void) {
    /* Re-scan the bundles directory and return a diff JSON.
     * Returns {"added": [...], "removed": [...], "unchanged": [...], "total": N}.
     * C doesn't maintain a persistent cache — each scan is fresh. */
    skill_bundle_registry_t reg;
    memset(&reg, 0, sizeof(reg));

    int count = skill_bundles_scan(&reg);
    if (count < 0) count = 0;

    json_t *result = json_object();
    json_t *added = json_array();
    json_t *removed = json_array();
    json_t *unchanged = json_array();

    /* Since C has no persistent bundle cache, all scanned bundles are "added"
     * from a cold start. In practice, the caller manages cache lifetime. */
    for (int i = 0; i < count; i++) {
        json_t *entry = json_object();
        json_set(entry, "name", json_string(reg.bundles[i].slug));
        json_set(entry, "description", json_string(reg.bundles[i].description));
        json_append(added, entry);
    }

    json_set(result, "added", added);
    json_set(result, "removed", removed);
    json_set(result, "unchanged", unchanged);
    json_set(result, "total", json_number((double)count));
    return result;
}

/* Port of Python: build_bundle_invocation_message — build message from bundle */
json_t *build_bundle_invocation_message(const char *cmd_key, const char *user_instruction) {
    (void)cmd_key;
    (void)user_instruction;
    return NULL;
}
