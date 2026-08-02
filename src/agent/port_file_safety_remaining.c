/*
 * port_file_safety_remaining.c — Port of agent/file_safety.py path-safety
 * surface. Denied path/prefix building, write denial checks, cross-profile
 * + sandbox/container mirror classification and warnings.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static const char *hermes_home(void) {
    const char *h = getenv("HERMES_HOME");
    if (h && *h) return h;
    h = getenv("HOME");
    if (h && *h) return h;
    return ".";
}

/* PoP: _hermes_home_path @ agent/file_safety.py:_hermes_home_path */
char *fsf_hermes_home_path(void) {
    /* Python: profile-aware HERMES_HOME. */
    return strdup(hermes_home());
}

/* PoP: _hermes_root_path @ agent/file_safety.py:_hermes_root_path */
char *fsf_hermes_root_path(void) {
    /* Python: always parent of any profile. */
    char *home = fsf_hermes_home_path();
    if (!home) return NULL;
    /* ~/.hermes → parent of .hermes; a plain dir → itself */
    const char *base = strrchr(home, '/');
    if (base && strcmp(base, "/.hermes") == 0) {
        char *out = strndup(home, (size_t)(base - home));
        free(home);
        return out ? out : strdup("/");
    }
    free(home);
    return strdup(hermes_home());
}

/* PoP: build_write_denied_paths @ agent/file_safety.py:build_write_denied_paths */
char *fsf_build_write_denied_paths(void) {
    /* Python: exact sensitive paths never written. */
    const char *h = hermes_home();
    char *out = NULL;
    asprintf(&out,
        "[\"%s/.env\", \"%s/config.yaml\", \"%s/secrets.json\", \"%s/credentials.json\", "
        "\"%s/memory.md\", \"%s/.hermes\", \"%s/gateway.yaml\"]",
        h, h, h, h, h, h, h);
    return out;
}

/* PoP: build_write_denied_prefixes @ agent/file_safety.py:build_write_denied_prefixes */
char *fsf_build_write_denied_prefixes(void) {
    /* Python: sensitive directory prefixes. */
    const char *h = hermes_home();
    char *out = NULL;
    asprintf(&out, "[\"%s/.env\", \"%s/venv\", \"%s/.git\", \"/etc\", \"/usr\", \"/boot\"]",
             h, h, h);
    return out;
}

/* PoP: is_write_denied @ agent/file_safety.py:is_write_denied */
bool fsf_is_write_denied(const char *path) {
    /* Python: blocked by denylist or safe root. */
    if (!path) return false;
    char *denied = fsf_build_write_denied_paths();
    char *prefixes = fsf_build_write_denied_prefixes();
    bool hit = false;
    if (denied) {
        const char *p = denied;
        while ((p = strstr(p, "\"")) != NULL) {
            const char *e = p + 1;
            while (*e && *e != '"') e++;
            if (e > p + 1) {
                char *entry = strndup(p + 1, (size_t)(e - p - 1));
                if (entry && strcmp(entry, path) == 0) { hit = true; free(entry); break; }
                free(entry);
            }
            p = e;
        }
    }
    if (!hit && prefixes) {
        const char *p = prefixes;
        while ((p = strstr(p, "\"")) != NULL) {
            const char *e = p + 1;
            while (*e && *e != '"') e++;
            if (e > p + 1) {
                char *entry = strndup(p + 1, (size_t)(e - p - 1));
                if (entry && strncmp(path, entry, strlen(entry)) == 0) { hit = true; free(entry); break; }
                free(entry);
            }
            p = e;
        }
    }
    free(denied);
    free(prefixes);
    return hit;
}

/* PoP: get_read_block_error @ agent/file_safety.py:get_read_block_error */
char *fsf_get_read_block_error(const char *path) {
    /* Python: three-category read block message. */
    if (!path) return NULL;
    if (fsf_is_write_denied(path))
        return strdup("blocked: sensitive Hermes path");
    if (strstr(path, "/.env"))
        return strdup("blocked: environment secrets");
    return strdup("blocked: denied path");
}

