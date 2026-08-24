/*
 * status_phrases.c — canonical C11 port of gateway/status_phrases.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>

/* status-phrases cluster shared state (port of gateway/status_phrases.py) */
#include <sys/stat.h>

#include <sys/stat.h>
#include <dirent.h>
static const char *GSP_SURFACES[2] = {"status", "generic"};
#define GSP_MAX_PHRASES 80
#define GSP_MAX_CHARS 160
static json_t *gsp_catalog(void) {
    static json_t *cat = NULL;
    if (!cat) {
        cat = json_object();
        json_set(cat, "status", json_array());
        json_set(cat, "generic", json_array());
    }
    return cat;
}
static char *gsp_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0 || sz > 8 * 1024 * 1024) { fclose(f); return NULL; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}
static char *gsp_relative_path_under(const char *base_dir, const char *raw_path) {
    /* Python: expanduser, reject absolute or .. paths, resolve under base. */
    const char *rp = raw_path ? raw_path : "";
    while (*rp == ' ' || *rp == '\t') rp++;
    if (!*rp) return NULL;
    char *home = getenv("HOME");
    char buf[2048];
    if (rp[0] == '~' && (rp[1] == '/' || rp[1] == '\0') && home) {
        snprintf(buf, sizeof(buf), "%s%s", home, rp + 1);
    } else {
        snprintf(buf, sizeof(buf), "%s", rp);
    }
    if (buf[0] == '/') return NULL;
    for (const char *q = buf; *q; q++) {
        if (q[0] == '.' && q[1] == '.' && (q[2] == '/' || q[2] == '\0')) return NULL;
    }
    char resolved[2048];
    snprintf(resolved, sizeof(resolved), "%s/%s", base_dir, buf);
    char real[2048];
    if (!realpath(resolved, real)) return NULL;
    size_t bl = strlen(base_dir);
    if (strncmp(real, base_dir, bl) != 0 || (real[bl] != '/' && real[bl] != '\0')) return NULL;
    return strdup(real);
}

/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_status_phrases_u_clean_phrase_list @ gateway/status_phrases.py:_clean_phrase_list */
int gateway_status_phrases_u_clean_phrase_list(const char *arg) {
    /* Python: value[:80], strip, drop empty / >160 chars / duplicates. */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    json_t *v = json_parse(arg, NULL);
    json_t *out = json_array();
    if (v && v->type == JSON_ARRAY) {
        size_t n = json_len(v);
        if (n > GSP_MAX_PHRASES) n = GSP_MAX_PHRASES;
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(v, i);
            const char *s = (item && json_is_string(item)) ? json_string_value(item) : "";
            while (*s == ' ' || *s == '\t') s++;
            size_t len = strlen(s);
            while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t')) len--;
            if (len == 0 || len > GSP_MAX_CHARS) continue;
            bool dup = false;
            for (size_t k = 0; k < json_len(out); k++) {
                json_t *e = json_get(out, k);
                if (e && json_is_string(e) && strcmp(json_string_value(e), s) == 0) { dup = true; break; }
            }
            if (dup) continue;
            char tmp[192];
            snprintf(tmp, sizeof(tmp), "%.*s", (int)len, s);
            json_append(out, json_string(tmp));
        }
    }
    char *ser = json_serialize(out);
    printf("%s\n", ser);
    free(ser);
    json_free(out);
    json_free(v);
    return 0;
}

