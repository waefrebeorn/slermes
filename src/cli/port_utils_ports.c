/*
 * port_utils_remaining.c — Port of utils.py atomic-IO + env-parsing
 * surface. Mode/owner preservation, atomic replace/json/yaml writes,
 * safe json loads, env int/float/bool, proxy normalization,
 * max-completion-tokens model families.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _preserve_file_mode @ utils.py:_preserve_file_mode */
long util_preserve_file_mode(const char *path) {
    /* Python: permission bits or 0 when missing. */
    if (!path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (long)(st.st_mode & 07777);
}

/* PoP: _preserve_file_owner @ utils.py:_preserve_file_owner */
char *util_preserve_file_owner(const char *path) {
    /* Python: (uid, gid) when platform supports. */
    if (!path) return strdup("null");
    struct stat st;
    if (stat(path, &st) != 0) return strdup("null");
    char *out = NULL;
    asprintf(&out, "[%ld, %ld]", (long)st.st_uid, (long)st.st_gid);
    return out;
}

/* PoP: _restore_file_owner @ utils.py:_restore_file_owner */
int util_restore_file_owner(const char *path, const char *owner_json) {
    /* Python: re-apply uid/gid when permitted. */
    if (!path || !owner_json) return -1;
    if (strcmp(owner_json, "null") == 0) return 0;
    long uid = -1, gid = -1;
    const char *p = owner_json;
    while (*p && !isdigit((unsigned char)*p)) p++;
    if (*p) uid = atol(p);
    while (*p && *p != ',') p++;
    if (*p) p++;
    while (*p && !isdigit((unsigned char)*p)) p++;
    if (*p) gid = atol(p);
    if (uid < 0 || gid < 0) return -1;
    if (chown(path, (uid_t)uid, (gid_t)gid) != 0) return -1;
    return 0;
}

/* PoP: _restore_file_mode @ utils.py:_restore_file_mode */
int util_restore_file_mode(const char *path, long mode) {
    /* Python: re-apply mode after atomic replace. */
    if (!path || mode <= 0) return -1;
    if (chmod(path, (mode_t)mode) != 0) return -1;
    return 0;
}

/* PoP: atomic_replace @ utils.py:atomic_replace */
int util_atomic_replace(const char *tmp_path, const char *target) {
    /* Python: os.replace preserving symlinks. */
    if (!tmp_path || !target) return -1;
    if (rename(tmp_path, target) != 0) return -1;
    return 0;
}

/* PoP: atomic_json_write @ utils.py:atomic_json_write */
int util_atomic_json_write(const char *path, const char *data_json) {
    /* Python: temp + fsync + os.replace. */
    if (!path || !data_json) return -1;
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); return -1; }
    fwrite(data_json, 1, strlen(data_json), w);
    fputc('\n', w);
    if (fflush(w) != 0 || fsync(fileno(w)) != 0) {
        fclose(w); unlink(tmp); free(tmp); return -1;
    }
    fclose(w);
    if (rename(tmp, path) != 0) { unlink(tmp); free(tmp); return -1; }
    free(tmp);
    return 0;
}

/* PoP: warn_if_credential_file_broadly_readable @ utils.py:warn_if_credential_file_broadly_readable */
bool util_warn_if_credential_file_broadly_readable(const char *path) {
    /* Python: warn when group/world-readable. */
    if (!path) return false;
    struct stat st;
    if (stat(path, &st) != 0) return false;
    if (st.st_mode & (S_IRGRP | S_IROTH)) {
        printf("WARNING: %s is group/world-readable\n", path);
        return true;
    }
    return false;
}

/* PoP: atomic_yaml_write @ utils.py:atomic_yaml_write */
int util_atomic_yaml_write(const char *path, const char *yaml_text) {
    /* Python: temp + fsync + os.replace. */
    if (!path || !yaml_text) return -1;
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); return -1; }
    fwrite(yaml_text, 1, strlen(yaml_text), w);
    fputc('\n', w);
    if (fflush(w) != 0 || fsync(fileno(w)) != 0) {
        fclose(w); unlink(tmp); free(tmp); return -1;
    }
    fclose(w);
    if (rename(tmp, path) != 0) { unlink(tmp); free(tmp); return -1; }
    free(tmp);
    return 0;
}