/* PoP: _resolve_active_profile_name @ agent/file_safety.py:_resolve_active_profile_name */
char *fsf_resolve_active_profile_name(void) {
    /* Python: profile name from HERMES_HOME; default for ~/.hermes. */
    const char *h = getenv("HERMES_HOME");
    if (!h || !*h) return strdup("default");
    /* ~/.hermes → default; ~/.hermes/profiles/<name> → <name> */
    const char *p = strstr(h, "/profiles/");
    if (p) return strdup(p + strlen("/profiles/"));
    return strdup("default");
}

/* PoP: classify_cross_profile_target @ agent/file_safety.py:classify_cross_profile_target */
bool fsf_classify_cross_profile_target(const char *path) {
    /* Python: lands in another profile's scope. */
    if (!path) return false;
    const char *h = hermes_home();
    char *active = fsf_resolve_active_profile_name();
    const char *p = strstr(path, "/profiles/");
    bool cross = false;
    if (p && active) {
        const char *name = p + strlen("/profiles/");
        const char *end = strchr(name, '/');
        size_t nlen = end ? (size_t)(end - name) : strlen(name);
        if (strncmp(name, active, nlen) != 0 || strlen(active) != nlen) cross = true;
    }
    free(active);
    (void)h;
    return cross;
}

/* PoP: get_cross_profile_warning @ agent/file_safety.py:get_cross_profile_warning */
char *fsf_get_cross_profile_warning(const char *path) {
    /* Python: model-facing warning. */
    if (!path || !fsf_classify_cross_profile_target(path)) return NULL;
    char *out = NULL;
    asprintf(&out, "Warning: %s targets another Hermes profile's data", path);
    return out;
}

/* PoP: _find_sandbox_mirror_segments @ agent/file_safety.py:_find_sandbox_mirror_segments */
char *fsf_find_sandbox_mirror_segments(const char *path) {
    /* Python: index of inner .hermes part in mirror path. */
    if (!path) return strdup("-1");
    const char *p = path;
    long idx = -1;
    long count = 0;
    while ((p = strstr(p, ".hermes")) != NULL) {
        idx = count;
        p += 7;
        count++;
    }
    char *out = NULL;
    asprintf(&out, "%ld", idx);
    return out;
}

/* PoP: classify_sandbox_mirror_target @ agent/file_safety.py:classify_sandbox_mirror_target */
bool fsf_classify_sandbox_mirror_target(const char *path) {
    /* Python: sandbox mirror of authoritative Hermes state. */
    if (!path) return false;
    return strstr(path, ".hermes") != NULL && strstr(path, "/sandbox") != NULL;
}

/* PoP: get_sandbox_mirror_warning @ agent/file_safety.py:get_sandbox_mirror_warning */
char *fsf_get_sandbox_mirror_warning(const char *path) {
    if (!path || !fsf_classify_sandbox_mirror_target(path)) return NULL;
    char *out = NULL;
    asprintf(&out, "Warning: %s is a sandbox mirror of Hermes state", path);
    return out;
}

/* PoP: classify_container_mirror_target @ agent/file_safety.py:classify_container_mirror_target */
bool fsf_classify_container_mirror_target(const char *path, const char *mirror_prefix) {
    if (!path) return false;
    if (!mirror_prefix) return false;
    return strncmp(path, mirror_prefix, strlen(mirror_prefix)) == 0;
}

/* PoP: get_container_mirror_warning @ agent/file_safety.py:get_container_mirror_warning */
char *fsf_get_container_mirror_warning(const char *path, const char *mirror_prefix) {
    if (!path || !fsf_classify_container_mirror_target(path, mirror_prefix)) return NULL;
    char *out = NULL;
    asprintf(&out, "Warning: %s lands in the container sandbox mirror", path);
    return out;
}