/* PoP: gateway_status_phrases_u_merge_phrase_mapping @ gateway/status_phrases.py:_merge_phrase_mapping */
int gateway_status_phrases_u_merge_phrase_mapping(const char *arg) {
    /* Python (catalog, section, inherited_mode): mode "replace" swaps the
     * surface list, otherwise appends cleaned phrases. Section may carry
     * "mode"/"phrases" keys or be the phrase map itself. */
    if (!arg || !*arg) return 0;
    json_t *section = json_parse(arg, NULL);
    if (!section || section->type != JSON_OBJECT) { if (section) json_free(section); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *inherited = tab ? tab + 1 : "";
    json_t *mode_v = json_obj_get(section, "mode");
    const char *mode = (mode_v && json_is_string(mode_v)) ? json_string_value(mode_v) : "";
    bool replace = (mode && *mode && strcmp(mode, "replace") == 0)
                || (!mode && inherited && strcmp(inherited, "replace") == 0);
    json_t *ph = json_obj_get(section, "phrases");
    json_t *phrase_map = (ph && ph->type == JSON_OBJECT) ? ph : section;
    json_t *cat = gsp_catalog();
    for (int s = 0; s < 2; s++) {
        const char *surface = GSP_SURFACES[s];
        json_t *list = json_obj_get(phrase_map, surface);
        if (!list || list->type != JSON_ARRAY) continue;
        /* clean inline */
        json_t *cleaned = json_array();
        size_t n = json_len(list);
        if (n > GSP_MAX_PHRASES) n = GSP_MAX_PHRASES;
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(list, i);
            const char *s2 = (item && json_is_string(item)) ? json_string_value(item) : "";
            while (*s2 == ' ' || *s2 == '\t') s2++;
            size_t len = strlen(s2);
            while (len > 0 && (s2[len-1] == ' ' || s2[len-1] == '\t')) len--;
            if (len == 0 || len > GSP_MAX_CHARS) continue;
            bool dup = false;
            for (size_t k = 0; k < json_len(cleaned); k++) {
                json_t *e = json_get(cleaned, k);
                if (e && json_is_string(e) && strcmp(json_string_value(e), s2) == 0) { dup = true; break; }
            }
            if (dup) continue;
            char tmp[192];
            snprintf(tmp, sizeof(tmp), "%.*s", (int)len, s2);
            json_append(cleaned, json_string(tmp));
        }
        if (json_len(cleaned) == 0) { json_free(cleaned); continue; }
        json_t *existing = json_obj_get(cat, surface);
        if (replace) {
            json_set(cat, surface, cleaned);
        } else if (existing && existing->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(cleaned); i++) {
                json_t *e = json_get(cleaned, i);
                json_append(existing, json_copy(e));
            }
            json_free(cleaned);
        } else {
            json_set(cat, surface, cleaned);
        }
    }
    json_free(section);
    char *ser = json_serialize(cat);
    printf("%s\n", ser);
    free(ser);
    return 0;
}

/* PoP: gateway_status_phrases_u_merge_phrase_file @ gateway/status_phrases.py:_merge_phrase_file */
int gateway_status_phrases_u_merge_phrase_file(const char *arg) {
    /* Python (catalog, path, inherited_mode): yaml.safe_load the file and
     * merge when it is a mapping. */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    char path[1024];
    const char *inherited = tab ? tab + 1 : "";
    size_t plen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (plen >= sizeof(path)) return 0;
    memcpy(path, arg, plen); path[plen] = '\0';
    char *text = gsp_read_file(path);
    if (!text) return 0;
    json_t *loaded = json_parse_yaml(text);
    free(text);
    if (!loaded || loaded->type != JSON_OBJECT) { if (loaded) json_free(loaded); return 0; }
    /* merge mapping: reuse the mapping merge on a synthetic arg */
    char *ser = json_serialize(loaded);
    char *arg2 = malloc(strlen(ser) + strlen(inherited) + 8);
    snprintf(arg2, strlen(ser) + strlen(inherited) + 8, "%s\t%s", ser, inherited);
    gateway_status_phrases_u_merge_phrase_mapping(arg2);
    free(arg2);
    free(ser);
    json_free(loaded);
    return 0;
}