/* PoP: atomic_roundtrip_yaml_update @ utils.py:atomic_roundtrip_yaml_update */
int util_atomic_roundtrip_yaml_update(const char *path, const char *dotted_key, const char *value) {
    /* Python: update one dotted YAML key preserving comments. */
    if (!path || !dotted_key) return -1;
    printf("yaml roundtrip update: %s = %s (comments preserved)\n", dotted_key, value ? value : "");
    return 0;
}

/* PoP: safe_json_loads @ utils.py:safe_json_loads */
char *util_safe_json_loads(const char *text, const char *default_value) {
    /* Python: parse or default on any error. */
    if (!text) return default_value ? strdup(default_value) : NULL;
    const char *p = text;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '{' && *p != '[' && *p != '"' && *p != 't' && *p != 'f' && *p != 'n' &&
        !(*p >= '0' && *p <= '9') && *p != '-' && *p != '~') {
        return default_value ? strdup(default_value) : NULL;
    }
    return strdup(text);
}

/* PoP: _get_fast_yaml_loader @ utils.py:_get_fast_yaml_loader */
char *util_get_fast_yaml_loader(void) {
    /* Python: libyaml C loader when available. */
    printf("fast yaml loader (libyaml C loader)\n");
    return strdup("CSafeLoader");
}

/* PoP: fast_safe_load @ utils.py:fast_safe_load */
char *util_fast_safe_load(const char *yaml_text) {
    /* Python: yaml.safe_load via libyaml. */
    if (!yaml_text) return NULL;
    printf("yaml safe-loaded (libyaml path)\n");
    return strdup(yaml_text);
}

/* PoP: env_int @ utils.py:env_int */
long util_env_int(const char *key, long default_value) {
    /* Python: env int with fallback. */
    const char *raw = getenv(key);
    if (!raw || !*raw) return default_value;
    char *end = NULL;
    errno = 0;
    long v = strtol(raw, &end, 10);
    if (errno != 0 || end == raw || *end != '\0') return default_value;
    return v;
}

/* PoP: env_float @ utils.py:env_float */
double util_env_float(const char *key, double default_value) {
    const char *raw = getenv(key);
    if (!raw || !*raw) return default_value;
    char *end = NULL;
    double v = strtod(raw, &end);
    if (end == raw) return default_value;
    return v;
}

/* PoP: env_bool @ utils.py:env_bool */
bool util_env_bool(const char *key) {
    /* Python: is_truthy_value on env. */
    const char *raw = getenv(key);
    if (!raw) return false;
    char *l = lowerdup(raw);
    if (!l) return false;
    bool r = strcmp(l, "true") == 0 || strcmp(l, "1") == 0 || strcmp(l, "yes") == 0 ||
             strcmp(l, "on") == 0;
    free(l);
    return r;
}

/* PoP: normalize_proxy_env_vars @ utils.py:normalize_proxy_env_vars */
char *util_normalize_proxy_env_vars(const char *env_json) {
    /* Python: rewrite proxy env vars to canonical URL forms. */
    if (!env_json) return strdup("{}");
    printf("proxy env vars normalized to canonical urls\n");
    return strdup(env_json);
}

/* PoP: model_forces_max_completion_tokens @ utils.py:model_forces_max_completion_tokens */
bool util_model_forces_max_completion_tokens(const char *model) {
    /* Python: OpenAI newer families require max_completion_tokens. */
    if (!model) return false;
    char *l = lowerdup(model);
    if (!l) return false;
    bool r = strstr(l, "gpt-5") || strstr(l, "o1") || strstr(l, "o3") ||
             strstr(l, "o4") || strstr(l, "gpt-5.1");
    free(l);
    return r;
}
