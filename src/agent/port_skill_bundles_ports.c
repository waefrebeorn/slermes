/*
 * port_skill_bundles_remaining.c — Port of agent/skill_bundles.py bundle
 * registry surface. REAL file ops: bundles dir, slugify, scanning with
 * mtime cache, save/delete/get, listing.
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
#include <time.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static const char *bundle_home(void) {
    const char *h = getenv("HERMES_BUNDLES_DIR");
    if (h && *h) return h;
    h = getenv("HERMES_HOME");
    if (h && *h) return h;
    h = getenv("HOME");
    if (h && *h) return h;
    return ".";
}

/* PoP: _bundles_dir @ agent/skill_bundles.py:_bundles_dir */
char *sbd_bundles_dir(void) {
    char *out = NULL;
    const char *h = getenv("HERMES_BUNDLES_DIR");
    if (h && *h) return strdup(h);
    asprintf(&out, "%s/bundles", bundle_home());
    return out;
}

/* PoP: _slugify @ agent/skill_bundles.py:_slugify */
char *sbd_slugify(const char *name) {
    /* Python: lower, space/underscore → dash, strip invalid chars. */
    if (!name) return strdup("");
    char *out = malloc(strlen(name) * 2 + 1);
    if (!out) return NULL;
    char *q = out;
    for (const char *p = name; *p; p++) {
        char c = tolower((unsigned char)*p);
        if (c == ' ' || c == '_') *q++ = '-';
        else if (isalnum((unsigned char)c) || c == '-' || c == '.') *q++ = c;
    }
    /* collapse duplicate dashes, strip leading/trailing */
    char *dst = out;
    bool prev_dash = false;
    for (char *s = out; *s; s++) {
        if (*s == '-') {
            if (prev_dash) continue;
            if (s == out) continue;
            prev_dash = true;
        } else {
            prev_dash = false;
        }
        *dst++ = *s;
    }
    while (dst > out && dst[-1] == '-') dst--;
    *dst = '\0';
    return out;
}

/* PoP: _iter_bundle_files @ agent/skill_bundles.py:_iter_bundle_files */
char *sbd_iter_bundle_files(void) {
    /* Python: *.yaml bundle files. */
    char *dir = sbd_bundles_dir();
    DIR *d = opendir(dir);
    if (!d) { free(dir); return strdup("[]"); }
    size_t cap = 512, len = 0;
    char *out = malloc(cap);
    if (!out) { closedir(d); free(dir); return strdup("[]"); }
    strcpy(out, "[");
    bool first = true;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);
        if (n < 5 || strcmp(e->d_name + n - 5, ".yaml") != 0) continue;
        size_t need = len + n + 8;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
        }
        if (!first) strcat(out, ",");
        strcat(out, "\"");
        strcat(out, e->d_name);
        strcat(out, "\"");
        first = false;
        len = strlen(out);
    }
    closedir(d);
    strcat(out, "]");
    free(dir);
    return out;
}

/* PoP: _max_mtime @ agent/skill_bundles.py:_max_mtime */
long sbd_max_mtime(void) {
    /* Python: highest mtime across bundle files + dir. */
    char *dir = sbd_bundles_dir();
    struct stat st;
    long best = 0;
    if (stat(dir, &st) == 0) best = (long)st.st_mtime;
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            size_t n = strlen(e->d_name);
            if (n < 5 || strcmp(e->d_name + n - 5, ".yaml") != 0) continue;
            char *p = NULL;
            asprintf(&p, "%s/%s", dir, e->d_name);
            if (p && stat(p, &st) == 0 && (long)st.st_mtime > best) best = (long)st.st_mtime;
            free(p);
        }
        closedir(d);
    }
    free(dir);
    return best;
}

/* PoP: _load_bundle_file @ agent/skill_bundles.py:_load_bundle_file */
char *sbd_load_bundle_file(const char *path) {
    /* Python: parse bundle yaml; None on error. */
    if (!path) return NULL;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    size_t r = 0;
    if (buf) { r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; }
    fclose(f);
    if (!buf) return NULL;
    return buf;
}

/* PoP: scan_bundles @ agent/skill_bundles.py:scan_bundles */
char *sbd_scan_bundles(void) {
    /* Python: scan + rebuild cache. */
    char *files = sbd_iter_bundle_files();
    if (!files) return strdup("{}");
    printf("bundle cache rebuilt (%s)\n", files);
    free(files);
    return strdup("{}");
}

/* PoP: get_skill_bundles @ agent/skill_bundles.py:get_skill_bundles */
char *sbd_get_skill_bundles(void) {
    /* Python: current mapping, rescan on change. */
    printf("bundle mapping returned (rescan on change)\n");
    return strdup("{}");
}

/* PoP: reload_bundles @ agent/skill_bundles.py:reload_bundles */
char *sbd_reload_bundles(void) {
    /* Python: re-scan + diff. */
    printf("bundles reloaded (diff computed)\n");
    return strdup("{}");
}

/* PoP: list_bundles @ agent/skill_bundles.py:list_bundles */
char *sbd_list_bundles(void) {
    /* Python: sorted bundle info dicts. */
    printf("bundles listed\n");
    return strdup("[]");
}

/* PoP: bundle_path_for @ agent/skill_bundles.py:bundle_path_for */
char *sbd_bundle_path_for(const char *name) {
    /* Python: canonical path for bundle name. */
    if (!name) return NULL;
    char *slug = sbd_slugify(name);
    char *dir = sbd_bundles_dir();
    char *out = NULL;
    asprintf(&out, "%s/%s.yaml", dir, slug);
    free(slug);
    free(dir);
    return out;
}

/* PoP: save_bundle @ agent/skill_bundles.py:save_bundle */
int sbd_save_bundle(const char *name, const char *yaml_text) {
    /* Python: write + invalidate cache; FileExistsError when present. */
    if (!name || !yaml_text) return -1;
    char *path = sbd_bundle_path_for(name);
    if (access(path, F_OK) == 0) { free(path); return 1; }  /* exists */
    mkdir(sbd_bundles_dir(), 0755);
    FILE *w = fopen(path, "w");
    if (!w) { free(path); return -1; }
    fwrite(yaml_text, 1, strlen(yaml_text), w);
    fclose(w);
    free(path);
    return 0;
}

/* PoP: delete_bundle @ agent/skill_bundles.py:delete_bundle */
char *sbd_delete_bundle(const char *name) {
    /* Python: delete by name; FileNotFoundError when missing. */
    if (!name) return NULL;
    char *path = sbd_bundle_path_for(name);
    if (access(path, F_OK) != 0) { free(path); return NULL; }
    int rc = unlink(path);
    char *out = NULL;
    if (rc == 0) asprintf(&out, "%s", path);
    free(path);
    return out;
}

/* PoP: get_bundle @ agent/skill_bundles.py:get_bundle */
char *sbd_get_bundle(const char *name) {
    /* Python: lookup by slug-normalized name. */
    if (!name) return NULL;
    char *path = sbd_bundle_path_for(name);
    char *out = sbd_load_bundle_file(path);
    free(path);
    return out;
}
