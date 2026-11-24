/*
 * runtime.c - C11 port of upstream hermes_platform/host/runtime.py
 * (gate w24_runtime_d45154).
 *
 * PoP: w24_runtime_d45154_termux @ hermes_platform/host/runtime.py:is_termux
 * PoP: w24_runtime_d45154_wsl @ hermes_platform/host/runtime.py:is_wsl
 * PoP: w24_runtime_d45154_proc_marker @ hermes_platform/host/runtime.py:_proc_file_has_marker
 * PoP: w24_runtime_d45154_root_mount @ hermes_platform/host/runtime.py:_root_mount_has_marker
 * PoP: w24_runtime_d45154_container @ hermes_platform/host/runtime.py:_detect_container
 * PoP: w24_runtime_d45154_constants @ hermes_platform/host/runtime.py:<marker tuples>
 *
 * Ported: every predicate as a function of the values it reads.  The seams are
 * os.getenv, open("/proc/version"), os.path.exists("/.dockerenv",
 * "/run/.containerenv") and the two /proc reads -- each is passed in as a value
 * here, so no file is opened and no process environment is consulted.  The
 * memo globals (_wsl_detected, _container_detected) belong to the caller: a
 * cache is not a decision.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_runtime_d45154.h"

/* _detect_container's /proc/1/cgroup markers. */
static const char *const RT_CGROUP_MARKERS[W24_RUNTIME_D45154_CGROUP_MARKERS] = {
    "docker", "podman", "/lxc/", "kubepods", "containerd", "crio",
};
/* Under cgroup v2 only the root mount identifies this process's container. */
static const char *const RT_ROOT_MARKERS[W24_RUNTIME_D45154_ROOT_MARKERS] = {
    "kubepods", "containerd", "crio",
};

static int rt_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* os.getenv(name, ""): a non-string env value is str()'d by Python only when it
 * came from a dict, so the shapes a fixture can carry are accepted as text. */
static const char *rt_text(const json_t *v, const char *def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    if (v->type == JSON_BOOL) return v->bool_val ? "True" : "False";
    return def;
}

/* str.lower() over ASCII, which is what /proc and the Termux fragment carry. */
static char *rt_lower(const char *s) {
    size_t i, n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        char c = s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[n] = '\0';
    return out;
}

/* Python's str.split() element count/field access: whitespace runs collapse. */
static int rt_field_at(const char *line, size_t len, size_t want, size_t *start, size_t *flen) {
    size_t i = 0, field = 0;
    while (i < len) {
        while (i < len && (line[i] == ' ' || line[i] == '\t' || line[i] == '\v' ||
                           line[i] == '\f' || line[i] == '\r' || line[i] == '\n'))
            i++;
        if (i >= len) break;
        *start = i;
        while (i < len && !(line[i] == ' ' || line[i] == '\t' || line[i] == '\v' ||
                            line[i] == '\f' || line[i] == '\r' || line[i] == '\n'))
            i++;
        *flen = i - *start;
        if (field == want) return 1;
        field++;
    }
    return 0;
}

static size_t rt_field_count(const char *line, size_t len) {
    size_t i = 0, n = 0;
    while (i < len) {
        while (i < len && (line[i] == ' ' || line[i] == '\t' || line[i] == '\v' ||
                           line[i] == '\f' || line[i] == '\r' || line[i] == '\n'))
            i++;
        if (i >= len) break;
        n++;
        while (i < len && !(line[i] == ' ' || line[i] == '\t' || line[i] == '\v' ||
                            line[i] == '\f' || line[i] == '\r' || line[i] == '\n'))
            i++;
    }
    return n;
}

/* any(marker in content for marker in markers) */
static int rt_any_marker(const char *content, const char *const *markers, size_t n) {
    size_t i;
    if (!content) return 0;
    for (i = 0; i < n; i++)
        if (markers[i] && strstr(content, markers[i])) return 1;
    return 0;
}

/* The "markers" array of a request, or a default tuple when it is absent. */
static const json_t *rt_marker_array(const json_t *proc) {
    const json_t *m = proc && proc->type == JSON_OBJECT ? json_obj_get(proc, "markers") : NULL;
    return (m && m->type == JSON_ARRAY) ? m : NULL;
}

static int rt_marker_in_array(const char *content, const json_t *markers) {
    size_t i, n = markers ? json_len(markers) : 0;
    for (i = 0; i < n; i++) {
        const json_t *m = json_get(markers, i);
        if (m && m->type == JSON_STRING && m->str_val && strstr(content, m->str_val)) return 1;
    }
    return 0;
}

/* _root_mount_has_marker: splitlines(), then only the lines whose whitespace
 * field 5 (index 4) is exactly "/" are searched -- scanning every mountinfo
 * line misclassifies a host that merely runs containers. */