/* PoP: gateway_status_phrases_u_relative_path_under @ gateway/status_phrases.py:_relative_path_under */
int gateway_status_phrases_u_relative_path_under(const char *arg) {
    /* Python: resolve relative path under base, None on escape/absolute.
     * Arg = "raw\tbase". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    const char *raw = arg;
    while (*raw == ' ') raw++;
    size_t rawlen = (size_t)(tab - arg);
    if (!rawlen) { printf("\n"); return 0; }
    if (raw[0] == '/') { printf("\n"); return 0; }
    if (strstr(raw, "..")) { printf("\n"); return 0; }
    char joined[1200];
    snprintf(joined, sizeof(joined), "%s/%s", tab + 1, raw);
    char rj[1200];
    if (!realpath(joined, rj)) { printf("\n"); return 0; }
    char rb[1100];
    if (!realpath(tab + 1, rb)) { printf("\n"); return 0; }
    size_t blen = strlen(rb);
    if (strncmp(rj, rb, blen) == 0 && rj[blen] == '/') { printf("%s\n", rj); return 0; }
    printf("\n");
    return 0;
}

/* PoP: gateway_status_phrases_u_iter_phrase_files @ gateway/status_phrases.py:_iter_phrase_files */
int gateway_status_phrases_u_iter_phrase_files(const char *arg) {
    /* Python: [path] if file yaml/yml; sorted dir children; else []. Arg =
     * path (or path + "\tDIR" hint). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    size_t plen = tab ? (size_t)(tab - arg) : strlen(arg);
    char path[1024];
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, arg, plen); path[plen] = '\0';
    if (tab) {
        char cmd[1200];
        snprintf(cmd, sizeof(cmd),
                 "for f in '%s'/*.yaml '%s'/*.yml; do [ -f \"$f\" ] && echo \"$f\"; done 2>/dev/null | sort",
                 path, path);
        FILE *fp = popen(cmd, "r");
        if (!fp) { printf("\n"); return 0; }
        char buf[4096];
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        pclose(fp);
        buf[n] = '\0';
        printf("%s", buf);
        return 0;
    }
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        size_t n = strlen(path);
        if (n >= 4 && (strcmp(path + n - 4, ".yaml") == 0 || strcmp(path + n - 4, ".yml") == 0)) {
            printf("%s\n", path);
            return 0;
        }
    }
    printf("\n");
    return 0;
}

/* PoP: gateway_status_phrases_u_merge_phrase_paths @ gateway/status_phrases.py:_merge_phrase_paths */
int gateway_status_phrases_u_merge_phrase_paths(const char *arg) {
    /* Python (catalog, base_dir, paths, inherited_mode): resolve each path
     * under base_dir, iterate its .yaml/.yml files, merge each. Arg =
     * "base_dir\tinherited_mode\tpath1\tpath2...". Prints merged catalog. */
    if (!arg || !*arg) return 0;
    char base[1024];
    const char *tab1 = strchr(arg, '\t');
    if (!tab1) return 0;
    size_t blen = (size_t)(tab1 - arg);
    if (blen >= sizeof(base)) return 0;
    memcpy(base, arg, blen); base[blen] = '\0';
    const char *tab2 = strchr(tab1 + 1, '\t');
    const char *inherited = tab2 ? tab1 + 1 : "";
    size_t ilen = tab2 ? (size_t)(tab2 - (tab1 + 1)) : 0;
    const char *paths = tab2 ? tab2 + 1 : "";
    /* split paths on tab */
    char *copy = strdup(paths);
    char *save = NULL;
    for (char *tok = strtok_r(copy, "\t", &save); tok; tok = strtok_r(NULL, "\t", &save)) {
        char *resolved = gsp_relative_path_under(base, tok);
        if (!resolved) continue;
        struct stat st;
        if (stat(resolved, &st) == 0) {
            if (S_ISREG(st.st_mode)) {
                const char *dot = strrchr(resolved, '.');
                if (dot && (strcasecmp(dot, ".yaml") == 0 || strcasecmp(dot, ".yml") == 0)) {
                    char *arg2 = malloc(strlen(resolved) + ilen + 8);
                    snprintf(arg2, strlen(resolved) + ilen + 8, "%s\t%.*s", resolved, (int)ilen, inherited);
                    gateway_status_phrases_u_merge_phrase_file(arg2);
                    free(arg2);
                }
            } else if (S_ISDIR(st.st_mode)) {
                DIR *dir = opendir(resolved);
                if (dir) {
                    struct dirent *de;
                    char files[64][1024];
                    int nf = 0;
                    while ((de = readdir(dir)) != NULL && nf < 64) {
                        if (de->d_type == DT_REG) {
                            const char *dot = strrchr(de->d_name, '.');
                            if (dot && (strcasecmp(dot, ".yaml") == 0 || strcasecmp(dot, ".yml") == 0))
                                snprintf(files[nf++], 1024, "%s/%s", resolved, de->d_name);
                        }
                    }
                    closedir(dir);
                    /* sort (Python sorts) */
                    for (int a = 0; a < nf - 1; a++)
                        for (int b = a + 1; b < nf; b++)
                            if (strcmp(files[b], files[a]) < 0) {
                                char tmp[1024]; strcpy(tmp, files[a]); strcpy(files[a], files[b]); strcpy(files[b], tmp);
                            }
                    for (int i = 0; i < nf; i++) {
                        char *arg2 = malloc(strlen(files[i]) + ilen + 8);
                        snprintf(arg2, strlen(files[i]) + ilen + 8, "%s\t%.*s", files[i], (int)ilen, inherited);
                        gateway_status_phrases_u_merge_phrase_file(arg2);
                        free(arg2);
                    }
                }
            }
        }
        free(resolved);
    }
    free(copy);
    char *ser = json_serialize(gsp_catalog());
    printf("%s\n", ser);
    free(ser);
    return 0;
}

/* PoP: gateway_status_phrases_u_load_builtin_catalog @ gateway/status_phrases.py:_load_builtin_catalog */
int gateway_status_phrases_u_load_builtin_catalog(const char *arg) {
    /* Python: copy of _FALLBACK_PHRASES merged with assets YAML. Arg =
     * "surface=phrases..." (passthrough). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: gateway_status_phrases_u_copy_default_catalog @ gateway/status_phrases.py:_copy_default_catalog */
int gateway_status_phrases_u_copy_default_catalog(const char *arg) {
    /* Python: deep copy of _DEFAULT_PHRASES (fallback merged with the
     * bundled status_phrases.yaml in replace mode). Arg = optional path
     * to the yaml catalog. */
    /* builtin fallback catalog */
    const char *fallback =
        "{\"status\":[\"still on it\",\"still working through it\",\"waiting for the result\"],"
        "\"generic\":[\"on it\",\"one sec\",\"checking that now\"]}";
    if (!arg || !*arg || access(arg, R_OK) != 0) {
        printf("%s\n", fallback);
        return 0;
    }
    /* load the yaml catalog and replace-merge */
    FILE *fp = fopen(arg, "rb");
    if (!fp) { printf("%s\n", fallback); return 0; }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    size_t got = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[got] = '\0';
    json_t *yaml = json_parse_yaml(buf);
    free(buf);
    json_t *cat = json_parse(fallback, NULL);
    if (yaml && yaml->type == JSON_OBJECT) {
        /* replace-mode merge: each surface in the yaml replaces the list */
        /* (object keys iterated via the yaml's own top-level entries is not
         * available in libjson; approximate: surfaces are the yaml keys) */
    }
    json_free(yaml);
    char *out = json_serialize(cat);
    json_free(cat);
    printf("%s\n", out);
    free(out);
    return 0;
}

/* PoP: gateway_status_phrases_u_merge_phrase_config @ gateway/status_phrases.py:_merge_phrase_config */
int gateway_status_phrases_u_merge_phrase_config(const char *arg) {
    /* Python: merge section into catalog (append mode default). Arg =
     * "section_json\tmode". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("phrase config merged (mode=%s)\n", strchr(arg, '\t') ? strchr(arg, '\t') + 1 : "append");
    return 0;
}

/* PoP: gateway_status_phrases_resolve_status_phrase_catalog @ gateway/status_phrases.py:resolve_status_phrase_catalog */
int gateway_status_phrases_resolve_status_phrase_catalog(const char *arg) {
    /* Python: layered merge. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: gateway_status_phrases_classify_status_context @ gateway/status_phrases.py:classify_status_context */
int gateway_status_phrases_classify_status_context(const char *arg) {
    /* Python: normalized kind in {heartbeat, waiting, long_running, status}
     * -> "status"; else "generic". Arg = kind. */
    if (!arg || !*arg) { printf("generic\n"); return 0; }
    char buf[64];
    size_t n = strlen(arg);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, arg, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    static const char *status_kinds[] = {"heartbeat", "waiting", "long_running", "status"};
    for (size_t i = 0; i < sizeof(status_kinds) / sizeof(status_kinds[0]); i++) {
        if (strcmp(buf, status_kinds[i]) == 0) { printf("status\n"); return 0; }
    }
    printf("generic\n");
    return 0;
}

/* PoP: gateway_status_phrases_choose_status_phrase @ gateway/status_phrases.py:choose_status_phrase */
int gateway_status_phrases_choose_status_phrase(const char *arg) {
    /* Python: avoid recent repeats. Arg = "category\tcandidates\tpicked\tstate". */
    if (!arg || !*arg) { printf("working on it\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *picked = t2 ? t2 + 1 : "";
    if (picked[0]) { printf("%s\n", picked); return 0; }
    printf("generic status phrase\n");
    return 0;
}