struct rt_scan {
    const char *const *names;   /* static tuple */
    size_t n;
    const json_t *array;        /* request-supplied marker array, or NULL */
};

static int rt_scan_line(const char *line, const struct rt_scan *s) {
    size_t i;
    if (s->array) return rt_marker_in_array(line, s->array);
    for (i = 0; i < s->n; i++)
        if (s->names[i] && strstr(line, s->names[i])) return 1;
    return 0;
}

static int rt_root_mount_scan(const char *content, const struct rt_scan *s) {
    const char *cur = content ? content : "";
    while (*cur) {
        const char *nl = strchr(cur, '\n');
        size_t len = nl ? (size_t)(nl - cur) : strlen(cur);
        size_t start = 0, flen = 0;
        if (rt_field_count(cur, len) >= 5 && rt_field_at(cur, len, 4, &start, &flen) &&
            flen == 1 && cur[start] == '/') {
            char *line = (char *)malloc(len + 1);
            int hit;
            if (!line) return 0;
            memcpy(line, cur, len);
            line[len] = '\0';
            hit = rt_scan_line(line, s);
            free(line);
            if (hit) return 1;
        }
        if (!nl) break;
        cur = nl + 1;
    }
    return 0;
}

json_t *w24_runtime_d45154_termux(const json_t *env) {
    const char *prefix;
    if (!env || env->type != JSON_OBJECT) return json_bool(false);
    if (rt_truthy(json_obj_get(env, "TERMUX_VERSION"))) return json_bool(true);
    prefix = rt_text(json_obj_get(env, "PREFIX"), "");
    return json_bool(strstr(prefix, "com.termux/files/usr") ? true : false);
}

json_t *w24_runtime_d45154_wsl(const json_t *proc) {
    const json_t *v = proc && proc->type == JSON_OBJECT ? json_obj_get(proc, "proc_version") : NULL;
    char *low;
    int hit;
    if (!v || v->type != JSON_STRING || !v->str_val) return json_bool(false);  /* OSError arm */
    low = rt_lower(v->str_val);
    if (!low) return json_bool(false);
    hit = strstr(low, "microsoft") != NULL;
    free(low);
    return json_bool(hit ? true : false);
}

json_t *w24_runtime_d45154_proc_marker(const json_t *proc) {
    const json_t *c = proc && proc->type == JSON_OBJECT ? json_obj_get(proc, "content") : NULL;
    const json_t *markers = rt_marker_array(proc);
    const char *content = (c && c->type == JSON_STRING) ? (c->str_val ? c->str_val : "") : "";
    if (markers) return json_bool(rt_marker_in_array(content, markers) ? true : false);
    return json_bool(rt_any_marker(content, RT_CGROUP_MARKERS,
                                   W24_RUNTIME_D45154_CGROUP_MARKERS) ? true : false);
}

json_t *w24_runtime_d45154_root_mount(const json_t *proc) {
    const json_t *c = proc && proc->type == JSON_OBJECT ? json_obj_get(proc, "content") : NULL;
    const json_t *markers = rt_marker_array(proc);
    const char *content = (c && c->type == JSON_STRING) ? (c->str_val ? c->str_val : "") : "";
    struct rt_scan scan;

    scan.names = RT_ROOT_MARKERS;
    scan.n = W24_RUNTIME_D45154_ROOT_MARKERS;
    scan.array = markers;
    return json_bool(rt_root_mount_scan(content, &scan) ? true : false);
}

json_t *w24_runtime_d45154_container(const json_t *env) {
    const json_t *cg, *mi;
    const char *text;
    struct rt_scan scan;

    if (!env || env->type != JSON_OBJECT) return json_bool(false);
    if (rt_truthy(json_obj_get(env, "dockerenv"))) return json_bool(true);
    if (rt_truthy(json_obj_get(env, "containerenv"))) return json_bool(true);
    if (rt_truthy(json_obj_get(env, "k8s_host"))) return json_bool(true);

    /* _proc_file_has_marker("/proc/1/cgroup", ...) */
    cg = json_obj_get(env, "cgroup");
    text = (cg && cg->type == JSON_STRING) ? (cg->str_val ? cg->str_val : "") : "";
    if (rt_any_marker(text, RT_CGROUP_MARKERS, W24_RUNTIME_D45154_CGROUP_MARKERS))
        return json_bool(true);

    /* _root_mount_has_marker("/proc/self/mountinfo", ...) */
    mi = json_obj_get(env, "mountinfo");
    text = (mi && mi->type == JSON_STRING) ? (mi->str_val ? mi->str_val : "") : "";
    scan.names = RT_ROOT_MARKERS;
    scan.n = W24_RUNTIME_D45154_ROOT_MARKERS;
    scan.array = NULL;
    return json_bool(rt_root_mount_scan(text, &scan) ? true : false);
}

